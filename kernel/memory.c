// раскидываем физическую память и страницы
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
typedef struct { uint64_t base; uint64_t end; uint64_t cursor; } pmm_region_t;
static pmm_region_t g_regions[PMM_MAX_REGIONS];
static size_t g_region_count = 0;
static size_t g_active_region = 0;
static uint64_t g_kernel_pml4_phys = 0;
static uint64_t g_total_usable_ram = 0;

static inline void* phys_to_virt(uint64_t phys) {
    return (void*)(phys + g_hhdm_offset);
}

void pmm_init(void) {
    if (g_hhdm_req.response) {
        g_hhdm_offset = g_hhdm_req.response->offset;
    }

    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    g_kernel_pml4_phys = cr3 & ~0xFFFULL;

    if (g_memmap_req.response) {
        for (uint64_t i = 0; i < g_memmap_req.response->entry_count; i++) {
            struct limine_memmap_entry *e = g_memmap_req.response->entries[i];
            if (e->type == LIMINE_MEMMAP_USABLE) {
                g_total_usable_ram += e->length;
                if (g_region_count < PMM_MAX_REGIONS) {
                    uint64_t base = (e->base + 0xFFFULL) & ~0xFFFULL;
                    uint64_t end = (e->base + e->length) & ~0xFFFULL;
                    if (end > base) {
                        g_regions[g_region_count].base = base;
                        g_regions[g_region_count].end = end;
                        g_regions[g_region_count].cursor = base;
                        g_region_count++;
                    }
                }
            }
        }
    }

    if (g_region_count == 0) {
        g_regions[0].base = 0x2000000ULL;
        g_regions[0].end = 0x2200000ULL;
        g_regions[0].cursor = g_regions[0].base;
        g_region_count = 1;
    }
    if (g_total_usable_ram == 0) {
        g_total_usable_ram = 256 * 1024 * 1024; 
    }
}

static uint64_t g_allocated_bytes = 0;

uint64_t pmm_alloc_pages(size_t count) {
    if (count == 0 || g_region_count == 0) return 0;
    uint64_t bytes = (uint64_t)count * PAGE_SIZE;
    for (size_t n = 0; n < g_region_count; n++) {
        size_t idx = (g_active_region + n) % g_region_count;
        pmm_region_t *r = &g_regions[idx];
        uint64_t cursor = (r->cursor + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        if (cursor <= r->end && bytes <= r->end - cursor) {
            r->cursor = cursor + bytes;
            g_active_region = idx;
            g_allocated_bytes += bytes;
            uint64_t *ptr = (uint64_t*)phys_to_virt(cursor);
            size_t total_qwords = (size_t)(bytes / sizeof(uint64_t));
            for (size_t i = 0; i < total_qwords; i++) ptr[i] = 0;
            return cursor;
        }
    }
    return 0;
}

uint64_t pmm_alloc_page(void) {
    return pmm_alloc_pages(1);
}

void pmm_get_stats(uint64_t *total, uint64_t *used) {
    if (total) *total = g_total_usable_ram;
    if (used) {
        uint64_t u = g_allocated_bytes;
        if (u > g_total_usable_ram) u = g_total_usable_ram;
        *used = u;
    }
}

uint64_t vmm_get_kernel_pml4(void) {
    return g_kernel_pml4_phys;
}

uint64_t vmm_create_user_pml4(void) {
    uint64_t user_pml4_phys = pmm_alloc_page();
    uint64_t *user_pml4 = (uint64_t*)phys_to_virt(user_pml4_phys);
    uint64_t *kernel_pml4 = (uint64_t*)phys_to_virt(g_kernel_pml4_phys);

    for (int i = 0; i < 256; i++) {
        user_pml4[i] = 0;
    }

    for (int i = 256; i < 512; i++) {
        user_pml4[i] = kernel_pml4[i];
    }

    return user_pml4_phys;
}

uint64_t vmm_get_page_phys(uint64_t pml4_phys, uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t *pml4 = (uint64_t*)phys_to_virt(pml4_phys);
    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;

    uint64_t *pdpt = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return 0;

    uint64_t *pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & PTE_PRESENT)) return 0;

    uint64_t *pt = (uint64_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);
    if (!(pt[pt_idx] & PTE_PRESENT)) return 0;

    return pt[pt_idx] & ~0xFFFULL;
}

void vmm_map_page(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t *pml4 = (uint64_t*)phys_to_virt(pml4_phys);

    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        uint64_t pdpt_phys = pmm_alloc_page();
        pml4[pml4_idx] = pdpt_phys | PTE_PRESENT | PTE_WRITABLE | (flags & (PTE_USER | PTE_WRITABLE));
    } else {
        if (flags & PTE_USER) pml4[pml4_idx] |= PTE_USER;
        if (flags & PTE_WRITABLE) pml4[pml4_idx] |= PTE_WRITABLE;
    }

    uint64_t *pdpt = (uint64_t*)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t pd_phys = pmm_alloc_page();
        pdpt[pdpt_idx] = pd_phys | PTE_PRESENT | PTE_WRITABLE | (flags & (PTE_USER | PTE_WRITABLE));
    } else {
        if (flags & PTE_USER) pdpt[pdpt_idx] |= PTE_USER;
        if (flags & PTE_WRITABLE) pdpt[pdpt_idx] |= PTE_WRITABLE;
    }

    uint64_t *pd = (uint64_t*)phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t pt_phys = pmm_alloc_page();
        pd[pd_idx] = pt_phys | PTE_PRESENT | PTE_WRITABLE | (flags & (PTE_USER | PTE_WRITABLE));
    } else {
        if (flags & PTE_USER) pd[pd_idx] |= PTE_USER;
        if (flags & PTE_WRITABLE) pd[pd_idx] |= PTE_WRITABLE;
    }

    uint64_t *pt = (uint64_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);

    pt[pt_idx] = (phys & ~0xFFFULL) | (flags & 0xFFFULL) | PTE_PRESENT;

    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_switch_pml4(uint64_t pml4_phys) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}
