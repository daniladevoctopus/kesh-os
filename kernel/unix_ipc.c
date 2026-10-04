#include "unix_ipc.h"
#include "fd.h"
#include "process.h"
#include "memory.h"
#include "timer.h"
#include "serial.h"
#include "log.h"

#include <stddef.h>
#include <stdint.h>

/* ========================================================================= */
/* 1. MEMFD: ANONYMOUS SHARED MEMORY BUFFERS                                */
/* ========================================================================= */

#define MAX_MEMFDS 32
#define MEMFD_MAX_PAGES 4096 /* Up to 16MB shared memory buffer */

typedef struct memfd_object {
    uint32_t refcount;
    int in_use;
    char name[64];
    uint64_t size;
    uint64_t page_count;
    uint64_t phys_pages[MEMFD_MAX_PAGES];
    uint32_t flags;
} memfd_object_t;

static memfd_object_t g_memfds[MAX_MEMFDS];

static memfd_object_t *alloc_memfd(void) {
    for (int i = 0; i < MAX_MEMFDS; i++) {
        if (!g_memfds[i].in_use && g_memfds[i].refcount == 0) {
            memfd_object_t *m = &g_memfds[i];
            m->in_use = 1;
            m->refcount = 1;
            m->size = 0;
            m->page_count = 0;
            m->flags = 0;
            m->name[0] = '\0';
            for (int p = 0; p < MEMFD_MAX_PAGES; p++) m->phys_pages[p] = 0;
            return m;
        }
    }
    return NULL;
}

int unix_memfd_create(int pid, const char *name, uint32_t flags) {
    memfd_object_t *m = alloc_memfd();
    if (!m) return -1;

    m->flags = flags;
    if (name) {
        int i = 0;
        while (name[i] && i < 63) { m->name[i] = name[i]; i++; }
        m->name[i] = '\0';
    }

    int fd = fd_create_custom(pid, FD_KIND_MEMFD, m, FD_OPEN_READ | FD_OPEN_WRITE);
    if (fd < 0) {
        m->in_use = 0;
        m->refcount = 0;
        return -1;
    }
    return fd;
}

int unix_memfd_ftruncate(int pid, int fd, uint64_t length) {
    int kind = -1;
    memfd_object_t *m = (memfd_object_t *)fd_get_custom(pid, fd, &kind);
    if (!m || kind != FD_KIND_MEMFD) return -1;

    uint64_t needed_pages = (length + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    if (needed_pages > MEMFD_MAX_PAGES) return -1;

    /* Allocate new pages if growing */
    while (m->page_count < needed_pages) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return -1;
        uint8_t *va = (uint8_t *)(phys + g_hhdm_offset);
        for (size_t b = 0; b < PAGE_SIZE; b++) va[b] = 0;
        m->phys_pages[m->page_count++] = phys;
    }

    /* Free excess pages if shrinking */
    while (m->page_count > needed_pages) {
        m->page_count--;
        if (m->phys_pages[m->page_count]) {
            pmm_free_page(m->phys_pages[m->page_count]);
            m->phys_pages[m->page_count] = 0;
        }
    }

    m->size = length;
    return 0;
}

uint64_t unix_memfd_mmap(int pid, int fd, uint64_t length, uint32_t prot, uint64_t offset) {
    int kind = -1;
    memfd_object_t *m = (memfd_object_t *)fd_get_custom(pid, fd, &kind);
    if (!m || kind != FD_KIND_MEMFD) return 0;

    uint64_t start_page = offset / PAGE_SIZE;
    uint64_t needed_pages = (length + PAGE_SIZE - 1ULL) / PAGE_SIZE;

    /* Auto-expand pages if length exceeds current allocated pages */
    if (start_page + needed_pages > m->page_count) {
        if (unix_memfd_ftruncate(pid, fd, (start_page + needed_pages) * PAGE_SIZE) != 0) {
            return 0;
        }
    }

    uint64_t base = process_vm_map_phys(&m->phys_pages[start_page], needed_pages, prot);
    if (!base) return 0;
    unix_memfd_retain(m);
    if (process_vm_attach_backing(base, FD_KIND_MEMFD, m) != 0) {
        unix_memfd_release(m);
        process_vm_unmap(base, needed_pages * PAGE_SIZE);
        return 0;
    }
    return base;
}

