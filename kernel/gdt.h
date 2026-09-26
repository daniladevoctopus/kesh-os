// селекторы сегментов
#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define KERNEL_CODE_SEL 0x28
#define KERNEL_DATA_SEL 0x30
#define USER_DATA_SEL   (0x38 | 3)
#define USER_CODE_SEL   (0x40 | 3)
#define TSS_SEL         0x48

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

void gdt_init(void);
void gdt_set_kernel_stack(uint64_t stack_top);

#endif
