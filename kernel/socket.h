#ifndef KESHOS_SOCKET_H
#define KESHOS_SOCKET_H

#include <stdint.h>

#define KESH_SOCKET_TCP 1
#define KESH_SOCKET_UDP 2
#define SOCKET_POLL_READ 1U
#define SOCKET_POLL_WRITE 2U
#define SOCKET_POLL_HANGUP 4U

typedef struct {
    uint32_t address;
    uint16_t port;
    uint16_t length;
    uint64_t data;
} socket_datagram_t;

void socket_init(void);
int socket_open(int owner_pid, int protocol);
int socket_connect(int owner_pid, int handle, uint32_t address, uint16_t port);
int socket_bind(int owner_pid, int handle, uint32_t address, uint16_t port);
int socket_send_to(int owner_pid, int handle, uint32_t address, uint16_t port, const void *data, uint16_t length);
int socket_recv_from(int owner_pid, int handle, uint32_t *address, uint16_t *port, void *data, uint16_t length, uint32_t timeout_ms);
int socket_poll(int owner_pid, int handle, uint32_t events);
int socket_send(int owner_pid, int handle, const void *data, uint16_t length);
int socket_recv(int owner_pid, int handle, void *data, uint16_t length, uint32_t timeout_ms);
int socket_close(int owner_pid, int handle);
void socket_close_owner(int owner_pid);

#endif
