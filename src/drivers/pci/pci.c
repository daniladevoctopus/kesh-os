#include "pci.h"
#include "memory.h"
#include "acpi.h"
#include <stddef.h>

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

typedef struct {
    acpi_sdt_header_t header;
    uint64_t reserved;
} __attribute__((packed)) pci_mcfg_t;

typedef struct {
    uint64_t base;
    uint16_t segment;
    uint8_t start_bus;
    uint8_t end_bus;
    uint32_t reserved;
} __attribute__((packed)) pci_mcfg_entry_t;

#define PCI_MAX_ECAM_REGIONS 8

static pci_mcfg_entry_t g_ecam_regions[PCI_MAX_ECAM_REGIONS];
static uint8_t g_ecam_region_count;
static volatile uint8_t g_pci_config_lock;

static uint64_t pci_config_lock(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    while (__atomic_test_and_set(&g_pci_config_lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    return flags;
}

static void pci_config_unlock(uint64_t flags) {
    __atomic_clear(&g_pci_config_lock, __ATOMIC_RELEASE);
    __asm__ volatile("pushq %0; popfq" :: "r"(flags) : "memory", "cc");
}

static volatile uint32_t *pci_ecam_address(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    for (uint8_t i = 0; i < g_ecam_region_count; ++i) {
        const pci_mcfg_entry_t *region = &g_ecam_regions[i];
        if (region->segment != 0 || bus < region->start_bus || bus > region->end_bus) continue;
        uint64_t address = region->base + ((uint64_t)(bus - region->start_bus) << 20) +
                           ((uint64_t)slot << 15) + ((uint64_t)function << 12) + (offset & 0xFCU);
        if (address < region->base || !g_hhdm_offset) return NULL;
        return (volatile uint32_t *)(address + g_hhdm_offset);
    }
    return NULL;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint64_t flags = pci_config_lock();
    volatile uint32_t *ecam = pci_ecam_address(bus, slot, function, offset);
    if (ecam) {
        uint32_t value = *ecam;
        pci_config_unlock(flags);
        return value;
    }
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)function << 8) | (offset & 0xFCu);
    outl(PCI_ADDR, address);
    uint32_t value = inl(PCI_DATA);
    pci_config_unlock(flags);
    return value;
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    uint64_t flags = pci_config_lock();
    volatile uint32_t *ecam = pci_ecam_address(bus, slot, function, offset);
    if (ecam) {
        *ecam = value;
        pci_config_unlock(flags);
        return;
    }
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)function << 8) | (offset & 0xFCu);
    outl(PCI_ADDR, address);
    outl(PCI_DATA, value);
    pci_config_unlock(flags);
}

int pci_init(void) {
    g_ecam_region_count = 0;
    g_pci_config_lock = 0;
    const pci_mcfg_t *mcfg = (const pci_mcfg_t *)acpi_find_table("MCFG");
    if (!mcfg || mcfg->header.length < sizeof(*mcfg)) return 0;
    uint32_t available = (mcfg->header.length - (uint32_t)sizeof(*mcfg)) / sizeof(pci_mcfg_entry_t);
    const pci_mcfg_entry_t *entries = (const pci_mcfg_entry_t *)((const uint8_t *)mcfg + sizeof(*mcfg));
    for (uint32_t i = 0; i < available && g_ecam_region_count < PCI_MAX_ECAM_REGIONS; ++i) {
        const pci_mcfg_entry_t *entry = &entries[i];
        if (!entry->base || (entry->base & 0xFFFFFULL) || entry->start_bus > entry->end_bus) continue;
        uint64_t span = ((uint64_t)entry->end_bus - entry->start_bus + 1ULL) << 20;
        if (entry->base > ~0ULL - span) continue;
        g_ecam_regions[g_ecam_region_count++] = *entry;
    }
    return g_ecam_region_count;
}

int pci_uses_ecam(void) { return g_ecam_region_count != 0; }

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read32(bus, slot, function, offset);
    return (uint16_t)(value >> ((offset & 2u) * 8u));
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read32(bus, slot, function, offset);
    return (uint8_t)(value >> ((offset & 3u) * 8u));
}

