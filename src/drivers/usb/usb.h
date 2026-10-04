#ifndef KESH_USB_H
#define KESH_USB_H

#include <stdint.h>

#define USB_MAX_CONTROLLERS 8

typedef enum {
    USB_CONTROLLER_UNKNOWN = 0,
    USB_CONTROLLER_UHCI,
    USB_CONTROLLER_OHCI,
    USB_CONTROLLER_EHCI,
    USB_CONTROLLER_XHCI
} usb_controller_kind_t;

typedef struct {
    uint8_t bus, slot, function;
    usb_controller_kind_t kind;
    uint8_t initialized;
    uint8_t port_count;
    uint8_t connected_ports;
    uint64_t mmio_base;
} usb_controller_info_t;

int usb_init(void);
int usb_is_controller_present(void);
uint32_t usb_controller_count(void);
const usb_controller_info_t *usb_controller_get(uint32_t index);
void usb_poll(void);

#endif
