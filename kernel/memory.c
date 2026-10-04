#include "memory.h"
#include "limine.h"
#include <stddef.h>

__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request g_hhdm_req = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request g_memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

uint64_t g_hhdm_offset = 0;

#define PMM_MAX_REGIONS 64
#define PMM_MAX_FREE_RANGES 16384
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

typedef struct {
    uint64_t base;
    uint64_t end;
} pmm_region_t;

typedef struct {
    uint64_t base;
    uint64_t end;
} pmm_range_t;

static pmm_region_t g_regions[PMM_MAX_REGIONS];
static size_t g_region_count = 0;
static pmm_range_t g_free_ranges[PMM_MAX_FREE_RANGES];
static size_t g_free_range_count = 0;
static uint64_t g_kernel_pml4_phys = 0;
static uint64_t g_total_usable_ram = 0;
static uint64_t g_allocated_bytes = 0;
static uint64_t g_peak_allocated_bytes = 0;
static uint64_t g_allocation_count = 0;
static uint64_t g_free_count = 0;
static uint64_t g_failed_allocations = 0;
static volatile uint8_t g_pmm_lock = 0;

static uint64_t pmm_lock(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    while (__atomic_test_and_set(&g_pmm_lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    return flags;
}

static void pmm_unlock(uint64_t flags) {
    __atomic_clear(&g_pmm_lock, __ATOMIC_RELEASE);
    if (flags & (1ULL << 9)) __asm__ volatile("sti" ::: "memory");
}

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + g_hhdm_offset);
}

static int range_overlaps(uint64_t a_base, uint64_t a_end, uint64_t b_base, uint64_t b_end) {
    return a_base < b_end && b_base < a_end;
}

static int pmm_managed_range(uint64_t base, uint64_t end) {
    for (size_t i = 0; i < g_region_count; i++) {
        if (base >= g_regions[i].base && end <= g_regions[i].end) return 1;
    }
    return 0;
}

static int pmm_insert_free_range(uint64_t base, uint64_t end) {
    if (base >= end || g_free_range_count >= PMM_MAX_FREE_RANGES) return -1;

    size_t pos = 0;
    while (pos < g_free_range_count && g_free_ranges[pos].base < base) pos++;

    if (pos > 0 && g_free_ranges[pos - 1].end > base) return -1;
    if (pos < g_free_range_count && g_free_ranges[pos].base < end) return -1;

    for (size_t i = g_free_range_count; i > pos; i--) {
        g_free_ranges[i] = g_free_ranges[i - 1];
    }
    g_free_ranges[pos].base = base;
    g_free_ranges[pos].end = end;
    g_free_range_count++;

    if (pos > 0 && g_free_ranges[pos - 1].end == g_free_ranges[pos].base) {
        g_free_ranges[pos - 1].end = g_free_ranges[pos].end;
        for (size_t i = pos; i + 1 < g_free_range_count; i++) {
            g_free_ranges[i] = g_free_ranges[i + 1];
        }
        g_free_range_count--;
        pos--;
    }

    if (pos + 1 < g_free_range_count && g_free_ranges[pos].end == g_free_ranges[pos + 1].base) {
        g_free_ranges[pos].end = g_free_ranges[pos + 1].end;
        for (size_t i = pos + 1; i + 1 < g_free_range_count; i++) {
            g_free_ranges[i] = g_free_ranges[i + 1];
        }
        g_free_range_count--;
    }
    return 0;
}

