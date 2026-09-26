#ifndef KESH_PCI_H
#define KESH_PCI_H

#include <stdint.h>

#define PCI_CLASS_STORAGE 0x01
#define PCI_CLASS_NETWORK 0x02
#define PCI_CLASS_DISPLAY 0x03
#define PCI_CLASS_BRIDGE  0x06
#define PCI_CLASS_SERIAL  0x0C

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor;
    uint16_t device;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
} pci_device_t;

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
void pci_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
int pci_find(uint16_t vendor, uint16_t device, pci_device_t *out);
int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, pci_device_t *out);
int pci_enumerate(pci_device_t *out, int max_devices);
uint64_t pci_bar_address(const pci_device_t *device, int bar_index, int *is_io);

#endif