int unix_memfd_read(void *custom_ptr, uint64_t *offset, void *dst, uint32_t count) {
    memfd_object_t *m = (memfd_object_t *)custom_ptr;
    if (!m || !offset || !dst || !count) return -1;
    if (*offset >= m->size) return 0;

    uint64_t avail = m->size - *offset;
    uint32_t to_read = count < avail ? count : (uint32_t)avail;
    uint32_t read_bytes = 0;

    while (read_bytes < to_read) {
        uint64_t cur = *offset + read_bytes;
        uint64_t p_idx = cur / PAGE_SIZE;
        uint64_t p_off = cur % PAGE_SIZE;
        uint32_t chunk = PAGE_SIZE - p_off;
        if (chunk > to_read - read_bytes) chunk = to_read - read_bytes;

        uint64_t phys = (p_idx < m->page_count) ? m->phys_pages[p_idx] : 0;
        if (phys) {
            const uint8_t *src = (const uint8_t *)(phys + g_hhdm_offset) + p_off;
            uint8_t *d = (uint8_t *)dst + read_bytes;
            for (uint32_t b = 0; b < chunk; b++) d[b] = src[b];
        } else {
            uint8_t *d = (uint8_t *)dst + read_bytes;
            for (uint32_t b = 0; b < chunk; b++) d[b] = 0;
        }
        read_bytes += chunk;
    }

    *offset += read_bytes;
    return (int)read_bytes;
}

int unix_memfd_write(void *custom_ptr, uint64_t *offset, const void *src, uint32_t count) {
    memfd_object_t *m = (memfd_object_t *)custom_ptr;
    if (!m || !offset || !src || !count) return -1;

    uint64_t end = *offset + count;
    uint64_t needed_pages = (end + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    if (needed_pages > MEMFD_MAX_PAGES) return -1;

    while (m->page_count < needed_pages) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return -1;
        uint8_t *va = (uint8_t *)(phys + g_hhdm_offset);
        for (size_t b = 0; b < PAGE_SIZE; b++) va[b] = 0;
        m->phys_pages[m->page_count++] = phys;
    }
    if (end > m->size) m->size = end;

    uint32_t written = 0;
    while (written < count) {
        uint64_t cur = *offset + written;
        uint64_t p_idx = cur / PAGE_SIZE;
        uint64_t p_off = cur % PAGE_SIZE;
        uint32_t chunk = PAGE_SIZE - p_off;
        if (chunk > count - written) chunk = count - written;

        uint64_t phys = m->phys_pages[p_idx];
        uint8_t *dst = (uint8_t *)(phys + g_hhdm_offset) + p_off;
        const uint8_t *s = (const uint8_t *)src + written;
        for (uint32_t b = 0; b < chunk; b++) dst[b] = s[b];
        written += chunk;
    }

    *offset += written;
    return (int)written;
}

void unix_memfd_release(void *custom_ptr) {
    memfd_object_t *m = (memfd_object_t *)custom_ptr;
    if (!m) return;
    if (m->refcount > 0) m->refcount--;
    if (m->refcount == 0) {
        for (uint64_t i = 0; i < m->page_count; i++) {
            if (m->phys_pages[i]) {
                pmm_free_page(m->phys_pages[i]);
                m->phys_pages[i] = 0;
            }
        }
        m->in_use = 0;
        m->page_count = 0;
        m->size = 0;
    }
}

void unix_memfd_retain(void *custom_ptr) {
    memfd_object_t *m = (memfd_object_t *)custom_ptr;
    if (m && m->in_use) m->refcount++;
}

uint64_t unix_memfd_get_size(void *custom_ptr) {
    if (!custom_ptr) return 0;
    return ((memfd_object_t *)custom_ptr)->size;
}

/* ========================================================================= */
/* 2. UNIX DOMAIN SOCKETS (STREAM & SCM_RIGHTS FD PASSING)                   */
/* ========================================================================= */

#define MAX_UNIX_SOCKETS 32
#define UNIX_RING_SIZE (64 * 1024)
#define MAX_QUEUED_FDS 16

typedef enum {
    UNIX_STATE_CLOSED = 0,
    UNIX_STATE_OPEN = 1,
    UNIX_STATE_LISTENING = 2,
    UNIX_STATE_CONNECTED = 3
} unix_state_t;

