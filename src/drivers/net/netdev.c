#include "netdev.h"
#include "e1000.h"
#include "rtl8139.h"
#include "virtio_net.h"
#include <stddef.h>

static netdev_info_t s_info;
static int s_ready = 0;
static int s_driver = 0;

static void copy_mac(uint8_t *dst, const uint8_t *src) {
    for (int i = 0; i < 6; i++) dst[i] = src[i];
}

int netdev_init(void) {
    s_ready = 0;
    s_driver = 0;
    for (int i = 0; i < 6; i++) s_info.mac[i] = 0;
    s_info.name = "none";
    s_info.kind = NETDEV_NONE;
    s_info.mtu = 1500;
    s_info.link_up = false;

    if (e1000_init()) {
        const uint8_t *mac = e1000_get_mac();
        copy_mac(s_info.mac, mac);
        s_info.name = "e1000";
        s_info.kind = NETDEV_ETHERNET;
        s_info.link_up = true;
        s_driver = 1;
        s_ready = 1;
        return 1;
    }

    if (rtl8139_init()) {
        const uint8_t *mac = rtl8139_get_mac();
        copy_mac(s_info.mac, mac);
        s_info.name = "rtl8139";
        s_info.kind = NETDEV_ETHERNET;
        s_info.link_up = true;
        s_driver = 2;
        s_ready = 1;
        return 1;
    }

    if (virtio_net_init()) {
        const uint8_t *mac = virtio_net_get_mac();
        copy_mac(s_info.mac, mac);
        s_info.name = "virtio-net";
        s_info.kind = NETDEV_ETHERNET;
        s_info.link_up = true;
        s_driver = 3;
        s_ready = 1;
        return 1;
    }

    return 0;
}

int netdev_send(const void *data, uint16_t len) {
    if (!s_ready) return -1;
    if (s_driver == 1) return e1000_send_packet(data, len);
    if (s_driver == 2) return rtl8139_send_packet(data, len);
    if (s_driver == 3) return virtio_net_send_packet(data, len);
    return -1;
}

int netdev_poll(void *buffer, uint16_t max_len) {
    if (!s_ready) return 0;
    if (s_driver == 1) return e1000_poll_packet(buffer, max_len);
    if (s_driver == 2) return rtl8139_poll_packet(buffer, max_len);
    if (s_driver == 3) return virtio_net_poll_packet(buffer, max_len);
    return 0;
}

const netdev_info_t *netdev_get_info(void) {
    return &s_info;
}

int netdev_is_ready(void) {
    return s_ready;
}