void pmm_init(void) {
    g_region_count = 0;
    g_free_range_count = 0;
    g_total_usable_ram = 0;
    g_allocated_bytes = 0;
    g_peak_allocated_bytes = 0;
    g_allocation_count = 0;
    g_free_count = 0;
    g_failed_allocations = 0;

    if (g_hhdm_req.response) {
        g_hhdm_offset = g_hhdm_req.response->offset;
    }

    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    g_kernel_pml4_phys = cr3 & PTE_ADDR_MASK;

    if (g_memmap_req.response) {
        for (uint64_t i = 0; i < g_memmap_req.response->entry_count; i++) {
            struct limine_memmap_entry *e = g_memmap_req.response->entries[i];
            if (e->type != LIMINE_MEMMAP_USABLE) continue;
            if (g_region_count >= PMM_MAX_REGIONS) continue;

            uint64_t end_raw = e->base + e->length;
            if (end_raw < e->base) continue;
            uint64_t base = (e->base + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
            uint64_t end = end_raw & ~(PAGE_SIZE - 1ULL);
            if (end <= base) continue;

            g_regions[g_region_count].base = base;
            g_regions[g_region_count].end = end;
            g_total_usable_ram += end - base;
            g_region_count++;
        }
    }

    if (g_region_count == 0) {
        g_regions[0].base = 0x02000000ULL;
        g_regions[0].end = 0x02200000ULL;
        g_region_count = 1;
        g_total_usable_ram = g_regions[0].end - g_regions[0].base;
    }

    for (size_t i = 0; i < g_region_count; i++) {
        pmm_insert_free_range(g_regions[i].base, g_regions[i].end);
    }
}

uint64_t pmm_alloc_pages(size_t count) {
    if (count == 0 || count > (size_t)(~0ULL / PAGE_SIZE)) {
        uint64_t flags = pmm_lock();
        g_failed_allocations++;
        pmm_unlock(flags);
        return 0;
    }
    uint64_t bytes = (uint64_t)count * PAGE_SIZE;
    uint64_t phys = 0;
    uint64_t flags = pmm_lock();
    size_t best = g_free_range_count;
    uint64_t best_size = ~0ULL;
    for (size_t i = 0; i < g_free_range_count; i++) {
        pmm_range_t *r = &g_free_ranges[i];
        uint64_t size = r->end - r->base;
        if (size >= bytes && size < best_size) { best = i; best_size = size; }
    }
    if (best < g_free_range_count) {
        pmm_range_t *r = &g_free_ranges[best];
        phys = r->base;
        r->base += bytes;
        if (r->base == r->end) {
            for (size_t j = best; j + 1 < g_free_range_count; j++) g_free_ranges[j] = g_free_ranges[j + 1];
            g_free_range_count--;
        }
        g_allocated_bytes += bytes;
        if (g_allocated_bytes > g_peak_allocated_bytes) g_peak_allocated_bytes = g_allocated_bytes;
        g_allocation_count++;
    } else {
        g_failed_allocations++;
    }
    pmm_unlock(flags);
    if (!phys) return 0;
    uint64_t *ptr = (uint64_t *)phys_to_virt(phys);
    size_t qwords = (size_t)(bytes / sizeof(uint64_t));
    for (size_t q = 0; q < qwords; q++) ptr[q] = 0;
    return phys;
}

uint64_t pmm_alloc_page(void) {
    return pmm_alloc_pages(1);
}

int pmm_free_pages(uint64_t phys, size_t count) {
    if (count == 0 || (phys & (PAGE_SIZE - 1ULL)) != 0) return -1;
    if (count > (size_t)(~0ULL / PAGE_SIZE)) return -1;
    uint64_t bytes = (uint64_t)count * PAGE_SIZE;
    uint64_t end = phys + bytes;
    if (end < phys) return -1;
    uint64_t flags = pmm_lock();
    if (!pmm_managed_range(phys, end)) { pmm_unlock(flags); return -1; }

    for (size_t i = 0; i < g_free_range_count; i++) {
        if (range_overlaps(phys, end, g_free_ranges[i].base, g_free_ranges[i].end)) { pmm_unlock(flags); return -1; }
    }

    if (pmm_insert_free_range(phys, end) != 0) { pmm_unlock(flags); return -1; }
    if (g_allocated_bytes >= bytes) g_allocated_bytes -= bytes;
    else g_allocated_bytes = 0;
    g_free_count++;
    pmm_unlock(flags);
    return 0;
}

int pmm_free_page(uint64_t phys) {
    return pmm_free_pages(phys, 1);
}

void pmm_get_stats(uint64_t *total, uint64_t *used) {
    uint64_t flags = pmm_lock();
    if (total) *total = g_total_usable_ram;
    if (used) {
        *used = g_allocated_bytes > g_total_usable_ram ? g_total_usable_ram : g_allocated_bytes;
    }
    pmm_unlock(flags);
}

void pmm_get_diagnostics(pmm_diagnostics_t *out) {
    if (!out) return;
    uint64_t flags = pmm_lock();
    uint64_t free_bytes = 0;
    uint64_t largest = 0;
    for (size_t i = 0; i < g_free_range_count; ++i) {
        uint64_t size = g_free_ranges[i].end - g_free_ranges[i].base;
        free_bytes += size;
        if (size > largest) largest = size;
    }
    out->total_bytes = g_total_usable_ram;
    out->allocated_bytes = g_allocated_bytes;
    out->free_bytes = free_bytes;
    out->largest_free_run = largest;
    out->peak_allocated_bytes = g_peak_allocated_bytes;
    out->allocation_count = g_allocation_count;
    out->free_count = g_free_count;
    out->failed_allocations = g_failed_allocations;
    out->free_range_count = (uint32_t)g_free_range_count;
    pmm_unlock(flags);
}

uint64_t vmm_get_kernel_pml4(void) {
    return g_kernel_pml4_phys;
}

uint64_t vmm_get_current_pml4(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & PTE_ADDR_MASK;
}

uint64_t vmm_create_user_pml4(void) {
    uint64_t user_pml4_phys = pmm_alloc_page();
    if (!user_pml4_phys) return 0;

    uint64_t *user_pml4 = (uint64_t *)phys_to_virt(user_pml4_phys);
    uint64_t *kernel_pml4 = (uint64_t *)phys_to_virt(g_kernel_pml4_phys);

    for (int i = 0; i < 256; i++) user_pml4[i] = 0;
    for (int i = 256; i < 512; i++) user_pml4[i] = kernel_pml4[i];
    return user_pml4_phys;
}

static uint64_t *vmm_get_pt(uint64_t pml4_phys, uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FFULL;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FFULL;
    uint64_t pd_idx = (virt >> 21) & 0x1FFULL;
    uint64_t *pml4 = (uint64_t *)phys_to_virt(pml4_phys);
    if (!(pml4[pml4_idx] & PTE_PRESENT) || (pml4[pml4_idx] & PTE_HUGE)) return NULL;

    uint64_t *pdpt = (uint64_t *)phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);
    if (!(pdpt[pdpt_idx] & PTE_PRESENT) || (pdpt[pdpt_idx] & PTE_HUGE)) return NULL;

    uint64_t *pd = (uint64_t *)phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);
    if (!(pd[pd_idx] & PTE_PRESENT) || (pd[pd_idx] & PTE_HUGE)) return NULL;

    return (uint64_t *)phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);
}

