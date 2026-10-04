#include "fd.h"
#include "unix_ipc.h"
#include "vfs.h"
#include "memory.h"
#include "tty.h"

#define FD_MAX_PROCESSES 16
#define FD_IO_MAX (8U * 1024U * 1024U)
#define FD_PIPE_MAX 16
#define FD_PIPE_BUFFER 4096

typedef struct {
    uint8_t allocated, readers, writers;
    uint16_t head, tail, used;
    volatile uint8_t lock;
    uint8_t data[FD_PIPE_BUFFER];
} fd_pipe_t;

typedef struct {
    uint32_t references;
    uint32_t flags;
    uint64_t offset;
    fd_kind_t kind;
    uint16_t pipe_id;
    void *custom_ptr;
    char path[FD_PATH_MAX];
} fd_file_t;

typedef struct {
    uint8_t open;
    uint16_t file_id;
} fd_entry_t;

#define FD_FILE_MAX (FD_MAX_PROCESSES * FD_MAX_PER_PROCESS)
static fd_entry_t g_fds[FD_MAX_PROCESSES][FD_MAX_PER_PROCESS];
static fd_file_t g_files[FD_FILE_MAX];
static fd_pipe_t g_pipes[FD_PIPE_MAX];

static int valid_pid(int pid) { return pid >= 0 && pid < FD_MAX_PROCESSES; }
static fd_entry_t *entry_for(int pid, int fd) {
    return valid_pid(pid) && fd >= 0 && fd < FD_MAX_PER_PROCESS && g_fds[pid][fd].open ? &g_fds[pid][fd] : 0;
}
static fd_file_t *file_for(fd_entry_t *entry) {
    return entry && entry->file_id < FD_FILE_MAX && g_files[entry->file_id].references ? &g_files[entry->file_id] : 0;
}
static int copy_path(char *out, const char *path) {
    int i = 0;
    while (path[i] && i + 1 < FD_PATH_MAX) { out[i] = path[i]; i++; }
    out[i] = '\0';
    return path[i] == '\0' ? 0 : -1;
}
static uint64_t add_clamped(uint64_t a, uint64_t b) { return b > ~0ULL - a ? ~0ULL : a + b; }
static void *alloc_buffer(uint64_t bytes, size_t *pages) {
    if (!bytes || bytes > FD_IO_MAX || !pages) return 0;
    *pages = (size_t)((bytes + PAGE_SIZE - 1ULL) / PAGE_SIZE);
    uint64_t phys = pmm_alloc_pages(*pages);
    return phys ? (void *)(phys + g_hhdm_offset) : 0;
}
static void free_buffer(void *buffer, size_t pages) {
    if (buffer && pages) pmm_free_pages((uint64_t)buffer - g_hhdm_offset, pages);
}
static void pipe_lock(fd_pipe_t *pipe) { while (__atomic_test_and_set(&pipe->lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause"); }
static void pipe_unlock(fd_pipe_t *pipe) { __atomic_clear(&pipe->lock, __ATOMIC_RELEASE); }
static int free_file_slot(void) { for (int i = 0; i < FD_FILE_MAX; ++i) if (!g_files[i].references) return i; return -1; }
static int free_fd_slot(int pid, int skip) { for (int i = 0; i < FD_MAX_PER_PROCESS; ++i) if (i != skip && !g_fds[pid][i].open) return i; return -1; }

void fd_init(void) {
    for (int pid = 0; pid < FD_MAX_PROCESSES; ++pid)
        for (int fd = 0; fd < FD_MAX_PER_PROCESS; ++fd) g_fds[pid][fd].open = 0;
    for (int file = 0; file < FD_FILE_MAX; ++file) g_files[file].references = 0;
    for (int pipe = 0; pipe < FD_PIPE_MAX; ++pipe) g_pipes[pipe].allocated = 0;
}

int fd_open(int pid, const char *path, uint32_t flags) {
    const uint32_t known_flags = FD_OPEN_READ | FD_OPEN_WRITE | FD_OPEN_CREATE | FD_OPEN_TRUNC | FD_OPEN_APPEND | FD_OPEN_DIRECTORY;
    if (!valid_pid(pid) || !path || !path[0] || (flags & ~known_flags) || !(flags & (FD_OPEN_READ | FD_OPEN_WRITE))) return -1;
    if ((flags & (FD_OPEN_TRUNC | FD_OPEN_APPEND | FD_OPEN_CREATE)) && !(flags & FD_OPEN_WRITE)) return -1;
    if ((flags & FD_OPEN_DIRECTORY) && (flags & (FD_OPEN_WRITE | FD_OPEN_CREATE | FD_OPEN_TRUNC | FD_OPEN_APPEND))) return -1;
    kesh_vfs_stat_t stat;
    int exists = vfs_stat(path, &stat) == 0;
    if (!exists) {
        uint8_t zero = 0;
        if (!(flags & FD_OPEN_CREATE) || vfs_write(path, &zero, 0) < 0 || vfs_stat(path, &stat) != 0) return -1;
    }
    if (stat.is_dir != ((flags & FD_OPEN_DIRECTORY) != 0)) return -1;
    if ((flags & FD_OPEN_TRUNC) && vfs_write(path, &(uint8_t){0}, 0) < 0) return -1;
    if ((flags & FD_OPEN_TRUNC) && vfs_stat(path, &stat) != 0) return -1;
    int file_id = -1;
    for (int file = 0; file < FD_FILE_MAX; ++file) if (!g_files[file].references) { file_id = file; break; }
    if (file_id < 0) return -1;
    for (int fd = 0; fd < FD_MAX_PER_PROCESS; ++fd) if (!g_fds[pid][fd].open) {
        fd_file_t *file = &g_files[file_id];
        if (copy_path(file->path, path) != 0) return -1;
        file->references = 1;
        file->flags = flags;
        file->offset = (flags & FD_OPEN_APPEND) ? stat.size : 0;
        file->kind = stat.is_dir ? FD_KIND_DIRECTORY : FD_KIND_FILE;
        g_fds[pid][fd].file_id = (uint16_t)file_id;
        g_fds[pid][fd].open = 1;
        return fd;
    }
    return -1;
}

int fd_pipe_create(int pid, int *read_fd, int *write_fd) {
    if (!valid_pid(pid) || !read_fd || !write_fd) return -1;
    int read_slot = free_fd_slot(pid, -1), write_slot = free_fd_slot(pid, read_slot);
    int read_file = free_file_slot();
    if (read_slot < 0 || write_slot < 0 || read_file < 0) return -1;
    g_files[read_file].references = 1;
    int write_file = free_file_slot();
    if (write_file < 0) { g_files[read_file].references = 0; return -1; }
    int pipe_id = -1;
    for (int i = 0; i < FD_PIPE_MAX; ++i) if (!g_pipes[i].allocated) { pipe_id = i; break; }
    if (pipe_id < 0) { g_files[read_file].references = 0; return -1; }
    fd_pipe_t *pipe = &g_pipes[pipe_id];
    pipe->allocated = 1; pipe->readers = 1; pipe->writers = 1;
    pipe->head = pipe->tail = pipe->used = 0; pipe->lock = 0;
    g_files[read_file].kind = FD_KIND_PIPE_READ; g_files[read_file].pipe_id = (uint16_t)pipe_id;
    g_files[read_file].flags = FD_OPEN_READ;
    g_files[write_file].references = 1; g_files[write_file].kind = FD_KIND_PIPE_WRITE;
    g_files[write_file].pipe_id = (uint16_t)pipe_id; g_files[write_file].flags = FD_OPEN_WRITE;
    g_fds[pid][read_slot].file_id = (uint16_t)read_file; g_fds[pid][read_slot].open = 1;
    g_fds[pid][write_slot].file_id = (uint16_t)write_file; g_fds[pid][write_slot].open = 1;
    *read_fd = read_slot; *write_fd = write_slot;
    return 0;
}

int fd_bind_pty(int pid, int pty_handle, uint32_t flags) {
    if (!valid_pid(pid) || !(flags & (FD_OPEN_READ | FD_OPEN_WRITE)) || (flags & ~(FD_OPEN_READ | FD_OPEN_WRITE))) return -1;
    int descriptor = free_fd_slot(pid, -1), file_id = free_file_slot();
    if (descriptor < 0 || file_id < 0) return -1;
    fd_file_t *file = &g_files[file_id];
    file->references = 1; file->flags = flags; file->offset = 0;
    file->kind = FD_KIND_PTY; file->pipe_id = (uint16_t)pty_handle;
    g_fds[pid][descriptor].file_id = (uint16_t)file_id;
    g_fds[pid][descriptor].open = 1;
    return descriptor;
}

int fd_bind_pty_stdio(int pid, int pty_handle) {
    if (!valid_pid(pid) || g_fds[pid][0].open || g_fds[pid][1].open || g_fds[pid][2].open) return -1;
    int file_id = free_file_slot();
    if (file_id < 0) return -1;
    fd_file_t *file = &g_files[file_id];
    file->references = 3; file->flags = FD_OPEN_READ | FD_OPEN_WRITE; file->offset = 0;
    file->kind = FD_KIND_PTY; file->pipe_id = (uint16_t)pty_handle;
    for (int descriptor = 0; descriptor < 3; ++descriptor) {
        g_fds[pid][descriptor].file_id = (uint16_t)file_id;
        g_fds[pid][descriptor].open = 1;
    }
    return 0;
}

int fd_dup(int pid, int fd) {
    return fd_dup_min(pid, fd, 0);
}

int fd_dup_min(int pid, int fd, int min_fd) {
    fd_entry_t *source = entry_for(pid, fd);
    fd_file_t *file = file_for(source);
    if (!file || min_fd < 0 || min_fd >= FD_MAX_PER_PROCESS) return -1;
    for (int candidate = min_fd; candidate < FD_MAX_PER_PROCESS; ++candidate) {
        if (!g_fds[pid][candidate].open) {
            g_fds[pid][candidate].file_id = source->file_id;
            g_fds[pid][candidate].open = 1;
            file->references++;
            return candidate;
        }
    }
    return -1;
}

int fd_dup2(int pid, int source_fd, int target_fd) {
    fd_entry_t *source = entry_for(pid, source_fd);
    fd_file_t *file = file_for(source);
    if (!file || target_fd < 0 || target_fd >= FD_MAX_PER_PROCESS) return -1;
    if (source_fd == target_fd) return target_fd;
    if (g_fds[pid][target_fd].open && fd_close(pid, target_fd) != 0) return -1;
    g_fds[pid][target_fd].file_id = source->file_id;
    g_fds[pid][target_fd].open = 1;
    file->references++;
    return target_fd;
}

int fd_inherit(int parent_pid, int child_pid) {
    if (!valid_pid(parent_pid) || !valid_pid(child_pid) || parent_pid == child_pid) return -1;
    for (int fd = 0; fd < FD_MAX_PER_PROCESS; ++fd) if (g_fds[child_pid][fd].open) return -1;
    for (int fd = 0; fd < FD_MAX_PER_PROCESS; ++fd) {
        fd_entry_t *source = entry_for(parent_pid, fd);
        fd_file_t *file = file_for(source);
        if (!file || file->kind == FD_KIND_PTY) continue;
        g_fds[child_pid][fd].file_id = source->file_id;
        g_fds[child_pid][fd].open = 1;
        file->references++;
    }
    return 0;
}

int fd_close(int pid, int fd) {
    fd_entry_t *entry = entry_for(pid, fd);
    fd_file_t *file = file_for(entry);
    if (!file) return -1;
    if (--file->references == 0) {
        if ((file->kind == FD_KIND_PIPE_READ || file->kind == FD_KIND_PIPE_WRITE) && file->pipe_id < FD_PIPE_MAX) {
            fd_pipe_t *pipe = &g_pipes[file->pipe_id];
            pipe_lock(pipe);
            if (file->kind == FD_KIND_PIPE_READ && pipe->readers) pipe->readers--;
            if (file->kind == FD_KIND_PIPE_WRITE && pipe->writers) pipe->writers--;
            int release = !pipe->readers && !pipe->writers;
            pipe_unlock(pipe);
            if (release) pipe->allocated = 0;
        }
        if (file->kind == FD_KIND_PTY) (void)pty_close(pid, file->pipe_id);
        if (file->kind == FD_KIND_UNIX_SOCKET) unix_socket_release(file->custom_ptr);
        if (file->kind == FD_KIND_MEMFD) unix_memfd_release(file->custom_ptr);
        if (file->kind == FD_KIND_EPOLL) unix_epoll_release(file->custom_ptr);
        if (file->kind == FD_KIND_EVENTFD) unix_eventfd_release(file->custom_ptr);
        if (file->kind == FD_KIND_EVDEV) { extern void evdev_release(void *); evdev_release(file->custom_ptr); }
        if (file->kind == FD_KIND_DRM_FB) { extern void drm_fb_release(void *); drm_fb_release(file->custom_ptr); }
        file->custom_ptr = 0;
        file->path[0] = '\0';
    }
    entry->open = 0;
    return 0;
}
void fd_close_owner(int pid) { if (valid_pid(pid)) for (int fd = 0; fd < FD_MAX_PER_PROCESS; ++fd) if (g_fds[pid][fd].open) (void)fd_close(pid, fd); }

int fd_read(int pid, int fd, void *out, uint32_t max_bytes) {
    fd_entry_t *entry = entry_for(pid, fd);
    fd_file_t *file = file_for(entry);
    if (!file || !(file->flags & FD_OPEN_READ) || !out || !max_bytes || max_bytes > FD_IO_MAX) return -1;
    if (file->kind == FD_KIND_UNIX_SOCKET) return unix_socket_recv_stream(file->custom_ptr, out, max_bytes);
    if (file->kind == FD_KIND_MEMFD) return unix_memfd_read(file->custom_ptr, &file->offset, out, max_bytes);
    if (file->kind == FD_KIND_EVDEV) { extern int evdev_read(void *, void *, uint32_t); return evdev_read(file->custom_ptr, out, max_bytes); }
    if (file->kind == FD_KIND_EVENTFD) return unix_eventfd_read(file->custom_ptr, out, max_bytes);
    if (file->kind == FD_KIND_PIPE_READ) {
        fd_pipe_t *pipe = file->pipe_id < FD_PIPE_MAX ? &g_pipes[file->pipe_id] : 0;
        if (!pipe || !pipe->allocated) return -1;
        pipe_lock(pipe);
        uint32_t count = max_bytes < pipe->used ? max_bytes : pipe->used;
        for (uint32_t i = 0; i < count; ++i) { ((uint8_t *)out)[i] = pipe->data[pipe->tail]; pipe->tail = (uint16_t)((pipe->tail + 1U) % FD_PIPE_BUFFER); }
        pipe->used = (uint16_t)(pipe->used - count);
        pipe_unlock(pipe);
        return (int)count;
    }
    if (file->kind == FD_KIND_PTY) return pty_read(pid, file->pipe_id, out, max_bytes);
    if (file->kind != FD_KIND_FILE) return -1;
    int copied = fd_read_at(pid, fd, file->offset, out, max_bytes);
    if (copied > 0) file->offset += (uint64_t)copied;
    return copied;
}

int fd_read_at(int pid, int fd, uint64_t offset, void *out, uint32_t max_bytes) {
    fd_file_t *file = file_for(entry_for(pid, fd));
    if (!file || file->kind != FD_KIND_FILE || !out || !max_bytes || max_bytes > FD_IO_MAX) return -1;
    extern int vfs_read_at(const char *path, uint64_t offset, void *buffer, int max_bytes);
    return vfs_read_at(file->path, offset, out, (int)max_bytes);
}

int fd_write(int pid, int fd, const void *data, uint32_t bytes) {
    fd_entry_t *entry = entry_for(pid, fd);
    fd_file_t *file = file_for(entry);
    if (!file || !(file->flags & FD_OPEN_WRITE) || (!data && bytes) || bytes > FD_IO_MAX) return -1;
    if (file->kind == FD_KIND_UNIX_SOCKET) return unix_socket_send_stream(file->custom_ptr, data, bytes);
    if (file->kind == FD_KIND_MEMFD) return unix_memfd_write(file->custom_ptr, &file->offset, data, bytes);
    if (file->kind == FD_KIND_EVENTFD) return unix_eventfd_write(file->custom_ptr, data, bytes);
    if (file->kind == FD_KIND_PIPE_WRITE) {
        fd_pipe_t *pipe = file->pipe_id < FD_PIPE_MAX ? &g_pipes[file->pipe_id] : 0;
        if (!pipe || !pipe->allocated) return -1;
        pipe_lock(pipe);
        if (!pipe->readers) { pipe_unlock(pipe); return -2; }
        uint32_t available = FD_PIPE_BUFFER - pipe->used;
        uint32_t count = bytes < available ? bytes : available;
        for (uint32_t i = 0; i < count; ++i) { pipe->data[pipe->head] = ((const uint8_t *)data)[i]; pipe->head = (uint16_t)((pipe->head + 1U) % FD_PIPE_BUFFER); }
        pipe->used = (uint16_t)(pipe->used + count);
        pipe_unlock(pipe);
        return (int)count;
    }
    if (file->kind == FD_KIND_PTY) return pty_write(pid, file->pipe_id, data, bytes);
    if (file->kind != FD_KIND_FILE) return -1;
    kesh_vfs_stat_t stat;
    if (vfs_stat(file->path, &stat) != 0 || stat.is_dir) return -1;
    uint64_t end = add_clamped(file->offset, bytes);
    uint64_t size = stat.size > end ? stat.size : end;
    if (size > FD_IO_MAX) return -1;
    size_t pages = 0;
    uint8_t *scratch = alloc_buffer(size ? size : 1, &pages);
    if (!scratch) return -1;
    if (stat.size && vfs_read(file->path, scratch, (int)stat.size) < 0) { free_buffer(scratch, pages); return -1; }
    for (uint32_t i = 0; i < bytes; ++i) scratch[file->offset + i] = ((const uint8_t *)data)[i];
    int result = vfs_write(file->path, scratch, (int)size);
    if (result >= 0) file->offset = end;
    free_buffer(scratch, pages);
    return result < 0 ? result : (int)bytes;
}

int64_t fd_seek(int pid, int fd, int64_t offset, int whence) {
    fd_entry_t *entry = entry_for(pid, fd);
    fd_file_t *file = file_for(entry);
    if (!file || (file->kind != FD_KIND_FILE && file->kind != FD_KIND_DIRECTORY) || (whence != 0 && whence != 1 && whence != 2)) return -1;
    kesh_vfs_stat_t stat;
    if (vfs_stat(file->path, &stat) != 0 || stat.is_dir != (file->kind == FD_KIND_DIRECTORY)) return -1;
    int64_t extent = (int64_t)stat.size;
    if (file->kind == FD_KIND_DIRECTORY) {
        kesh_vfs_entry_t entries[VFS_MAX_CHILDREN];
        extent = vfs_list(file->path, entries, VFS_MAX_CHILDREN);
        if (extent < 0) return -1;
    }
    int64_t base = whence == 0 ? 0 : whence == 1 ? (int64_t)file->offset : extent;
    if ((offset > 0 && base > 0x7FFFFFFFFFFFFFFFLL - offset) || (offset < 0 && base < -offset)) return -1;
    int64_t next = base + offset;
    if (next < 0 || (file->kind == FD_KIND_FILE && (uint64_t)next > FD_IO_MAX) ||
        (file->kind == FD_KIND_DIRECTORY && next > extent)) return -1;
    file->offset = (uint64_t)next;
    return next;
}

int fd_poll(int pid, int fd, uint32_t events) {
    if (events & ~(FD_POLL_READ | FD_POLL_WRITE | FD_POLL_HANGUP)) return -1;
    fd_file_t *file = file_for(entry_for(pid, fd));
    if (!file) return -1;
    if (file->kind == FD_KIND_UNIX_SOCKET) return unix_socket_poll_events(file->custom_ptr, events);
    if (file->kind == FD_KIND_MEMFD) return (int)(events & (FD_POLL_READ | FD_POLL_WRITE));
    if (file->kind == FD_KIND_EVDEV) { extern int evdev_poll(void *, uint32_t); return evdev_poll(file->custom_ptr, events); }
    if (file->kind == FD_KIND_DRM_FB) return (int)(events & (FD_POLL_READ | FD_POLL_WRITE));
    if (file->kind == FD_KIND_EVENTFD) return unix_eventfd_poll(file->custom_ptr, events);
    if (file->kind == FD_KIND_FILE || file->kind == FD_KIND_DIRECTORY) {
        uint32_t ready = 0;
        if (file->kind == FD_KIND_FILE && (file->flags & FD_OPEN_READ)) ready |= FD_POLL_READ;
        if (file->kind == FD_KIND_DIRECTORY && (file->flags & FD_OPEN_READ)) {
            kesh_vfs_entry_t entries[VFS_MAX_CHILDREN];
            int count = vfs_list(file->path, entries, VFS_MAX_CHILDREN);
            if (count >= 0 && file->offset < (uint64_t)count) ready |= FD_POLL_READ;
        }
        if (file->kind == FD_KIND_FILE && (file->flags & FD_OPEN_WRITE)) ready |= FD_POLL_WRITE;
        return (int)(events & ready);
    }
    if (file->kind == FD_KIND_PTY) {
        uint32_t ready = (file->flags & FD_OPEN_WRITE) ? FD_POLL_WRITE : 0;
        if ((file->flags & FD_OPEN_READ) && pty_available(pid, file->pipe_id) > 0) ready |= FD_POLL_READ;
        return (int)(events & ready);
    }
    if (file->pipe_id >= FD_PIPE_MAX || !g_pipes[file->pipe_id].allocated) return -1;
    fd_pipe_t *pipe = &g_pipes[file->pipe_id];
    pipe_lock(pipe);
    uint32_t ready = 0;
    if (file->kind == FD_KIND_PIPE_READ && pipe->used) ready |= FD_POLL_READ;
    if (file->kind == FD_KIND_PIPE_WRITE && pipe->used < FD_PIPE_BUFFER && pipe->readers) ready |= FD_POLL_WRITE;
    if ((file->kind == FD_KIND_PIPE_READ && !pipe->writers) || (file->kind == FD_KIND_PIPE_WRITE && !pipe->readers)) ready |= FD_POLL_HANGUP;
    pipe_unlock(pipe);
    return (int)(ready & events);
}

int fd_readdir(int pid, int fd, fd_dirent_t *out) {
    fd_file_t *file = file_for(entry_for(pid, fd));
    if (!file || file->kind != FD_KIND_DIRECTORY || !(file->flags & FD_OPEN_READ) || !out) return -1;
    kesh_vfs_entry_t entries[VFS_MAX_CHILDREN];
    int count = vfs_list(file->path, entries, VFS_MAX_CHILDREN);
    if (count < 0) return -1;
    if (file->offset >= (uint64_t)count) return 0;
    kesh_vfs_entry_t *source = &entries[file->offset++];
    int i = 0;
    while (source->name[i] && i + 1 < (int)sizeof(out->name)) { out->name[i] = source->name[i]; i++; }
    out->name[i] = 0;
    out->size = source->size;
    out->is_dir = source->is_dir;
    return 1;
}

int fd_stat(int pid, int fd, uint64_t *size, uint8_t *is_dir) {
    fd_file_t *file = file_for(entry_for(pid, fd));
    if (!file || (file->kind != FD_KIND_FILE && file->kind != FD_KIND_DIRECTORY)) return -1;
    kesh_vfs_stat_t stat;
    if (vfs_stat(file->path, &stat) != 0) return -1;
    if (size) *size = stat.size;
    if (is_dir) *is_dir = stat.is_dir;
    return 0;
}

int fd_create_custom(int pid, int kind, void *custom_ptr, uint32_t flags) {
    if (!valid_pid(pid)) return -1;
    int file_id = -1;
    for (int file = 0; file < FD_FILE_MAX; ++file) if (!g_files[file].references) { file_id = file; break; }
    if (file_id < 0) return -1;
    for (int fd = 0; fd < FD_MAX_PER_PROCESS; ++fd) if (!g_fds[pid][fd].open) {
        fd_file_t *file = &g_files[file_id];
        file->references = 1;
        file->flags = flags | FD_OPEN_READ | FD_OPEN_WRITE;
        file->offset = 0;
        file->kind = (fd_kind_t)kind;
        file->custom_ptr = custom_ptr;
        file->path[0] = '\0';
        g_fds[pid][fd].file_id = (uint16_t)file_id;
        g_fds[pid][fd].open = 1;
        return fd;
    }
    return -1;
}

void *fd_get_custom(int pid, int fd, int *out_kind) {
    fd_entry_t *entry = entry_for(pid, fd);
    if (!entry) return 0;
    fd_file_t *file = file_for(entry);
    if (!file) return 0;
    if (out_kind) *out_kind = (int)file->kind;
    return file->custom_ptr;
}

int fd_get_kind(int pid, int fd) {
    fd_entry_t *entry = entry_for(pid, fd);
    if (!entry) return -1;
    fd_file_t *file = file_for(entry);
    return file ? (int)file->kind : -1;
}

uint16_t fd_get_file_id(int pid, int fd) {
    fd_entry_t *entry = entry_for(pid, fd);
    return entry ? entry->file_id : 0xFFFFU;
}

int fd_bind_file_id(int pid, uint16_t file_id) {
    if (!valid_pid(pid) || file_id >= FD_FILE_MAX || !g_files[file_id].references) return -1;
    for (int fd = 0; fd < FD_MAX_PER_PROCESS; ++fd) if (!g_fds[pid][fd].open) {
        g_files[file_id].references++;
        g_fds[pid][fd].file_id = file_id;
        g_fds[pid][fd].open = 1;
        return fd;
    }
    return -1;
}
