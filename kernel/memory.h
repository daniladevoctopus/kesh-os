// структуры страниц и аллокатора
#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096ULL

#define PTE_PRESENT   (1ULL << 0)
#define PTE_WRITABLE  (1ULL << 1)
#define PTE_USER      (1ULL << 2)

extern uint64_t g_hhdm_offset;

void pmm_init(void);
uint64_t pmm_alloc_page(void);
uint64_t pmm_alloc_pages(size_t count);
void pmm_get_stats(uint64_t *total, uint64_t *used);

uint64_t vmm_get_kernel_pml4(void);
uint64_t vmm_create_user_pml4(void);
uint64_t vmm_get_page_phys(uint64_t pml4_phys, uint64_t virt);
void vmm_map_page(uint64_t pml4_phys, uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_switch_pml4(uint64_t pml4_phys);

#endif