uint64_t vmm_get_page_phys(uint64_t pml4_phys, uint64_t virt) {
    uint64_t *pt = vmm_get_pt(pml4_phys, virt);
    if (!pt) return 0;
    uint64_t pte = pt[(virt >> 12) & 0x1FFULL];
    if (!(pte & PTE_PRESENT)) return 0;
    return pte & PTE_ADDR_MASK;
}

uint64_t vmm_get_page_flags(uint64_t pml4_phys, uint64_t virt) {
    uint64_t *pt = vmm_get_pt(pml4_phys, virt);
    if (!pt) return 0;
    return pt[(virt >> 12) & 0x1FFULL];
}

int vmm_map_page(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!pml4_phys || (virt & (PAGE_SIZE - 1ULL)) != 0 || (phys & (PAGE_SIZE - 1ULL)) != 0) return -1;
    if ((virt >> 47) != 0 && (virt >> 47) != 0x1FFFFULL) return -1;
    if (pml4_phys == g_kernel_pml4_phys && (flags & PTE_USER)) return -1;
    if ((flags & PTE_USER) && virt >= USER_VA_LIMIT) return -1;
    if ((flags & (PTE_USER | PTE_WRITABLE)) == (PTE_USER | PTE_WRITABLE) && !(flags & PTE_NO_EXECUTE)) return -1;

    uint64_t *pml4 = (uint64_t *)phys_to_virt(pml4_phys);
    uint64_t pml4_idx = (virt >> 39) & 0x1FFULL;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FFULL;
    uint64_t pd_idx = (virt >> 21) & 0x1FFULL;
    uint64_t pt_idx = (virt >> 12) & 0x1FFULL;
    uint64_t table_flags = PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    uint64_t new_pdpt_phys = 0;
    uint64_t new_pd_phys = 0;
    uint64_t new_pt_phys = 0;

    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        uint64_t pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) return -1;
        pml4[pml4_idx] = pdpt_phys | table_flags;
        new_pdpt_phys = pdpt_phys;
    } else if (flags & PTE_USER) {
        pml4[pml4_idx] |= PTE_USER;
    }

    uint64_t *pdpt = (uint64_t *)phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t pd_phys = pmm_alloc_page();
        if (!pd_phys) goto rollback;
        pdpt[pdpt_idx] = pd_phys | table_flags;
        new_pd_phys = pd_phys;
    } else if (flags & PTE_USER) {
        pdpt[pdpt_idx] |= PTE_USER;
    }

    uint64_t *pd = (uint64_t *)phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t pt_phys = pmm_alloc_page();
        if (!pt_phys) goto rollback;
        pd[pd_idx] = pt_phys | table_flags;
        new_pt_phys = pt_phys;
    } else if (flags & PTE_USER) {
        pd[pd_idx] |= PTE_USER;
    }

    uint64_t *pt = (uint64_t *)phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);
    if (pt[pt_idx] & PTE_PRESENT) return -1;
    pt[pt_idx] = (phys & PTE_ADDR_MASK) | PTE_PRESENT | (flags & (0xFFFULL | PTE_NO_EXECUTE));
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    return 0;

