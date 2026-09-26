#ifndef KESH_NETDEV_H
#define KESH_NETDEV_H

#include <stdint.h>
#include <stdbool.h>

#define NETDEV_MAX_FRAME 2048

typedef enum {
    NETDEV_NONE = 0,
    NETDEV_ETHERNET = 1,
    NETDEV_WIFI = 2,
    NETDEV_VIRTIO = 3
} netdev_kind_t;

typedef struct {
    const char *name;
    netdev_kind_t kind;
    uint8_t mac[6];
    uint16_t mtu;
    bool link_up;
} netdev_info_t;

int netdev_init(void);
int netdev_send(const void *data, uint16_t len);
int netdev_poll(void *buffer, uint16_t max_len);
const netdev_info_t *netdev_get_info(void);
int netdev_is_ready(void);

#endif
