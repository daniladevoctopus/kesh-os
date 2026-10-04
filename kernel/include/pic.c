// контроллер pic8259
#include "pic.h"
#include "../log.h"
#include "../serial.h"

extern int apic_is_available(void);
extern void apic_eoi(void);

static int g_apic_mode = 0;

void pic_remap(void) {
    KLOG_ENTER("pic");
    uint8_t mask1 = inb(0x21);
    uint8_t mask2 = inb(0xA1);

    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, mask1);
    outb(0xA1, mask2);

    pic_set_mask(14);
    pic_set_mask(15);
    KLOG_TRACE("pic", "remap vector_master=0x20 vector_slave=0x28 mask_before=%02x/%02x mask_after=%02x/%02x",
               (unsigned)mask1, (unsigned)mask2, (unsigned)inb(0x21), (unsigned)inb(0xA1));
    KLOG_LEAVE("pic", 0);
}

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

void pic_set_apic_mode(int enabled) {
    g_apic_mode = enabled ? 1 : 0;
    if (g_apic_mode) {
        outb(PIC1_DATA, 0xFF);
        outb(PIC2_DATA, 0xFF);
    }
}

void pic_send_eoi(uint8_t irq) {
    if (g_apic_mode && apic_is_available()) {
        apic_eoi();
        return;
    }
    if (irq == 7) {
        outb(PIC1_CMD, 0x0B);
        if (!(inb(PIC1_CMD) & 0x80)) return; 
    }
    if (irq == 15) {
        outb(PIC2_CMD, 0x0B);
        if (!(inb(PIC2_CMD) & 0x80)) {
            outb(PIC1_CMD, PIC_EOI); 
            return;
        }
    }
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_set_mask(uint8_t irq_line) {
    uint16_t port = irq_line < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = irq_line < 8 ? irq_line : (uint8_t)(irq_line - 8);
    uint8_t value = (uint8_t)(inb(port) | (1 << bit));
    outb(port, value);
}

void pic_clear_mask(uint8_t irq_line) {
    uint16_t port = irq_line < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = irq_line < 8 ? irq_line : (uint8_t)(irq_line - 8);
    uint8_t value = (uint8_t)(inb(port) & ~(1 << bit));
    outb(port, value);
    KLOG_TRACE("pic", "unmask irq=%u mask=%02x", (unsigned)irq_line, (unsigned)value);
}

uint8_t pic_get_mask(uint8_t irq_line) {
    uint16_t port = irq_line < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t mask = inb(port);
    uint8_t bit = irq_line < 8 ? irq_line : (uint8_t)(irq_line - 8);
    return (uint8_t)((mask >> bit) & 1U);
}

static irq_handler_t irq_routines[16];
static volatile uint32_t g_irq_trace_count = 0;

void irq_install_handler(int irq, irq_handler_t handler) {
    if (irq >= 0 && irq < 16) irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq) {
    if (irq >= 0 && irq < 16) irq_routines[irq] = 0;
}

void irq_common_handler(uint64_t *stack_ptr) {
    uint64_t vector = stack_ptr[15]; 
    int irq_line = (int)(vector - 32);

    if (irq_line == 14) {
        (void)inb(0x1F7);
    } else if (irq_line == 15) {
        (void)inb(0x177);
    }

    if (g_irq_trace_count < 8) {
        ++g_irq_trace_count;
        serial_print("[IRQ ] vector=");
        serial_print_dec(vector);
        serial_print(" line=");
        serial_print_dec((uint64_t)(irq_line >= 0 ? irq_line : 255));
        serial_print(" controller=");
        serial_print((g_apic_mode && apic_is_available()) ? "APIC" : "PIC");
        serial_print(" handler=");
        serial_print((irq_line >= 0 && irq_line < 16 && irq_routines[irq_line]) ? "yes" : "no");
        serial_print("\n");
    }

    if (irq_line >= 0 && irq_line < 16 && irq_routines[irq_line]) {
        irq_routines[irq_line]();
    }

    pic_send_eoi((uint8_t)irq_line);
}
