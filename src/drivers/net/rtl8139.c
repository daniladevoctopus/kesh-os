#include "rtl8139.h"
#include "../../kernel/memory.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
#define RX_BUFFER_SIZE 8192
#define TX_BUFFER_SIZE 2048
#define RX_WRAP 0x80
#define CMD 0x37
#define CMD_RESET 0x10
#define CMD_RX_ENABLE 0x08
#define CMD_TX_ENABLE 0x04
#define REG_MAC 0x00
#define REG_MAR0 0x08
#define REG_TX_STATUS0 0x10
#define REG_TX_ADDR0 0x20
#define REG_RX_START 0x30
#define REG_CMD 0x37
#define REG_RX_CONFIG 0x44
#define REG_TX_CONFIG 0x40
#define REG_MISS 0x4C
#define REG_CONFIG1 0x52
#define REG_ISR 0x3E
#define REG_IMR 0x3C
#define REG_CAPR 0x38
#define REG_CBR 0x3A
#define ISR_RX_OK 0x0001
#define ISR_RX_ERR 0x0002
#define ISR_TX_OK 0x0004
#define ISR_TX_ERR 0x0008

static uint16_t s_io = 0;
static uint8_t s_mac[6];
static uint8_t *s_rx = 0;
static uint64_t s_rx_phys = 0;
static uint8_t *s_tx = 0;
static uint64_t s_tx_phys = 0;
static uint16_t s_rx_offset = 0;
static uint8_t s_tx_index = 0;
static int s_ready = 0;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void delay(void) {
    for (volatile int i = 0; i < 10000; i++) __asm__ volatile ("pause");
}

static uint16_t rx_read16(uint16_t off) {
    return (uint16_t)s_rx[off % RX_BUFFER_SIZE] | ((uint16_t)s_rx[(off + 1) % RX_BUFFER_SIZE] << 8);
}

bool rtl8139_init(void) {
    uint8_t bus_found = 0, slot_found = 0, func_found = 0;
    int found = 0;
    for (uint16_t bus = 0; bus < 256 && !found; bus++) {
        for (uint8_t slot = 0; slot < 32 && !found; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_read_config((uint8_t)bus, slot, func, 0x00);
                uint16_t vendor = (uint16_t)(id & 0xFFFFu);
                uint16_t device = (uint16_t)(id >> 16);
                if (vendor == 0x10EC && (device == 0x8139 || device == 0x8139)) {
                    bus_found = (uint8_t)bus;
                    slot_found = slot;
                    func_found = func;
                    found = 1;
                    break;
                }
            }
        }
    }
    if (!found) return false;

    uint32_t command = pci_read_config(bus_found, slot_found, func_found, 0x04);
    command |= 0x00000005u;
    outl(PCI_CONFIG_ADDRESS, 0x80000000u | ((uint32_t)bus_found << 16) | ((uint32_t)slot_found << 11) | ((uint32_t)func_found << 8) | 0x04);
    outl(PCI_CONFIG_DATA, command);

    uint32_t bar0 = pci_read_config(bus_found, slot_found, func_found, 0x10);
    if ((bar0 & 1u) == 0) return false;
    s_io = (uint16_t)(bar0 & 0xFFFCu);
    if (!s_io) return false;

    outb(s_io + REG_CONFIG1, 0x00);
    outb(s_io + REG_CMD, CMD_RESET);
    for (int i = 0; i < 100000; i++) {
        if (!(inb(s_io + REG_CMD) & CMD_RESET)) break;
    }

    for (int i = 0; i < 6; i++) s_mac[i] = inb(s_io + REG_MAC + i);
    for (int i = 0; i < 4; i++) outl(s_io + REG_MAR0 + i * 4, 0);

    s_rx_phys = pmm_alloc_pages(2);
    s_rx = (uint8_t *)(s_rx_phys + g_hhdm_offset);
    s_tx_phys = pmm_alloc_pages(2);
    s_tx = (uint8_t *)(s_tx_phys + g_hhdm_offset);
    if (!s_rx || !s_tx) return false;

    outl(s_io + REG_RX_START, (uint32_t)s_rx_phys);
    outw(s_io + REG_CAPR, 0xFFF0);
    outl(s_io + REG_RX_CONFIG, 0x0000000Fu);
    outl(s_io + REG_TX_CONFIG, 0x03000700u);
    outl(s_io + REG_MISS, 0);
    outw(s_io + REG_IMR, 0);
    outb(s_io + REG_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);
    s_rx_offset = 0;
    s_tx_index = 0;
    s_ready = 1;

    for (int i = 0; i < 10000; i++) __asm__ volatile ("pause");
    return true;
}

const uint8_t *rtl8139_get_mac(void) {
    return s_mac;
}

int rtl8139_send_packet(const void *data, uint16_t len) {
    if (!s_ready || !data || len == 0 || len > TX_BUFFER_SIZE) return -1;
    uint8_t *dst = s_tx + (uint32_t)s_tx_index * TX_BUFFER_SIZE;
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < len; i++) dst[i] = src[i];
    uint32_t addr = (uint32_t)(s_tx_phys + (uint64_t)s_tx_index * TX_BUFFER_SIZE);
    outl(s_io + REG_TX_ADDR0 + s_tx_index * 4, addr);
    outl(s_io + REG_TX_STATUS0 + s_tx_index * 4, len);
    for (int i = 0; i < 100000; i++) {
        uint32_t status = inl(s_io + REG_TX_STATUS0 + s_tx_index * 4);
        if (status & 0x8000u) break;
    }
    s_tx_index = (uint8_t)((s_tx_index + 1) & 3);
    return (int)len;
}

int rtl8139_poll_packet(void *out_buf, uint16_t max_len) {
    if (!s_ready || !out_buf || max_len == 0) return 0;
    uint8_t cmd = inb(s_io + REG_CMD);
    if (!(cmd & CMD_RX_ENABLE)) return 0;
    uint16_t cbr = inw(s_io + REG_CBR);
    (void)cbr;
    uint16_t capr = inw(s_io + REG_CAPR);
    uint16_t current = (uint16_t)((capr + 0x10u) & 0x1FFFu);
    if (current == s_rx_offset) return 0;

    uint16_t packet_pos = s_rx_offset;
    uint16_t status = rx_read16(packet_pos);
    uint16_t len = rx_read16((uint16_t)(packet_pos + 2));
    if (!(status & 0x0001u) || len < 4 || len > 2048) {
        s_rx_offset = 0;
        outw(s_io + REG_CAPR, 0xFFF0);
        return 0;
    }

    uint16_t payload_len = (uint16_t)(len - 4);
    if (payload_len > max_len) payload_len = max_len;
    for (uint16_t i = 0; i < payload_len; i++) out_buf ? ((uint8_t *)out_buf)[i] = s_rx[(packet_pos + 4 + i) % RX_BUFFER_SIZE] : 0;

    s_rx_offset = (uint16_t)((packet_pos + len + 4 + 3) & ~3u);
    s_rx_offset &= 0x1FFFu;
    outw(s_io + REG_CAPR, (uint16_t)((s_rx_offset - 16) & 0x1FFFu));
    return (int)payload_len;
}
