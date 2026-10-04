#include "serial.h"

#define COM1 0x3F8U
#define SERIAL_LSR (COM1 + 5U)
#define SERIAL_THR (COM1 + 0U)
#define SERIAL_FIFO (COM1 + 2U)

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static volatile uint32_t g_serial_lock = 0;

static uint64_t serial_irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0" : "=r"(flags));
    __asm__ volatile("cli" : : : "memory");
    return flags;
}

static void serial_irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) __asm__ volatile("sti" : : : "memory");
}

static void serial_lock(void) {
    while (__atomic_test_and_set(&g_serial_lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
}

static void serial_unlock(void) {
    __atomic_clear(&g_serial_lock, __ATOMIC_RELEASE);
}

static void serial_wait_tx(void) {
    /* THRE (bit 5) means the UART transmitter holding register can accept data. */
    for (uint32_t timeout = 0; timeout < 1000000U; ++timeout) {
        if (inb(SERIAL_LSR) & 0x20U) return;
        __asm__ volatile ("pause");
    }
}

void serial_init(void) {
    outb(COM1 + 1U, 0x00); /* Disable interrupts. */
    outb(COM1 + 3U, 0x80); /* Enable divisor latch. */
    outb(COM1 + 0U, 0x03); /* 38400 baud divisor = 3. */
    outb(COM1 + 1U, 0x00);
    outb(COM1 + 3U, 0x03); /* 8N1. */
    outb(SERIAL_FIFO, 0xC7); /* Enable + clear FIFO, 14-byte threshold. */
    outb(COM1 + 4U, 0x0B); /* IRQs enabled, RTS/DTR set. */
}

static void serial_write_char_unlocked(char c) {
    serial_wait_tx();
    outb(SERIAL_THR, (uint8_t)c);
}

void serial_write_char(char c) {
    uint64_t flags = serial_irq_save();
    serial_lock();
    serial_write_char_unlocked(c);
    serial_unlock();
    serial_irq_restore(flags);
}

void serial_print(const char *str) {
    if (!str) return;
    uint64_t flags = serial_irq_save();
    serial_lock();
    while (*str) serial_write_char_unlocked(*str++);
    serial_unlock();
    serial_irq_restore(flags);
}

void serial_print_hex(uint64_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        buf[2 + i] = hex_chars[(val >> ((15 - i) * 4)) & 0xFU];
    }
    buf[18] = '\0';
    serial_print(buf);
}

void serial_print_dec(uint64_t val) {
    if (val == 0) {
        serial_write_char('0');
        return;
    }

    char buf[32];
    int i = 0;
    while (val > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (val % 10U));
        val /= 10U;
    }
    while (i > 0) serial_write_char(buf[--i]);
}
