// гдт дескрипторы для ядра и юзерленда
#include "gdt.h"
#include <stddef.h>

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

static uint64_t gdt_entries[11];
static struct gdt_ptr g_gdtp;
static struct tss_entry g_tss;

static uint8_t kernel_interrupt_stack[16384] __attribute__((aligned(16)));

void gdt_set_kernel_stack(uint64_t stack_top) {
    g_tss.rsp0 = stack_top;
}

void gdt_init(void) {

    gdt_entries[0] = 0x0000000000000000ULL;

    gdt_entries[1] = 0x0000ffff00009a00ULL;

    gdt_entries[2] = 0x0000ffff00009200ULL;

    gdt_entries[3] = 0x00cf9a000000ffffULL;

    gdt_entries[4] = 0x00cf92000000ffffULL;

    gdt_entries[5] = 0x00209A0000000000ULL;

    gdt_entries[6] = 0x0000920000000000ULL;

    gdt_entries[7] = 0x0000F20000000000ULL;

    gdt_entries[8] = 0x0020FA0000000000ULL;

    for (size_t i = 0; i < sizeof(g_tss); i++) {
        ((uint8_t*)&g_tss)[i] = 0;
    }
    g_tss.rsp0 = (uint64_t)&kernel_interrupt_stack[sizeof(kernel_interrupt_stack) - 16];
    g_tss.iopb_offset = sizeof(struct tss_entry);

    uint64_t tss_base = (uint64_t)&g_tss;
    uint32_t tss_limit = sizeof(struct tss_entry) - 1;

    struct tss_descriptor *tss_desc = (struct tss_descriptor*)&gdt_entries[9];
    tss_desc->limit_low = (uint16_t)(tss_limit & 0xFFFF);
    tss_desc->base_low = (uint16_t)(tss_base & 0xFFFF);
    tss_desc->base_mid = (uint8_t)((tss_base >> 16) & 0xFF);
    tss_desc->flags1 = 0x89; 
    tss_desc->flags2 = (uint8_t)((tss_limit >> 16) & 0x0F);
    tss_desc->base_high = (uint8_t)((tss_base >> 24) & 0xFF);
    tss_desc->base_upper = (uint32_t)((tss_base >> 32) & 0xFFFFFFFF);
    tss_desc->reserved = 0;

    g_gdtp.limit = (uint16_t)(sizeof(gdt_entries) - 1);
    g_gdtp.base = (uint64_t)&gdt_entries[0];

    __asm__ volatile ("lgdt %0" : : "m"(g_gdtp));

    __asm__ volatile (
        "mov $0x30, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        : : : "rax", "memory"
    );

    __asm__ volatile (
        "pushq $0x28\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        : : : "rax", "memory"
    );

    uint16_t tss_sel = TSS_SEL;
    __asm__ volatile ("ltr %0" : : "r"(tss_sel));
}