rollback:
    if (new_pt_phys) {
        pd[pd_idx] = 0;
        pmm_free_page(new_pt_phys);
    }
    if (new_pd_phys) {
        pdpt[pdpt_idx] = 0;
        pmm_free_page(new_pd_phys);
    }
    if (new_pdpt_phys) {
        pml4[pml4_idx] = 0;
        pmm_free_page(new_pdpt_phys);
    }
    return -1;
}

int vmm_set_page_flags(uint64_t pml4_phys, uint64_t virt, uint64_t flags) {
    if (!pml4_phys || (virt & (PAGE_SIZE - 1ULL)) != 0) return -1;
    if (pml4_phys == g_kernel_pml4_phys && (flags & PTE_USER)) return -1;
    if ((flags & PTE_USER) && virt >= USER_VA_LIMIT) return -1;
    if ((flags & (PTE_USER | PTE_WRITABLE)) == (PTE_USER | PTE_WRITABLE) && !(flags & PTE_NO_EXECUTE)) return -1;
    uint64_t *pt = vmm_get_pt(pml4_phys, virt);
    if (!pt) return -1;
    uint64_t index = (virt >> 12) & 0x1FFULL;
    uint64_t old = pt[index];
    if (!(old & PTE_PRESENT)) return -1;
    pt[index] = (old & PTE_ADDR_MASK) | PTE_PRESENT | (flags & (0xFFFULL | PTE_NO_EXECUTE));
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    return 0;
}

static int table_empty(const uint64_t *table) {
    for (int i = 0; i < 512; ++i) if (table[i] & PTE_PRESENT) return 0;
    return 1;
}

int vmm_unmap_page(uint64_t pml4_phys, uint64_t virt, int free_phys) {
    if (!pml4_phys || (virt & (PAGE_SIZE - 1ULL))) return -1;
    uint64_t pml4_index = (virt >> 39) & 0x1FFULL;
    uint64_t pdpt_index = (virt >> 30) & 0x1FFULL;
    uint64_t pd_index = (virt >> 21) & 0x1FFULL;
    uint64_t pt_index = (virt >> 12) & 0x1FFULL;
    uint64_t *pml4 = (uint64_t *)phys_to_virt(pml4_phys);
    if (!(pml4[pml4_index] & PTE_PRESENT) || (pml4[pml4_index] & PTE_HUGE)) return -1;
    uint64_t pdpt_phys = pml4[pml4_index] & PTE_ADDR_MASK;
    uint64_t *pdpt = (uint64_t *)phys_to_virt(pdpt_phys);
    if (!(pdpt[pdpt_index] & PTE_PRESENT) || (pdpt[pdpt_index] & PTE_HUGE)) return -1;
    uint64_t pd_phys = pdpt[pdpt_index] & PTE_ADDR_MASK;
    uint64_t *pd = (uint64_t *)phys_to_virt(pd_phys);
    if (!(pd[pd_index] & PTE_PRESENT) || (pd[pd_index] & PTE_HUGE)) return -1;
    uint64_t pt_phys = pd[pd_index] & PTE_ADDR_MASK;
    uint64_t *pt = (uint64_t *)phys_to_virt(pt_phys);
    uint64_t old = pt[pt_index];
    if (!(old & PTE_PRESENT)) return -1;
    uint64_t phys = old & PTE_ADDR_MASK;
    pt[pt_index] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    int result = free_phys ? pmm_free_page(phys) : 0;
    if (table_empty(pt)) {
        pd[pd_index] = 0;
        if (pmm_free_page(pt_phys) != 0) result = -1;
        if (table_empty(pd)) {
            pdpt[pdpt_index] = 0;
            if (pmm_free_page(pd_phys) != 0) result = -1;
            if (pml4_index < 256 && table_empty(pdpt)) {
                pml4[pml4_index] = 0;
                if (pmm_free_page(pdpt_phys) != 0) result = -1;
            }
        }
    }
    return result;
}

