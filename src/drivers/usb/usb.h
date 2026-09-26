#ifndef KESH_USB_H
#define KESH_USB_H

#include <stdint.h>

int usb_init(void);
int usb_is_controller_present(void);
uint32_t usb_controller_count(void);

#endif