static void fill_device(pci_device_t *d, uint8_t bus, uint8_t slot, uint8_t function) {
    uint32_t id = pci_read32(bus, slot, function, 0x00);
    uint32_t class_reg = pci_read32(bus, slot, function, 0x08);
    d->bus = bus;
    d->slot = slot;
    d->function = function;
    d->vendor = (uint16_t)(id & 0xFFFFu);
    d->device = (uint16_t)(id >> 16);
    d->revision = (uint8_t)(class_reg & 0xFFu);
    d->prog_if = (uint8_t)((class_reg >> 8) & 0xFFu);
    d->subclass = (uint8_t)((class_reg >> 16) & 0xFFu);
    d->class_code = (uint8_t)((class_reg >> 24) & 0xFFu);
}

int pci_find(uint16_t vendor, uint16_t device, pci_device_t *out) {
    if (!out) return -1;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t function = 0; function < 8; function++) {
                uint16_t v = pci_read16((uint8_t)bus, slot, function, 0x00);
                if (v == 0xFFFFu) {
                    if (function == 0) break;
                    continue;
                }
                uint16_t d = pci_read16((uint8_t)bus, slot, function, 0x02);
                if (v == vendor && d == device) {
                    fill_device(out, (uint8_t)bus, slot, function);
                    return 0;
                }
            }
        }
    }
    return -1;
}

int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, pci_device_t *out) {
    if (!out) return -1;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t function = 0; function < 8; function++) {
                uint16_t vendor = pci_read16((uint8_t)bus, slot, function, 0x00);
                if (vendor == 0xFFFFu) {
                    if (function == 0) break;
                    continue;
                }
                fill_device(out, (uint8_t)bus, slot, function);
                if (out->class_code == class_code && out->subclass == subclass &&
                    (prog_if == 0xFFu || out->prog_if == prog_if)) return 0;
            }
        }
    }
    return -1;
}

int pci_enumerate(pci_device_t *out, int max_devices) {
    int count = 0;
    for (uint16_t bus = 0; bus < 256 && count < max_devices; bus++) {
        for (uint8_t slot = 0; slot < 32 && count < max_devices; slot++) {
            for (uint8_t function = 0; function < 8 && count < max_devices; function++) {
                uint16_t vendor = pci_read16((uint8_t)bus, slot, function, 0x00);
                if (vendor == 0xFFFFu) {
                    if (function == 0) break;
                    continue;
                }
                fill_device(&out[count++], (uint8_t)bus, slot, function);
                if (function == 0) {
                    uint8_t header = pci_read8((uint8_t)bus, slot, function, 0x0E);
                    if (!(header & 0x80u)) break;
                }
            }
        }
    }
    return count;
}

uint64_t pci_bar_address(const pci_device_t *device, int bar_index, int *is_io) {
    if (!device || bar_index < 0 || bar_index > 5) return 0;
    uint8_t offset = (uint8_t)(0x10 + bar_index * 4);
    uint32_t low = pci_read32(device->bus, device->slot, device->function, offset);
    if (!low || low == 0xFFFFFFFFu) return 0;
    if (low & 1u) {
        if (is_io) *is_io = 1;
        return low & ~3u;
    }
    if (is_io) *is_io = 0;
    uint32_t type = (low >> 1) & 3u;
    if (type == 2 && bar_index < 5) {
        uint32_t high = pci_read32(device->bus, device->slot, device->function, (uint8_t)(offset + 4));
        return ((uint64_t)high << 32) | (low & ~0xFu);
    }
    return low & ~0xFu;
}