static void vmm_free_pt(uint64_t pt_phys) {
    uint64_t *pt = (uint64_t *)phys_to_virt(pt_phys);
    for (int i = 0; i < 512; i++) {
        uint64_t entry = pt[i];
        if (!(entry & PTE_PRESENT)) continue;
        uint64_t phys = entry & PTE_ADDR_MASK;
        pt[i] = 0;
        pmm_free_page(phys);
    }
}

static void vmm_free_pd(uint64_t pd_phys) {
    uint64_t *pd = (uint64_t *)phys_to_virt(pd_phys);
    for (int i = 0; i < 512; i++) {
        uint64_t entry = pd[i];
        if (!(entry & PTE_PRESENT)) continue;
        if (entry & PTE_HUGE) {
            pd[i] = 0;
            continue;
        }
        uint64_t pt_phys = entry & PTE_ADDR_MASK;
        vmm_free_pt(pt_phys);
        pd[i] = 0;
        pmm_free_page(pt_phys);
    }
}

static void vmm_free_pdpt(uint64_t pdpt_phys) {
    uint64_t *pdpt = (uint64_t *)phys_to_virt(pdpt_phys);
    for (int i = 0; i < 512; i++) {
        uint64_t entry = pdpt[i];
        if (!(entry & PTE_PRESENT)) continue;
        if (entry & PTE_HUGE) {
            pdpt[i] = 0;
            continue;
        }
        uint64_t pd_phys = entry & PTE_ADDR_MASK;
        vmm_free_pd(pd_phys);
        pdpt[i] = 0;
        pmm_free_page(pd_phys);
    }
}

int vmm_destroy_user_pml4(uint64_t pml4_phys) {
    if (!pml4_phys || pml4_phys == g_kernel_pml4_phys) return -1;
    if (vmm_get_current_pml4() == pml4_phys) vmm_switch_pml4(g_kernel_pml4_phys);
    uint64_t *pml4 = (uint64_t *)phys_to_virt(pml4_phys);

    for (int i = 0; i < 256; i++) {
        uint64_t entry = pml4[i];
        if (!(entry & PTE_PRESENT)) continue;
        if (entry & PTE_HUGE) {
            pml4[i] = 0;
            continue;
        }
        uint64_t pdpt_phys = entry & PTE_ADDR_MASK;
        vmm_free_pdpt(pdpt_phys);
        pml4[i] = 0;
        pmm_free_page(pdpt_phys);
    }

    pmm_free_page(pml4_phys);
    return 0;
}

void vmm_switch_pml4(uint64_t pml4_phys) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4_phys & PTE_ADDR_MASK) : "memory");
}

