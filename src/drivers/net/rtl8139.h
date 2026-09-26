#ifndef KESH_RTL8139_H
#define KESH_RTL8139_H

#include <stdint.h>
#include <stdbool.h>

bool rtl8139_init(void);
const uint8_t *rtl8139_get_mac(void);
int rtl8139_send_packet(const void *data, uint16_t len);
int rtl8139_poll_packet(void *out_buf, uint16_t max_len);

#endif
