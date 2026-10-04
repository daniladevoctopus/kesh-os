// таблица прерываний idt чтоб не крашилось
#include "idt.h"
#include "pic.h"
#include "../log.h"

static struct idt_entry idt[256];
static struct idtr idtr;

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags) {
    struct idt_entry *descriptor = &idt[vector];
    uint64_t addr = (uint64_t)isr;

    descriptor->isr_low    = addr & 0xFFFF;

    descriptor->kernel_cs  = 0x28;
    descriptor->ist        = 0;
    descriptor->attributes = flags;
    descriptor->isr_mid    = (addr >> 16) & 0xFFFF;
    descriptor->isr_high   = (addr >> 32) & 0xFFFFFFFF;
    descriptor->reserved   = 0;
}

static void idt_install_exception_handlers(void) {

    void *stubs[32] = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    for (int i = 0; i < 32; i++) {
        idt_set_descriptor((uint8_t)i, stubs[i], 0x8E);
    }
}

static void idt_install_irq_handlers(void) {
    void *stubs[16] = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
    };
    for (int i = 0; i < 16; i++) {
        idt_set_descriptor((uint8_t)(32 + i), stubs[i], 0x8E);
    }
}

void idt_load_cpu(void) {
    asm volatile ("lidt %0" : : "m"(idtr));
}

void idt_init(void) {
    KLOG_ENTER("idt");
    idtr.base = (uint64_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(struct idt_entry) * 256 - 1;

    idt_install_exception_handlers();
    idt_install_irq_handlers();
    pic_remap();
    idt_load_cpu();
    KLOG_DEBUG("idt", "IDT loaded base=%p limit=%u irq_vectors=0x20..0x2F",
               (void *)idtr.base, (unsigned)idtr.limit);
    KLOG_LEAVE("idt", 0);
}