typedef struct unix_socket unix_socket_t;

struct unix_socket {
    uint32_t refcount;
    int in_use;
    int id;
    int owner_pid;
    unix_state_t state;
    char path[108];

    unix_socket_t *peer;

    /* Inbound data ring buffer */
    uint8_t ring[UNIX_RING_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;

    /* Queued SCM_RIGHTS file_ids */
    uint16_t queued_file_ids[MAX_QUEUED_FDS];
    int qfd_head;
    int qfd_tail;
    int qfd_count;

    /* Listen backlog of pending connected peers */
    unix_socket_t *backlog[16];
    int backlog_head;
    int backlog_tail;
    int backlog_count;
};

static unix_socket_t g_unix_sockets[MAX_UNIX_SOCKETS];

static unix_socket_t *alloc_unix_socket(int pid) {
    for (int i = 0; i < MAX_UNIX_SOCKETS; i++) {
        if (!g_unix_sockets[i].in_use && g_unix_sockets[i].refcount == 0) {
            unix_socket_t *s = &g_unix_sockets[i];
            s->in_use = 1;
            s->refcount = 1;
            s->id = i;
            s->owner_pid = pid;
            s->state = UNIX_STATE_OPEN;
            s->path[0] = '\0';
            s->peer = NULL;
            s->head = 0;
            s->tail = 0;
            s->count = 0;
            s->qfd_head = 0;
            s->qfd_tail = 0;
            s->qfd_count = 0;
            s->backlog_head = 0;
            s->backlog_tail = 0;
            s->backlog_count = 0;
            return s;
        }
    }
    return NULL;
}

int unix_socket_create(int pid, int type, int protocol) {
    (void)protocol;
    int clean_type = type & 0xFF;
    if (clean_type != SOCK_STREAM && clean_type != SOCK_DGRAM) return -1;

    unix_socket_t *s = alloc_unix_socket(pid);
    if (!s) return -1;

    int fd = fd_create_custom(pid, FD_KIND_UNIX_SOCKET, s, FD_OPEN_READ | FD_OPEN_WRITE);
    if (fd < 0) {
        s->in_use = 0;
        s->refcount = 0;
        return -1;
    }
    return fd;
}

int unix_socket_bind(int pid, int fd, const char *path) {
    int kind = -1;
    unix_socket_t *s = (unix_socket_t *)fd_get_custom(pid, fd, &kind);
    if (!s || kind != FD_KIND_UNIX_SOCKET || !path || !path[0]) return -1;

    int i = 0;
    while (path[i] && i < 107) { s->path[i] = path[i]; i++; }
    s->path[i] = '\0';
    return 0;
}

int unix_socket_listen(int pid, int fd, int backlog) {
    (void)backlog;
    int kind = -1;
    unix_socket_t *s = (unix_socket_t *)fd_get_custom(pid, fd, &kind);
    if (!s || kind != FD_KIND_UNIX_SOCKET) return -1;

    s->state = UNIX_STATE_LISTENING;
    return 0;
}

static int str_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

int unix_socket_connect(int pid, int fd, const char *path) {
    int kind = -1;
    unix_socket_t *client = (unix_socket_t *)fd_get_custom(pid, fd, &kind);
    if (!client || kind != FD_KIND_UNIX_SOCKET || !path) return -1;

    /* Find listening socket matching path */
    unix_socket_t *listener = NULL;
    for (int i = 0; i < MAX_UNIX_SOCKETS; i++) {
        if (g_unix_sockets[i].in_use && g_unix_sockets[i].state == UNIX_STATE_LISTENING &&
            str_equal(g_unix_sockets[i].path, path)) {
            listener = &g_unix_sockets[i];
            break;
        }
    }
    if (!listener) return -1;

    if (listener->backlog_count >= 16) return -1;

    /* Create peer socket on server side */
    unix_socket_t *server_peer = alloc_unix_socket(listener->owner_pid);
    if (!server_peer) return -1;

    client->peer = server_peer;
    server_peer->peer = client;
    client->state = UNIX_STATE_CONNECTED;
    server_peer->state = UNIX_STATE_CONNECTED;

    /* Enqueue to listener's backlog */
    listener->backlog[listener->backlog_head] = server_peer;
    listener->backlog_head = (listener->backlog_head + 1) % 16;
    listener->backlog_count++;

    return 0;
}

int unix_socket_accept(int pid, int fd, char *addr_out, int nonblock) {
    (void)addr_out;
    int kind = -1;
    unix_socket_t *listener = (unix_socket_t *)fd_get_custom(pid, fd, &kind);
    if (!listener || kind != FD_KIND_UNIX_SOCKET || listener->state != UNIX_STATE_LISTENING) return -1;

    if (listener->backlog_count == 0) {
        if (nonblock) return -1;
        extern uint8_t g_syscall_should_yield;
        g_syscall_should_yield = 1;
        return -1;
    }

    unix_socket_t *server_peer = listener->backlog[listener->backlog_tail];
    listener->backlog_tail = (listener->backlog_tail + 1) % 16;
    listener->backlog_count--;

    int new_fd = fd_create_custom(pid, FD_KIND_UNIX_SOCKET, server_peer, FD_OPEN_READ | FD_OPEN_WRITE);
    return new_fd;
}

int unix_socket_pair(int pid, int sv[2]) {
    unix_socket_t *s0 = alloc_unix_socket(pid);
    unix_socket_t *s1 = alloc_unix_socket(pid);
    if (!s0 || !s1) {
        if (s0) unix_socket_release(s0);
        if (s1) unix_socket_release(s1);
        return -1;
    }

    s0->peer = s1;
    s1->peer = s0;
    s0->state = UNIX_STATE_CONNECTED;
    s1->state = UNIX_STATE_CONNECTED;

    sv[0] = fd_create_custom(pid, FD_KIND_UNIX_SOCKET, s0, FD_OPEN_READ | FD_OPEN_WRITE);
    sv[1] = fd_create_custom(pid, FD_KIND_UNIX_SOCKET, s1, FD_OPEN_READ | FD_OPEN_WRITE);
    if (sv[0] < 0 || sv[1] < 0) {
        unix_socket_release(s0);
        unix_socket_release(s1);
        return -1;
    }
    return 0;
}

int unix_socket_get_peercred(int pid, int fd, int *peer_pid, uint32_t *uid, uint32_t *gid) {
    int kind = -1;
    unix_socket_t *socket = (unix_socket_t *)fd_get_custom(pid, fd, &kind);
    if (!socket || kind != FD_KIND_UNIX_SOCKET || socket->state != UNIX_STATE_CONNECTED ||
        !socket->peer || !socket->peer->in_use) return -1;

    credentials_t credentials;
    if (process_get_credentials(socket->peer->owner_pid, &credentials) != 0) return -1;
    if (peer_pid) *peer_pid = socket->peer->owner_pid;
    if (uid) *uid = credentials.uid;
    if (gid) *gid = credentials.gid;
    return 0;
}

int unix_socket_send_stream(void *custom_ptr, const void *src, uint32_t count) {
    unix_socket_t *s = (unix_socket_t *)custom_ptr;
    if (!s || s->state != UNIX_STATE_CONNECTED || !s->peer || !src || !count) return -1;
    unix_socket_t *peer = s->peer;

    uint32_t sent = 0;
    const uint8_t *b = (const uint8_t *)src;
    while (sent < count) {
        uint32_t space = UNIX_RING_SIZE - peer->count;
        if (space == 0) break;
        uint32_t chunk = count - sent;
        if (chunk > space) chunk = space;

        for (uint32_t i = 0; i < chunk; i++) {
            peer->ring[peer->head] = b[sent++];
            peer->head = (peer->head + 1) % UNIX_RING_SIZE;
            peer->count++;
        }
    }
    return sent > 0 ? (int)sent : -1;
}

int unix_socket_recv_stream(void *custom_ptr, void *dst, uint32_t count) {
    unix_socket_t *s = (unix_socket_t *)custom_ptr;
    if (!s || !dst || !count) return -1;
    if (s->count == 0) {
        if (s->state != UNIX_STATE_CONNECTED || !s->peer) return 0; /* EOF */
        return -11; /* EAGAIN */
    }

    uint32_t read_bytes = 0;
    uint8_t *b = (uint8_t *)dst;
    while (read_bytes < count && s->count > 0) {
        b[read_bytes++] = s->ring[s->tail];
        s->tail = (s->tail + 1) % UNIX_RING_SIZE;
        s->count--;
    }
    return (int)read_bytes;
}

int unix_socket_poll_events(void *custom_ptr, uint32_t events) {
    unix_socket_t *s = (unix_socket_t *)custom_ptr;
    if (!s) return -1;
    uint32_t ready = 0;

    if (s->state == UNIX_STATE_LISTENING) {
        if (s->backlog_count > 0) ready |= FD_POLL_READ;
        return (int)(ready & events);
    }

    if (s->state == UNIX_STATE_CONNECTED) {
        if (s->count > 0) ready |= FD_POLL_READ;
        if (s->peer && (UNIX_RING_SIZE - s->peer->count) > 0) ready |= FD_POLL_WRITE;
        if (!s->peer || s->peer->state == UNIX_STATE_CLOSED) ready |= FD_POLL_HANGUP;
    } else {
        ready |= FD_POLL_HANGUP;
    }
    return (int)(ready & events);
}

/* SCM_RIGHTS structure matching Linux msghdr and cmsghdr */
struct u_iovec {
    uint64_t iov_base;
    uint64_t iov_len;
};

struct u_msghdr {
    uint64_t msg_name;
    uint32_t msg_namelen;
    uint32_t __pad1;
    uint64_t msg_iov;
    uint64_t msg_iovlen;
    uint64_t msg_control;
    uint64_t msg_controllen;
    int32_t  msg_flags;
    int32_t  __pad2;
};

struct u_cmsghdr {
    uint64_t cmsg_len;
    int32_t  cmsg_level;
    int32_t  cmsg_type;
};

int64_t unix_socket_sendmsg(int pid, int fd, uint64_t u_msghdr_ptr, int flags) {
    (void)flags;
    int kind = -1;
    unix_socket_t *s = (unix_socket_t *)fd_get_custom(pid, fd, &kind);
    if (!s || kind != FD_KIND_UNIX_SOCKET || !u_msghdr_ptr) return -1;
    if (!s->peer || s->state != UNIX_STATE_CONNECTED) return -1;

    struct u_msghdr msg;
    if (copy_from_user(&msg, (const void *)u_msghdr_ptr, sizeof(msg)) != 0) return -14;

    /* Process ancillary data (SCM_RIGHTS file descriptors) */
    if (msg.msg_control && msg.msg_controllen >= sizeof(struct u_cmsghdr)) {
        struct u_cmsghdr cmsg;
        if (copy_from_user(&cmsg, (const void *)msg.msg_control, sizeof(cmsg)) == 0) {
            if (cmsg.cmsg_level == SOL_SOCKET && cmsg.cmsg_type == SCM_RIGHTS) {
                uint64_t data_len = cmsg.cmsg_len > sizeof(cmsg) ? cmsg.cmsg_len - sizeof(cmsg) : 0;
                int num_fds = (int)(data_len / sizeof(int));
                if (num_fds > 0 && num_fds <= 8) {
                    int passed_fds[8];
                    if (copy_from_user(passed_fds, (const void *)(msg.msg_control + sizeof(cmsg)), (size_t)(num_fds * sizeof(int))) == 0) {
                        for (int f = 0; f < num_fds; f++) {
                            uint16_t file_id = fd_get_file_id(pid, passed_fds[f]);
                            if (file_id != 0xFFFFU && s->peer->qfd_count < MAX_QUEUED_FDS) {
                                s->peer->queued_file_ids[s->peer->qfd_head] = file_id;
                                s->peer->qfd_head = (s->peer->qfd_head + 1) % MAX_QUEUED_FDS;
                                s->peer->qfd_count++;
                            }
                        }
                    }
                }
            }
        }
    }

    /* Send payload iovecs */
    int64_t total_sent = 0;
    for (uint64_t i = 0; i < msg.msg_iovlen && i < 16; i++) {
        struct u_iovec iov;
        if (copy_from_user(&iov, (const void *)(msg.msg_iov + i * sizeof(iov)), sizeof(iov)) != 0) break;
        if (!iov.iov_base || !iov.iov_len) continue;

        char kbuf[512];
        size_t left = (size_t)iov.iov_len;
        size_t offset = 0;
        while (left > 0) {
            size_t chunk = left > sizeof(kbuf) ? sizeof(kbuf) : left;
            if (copy_from_user(kbuf, (const void *)(iov.iov_base + offset), chunk) != 0) return total_sent > 0 ? total_sent : -14;
            int sent = unix_socket_send_stream(s, kbuf, (uint32_t)chunk);
            if (sent <= 0) return total_sent > 0 ? total_sent : sent;
            total_sent += sent;
            offset += sent;
            left -= sent;
        }
    }
    return total_sent;
}

int64_t unix_socket_recvmsg(int pid, int fd, uint64_t u_msghdr_ptr, int flags) {
    (void)flags;
    int kind = -1;
    unix_socket_t *s = (unix_socket_t *)fd_get_custom(pid, fd, &kind);
    if (!s || kind != FD_KIND_UNIX_SOCKET || !u_msghdr_ptr) return -1;

    struct u_msghdr msg;
    if (copy_from_user(&msg, (const void *)u_msghdr_ptr, sizeof(msg)) != 0) return -14;

    /* Deliver queued SCM_RIGHTS fds if requested */
    int delivered_count = 0;
    if (msg.msg_control && msg.msg_controllen >= sizeof(struct u_cmsghdr) + sizeof(int) && s->qfd_count > 0) {
        int delivered_fds[8];
        int num_deliv = 0;
        while (s->qfd_count > 0 && num_deliv < 8) {
            uint16_t fid = s->queued_file_ids[s->qfd_tail];
            s->qfd_tail = (s->qfd_tail + 1) % MAX_QUEUED_FDS;
            s->qfd_count--;

            int new_fd = fd_bind_file_id(pid, fid);
            if (new_fd >= 0) delivered_fds[num_deliv++] = new_fd;
        }

        if (num_deliv > 0) {
            struct u_cmsghdr cmsg;
            cmsg.cmsg_level = SOL_SOCKET;
            cmsg.cmsg_type = SCM_RIGHTS;
            cmsg.cmsg_len = sizeof(cmsg) + (uint64_t)(num_deliv * sizeof(int));
            copy_to_user((void *)msg.msg_control, &cmsg, sizeof(cmsg));
            copy_to_user((void *)(msg.msg_control + sizeof(cmsg)), delivered_fds, (size_t)(num_deliv * sizeof(int)));
            msg.msg_controllen = cmsg.cmsg_len;
            delivered_count = num_deliv;
        }
    }
    if (msg.msg_control) {
        if (delivered_count == 0) msg.msg_controllen = 0;
        copy_to_user((void *)u_msghdr_ptr, &msg, sizeof(msg));
    }

    /* Receive payload into user iovecs */
    int64_t total_read = 0;
    for (uint64_t i = 0; i < msg.msg_iovlen && i < 16; i++) {
        struct u_iovec iov;
        if (copy_from_user(&iov, (const void *)(msg.msg_iov + i * sizeof(iov)), sizeof(iov)) != 0) break;
        if (!iov.iov_base || !iov.iov_len) continue;

        char kbuf[512];
        size_t left = (size_t)iov.iov_len;
        size_t offset = 0;
        while (left > 0) {
            size_t chunk = left > sizeof(kbuf) ? sizeof(kbuf) : left;
            int r = unix_socket_recv_stream(s, kbuf, (uint32_t)chunk);
            if (r <= 0) return total_read > 0 ? total_read : r;
            if (copy_to_user((void *)(iov.iov_base + offset), kbuf, (size_t)r) != 0) return total_read > 0 ? total_read : -14;
            total_read += r;
            offset += r;
            left -= r;
            if ((uint32_t)r < chunk) break;
        }
    }
    return total_read;
}

void unix_socket_release(void *custom_ptr) {
    unix_socket_t *s = (unix_socket_t *)custom_ptr;
    if (!s) return;
    if (s->refcount > 0) s->refcount--;
    if (s->refcount == 0) {
        s->state = UNIX_STATE_CLOSED;
        if (s->peer) {
            s->peer->peer = NULL;
            s->peer = NULL;
        }
        s->in_use = 0;
    }
}

/* ========================================================================= */
/* 3. EPOLL SUBSYSTEM                                                        */
/* ========================================================================= */

#define MAX_EPOLLS 16
#define MAX_EPOLL_ITEMS 32

typedef struct {
    int fd;
    uint32_t events;
    uint64_t data;
    int active;
} epoll_item_t;

typedef struct {
    uint32_t refcount;
    int in_use;
    int id;
    int owner_pid;
    epoll_item_t items[MAX_EPOLL_ITEMS];
    int count;
} epoll_instance_t;

static epoll_instance_t g_epolls[MAX_EPOLLS];

#define MAX_EVENTFDS 32
#define EFD_SEMAPHORE 1

typedef struct {
    int in_use;
    int semaphore;
    volatile uint8_t lock;
    uint64_t counter;
} eventfd_object_t;

static eventfd_object_t g_eventfds[MAX_EVENTFDS];

int unix_eventfd_create(int pid, uint32_t initial_value, int flags) {
    if (flags & ~(1 | 00004000 | 02000000)) return -1;
    for (int i = 0; i < MAX_EVENTFDS; ++i) {
        if (g_eventfds[i].in_use) continue;
        eventfd_object_t *event = &g_eventfds[i];
        event->in_use = 1;
        event->semaphore = (flags & EFD_SEMAPHORE) != 0;
        event->lock = 0;
        event->counter = initial_value;
        int fd = fd_create_custom(pid, FD_KIND_EVENTFD, event, FD_OPEN_READ | FD_OPEN_WRITE);
        if (fd >= 0) return fd;
        event->in_use = 0;
        return -1;
    }
    return -1;
}

int unix_eventfd_read(void *custom_ptr, void *dst, uint32_t count) {
    eventfd_object_t *event = (eventfd_object_t *)custom_ptr;
    if (!event || !event->in_use || !dst || count < sizeof(uint64_t)) return -1;
    while (__atomic_test_and_set(&event->lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    if (event->counter == 0) {
        __atomic_clear(&event->lock, __ATOMIC_RELEASE);
        return 0;
    }
    uint64_t value = event->semaphore ? 1 : event->counter;
    event->counter -= value;
    *(uint64_t *)dst = value;
    __atomic_clear(&event->lock, __ATOMIC_RELEASE);
    return sizeof(uint64_t);
}

int unix_eventfd_write(void *custom_ptr, const void *src, uint32_t count) {
    eventfd_object_t *event = (eventfd_object_t *)custom_ptr;
    if (!event || !event->in_use || !src || count < sizeof(uint64_t)) return -1;
    uint64_t value = *(const uint64_t *)src;
    if (value == ~0ULL) return -1;
    while (__atomic_test_and_set(&event->lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    if (value > (~0ULL - 1ULL) - event->counter) {
        __atomic_clear(&event->lock, __ATOMIC_RELEASE);
        return -1;
    }
    event->counter += value;
    __atomic_clear(&event->lock, __ATOMIC_RELEASE);
    return sizeof(uint64_t);
}

int unix_eventfd_poll(void *custom_ptr, uint32_t events) {
    eventfd_object_t *event = (eventfd_object_t *)custom_ptr;
    if (!event || !event->in_use) return -1;
    uint32_t ready = FD_POLL_WRITE;
    if (event->counter) ready |= FD_POLL_READ;
    return (int)(ready & events);
}

void unix_eventfd_release(void *custom_ptr) {
    eventfd_object_t *event = (eventfd_object_t *)custom_ptr;
    if (event) event->in_use = 0;
}

int unix_epoll_create(int pid, int flags) {
    (void)flags;
    for (int i = 0; i < MAX_EPOLLS; i++) {
        if (!g_epolls[i].in_use && g_epolls[i].refcount == 0) {
            epoll_instance_t *ep = &g_epolls[i];
            ep->in_use = 1;
            ep->refcount = 1;
            ep->id = i;
            ep->owner_pid = pid;
            ep->count = 0;
            for (int k = 0; k < MAX_EPOLL_ITEMS; k++) ep->items[k].active = 0;
            return fd_create_custom(pid, FD_KIND_EPOLL, ep, 0);
        }
    }
    return -1;
}

int unix_epoll_ctl(int pid, int epfd, int op, int target_fd, uint64_t u_event_ptr) {
    int kind = -1;
    epoll_instance_t *ep = (epoll_instance_t *)fd_get_custom(pid, epfd, &kind);
    if (!ep || kind != FD_KIND_EPOLL) return -1;

    struct linux_epoll_event ev = {0, {0}};
    if (op != EPOLL_CTL_DEL) {
        if (!u_event_ptr || copy_from_user(&ev, (const void *)u_event_ptr, sizeof(ev)) != 0) return -1;
    }

    if (op == EPOLL_CTL_ADD) {
        for (int i = 0; i < MAX_EPOLL_ITEMS; i++) {
            if (ep->items[i].active && ep->items[i].fd == target_fd) return -1; /* EEXIST */
        }
        for (int i = 0; i < MAX_EPOLL_ITEMS; i++) {
            if (!ep->items[i].active) {
                ep->items[i].active = 1;
                ep->items[i].fd = target_fd;
                ep->items[i].events = ev.events;
                ep->items[i].data = ev.data.u64;
                ep->count++;
                return 0;
            }
        }
        return -1; /* ENOMEM */
    } else if (op == EPOLL_CTL_MOD) {
        for (int i = 0; i < MAX_EPOLL_ITEMS; i++) {
            if (ep->items[i].active && ep->items[i].fd == target_fd) {
                ep->items[i].events = ev.events;
                ep->items[i].data = ev.data.u64;
                return 0;
            }
        }
        return -1; /* ENOENT */
    } else if (op == EPOLL_CTL_DEL) {
        for (int i = 0; i < MAX_EPOLL_ITEMS; i++) {
            if (ep->items[i].active && ep->items[i].fd == target_fd) {
                ep->items[i].active = 0;
                ep->count--;
                return 0;
            }
        }
        return -1; /* ENOENT */
    }
    return -1;
}

int unix_epoll_wait(int pid, int epfd, uint64_t u_events_ptr, int maxevents, int timeout_ms) {
    int kind = -1;
    epoll_instance_t *ep = (epoll_instance_t *)fd_get_custom(pid, epfd, &kind);
    if (!ep || kind != FD_KIND_EPOLL || !u_events_ptr || maxevents <= 0) return -1;

    int ready_count = 0;
    struct linux_epoll_event ready_events[16];

    for (int i = 0; i < MAX_EPOLL_ITEMS && ready_count < maxevents && ready_count < 16; i++) {
        if (!ep->items[i].active) continue;
        uint32_t requested = 0;
        if (ep->items[i].events & (EPOLLIN | EPOLLPRI)) requested |= FD_POLL_READ;
        if (ep->items[i].events & EPOLLOUT) requested |= FD_POLL_WRITE;
        requested |= FD_POLL_HANGUP;
        int rev = fd_poll(pid, ep->items[i].fd, requested);
        if (rev > 0) {
            uint32_t returned = 0;
            if (rev & FD_POLL_READ) returned |= EPOLLIN;
            if (rev & FD_POLL_WRITE) returned |= EPOLLOUT;
            if (rev & FD_POLL_HANGUP) returned |= EPOLLHUP;
            ready_events[ready_count].events = returned;
            ready_events[ready_count].data.u64 = ep->items[i].data;
            ready_count++;
        }
    }

    if (ready_count > 0) {
        size_t bytes = (size_t)(ready_count * sizeof(struct linux_epoll_event));
        if (copy_to_user((void *)u_events_ptr, ready_events, bytes) != 0) return -14;
        return ready_count;
    }

    if (timeout_ms == 0) return 0;

    extern uint8_t g_syscall_should_yield;
    g_syscall_should_yield = 1;
    return -4; /* -EINTR */
}

void unix_epoll_release(void *custom_ptr) {
    epoll_instance_t *ep = (epoll_instance_t *)custom_ptr;
    if (!ep) return;
    if (ep->refcount > 0) ep->refcount--;
    if (ep->refcount == 0) ep->in_use = 0;
}

void unix_ipc_init(void) {
    for (int i = 0; i < MAX_MEMFDS; i++) {
        g_memfds[i].in_use = 0;
        g_memfds[i].refcount = 0;
    }
    for (int i = 0; i < MAX_UNIX_SOCKETS; i++) {
        g_unix_sockets[i].in_use = 0;
        g_unix_sockets[i].refcount = 0;
    }
    for (int i = 0; i < MAX_EPOLLS; i++) {
        g_epolls[i].in_use = 0;
        g_epolls[i].refcount = 0;
    }
    for (int i = 0; i < MAX_EVENTFDS; ++i) g_eventfds[i].in_use = 0;
    serial_print("[UNIX_IPC] Wayland IPC initialized: memfd_create, AF_UNIX, SCM_RIGHTS, epoll.\n");
}