int pci_probe_bar(const pci_device_t *device, int bar_index, pci_bar_resource_t *resource) {
    if (!device || !resource || bar_index < 0 || bar_index > 5) return -1;
    uint8_t header_type = pci_read8(device->bus, device->slot, device->function, 0x0E) & 0x7FU;
    int bar_count = header_type == 0 ? 6 : header_type == 1 ? 2 : 0;
    if (!bar_count || bar_index >= bar_count) return -1;
    uint8_t offset = (uint8_t)(0x10 + bar_index * 4);
    uint32_t command_status = pci_read32(device->bus, device->slot, device->function, 0x04);
    uint32_t original_low = pci_read32(device->bus, device->slot, device->function, offset);
    if (!original_low || original_low == 0xFFFFFFFFU) return -1;
    pci_write32(device->bus, device->slot, device->function, 0x04, command_status & ~3U);
    resource->is_io = (uint8_t)(original_low & 1U);
    resource->is_64 = 0;
    resource->prefetchable = 0;
    resource->address = 0;
    resource->size = 0;
    if (resource->is_io) {
        pci_write32(device->bus, device->slot, device->function, offset, 0xFFFFFFFFU);
        uint32_t mask = pci_read32(device->bus, device->slot, device->function, offset) & ~3U;
        pci_write32(device->bus, device->slot, device->function, offset, original_low);
        resource->address = original_low & ~3U;
        if (mask) resource->size = (uint64_t)(~mask + 1U);
    } else {
        uint32_t type = (original_low >> 1) & 3U;
        resource->prefetchable = (uint8_t)((original_low >> 3) & 1U);
        if (type == 2 && bar_index + 1 < bar_count) {
            uint32_t original_high = pci_read32(device->bus, device->slot, device->function, (uint8_t)(offset + 4));
            pci_write32(device->bus, device->slot, device->function, offset, 0xFFFFFFFFU);
            pci_write32(device->bus, device->slot, device->function, (uint8_t)(offset + 4), 0xFFFFFFFFU);
            uint64_t mask = ((uint64_t)pci_read32(device->bus, device->slot, device->function, (uint8_t)(offset + 4)) << 32) |
                            (pci_read32(device->bus, device->slot, device->function, offset) & ~0xFULL);
            pci_write32(device->bus, device->slot, device->function, offset, original_low);
            pci_write32(device->bus, device->slot, device->function, (uint8_t)(offset + 4), original_high);
            resource->address = ((uint64_t)original_high << 32) | (original_low & ~0xFULL);
            resource->is_64 = 1;
            if (mask) resource->size = ~mask + 1ULL;
        } else {
            pci_write32(device->bus, device->slot, device->function, offset, 0xFFFFFFFFU);
            uint32_t mask = pci_read32(device->bus, device->slot, device->function, offset) & ~0xFU;
            pci_write32(device->bus, device->slot, device->function, offset, original_low);
            resource->address = original_low & ~0xFULL;
            if (mask) resource->size = (uint64_t)(~mask + 1U);
        }
    }
    pci_write32(device->bus, device->slot, device->function, 0x04, command_status);
    return resource->size ? 0 : -1;
}

int pci_find_capability(const pci_device_t *device, uint8_t capability_id, uint8_t *out_offset) {
    if (!device) return -1;
    uint16_t status = pci_read16(device->bus, device->slot, device->function, 0x06);
    if (!(status & (1U << 4))) return -1;
    uint8_t offset = pci_read8(device->bus, device->slot, device->function, 0x34) & 0xFCU;
    for (int visited = 0; offset >= 0x40 && visited < 48; ++visited) {
        uint8_t id = pci_read8(device->bus, device->slot, device->function, offset);
        uint8_t next = pci_read8(device->bus, device->slot, device->function, (uint8_t)(offset + 1)) & 0xFCU;
        if (id == capability_id) { if (out_offset) *out_offset = offset; return 0; }
        if (!next || next == offset) break;
        offset = next;
    }
    return -1;
}

uint16_t pci_command(const pci_device_t *device) {
    return device ? pci_read16(device->bus, device->slot, device->function, 0x04) : 0;
}

int pci_enable_device(const pci_device_t *device, uint16_t command_bits) {
    if (!device) return -1;
    uint32_t value = pci_read32(device->bus, device->slot, device->function, 0x04);
    if ((value & 0xFFFFU) == 0xFFFFU) return -1;
    uint16_t before = (uint16_t)value;
    uint16_t after = (uint16_t)(before | command_bits);
    if (after == before) return 0;
    value = (value & 0xFFFF0000U) | after;
    pci_write32(device->bus, device->slot, device->function, 0x04, value);
    return (pci_command(device) & command_bits) == command_bits ? 0 : -1;
}

