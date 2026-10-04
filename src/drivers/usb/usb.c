#include "usb.h"
#include "../pci/pci.h"
#include "memory.h"
#include "timer.h"

static usb_controller_info_t s_controllers[USB_MAX_CONTROLLERS];
static uint32_t s_count;

static usb_controller_kind_t controller_kind(uint8_t prog_if) {
    if (prog_if == 0x00) return USB_CONTROLLER_UHCI;
    if (prog_if == 0x10) return USB_CONTROLLER_OHCI;
    if (prog_if == 0x20) return USB_CONTROLLER_EHCI;
    if (prog_if == 0x30) return USB_CONTROLLER_XHCI;
    return USB_CONTROLLER_UNKNOWN;
}

static int wait_bit(volatile uint32_t *reg, uint32_t mask, int set, uint32_t timeout_ms) {
    uint64_t deadline = timer_millis() + timeout_ms;
    while (((*reg & mask) != 0) != set) {
        if (timer_millis() >= deadline) return -1;
        __asm__ volatile("pause");
    }
    return 0;
}

static int xhci_start(usb_controller_info_t *info, const pci_device_t *device) {
    int is_io = 0;
    uint64_t physical = pci_bar_address(device, 0, &is_io);
    if (!physical || is_io || !g_hhdm_offset) return -1;
    if (pci_enable_device(device, PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER) != 0) return -1;
    volatile uint8_t *cap = (volatile uint8_t *)(physical + g_hhdm_offset);
    uint8_t cap_length = cap[0];
    if (cap_length < 0x20) return -1;
    uint32_t hcs1 = *(volatile uint32_t *)(cap + 0x04);
    uint8_t ports = (uint8_t)(hcs1 >> 24);
    if (!ports) return -1;
    volatile uint8_t *op = cap + cap_length;
    volatile uint32_t *command = (volatile uint32_t *)(op + 0x00);
    volatile uint32_t *status = (volatile uint32_t *)(op + 0x04);
    *command &= ~1U;
    if (wait_bit(status, 1U, 1, 1000) != 0) return -1;
    *command |= 2U;
    if (wait_bit(command, 2U, 0, 1000) != 0 || wait_bit(status, 1U << 11, 0, 1000) != 0) return -1;
    info->mmio_base = physical;
    info->port_count = ports;
    info->initialized = 1;
    return 0;
}

static void update_xhci_ports(usb_controller_info_t *info) {
    if (!info->initialized || !info->mmio_base) return;
    volatile uint8_t *cap = (volatile uint8_t *)(info->mmio_base + g_hhdm_offset);
    volatile uint8_t *op = cap + cap[0];
    uint8_t connected = 0;
    for (uint8_t port = 0; port < info->port_count; ++port) {
        volatile uint32_t *portsc = (volatile uint32_t *)(op + 0x400 + (uint32_t)port * 0x10U);
        uint32_t value = *portsc;
        if (!(value & (1U << 9))) *portsc = (value & ~0x00FE0000U) | (1U << 9);
        if (value & 1U) connected++;
    }
    info->connected_ports = connected;
}

int usb_init(void) {
    s_count = 0;
    pci_device_t devices[64];
    int count = pci_enumerate(devices, 64);
    for (int i = 0; i < count && s_count < USB_MAX_CONTROLLERS; ++i) {
        pci_device_t *d = &devices[i];
        if (d->class_code != PCI_CLASS_SERIAL || d->subclass != 0x03) continue;
        s_controllers[s_count].bus = d->bus;
        s_controllers[s_count].slot = d->slot;
        s_controllers[s_count].function = d->function;
        s_controllers[s_count].kind = controller_kind(d->prog_if);
        s_controllers[s_count].initialized = 0;
        s_controllers[s_count].port_count = 0;
        s_controllers[s_count].connected_ports = 0;
        s_controllers[s_count].mmio_base = 0;
        if (s_controllers[s_count].kind == USB_CONTROLLER_XHCI) {
            (void)xhci_start(&s_controllers[s_count], d);
            update_xhci_ports(&s_controllers[s_count]);
        }
        s_count++;
    }
    return s_count ? 1 : 0;
}

int usb_is_controller_present(void) {
    return s_count != 0;
}

uint32_t usb_controller_count(void) {
    return s_count;
}

const usb_controller_info_t *usb_controller_get(uint32_t index) {
    return index < s_count ? &s_controllers[index] : 0;
}

void usb_poll(void) {
    for (uint32_t i = 0; i < s_count; ++i) {
        if (s_controllers[i].kind == USB_CONTROLLER_XHCI) update_xhci_ports(&s_controllers[i]);
    }
}
