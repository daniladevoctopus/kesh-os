#include "virtio_net.h"
#include "../../kernel/memory.h"
#include "../pci/pci.h"
#include <stddef.h>

#define VIRTIO_VENDOR 0x1AF4
#define VIRTIO_NET_LEGACY 0x1000
#define VIRTIO_NET_TRANSITIONAL 0x1041
#define VIRTIO_PCI_HOST_FEATURES 0x00
#define VIRTIO_PCI_GUEST_FEATURES 0x04
#define VIRTIO_PCI_QUEUE_PFN 0x08
#define VIRTIO_PCI_QUEUE_SIZE 0x0C
#define VIRTIO_PCI_QUEUE_NOTIFY 0x10
#define VIRTIO_PCI_STATUS 0x12
#define VIRTIO_PCI_ISR 0x13
#define VIRTIO_PCI_CONFIG 0x14

#define VIRTIO_STATUS_ACK 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FAILED 128

#define VRING_DESC_F_NEXT 1
#define VRING_DESC_F_WRITE 2
#define VIRTIO_NET_HDR_LEN 10
#define QUEUE_SIZE 256
#define RX_PACKET_SIZE 2048
#define RX_SLOT_SIZE 4096
#define TX_SLOT_SIZE 4096

struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[QUEUE_SIZE];
} __attribute__((packed));

struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[QUEUE_SIZE];
} __attribute__((packed));

typedef struct {
    uint16_t io_base;
    uint16_t queue_size;
    volatile struct vring_desc *desc;
    volatile struct vring_avail *avail;
    volatile struct vring_used *used;
    uint64_t queue_phys;
    uint16_t last_used;
    uint64_t buf_phys[QUEUE_SIZE];
    uint8_t *buf_virt[QUEUE_SIZE];
    uint8_t free_desc[QUEUE_SIZE];
} virtio_queue_t;

static pci_device_t s_pci;
static uint8_t s_mac[6];
static int s_ready = 0;
static virtio_queue_t s_rxq;
static virtio_queue_t s_txq;

static inline void io_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t io_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t io_inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t io_inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static int find_device(void) {
    if (pci_find(VIRTIO_VENDOR, VIRTIO_NET_LEGACY, &s_pci) == 0) return 0;
    if (pci_find(VIRTIO_VENDOR, VIRTIO_NET_TRANSITIONAL, &s_pci) == 0) return 0;
    return -1;
}

static int queue_setup(virtio_queue_t *q, uint16_t queue_index) {
    int is_io = 0;
    uint64_t bar = pci_bar_address(&s_pci, 0, &is_io);
    if (!bar || !is_io || bar > 0xFFFFu) return -1;
    q->io_base = (uint16_t)bar;
    io_outw(q->io_base + 0x0E, queue_index);
    q->queue_size = io_inw(q->io_base + VIRTIO_PCI_QUEUE_SIZE);
    if (q->queue_size == 0 || q->queue_size > QUEUE_SIZE) return -2;

    uint64_t avail_end = (uint64_t)sizeof(struct vring_desc) * q->queue_size +
                         sizeof(uint16_t) * 2u + sizeof(uint16_t) * q->queue_size;
    uint64_t used_off = (avail_end + 4095u) & ~4095u;
    uint64_t total_bytes = used_off + sizeof(uint16_t) * 2u + sizeof(struct vring_used_elem) * q->queue_size;
    size_t pages = (size_t)((total_bytes + 4095u) / 4096u);
    q->queue_phys = pmm_alloc_pages(pages);
    if (!q->queue_phys || q->queue_phys > 0xFFFFFFFFULL) return -3;
    uint8_t *base = (uint8_t*)(q->queue_phys + g_hhdm_offset);
    q->desc = (volatile struct vring_desc*)base;
    q->avail = (volatile struct vring_avail*)(base + sizeof(struct vring_desc) * q->queue_size);
    q->used = (volatile struct vring_used*)(base + used_off);
    q->last_used = 0;
    for (int i = 0; i < QUEUE_SIZE; i++) q->free_desc[i] = 1;
    io_outl(q->io_base + VIRTIO_PCI_QUEUE_PFN, (uint32_t)(q->queue_phys >> 12));
    return 0;
}

static void queue_publish(virtio_queue_t *q, uint16_t desc_index) {
    uint16_t slot = q->avail->idx % q->queue_size;
    q->avail->ring[slot] = desc_index;
    __asm__ volatile ("sfence" ::: "memory");
    q->avail->idx++;
}

