#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096ULL

#define PTE_PRESENT    (1ULL << 0)
#define PTE_WRITABLE   (1ULL << 1)
#define PTE_USER       (1ULL << 2)
#define PTE_WRITE_THROUGH (1ULL << 3)
#define PTE_CACHE_DISABLE  (1ULL << 4)
#define PTE_ACCESSED   (1ULL << 5)
#define PTE_DIRTY      (1ULL << 6)
#define PTE_HUGE       (1ULL << 7)
#define PTE_GLOBAL     (1ULL << 8)
#define PTE_NO_EXECUTE (1ULL << 63)

#define USER_ACCESS_READ  0x01ULL
#define USER_ACCESS_WRITE 0x02ULL
#define USER_ACCESS_EXEC  0x04ULL
#define USER_VA_LIMIT     0x0000800000000000ULL

extern uint64_t g_hhdm_offset;

typedef struct {
    uint64_t total_bytes;
    uint64_t allocated_bytes;
    uint64_t free_bytes;
    uint64_t largest_free_run;
    uint64_t peak_allocated_bytes;
    uint64_t allocation_count;
    uint64_t free_count;
    uint64_t failed_allocations;
    uint32_t free_range_count;
} pmm_diagnostics_t;

void pmm_init(void);
uint64_t pmm_alloc_page(void);
uint64_t pmm_alloc_pages(size_t count);
int pmm_free_page(uint64_t phys);
int pmm_free_pages(uint64_t phys, size_t count);
void pmm_get_stats(uint64_t *total, uint64_t *used);
void pmm_get_diagnostics(pmm_diagnostics_t *out);

uint64_t vmm_get_kernel_pml4(void);
uint64_t vmm_get_current_pml4(void);
uint64_t vmm_create_user_pml4(void);
int vmm_destroy_user_pml4(uint64_t pml4_phys);
uint64_t vmm_get_page_phys(uint64_t pml4_phys, uint64_t virt);
uint64_t vmm_get_page_flags(uint64_t pml4_phys, uint64_t virt);
int vmm_map_page(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_set_page_flags(uint64_t pml4_phys, uint64_t virt, uint64_t flags);
int vmm_unmap_page(uint64_t pml4_phys, uint64_t virt, int free_phys);
void vmm_switch_pml4(uint64_t pml4_phys);

int user_range_valid(uint64_t addr, size_t size, uint64_t access);
int copy_from_user(void *dst, const void *user_src, size_t size);
int copy_to_user(void *user_dst, const void *src, size_t size);
int64_t user_strnlen(const char *user_str, size_t max_len);

#endif