int pci_enable_msi(const pci_device_t *device, uint8_t vector, uint8_t destination_apic_id) {
    if (!device || vector < 32) return -1;
    uint8_t capability = 0;
    if (pci_find_capability(device, 0x05, &capability) != 0) return -1;
    uint16_t control = pci_read16(device->bus, device->slot, device->function, (uint8_t)(capability + 2));
    uint32_t address = 0xFEE00000U | ((uint32_t)destination_apic_id << 12);
    pci_write32(device->bus, device->slot, device->function, (uint8_t)(capability + 4), address);
    uint8_t data_offset = (control & (1U << 7)) ? (uint8_t)(capability + 12) : (uint8_t)(capability + 8);
    uint32_t data_register = pci_read32(device->bus, device->slot, device->function, data_offset);
    uint32_t shift = (uint32_t)(data_offset & 2U) * 8U;
    data_register = (data_register & ~(0xFFFFU << shift)) | ((uint32_t)vector << shift);
    pci_write32(device->bus, device->slot, device->function, data_offset, data_register);
    uint32_t header = pci_read32(device->bus, device->slot, device->function, capability);
    header |= 1U << 16;
    header &= ~(7U << 20);
    pci_write32(device->bus, device->slot, device->function, capability, header);
    uint32_t command_status = pci_read32(device->bus, device->slot, device->function, 0x04);
    pci_write32(device->bus, device->slot, device->function, 0x04, command_status | (1U << 10));
    return (pci_read16(device->bus, device->slot, device->function, (uint8_t)(capability + 2)) & 1U) ? 0 : -1;
}

int pci_disable_msi(const pci_device_t *device) {
    if (!device) return -1;
    uint8_t capability = 0;
    if (pci_find_capability(device, 0x05, &capability) != 0) return -1;
    uint32_t header = pci_read32(device->bus, device->slot, device->function, capability);
    pci_write32(device->bus, device->slot, device->function, capability, header & ~(1U << 16));
    return 0;
}

int pci_enable_msix(const pci_device_t *device, uint16_t table_index, uint8_t vector, uint8_t destination_apic_id) {
    if (!device || vector < 32 || !g_hhdm_offset) return -1;
    uint8_t capability = 0;
    if (pci_find_capability(device, 0x11, &capability) != 0) return -1;
    uint16_t control = pci_read16(device->bus, device->slot, device->function, (uint8_t)(capability + 2));
    uint16_t table_count = (uint16_t)((control & 0x07FFU) + 1U);
    if (table_index >= table_count) return -1;
    uint32_t table = pci_read32(device->bus, device->slot, device->function, (uint8_t)(capability + 4));
    uint8_t bir = (uint8_t)(table & 7U);
    if (bir > 5) return -1;
    int is_io = 0;
    uint64_t bar = pci_bar_address(device, bir, &is_io);
    if (!bar || is_io) return -1;
    uint64_t table_offset = table & ~7ULL;
    uint64_t entry_offset = table_offset + (uint64_t)table_index * 16ULL;
    if (entry_offset < table_offset) return -1;
    volatile uint32_t *entry = (volatile uint32_t *)(bar + entry_offset + g_hhdm_offset);
    entry[3] |= 1U;
    entry[0] = 0xFEE00000U | ((uint32_t)destination_apic_id << 12);
    entry[1] = 0;
    entry[2] = vector;
    entry[3] &= ~1U;
    uint32_t header = pci_read32(device->bus, device->slot, device->function, capability);
    header |= 1U << 31;
    header &= ~(1U << 30);
    pci_write32(device->bus, device->slot, device->function, capability, header);
    uint32_t command_status = pci_read32(device->bus, device->slot, device->function, 0x04);
    pci_write32(device->bus, device->slot, device->function, 0x04, command_status | (1U << 10));
    return (pci_read16(device->bus, device->slot, device->function, (uint8_t)(capability + 2)) & (1U << 15)) ? 0 : -1;
}

int pci_disable_msix(const pci_device_t *device) {
    if (!device) return -1;
    uint8_t capability = 0;
    if (pci_find_capability(device, 0x11, &capability) != 0) return -1;
    uint32_t header = pci_read32(device->bus, device->slot, device->function, capability);
    pci_write32(device->bus, device->slot, device->function, capability, header & ~(1U << 31));
    return 0;
}
