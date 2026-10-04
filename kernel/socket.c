#include "socket.h"
#include "../src/drivers/net/net_stack.h"
#include "timer.h"

#define SOCKET_MAX 8
typedef struct {
    int owner_pid;
    uint8_t open, connected, protocol, udp_ready;
    uint16_t local_port, remote_port, udp_length;
    uint32_t remote_address, udp_source_address;
    uint16_t udp_source_port;
    uint8_t udp_data[2048];
} socket_t;
static socket_t g_sockets[SOCKET_MAX];
static int g_connected_handle = -1;

static void socket_udp_input(uint32_t source_ip, uint16_t source_port, uint16_t destination_port,
                             const void *data, uint16_t length) {
    for (int i = 0; i < SOCKET_MAX; ++i) {
        socket_t *socket = &g_sockets[i];
        if (!socket->open || socket->protocol != KESH_SOCKET_UDP || socket->local_port != destination_port) continue;
        if (socket->connected && (socket->remote_address != source_ip || socket->remote_port != source_port)) continue;
        if (length > sizeof(socket->udp_data)) length = sizeof(socket->udp_data);
        for (uint16_t b = 0; b < length; ++b) socket->udp_data[b] = ((const uint8_t *)data)[b];
        socket->udp_length = length; socket->udp_source_address = source_ip;
        socket->udp_source_port = source_port; socket->udp_ready = 1;
        return;
    }
}

void socket_init(void) {
    for (int i = 0; i < SOCKET_MAX; ++i) { g_sockets[i].owner_pid = -1; g_sockets[i].open = 0; g_sockets[i].connected = 0; g_sockets[i].udp_ready = 0; }
    g_connected_handle = -1;
    net_udp_set_handler(socket_udp_input);
}
static socket_t *owned(int owner_pid, int handle) {
    return handle >= 0 && handle < SOCKET_MAX && g_sockets[handle].open && g_sockets[handle].owner_pid == owner_pid ? &g_sockets[handle] : 0;
}
int socket_open(int owner_pid, int protocol) {
    if (owner_pid < 0 || (protocol != KESH_SOCKET_TCP && protocol != KESH_SOCKET_UDP)) return -1;
    for (int i = 0; i < SOCKET_MAX; ++i) if (!g_sockets[i].open) {
        g_sockets[i].owner_pid = owner_pid; g_sockets[i].open = 1; g_sockets[i].connected = 0;
        g_sockets[i].protocol = (uint8_t)protocol; g_sockets[i].local_port = 0; g_sockets[i].udp_ready = 0;
        return i;
    }
    return -1;
}
int socket_connect(int owner_pid, int handle, uint32_t address, uint16_t port) {
    socket_t *socket = owned(owner_pid, handle);
    if (!socket || !address || !port) return -1;
    if (socket->protocol == KESH_SOCKET_UDP) {
        if (!socket->local_port) socket->local_port = (uint16_t)(49152 + handle);
        socket->remote_address = address; socket->remote_port = port; socket->connected = 1;
        return 0;
    }
    if (g_connected_handle >= 0 && g_connected_handle != handle) return -1;
    if (net_tcp_connect(address, port, 5000) != 0) return -1;
    socket->connected = 1; g_connected_handle = handle;
    return 0;
}
int socket_bind(int owner_pid, int handle, uint32_t address, uint16_t port) {
    socket_t *socket = owned(owner_pid, handle);
    if (!socket || socket->protocol != KESH_SOCKET_UDP || !port) return -1;
    uint32_t local = net_get_my_ip();
    if (address && address != local) return -1;
    for (int i = 0; i < SOCKET_MAX; ++i) {
        if (i != handle && g_sockets[i].open && g_sockets[i].protocol == KESH_SOCKET_UDP && g_sockets[i].local_port == port) return -1;
    }
    socket->local_port = port;
    return 0;
}
int socket_send_to(int owner_pid, int handle, uint32_t address, uint16_t port, const void *data, uint16_t length) {
    socket_t *socket = owned(owner_pid, handle);
    if (!socket || socket->protocol != KESH_SOCKET_UDP || !address || !port || !data || !length) return -1;
    if (!socket->local_port) socket->local_port = (uint16_t)(49152 + handle);
    return net_udp_send(address, socket->local_port, port, data, length);
}

int socket_recv_from(int owner_pid, int handle, uint32_t *address, uint16_t *port,
                     void *data, uint16_t length, uint32_t timeout_ms) {
    socket_t *socket = owned(owner_pid, handle);
    if (!socket || socket->protocol != KESH_SOCKET_UDP || !socket->local_port || !data || !length) return -1;
    uint64_t start = timer_millis();
    while (!socket->udp_ready) {
        net_poll();
        if (timer_millis() - start >= timeout_ms) return 0;
        __asm__ volatile("pause");
    }
    uint16_t count = socket->udp_length < length ? socket->udp_length : length;
    for (uint16_t i = 0; i < count; ++i) ((uint8_t *)data)[i] = socket->udp_data[i];
    if (address) *address = socket->udp_source_address;
    if (port) *port = socket->udp_source_port;
    socket->udp_ready = 0;
    return count;
}
int socket_send(int owner_pid, int handle, const void *data, uint16_t length) {
    socket_t *socket = owned(owner_pid, handle);
    if (!socket || !socket->connected || !data || !length) return -1;
    if (socket->protocol == KESH_SOCKET_UDP) return net_udp_send(socket->remote_address, socket->local_port, socket->remote_port, data, length);
    return net_tcp_send(data, length);
}
int socket_recv(int owner_pid, int handle, void *data, uint16_t length, uint32_t timeout_ms) {
    socket_t *socket = owned(owner_pid, handle);
    if (!socket || !socket->connected || !data || !length) return -1;
    if (socket->protocol == KESH_SOCKET_TCP) return net_tcp_recv(data, length, timeout_ms);
    return socket_recv_from(owner_pid, handle, 0, 0, data, length, timeout_ms);
}
int socket_poll(int owner_pid, int handle, uint32_t events) {
    if (events & ~(SOCKET_POLL_READ | SOCKET_POLL_WRITE | SOCKET_POLL_HANGUP)) return -1;
    socket_t *socket = owned(owner_pid, handle);
    if (!socket) return -1;
    net_poll();
    uint32_t ready = 0;
    if (socket->protocol == KESH_SOCKET_UDP) {
        if (socket->udp_ready) ready |= SOCKET_POLL_READ;
        ready |= SOCKET_POLL_WRITE;
    } else {
        if (net_tcp_available() > 0) ready |= SOCKET_POLL_READ;
        if (net_tcp_is_connected()) ready |= SOCKET_POLL_WRITE;
        if (socket->connected && !net_tcp_is_connected()) ready |= SOCKET_POLL_HANGUP;
    }
    return (int)(ready & events);
}
int socket_close(int owner_pid, int handle) {
    socket_t *socket = owned(owner_pid, handle);
    if (!socket) return -1;
    if (socket->protocol == KESH_SOCKET_TCP && socket->connected && g_connected_handle == handle) { net_tcp_close(); g_connected_handle = -1; }
    socket->open = 0; socket->connected = 0; socket->owner_pid = -1;
    return 0;
}
void socket_close_owner(int owner_pid) { for (int i = 0; i < SOCKET_MAX; ++i) if (g_sockets[i].open && g_sockets[i].owner_pid == owner_pid) (void)socket_close(owner_pid, i); }
