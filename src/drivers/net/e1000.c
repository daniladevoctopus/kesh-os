// сетевуха intel e1000
#include "e1000.h"
#include "../../kernel/memory.h"
#include <stddef.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define REG_CTRL       0x0000
#define REG_STATUS     0x0008
#define REG_EERD       0x0014
#define REG_ICR        0x00D0
#define REG_IMS        0x00D8
#define REG_IMC        0x00E0
#define REG_RCTL       0x0100
#define REG_TCTL       0x0400
#define REG_RDBAL      0x2800
#define REG_RDBAH      0x2804
#define REG_RDLEN      0x2808
#define REG_RDH        0x2810
#define REG_RDT        0x2818
#define REG_TDBAL      0x3800
#define REG_TDBAH      0x3804
#define REG_TDLEN      0x3808
#define REG_TDH        0x3810
#define REG_TDT        0x3818
#define REG_MTA        0x5200
#define REG_RAL        0x5400
#define REG_RAH        0x5404

#define CTRL_SLU       (1 << 6)   
#define CTRL_ASDE      (1 << 5)   

#define RCTL_EN        (1 << 1)
#define RCTL_SBP       (1 << 2)
#define RCTL_UPE       (1 << 3)
#define RCTL_MPE       (1 << 4)
#define RCTL_LPE       (1 << 5)
#define RCTL_BAM       (1 << 15)
#define RCTL_BSIZE_2K  (0 << 16)
#define RCTL_SECRC     (1 << 26)

#define TCTL_EN        (1 << 1)
#define TCTL_PSP       (1 << 3)

#define TX_CMD_EOP     (1 << 0)
#define TX_CMD_IFCS    (1 << 1)
#define TX_CMD_RS      (1 << 3)

static uint64_t s_mmio_base = 0;
static uint8_t s_mac[6] = {0};
static bool s_has_eeprom = false;

static volatile e1000_rx_desc_t *s_rx_descs = NULL;
static uint8_t *s_rx_buffers = NULL;
static uint32_t s_rx_cur = 0;

static volatile e1000_tx_desc_t *s_tx_descs = NULL;
static uint8_t *s_tx_buffers = NULL;
static uint32_t s_tx_tail = 0;

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) |
                       (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) |
                       (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, val);
}

static inline void e1000_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(s_mmio_base + reg) = val;
}

static inline uint32_t e1000_read(uint32_t reg) {
    return *(volatile uint32_t*)(s_mmio_base + reg);
}

static uint16_t e1000_eeprom_read(uint8_t addr) {
    uint32_t temp = ((uint32_t)addr << 8) | 1;
    e1000_write(REG_EERD, temp);
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint32_t val = e1000_read(REG_EERD);
        if (val & (1 << 4)) {
            return (uint16_t)((val >> 16) & 0xFFFF);
        }
    }
    return 0;
}

static void serial_print(const char *s) {
    while (s && *s) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)*s++), "Nd"((uint16_t)0x3F8));
    }
}

static void serial_print_hex8(uint8_t b) {
    const char hex[] = "0123456789ABCDEF";
    char str[3];
    str[0] = hex[(b >> 4) & 0xF];
    str[1] = hex[b & 0xF];
    str[2] = '\0';
    serial_print(str);
}

const uint8_t* e1000_get_mac(void) {
    return s_mac;
}

