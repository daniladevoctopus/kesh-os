#ifndef KESH_VIRTIO_NET_H
#define KESH_VIRTIO_NET_H

#include <stdint.h>
#include <stdbool.h>

bool virtio_net_init(void);
const uint8_t *virtio_net_get_mac(void);
int virtio_net_send_packet(const void *data, uint16_t len);
int virtio_net_poll_packet(void *out_buf, uint16_t max_len);

#endif
