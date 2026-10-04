// порты pic
#ifndef PIC_H
#define PIC_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void pic_remap(void);

void pic_send_eoi(uint8_t irq);
void pic_set_apic_mode(int enabled);
void pic_set_mask(uint8_t irq_line);
void pic_clear_mask(uint8_t irq_line);
uint8_t pic_get_mask(uint8_t irq_line);

typedef void (*irq_handler_t)(void);
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);

#endif