bool e1000_init(void) {
    serial_print("[E1000] Scanning PCI bus for Intel Gigabit Ethernet...\n");

    uint8_t found_bus = 0, found_slot = 0, found_func = 0;
    bool found = false;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read_config(bus, slot, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xFFFF);
            uint16_t device = (uint16_t)((id >> 16) & 0xFFFF);

            if (vendor == 0x8086) {
                if (device == 0x100E || device == 0x1004 || device == 0x100F ||
                    device == 0x1015 || device == 0x107C) {
                    found_bus = (uint8_t)bus;
                    found_slot = slot;
                    found_func = 0;
                    found = true;
                    break;
                }
            }
        }
        if (found) break;
    }

    if (!found) {
        serial_print("[E1000] No Intel 82540EM/compatible card found on PCI bus.\n");
        return false;
    }

    serial_print("[E1000] Found Intel 82540EM Ethernet controller!\n");

    uint32_t cmd = pci_read_config(found_bus, found_slot, found_func, 0x04);
    cmd |= (1 << 0) | (1 << 1) | (1 << 2); 
    pci_write_config(found_bus, found_slot, found_func, 0x04, cmd);

    uint32_t bar0 = pci_read_config(found_bus, found_slot, found_func, 0x10);
    uint64_t phys_base = (uint64_t)(bar0 & ~0xFULL);
    s_mmio_base = phys_base + g_hhdm_offset;

    uint32_t ctrl = e1000_read(REG_CTRL);
    ctrl |= CTRL_SLU | CTRL_ASDE;
    ctrl &= ~(1 << 3);  
    ctrl &= ~(1U << 31); 
    e1000_write(REG_CTRL, ctrl);

    e1000_write(REG_EERD, 1);
    for (int i = 0; i < 1000; i++) {
        if (e1000_read(REG_EERD) & (1 << 4)) {
            s_has_eeprom = true;
            break;
        }
    }

    if (s_has_eeprom) {
        uint16_t w0 = e1000_eeprom_read(0);
        uint16_t w1 = e1000_eeprom_read(1);
        uint16_t w2 = e1000_eeprom_read(2);
        s_mac[0] = (uint8_t)(w0 & 0xFF);
        s_mac[1] = (uint8_t)((w0 >> 8) & 0xFF);
        s_mac[2] = (uint8_t)(w1 & 0xFF);
        s_mac[3] = (uint8_t)((w1 >> 8) & 0xFF);
        s_mac[4] = (uint8_t)(w2 & 0xFF);
        s_mac[5] = (uint8_t)((w2 >> 8) & 0xFF);
    } else {
        uint32_t ral = e1000_read(REG_RAL);
        uint32_t rah = e1000_read(REG_RAH);
        s_mac[0] = (uint8_t)(ral & 0xFF);
        s_mac[1] = (uint8_t)((ral >> 8) & 0xFF);
        s_mac[2] = (uint8_t)((ral >> 16) & 0xFF);
        s_mac[3] = (uint8_t)((ral >> 24) & 0xFF);
        s_mac[4] = (uint8_t)(rah & 0xFF);
        s_mac[5] = (uint8_t)((rah >> 8) & 0xFF);
    }

    serial_print("[E1000] MAC Address: ");
    for (int i = 0; i < 6; i++) {
        serial_print_hex8(s_mac[i]);
        if (i < 5) serial_print(":");
    }
    serial_print("\n");

    uint32_t ral = (uint32_t)s_mac[0] | ((uint32_t)s_mac[1] << 8) |
                   ((uint32_t)s_mac[2] << 16) | ((uint32_t)s_mac[3] << 24);
    uint32_t rah = (uint32_t)s_mac[4] | ((uint32_t)s_mac[5] << 8) | (1U << 31); 
    e1000_write(REG_RAL, ral);
    e1000_write(REG_RAH, rah);

    for (int i = 0; i < 128; i++) {
        e1000_write(REG_MTA + i * 4, 0);
    }

    e1000_write(REG_IMC, 0xFFFFFFFF);

    uint64_t rx_ring_phys = pmm_alloc_pages(1);
    uint64_t rx_bufs_phys = pmm_alloc_pages(16);
    s_rx_descs = (volatile e1000_rx_desc_t*)(rx_ring_phys + g_hhdm_offset);
    s_rx_buffers = (uint8_t*)(rx_bufs_phys + g_hhdm_offset);
    s_rx_cur = 0;

    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        s_rx_descs[i].buffer_addr = rx_bufs_phys + (uint64_t)i * E1000_RX_BUFFER_SIZE;
        s_rx_descs[i].status = 0;
        s_rx_descs[i].errors = 0;
    }

    e1000_write(REG_RDBAL, (uint32_t)rx_ring_phys);
    e1000_write(REG_RDBAH, (uint32_t)(rx_ring_phys >> 32));
    e1000_write(REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(REG_RDH, 0);
    e1000_write(REG_RDT, E1000_NUM_RX_DESC - 1);

    uint64_t tx_ring_phys = pmm_alloc_pages(1);
    uint64_t tx_bufs_phys = pmm_alloc_pages(16);
    s_tx_descs = (volatile e1000_tx_desc_t*)(tx_ring_phys + g_hhdm_offset);
    s_tx_buffers = (uint8_t*)(tx_bufs_phys + g_hhdm_offset);
    s_tx_tail = 0;

    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        s_tx_descs[i].buffer_addr = tx_bufs_phys + (uint64_t)i * E1000_TX_BUFFER_SIZE;
        s_tx_descs[i].cmd = 0;
        s_tx_descs[i].status = 1; 
    }

    e1000_write(REG_TDBAL, (uint32_t)tx_ring_phys);
    e1000_write(REG_TDBAH, (uint32_t)(tx_ring_phys >> 32));
    e1000_write(REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(REG_TDH, 0);
    e1000_write(REG_TDT, 0);

    uint32_t tctl = TCTL_EN | TCTL_PSP | (15 << 4) | (64 << 12);
    e1000_write(REG_TCTL, tctl);

    uint32_t rctl = RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_BAM | RCTL_BSIZE_2K | RCTL_SECRC;
    e1000_write(REG_RCTL, rctl);

    serial_print("[E1000] Initialization complete. Link forced UP, RX/TX rings enabled.\n");
    return true;
}

int e1000_send_packet(const void *data, uint16_t len) {
    if (!s_tx_descs || len == 0 || len > E1000_TX_BUFFER_SIZE) return -1;

    if (!(s_tx_descs[s_tx_tail].status & 1)) {
        return -2; 
    }

    uint8_t *dst = s_tx_buffers + s_tx_tail * E1000_TX_BUFFER_SIZE;
    const uint8_t *src = (const uint8_t*)data;
    for (uint16_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }

    s_tx_descs[s_tx_tail].length = len;
    s_tx_descs[s_tx_tail].cmd = TX_CMD_EOP | TX_CMD_IFCS | TX_CMD_RS;
    s_tx_descs[s_tx_tail].status = 0;

    uint32_t cur = s_tx_tail;
    s_tx_tail = (s_tx_tail + 1) % E1000_NUM_TX_DESC;
    e1000_write(REG_TDT, s_tx_tail);

    for (volatile int timeout = 0; timeout < 100000; timeout++) {
        if (s_tx_descs[cur].status & 1) break;
    }

    return (int)len;
}

int e1000_poll_packet(void *out_buf, uint16_t max_len) {
    if (!s_rx_descs || !out_buf) return 0;

    if (!(s_rx_descs[s_rx_cur].status & 1)) {
        return 0; 
    }

    uint16_t len = s_rx_descs[s_rx_cur].length;
    if (len > max_len) len = max_len;

    uint8_t *src = s_rx_buffers + s_rx_cur * E1000_RX_BUFFER_SIZE;
    uint8_t *dst = (uint8_t*)out_buf;
    for (uint16_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }

    s_rx_descs[s_rx_cur].status = 0;
    e1000_write(REG_RDT, s_rx_cur);
    s_rx_cur = (s_rx_cur + 1) % E1000_NUM_RX_DESC;

    return (int)len;
}