bool virtio_net_init(void) {
    s_ready = 0;
    if (find_device() != 0) return false;
    uint32_t command = pci_read32(s_pci.bus, s_pci.slot, s_pci.function, 0x04);
    pci_write32(s_pci.bus, s_pci.slot, s_pci.function, 0x04, command | 0x7u);

    int is_io = 0;
    uint64_t bar = pci_bar_address(&s_pci, 0, &is_io);
    if (!bar || !is_io || bar > 0xFFFFu) return false;
    uint16_t io = (uint16_t)bar;
    uint8_t status = 0;
    io_outb(io + VIRTIO_PCI_STATUS, status);
    io_outb(io + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACK);
    io_outb(io + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    uint32_t features = io_inl(io + VIRTIO_PCI_HOST_FEATURES);
    uint32_t accepted = features & ((1u << 5) | (1u << 16));
    io_outl(io + VIRTIO_PCI_GUEST_FEATURES, accepted);

    for (int i = 0; i < 6; i++) s_mac[i] = io_inb((uint16_t)(io + VIRTIO_PCI_CONFIG + i));
    int all_zero = 1;
    int all_ff = 1;
    for (int i = 0; i < 6; i++) {
        if (s_mac[i] != 0) all_zero = 0;
        if (s_mac[i] != 0xFF) all_ff = 0;
    }
    if (all_zero || all_ff) return false;

    if (queue_setup(&s_rxq, 0) != 0 || queue_setup(&s_txq, 1) != 0) {
        io_outb(io + VIRTIO_PCI_STATUS, VIRTIO_STATUS_FAILED);
        return false;
    }

    for (int i = 0; i < 8; i++) {
        s_rxq.buf_phys[i] = pmm_alloc_pages(1);
        if (!s_rxq.buf_phys[i] || s_rxq.buf_phys[i] > 0xFFFFFFFFULL) return false;
        s_rxq.buf_virt[i] = (uint8_t*)(s_rxq.buf_phys[i] + g_hhdm_offset);
        s_rxq.desc[i].addr = s_rxq.buf_phys[i];
        s_rxq.desc[i].len = RX_SLOT_SIZE;
        s_rxq.desc[i].flags = VRING_DESC_F_WRITE;
        s_rxq.desc[i].next = 0;
        queue_publish(&s_rxq, (uint16_t)i);
        s_rxq.free_desc[i] = 0;
    }
    s_rxq.used->idx = 0;
    s_rxq.last_used = 0;
    s_txq.buf_phys[0] = pmm_alloc_pages(1);
    if (!s_txq.buf_phys[0] || s_txq.buf_phys[0] > 0xFFFFFFFFULL) return false;
    s_txq.buf_virt[0] = (uint8_t*)(s_txq.buf_phys[0] + g_hhdm_offset);
    s_txq.desc[0].flags = 0;
    s_txq.desc[0].next = 0;

    __asm__ volatile ("sfence" ::: "memory");
    io_outb(io + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);
    io_outw(io + VIRTIO_PCI_QUEUE_NOTIFY, 0);
    s_ready = 1;
    return true;
}

const uint8_t *virtio_net_get_mac(void) {
    return s_mac;
}

int virtio_net_send_packet(const void *data, uint16_t len) {
    if (!s_ready || !data || len == 0 || len > 1900) return -1;
    uint8_t *buf = s_txq.buf_virt[0];
    for (int i = 0; i < VIRTIO_NET_HDR_LEN; i++) buf[i] = 0;
    const uint8_t *src = (const uint8_t*)data;
    for (uint16_t i = 0; i < len; i++) buf[VIRTIO_NET_HDR_LEN + i] = src[i];
    s_txq.desc[0].addr = s_txq.buf_phys[0];
    s_txq.desc[0].len = VIRTIO_NET_HDR_LEN + len;
    s_txq.desc[0].flags = 0;
    uint16_t before = s_txq.avail->idx;
    queue_publish(&s_txq, 0);
    io_outw((uint16_t)(s_txq.io_base + VIRTIO_PCI_QUEUE_NOTIFY), 1);
    for (volatile int t = 0; t < 120000; t++) {
        if (s_txq.used->idx != s_txq.last_used) {
            s_txq.last_used = s_txq.used->idx;
            return (int)len;
        }
    }
    if (s_txq.avail->idx == before) return -2;
    return (int)len;
}

int virtio_net_poll_packet(void *out_buf, uint16_t max_len) {
    if (!s_ready || !out_buf || max_len == 0) return 0;
    if (s_rxq.used->idx == s_rxq.last_used) return 0;
    uint16_t slot = s_rxq.last_used % s_rxq.queue_size;
    uint32_t id = s_rxq.used->ring[slot].id;
    uint32_t used_len = s_rxq.used->ring[slot].len;
    s_rxq.last_used++;
    if (id >= s_rxq.queue_size || id >= 8 || used_len <= VIRTIO_NET_HDR_LEN) return 0;
    uint32_t payload_len = used_len - VIRTIO_NET_HDR_LEN;
    if (payload_len > max_len) payload_len = max_len;
    uint8_t *src = s_rxq.buf_virt[id] + VIRTIO_NET_HDR_LEN;
    uint8_t *dst = (uint8_t*)out_buf;
    for (uint32_t i = 0; i < payload_len; i++) dst[i] = src[i];
    s_rxq.desc[id].addr = s_rxq.buf_phys[id];
    s_rxq.desc[id].len = RX_SLOT_SIZE;
    s_rxq.desc[id].flags = VRING_DESC_F_WRITE;
    s_rxq.desc[id].next = 0;
    queue_publish(&s_rxq, (uint16_t)id);
    io_outw((uint16_t)(s_rxq.io_base + VIRTIO_PCI_QUEUE_NOTIFY), 0);
    return (int)payload_len;
}
