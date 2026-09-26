#include "usb.h"
#include "../pci/pci.h"

static uint32_t s_count = 0;

int usb_init(void) {
    pci_device_t d;
    s_count = 0;
    for (uint8_t subclass = 0; subclass <= 3; subclass++) {
        if (pci_find_class(0x0C, subclass, 0xFF, &d) == 0) s_count++;
    }
    return s_count ? 1 : 0;
}

int usb_is_controller_present(void) {
    return s_count != 0;
}

uint32_t usb_controller_count(void) {
    return s_count;
}