int user_range_valid(uint64_t addr, size_t size, uint64_t access) {
    if (size == 0) return 1;
    uint64_t end = addr + (uint64_t)size;
    if (end < addr || addr >= USER_VA_LIMIT || end > USER_VA_LIMIT) return 0;

    uint64_t cr3 = vmm_get_current_pml4();
    if (!cr3 || cr3 == g_kernel_pml4_phys) return 0;

    uint64_t page = addr & ~(PAGE_SIZE - 1ULL);
    uint64_t last = (end - 1ULL) & ~(PAGE_SIZE - 1ULL);
    for (;;) {
        uint64_t *pml4 = (uint64_t *)phys_to_virt(cr3);
        uint64_t pml4e = pml4[(page >> 39) & 0x1FFULL];
        if (!(pml4e & PTE_PRESENT) || !(pml4e & PTE_USER) || (pml4e & PTE_HUGE)) return 0;
        uint64_t *pdpt = (uint64_t *)phys_to_virt(pml4e & PTE_ADDR_MASK);
        uint64_t pdpte = pdpt[(page >> 30) & 0x1FFULL];
        if (!(pdpte & PTE_PRESENT) || !(pdpte & PTE_USER) || (pdpte & PTE_HUGE)) return 0;
        uint64_t *pd = (uint64_t *)phys_to_virt(pdpte & PTE_ADDR_MASK);
        uint64_t pde = pd[(page >> 21) & 0x1FFULL];
        if (!(pde & PTE_PRESENT) || !(pde & PTE_USER) || (pde & PTE_HUGE)) return 0;
        uint64_t pte = vmm_get_page_flags(cr3, page);
        if (!(pte & PTE_PRESENT) || !(pte & PTE_USER)) return 0;
        if ((access & USER_ACCESS_WRITE) && !(pte & PTE_WRITABLE)) return 0;
        if ((access & USER_ACCESS_EXEC) && (pte & PTE_NO_EXECUTE)) return 0;
        if (page == last) break;
        page += PAGE_SIZE;
    }
    return 1;
}

int copy_from_user(void *dst, const void *user_src, size_t size) {
    if (size == 0) return 0;
    if (!dst || !user_src || !user_range_valid((uint64_t)user_src, size, USER_ACCESS_READ)) return -1;

    uint8_t *out = (uint8_t *)dst;
    uint64_t src = (uint64_t)user_src;
    size_t left = size;
    uint64_t cr3 = vmm_get_current_pml4();
    while (left) {
        uint64_t page = src & ~(PAGE_SIZE - 1ULL);
        uint64_t phys = vmm_get_page_phys(cr3, page);
        size_t offset = (size_t)(src & (PAGE_SIZE - 1ULL));
        size_t chunk = PAGE_SIZE - offset;
        if (chunk > left) chunk = left;
        uint8_t *in = (uint8_t *)phys_to_virt(phys) + offset;
        for (size_t i = 0; i < chunk; i++) out[i] = in[i];
        out += chunk;
        src += chunk;
        left -= chunk;
    }
    return 0;
}

int copy_to_user(void *user_dst, const void *src, size_t size) {
    if (size == 0) return 0;
    if (!src || !user_dst || !user_range_valid((uint64_t)user_dst, size, USER_ACCESS_WRITE)) return -1;

    const uint8_t *in = (const uint8_t *)src;
    uint64_t dst = (uint64_t)user_dst;
    size_t left = size;
    uint64_t cr3 = vmm_get_current_pml4();
    while (left) {
        uint64_t page = dst & ~(PAGE_SIZE - 1ULL);
        uint64_t phys = vmm_get_page_phys(cr3, page);
        size_t offset = (size_t)(dst & (PAGE_SIZE - 1ULL));
        size_t chunk = PAGE_SIZE - offset;
        if (chunk > left) chunk = left;
        uint8_t *out = (uint8_t *)phys_to_virt(phys) + offset;
        for (size_t i = 0; i < chunk; i++) out[i] = in[i];
        in += chunk;
        dst += chunk;
        left -= chunk;
    }
    return 0;
}

int64_t user_strnlen(const char *user_str, size_t max_len) {
    if (!user_str || max_len == 0) return -1;
    uint64_t addr = (uint64_t)user_str;
    size_t checked = 0;
    uint64_t cr3 = vmm_get_current_pml4();
    if (!cr3 || cr3 == g_kernel_pml4_phys) return -1;

    while (checked < max_len) {
        uint64_t page = addr & ~(PAGE_SIZE - 1ULL);
        uint64_t pte = vmm_get_page_flags(cr3, page);
        if (!(pte & PTE_PRESENT) || !(pte & PTE_USER)) return -1;
        uint64_t phys = pte & PTE_ADDR_MASK;
        size_t offset = (size_t)(addr & (PAGE_SIZE - 1ULL));
        size_t chunk = PAGE_SIZE - offset;
        if (chunk > max_len - checked) chunk = max_len - checked;
        const char *s = (const char *)phys_to_virt(phys) + offset;
        for (size_t i = 0; i < chunk; i++) {
            if (s[i] == '\0') return (int64_t)(checked + i);
        }
        checked += chunk;
        addr += chunk;
    }
    return -2;
}
