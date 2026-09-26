#include "pci.h"
#include <stddef.h>

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)function << 8) | (offset & 0xFCu);
    outl(PCI_ADDR, address);
    return inl(PCI_DATA);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)function << 8) | (offset & 0xFCu);
    outl(PCI_ADDR, address);
    outl(PCI_DATA, value);
}

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
