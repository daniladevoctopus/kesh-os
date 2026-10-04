#ifndef KESHOS_UNIX_IPC_H
#define KESHOS_UNIX_IPC_H

#include <stdint.h>
#include <stddef.h>

/* MemFD Definitions */
#define MFD_CLOEXEC       0x0001U
#define MFD_ALLOW_SEALING 0x0002U

#define F_SEAL_SEAL   0x0001
#define F_SEAL_SHRINK 0x0002
#define F_SEAL_GROW   0x0004
#define F_SEAL_WRITE  0x0008

/* Socket Definitions */
#define AF_UNIX       1
#define SOCK_STREAM   1
#define SOCK_DGRAM    2
#define SOCK_CLOEXEC  02000000
#define SOCK_NONBLOCK 00004000

#define SOL_SOCKET  1
#define SCM_RIGHTS  1

/* Epoll Definitions */
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLLIN       0x001
#define EPOLLPRI      0x002
#define EPOLLOUT      0x004
#define EPOLLERR      0x008
#define EPOLLHUP      0x010
#define EPOLLRDHUP    0x2000

typedef union {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} linux_epoll_data_t;

struct linux_epoll_event {
    uint32_t events;
    linux_epoll_data_t data;
} __attribute__((packed));

/* MemFD API */
void unix_ipc_init(void);
int unix_memfd_create(int pid, const char *name, uint32_t flags);
int unix_memfd_ftruncate(int pid, int fd, uint64_t length);
uint64_t unix_memfd_mmap(int pid, int fd, uint64_t length, uint32_t prot, uint64_t offset);
void unix_memfd_retain(void *custom_ptr);
int unix_memfd_read(void *custom_ptr, uint64_t *offset, void *dst, uint32_t count);
int unix_memfd_write(void *custom_ptr, uint64_t *offset, const void *src, uint32_t count);
uint64_t unix_memfd_get_size(void *custom_ptr);
void unix_memfd_release(void *custom_ptr);

/* UNIX Domain Socket API */
int unix_socket_create(int pid, int type, int protocol);
int unix_socket_bind(int pid, int fd, const char *path);
int unix_socket_listen(int pid, int fd, int backlog);
int unix_socket_connect(int pid, int fd, const char *path);
int unix_socket_accept(int pid, int fd, char *addr_out, int nonblock);
int unix_socket_pair(int pid, int sv[2]);
int unix_socket_get_peercred(int pid, int fd, int *peer_pid, uint32_t *uid, uint32_t *gid);
int unix_socket_recv_stream(void *custom_ptr, void *dst, uint32_t count);
int unix_socket_send_stream(void *custom_ptr, const void *src, uint32_t count);
int unix_socket_poll_events(void *custom_ptr, uint32_t events);
int64_t unix_socket_sendmsg(int pid, int fd, uint64_t u_msghdr, int flags);
int64_t unix_socket_recvmsg(int pid, int fd, uint64_t u_msghdr, int flags);
void unix_socket_release(void *custom_ptr);

/* Epoll API */
int unix_epoll_create(int pid, int flags);
int unix_epoll_ctl(int pid, int epfd, int op, int target_fd, uint64_t u_event);
int unix_epoll_wait(int pid, int epfd, uint64_t u_events, int maxevents, int timeout_ms);
void unix_epoll_release(void *custom_ptr);

int unix_eventfd_create(int pid, uint32_t initial_value, int flags);
int unix_eventfd_read(void *custom_ptr, void *dst, uint32_t count);
int unix_eventfd_write(void *custom_ptr, const void *src, uint32_t count);
int unix_eventfd_poll(void *custom_ptr, uint32_t events);
void unix_eventfd_release(void *custom_ptr);

#endif
