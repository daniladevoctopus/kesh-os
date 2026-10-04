#ifndef KESHOS_FD_H
#define KESHOS_FD_H

#include <stdint.h>

#define FD_MAX_PER_PROCESS 64
#define FD_PATH_MAX 256
#define FD_OPEN_READ  0x01U
#define FD_OPEN_WRITE 0x02U
#define FD_OPEN_CREATE 0x04U
#define FD_OPEN_TRUNC  0x08U
#define FD_OPEN_APPEND 0x10U
#define FD_OPEN_DIRECTORY 0x20U
#define FD_POLL_READ   0x01U
#define FD_POLL_WRITE  0x02U
#define FD_POLL_HANGUP 0x04U

typedef enum {
    FD_KIND_FILE = 0,
    FD_KIND_DIRECTORY = 1,
    FD_KIND_PIPE_READ = 2,
    FD_KIND_PIPE_WRITE = 3,
    FD_KIND_PTY = 4,
    FD_KIND_UNIX_SOCKET = 5,
    FD_KIND_MEMFD = 6,
    FD_KIND_EPOLL = 7,
    FD_KIND_EVDEV = 8,
    FD_KIND_DRM_FB = 9,
    FD_KIND_EVENTFD = 10
} fd_kind_t;

typedef struct {
    char name[64];
    uint32_t size;
    uint8_t is_dir;
} fd_dirent_t;

void fd_init(void);
int fd_open(int pid, const char *path, uint32_t flags);
int fd_dup(int pid, int fd);
int fd_dup_min(int pid, int fd, int min_fd);
int fd_dup2(int pid, int source_fd, int target_fd);
int fd_pipe_create(int pid, int *read_fd, int *write_fd);
int fd_bind_pty(int pid, int pty_handle, uint32_t flags);
int fd_bind_pty_stdio(int pid, int pty_handle);
int fd_close(int pid, int fd);
int fd_read(int pid, int fd, void *out, uint32_t max_bytes);
int fd_read_at(int pid, int fd, uint64_t offset, void *out, uint32_t max_bytes);
int fd_write(int pid, int fd, const void *data, uint32_t bytes);
int64_t fd_seek(int pid, int fd, int64_t offset, int whence);
int fd_poll(int pid, int fd, uint32_t events);
int fd_readdir(int pid, int fd, fd_dirent_t *out);
int fd_stat(int pid, int fd, uint64_t *size, uint8_t *is_dir);
int fd_inherit(int parent_pid, int child_pid);
void fd_close_owner(int pid);

/* Custom FD Extensions (UNIX Sockets, MemFD, Epoll) */
int fd_create_custom(int pid, int kind, void *custom_ptr, uint32_t flags);
void *fd_get_custom(int pid, int fd, int *out_kind);
int fd_get_kind(int pid, int fd);
uint16_t fd_get_file_id(int pid, int fd);
int fd_bind_file_id(int pid, uint16_t file_id);

#endif
