// апишка keshos для приложений
#ifndef KESH_H
#define KESH_H

typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#ifdef __SIZE_TYPE__
typedef __SIZE_TYPE__ size_t;
#else
typedef unsigned long size_t;
#endif
#define NULL ((void*)0)

#define SYS_EXIT           0
#define SYS_PRINT          1
#define SYS_YIELD          2
#define SYS_GET_TIME       3
#define SYS_TEST           4
#define SYS_CREATE_WINDOW  5
#define SYS_UPDATE_WINDOW  6
#define SYS_POLL_EVENT     7
#define SYS_SLEEP          8
#define SYS_VFS_LIST       9
#define SYS_VFS_READ       10
#define SYS_VFS_STAT       11
#define SYS_EXEC           12
#define SYS_GET_PROCS      13
#define SYS_KILL           14
#define SYS_GET_SYSINFO    15
#define SYS_VFS_MKDIR      16
#define SYS_VFS_DELETE     17
#define SYS_VFS_RESCAN     18
#define SYS_GET_SETTINGS   19
#define SYS_SET_SETTINGS   20
#define SYS_PANIC          21
#define SYS_VFS_WRITE      22
#define SYS_NET_INFO       23
#define SYS_NET_FETCH      24
#define SYS_THREAD_CREATE  25
#define SYS_THREAD_EXIT    26
#define SYS_GETRANDOM      27
#define SYS_SOCKET         28
#define SYS_SOCKET_CONNECT 29
#define SYS_SOCKET_SEND    30
#define SYS_SOCKET_RECV    31
#define SYS_SOCKET_CLOSE   32
#define SYS_FD_OPEN        33
#define SYS_FD_CLOSE       34
#define SYS_FD_READ        35
#define SYS_FD_WRITE       36
#define SYS_FD_SEEK        37
#define SYS_IPC_SEND       38
#define SYS_IPC_RECV       39
#define SYS_GET_CREDENTIALS 40
#define SYS_IPC_LAST_SENDER 41
#define SYS_FD_DUP         42
#define SYS_DIAGNOSTICS_READ 43
#define SYS_TTY_CREATE       44
#define SYS_TTY_CLOSE        45
#define SYS_TTY_READ         46
#define SYS_TTY_WRITE        47
#define SYS_TTY_AVAILABLE    48
#define SYS_CLOCK_GET        49
#define SYS_VM_MAP           50
#define SYS_VM_UNMAP         51
#define SYS_PIPE_CREATE      52
#define SYS_FD_POLL          53
#define SYS_PTY_CREATE       54
#define SYS_PTY_ATTACH       55
#define SYS_PTY_READ         56
#define SYS_PTY_WRITE        57
#define SYS_PTY_AVAILABLE    58
#define SYS_PTY_SET_MODE     59
#define SYS_PTY_CLOSE        60
#define SYS_PTY_CURRENT      61
#define SYS_GET_PGID         62
#define SYS_SET_PGID         63
#define SYS_PTY_SET_FG       64
#define SYS_PTY_GET_FG       65
#define SYS_PTY_SIGNAL       66
#define SYS_PTY_OPEN_FD      67
#define SYS_FD_READDIR       68
#define SYS_SOCKET_BIND      69
#define SYS_FD_DUP2          70
#define SYS_SOCKET_SENDTO    71
#define SYS_SOCKET_RECVFROM  72
#define SYS_SOCKET_POLL      73
#define SYS_AUDIO_DEVICE     74
#define SYS_AUDIO_VOLUME     75
#define SYS_AUDIO_PLAY       76
#define SYS_IPC_SEND_TYPED   77
#define SYS_IPC_RECV_TYPED   78
#define SYS_ABI_INFO         79
#define SYS_DIAGNOSTICS_QUERY 80
#define SYS_SERVICE_HEARTBEAT 81
#define SYS_PERMISSION_REVOKE 82
#define SYS_TRUST_STORE_RELOAD 83
#define SYS_CPU_INFO          84

#define KESH_ABI_VERSION 1U
#define KESH_ABI_FEATURE_USER_COPY (1ULL << 0)
#define KESH_ABI_FEATURE_WX        (1ULL << 1)
#define KESH_ABI_FEATURE_TYPED_IPC (1ULL << 2)
#define KESH_ABI_FEATURE_FD        (1ULL << 3)
#define KESH_ABI_FEATURE_CA_UPDATE (1ULL << 4)
#define KESH_ABI_FEATURE_CPU_INFO  (1ULL << 5)

#define KESH_POLL_READ       0x01U
#define KESH_POLL_WRITE      0x02U
#define KESH_POLL_HANGUP     0x04U
#define KESH_PTY_CANONICAL   0x01U
#define KESH_PTY_ECHO        0x02U
#define KESH_SIGINT          2
#define KESH_SIGKILL         9
#define KESH_SIGTERM         15
#define KESH_SIGCONT         18
#define KESH_SIGSTOP         19

#define KESH_CLOCK_MONOTONIC 1
#define KESH_CLOCK_REALTIME  2
#define KESH_PROT_READ       0x01U
#define KESH_PROT_WRITE      0x02U
#define KESH_PROT_EXEC       0x04U

#define KESH_SOCKET_TCP 1
#define KESH_SOCKET_UDP 2
#define KESH_AUDIO_NONE 0
#define KESH_AUDIO_AC97 1
#define KESH_AUDIO_HDA 2
#define KESH_FD_READ  0x01U
#define KESH_FD_WRITE 0x02U
#define KESH_FD_CREATE 0x04U
#define KESH_FD_TRUNC  0x08U
#define KESH_FD_APPEND 0x10U
#define KESH_FD_DIRECTORY 0x20U

#define ACCENT_AMBER 0
#define ACCENT_BLUE  1
#define ACCENT_RED   2

typedef struct {
    uint64_t seconds;
    uint32_t nanoseconds;
    uint32_t clock_id;
} kesh_timespec_t;

typedef struct {
    uint32_t version;
    uint32_t max_syscall;
    uint64_t features;
    uint32_t page_size;
    uint32_t max_io_size;
} kesh_abi_info_t;

typedef struct {
    char vendor[16];
    char brand[64];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
} kesh_cpu_info_t;

typedef struct {
    uint64_t buffer;
    uint32_t size;
    int32_t min_level;
    int32_t max_level;
    char module[24];
} kesh_diagnostics_query_t;

typedef struct {
    char name[64];
    uint32_t size;
    uint8_t is_dir;
} kesh_dirent_t;

typedef struct {
    uint32_t address;
    uint16_t port;
    uint16_t length;
    uint64_t data;
} kesh_datagram_t;

typedef struct {
    uint64_t data;
    uint32_t size;
    int32_t sender;
    int32_t type;
    uint32_t reserved;
} kesh_ipc_request_t;

typedef struct {
    int wallpaper_choice;
    int dock_mag_enabled;
    int dock_mag_level;
    int window_controls_align;
    int dark_mode;
    int accent_color;
    int theme_id;
    int animations_enabled;
    int animation_speed;
    int interface_density;
    int show_seconds;
    int liquid_glass;
} kesh_settings_t;

typedef struct {
    int pid;
    char name[32];
    int state;
    uint64_t memory_bytes;
} kesh_proc_info_t;

typedef struct {
    uint32_t uid;
    uint32_t gid;
    uint32_t groups;
} kesh_credentials_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t total_ram_bytes;
    uint64_t free_ram_bytes;
    uint64_t uptime_ms;
    uint32_t active_procs;
    uint32_t screen_w;
    uint32_t screen_h;
    uint64_t disk_total_bytes;
    uint64_t disk_free_bytes;
} kesh_sysinfo_t;

#define EVENT_NONE       0
#define EVENT_MOUSE_MOVE 1
#define EVENT_MOUSE_DOWN 2
#define EVENT_MOUSE_UP   3
#define EVENT_KEY_DOWN   4
#define EVENT_CLOSE      5

typedef struct {
    int type;
    int mx;
    int my;
    int btn;
    int key;
} kesh_event_t;

#define VFS_NAME_MAX 64
typedef struct {
    char name[VFS_NAME_MAX];
    uint32_t size;
    uint8_t is_dir;
} kesh_vfs_entry_t;

typedef struct {
    uint32_t size;
    uint8_t is_dir;
} kesh_vfs_stat_t;

typedef struct {
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint8_t mac[6];
    int link_up;
    char driver[16];
} kesh_net_info_t;

static inline int64_t kesh_syscall0(int64_t num) {
    int64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t kesh_syscall1(int64_t num, int64_t a1) {
    int64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t kesh_syscall2(int64_t num, int64_t a1, int64_t a2) {
    int64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t kesh_syscall3(int64_t num, int64_t a1, int64_t a2, int64_t a3) {
    int64_t ret;
    register int64_t r10 __asm__("r10") = a3;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}

void kesh_exit(int code);

static inline void kesh_print(const char *str) {
    kesh_syscall1(SYS_PRINT, (int64_t)str);
}

static inline void kesh_yield(void) {
    kesh_syscall0(SYS_YIELD);
}

static inline uint64_t kesh_time(void) {
    return (uint64_t)kesh_syscall0(SYS_GET_TIME);
}

static inline int kesh_clock_get(int clock_id, kesh_timespec_t *value) {
    return (int)kesh_syscall2(SYS_CLOCK_GET, clock_id, (int64_t)value);
}

static inline void *kesh_vm_map(uint64_t size, uint32_t protection) {
    return (void *)kesh_syscall2(SYS_VM_MAP, (int64_t)size, protection);
}

static inline int kesh_vm_unmap(void *address, uint64_t size) {
    return (int)kesh_syscall2(SYS_VM_UNMAP, (int64_t)address, (int64_t)size);
}
static inline int kesh_pipe(int descriptors[2]) { return (int)kesh_syscall1(SYS_PIPE_CREATE, (int64_t)descriptors); }
static inline int kesh_poll(int fd, uint32_t events) { return (int)kesh_syscall2(SYS_FD_POLL, fd, events); }

static inline uint32_t* kesh_create_window(int width, int height, const char *title) {
    return (uint32_t*)kesh_syscall3(SYS_CREATE_WINDOW, width, height, (int64_t)title);
}

static inline void kesh_update_window(int win_id) {
    kesh_syscall1(SYS_UPDATE_WINDOW, win_id);
}

static inline int kesh_poll_event(int win_id, kesh_event_t *ev) {
    return (int)kesh_syscall2(SYS_POLL_EVENT, win_id, (int64_t)ev);
}

static inline void kesh_sleep(uint32_t ms) {
    kesh_syscall1(SYS_SLEEP, ms);
}

static inline int kesh_vfs_list(const char *path, kesh_vfs_entry_t *entries, int max_entries) {
    return (int)kesh_syscall3(SYS_VFS_LIST, (int64_t)path, (int64_t)entries, (int64_t)max_entries);
}

static inline int kesh_vfs_read(const char *path, void *buffer, int max_bytes) {
    return (int)kesh_syscall3(SYS_VFS_READ, (int64_t)path, (int64_t)buffer, (int64_t)max_bytes);
}

static inline int kesh_vfs_write(const char *path, const void *buffer, int bytes) {
    return (int)kesh_syscall3(SYS_VFS_WRITE, (int64_t)path, (int64_t)buffer, (int64_t)bytes);
}

static inline int kesh_vfs_stat(const char *path, kesh_vfs_stat_t *stat) {
    return (int)kesh_syscall2(SYS_VFS_STAT, (int64_t)path, (int64_t)stat);
}

static inline int kesh_vfs_mkdir(const char *path) {
    return (int)kesh_syscall1(SYS_VFS_MKDIR, (int64_t)path);
}

static inline int kesh_vfs_delete(const char *path) {
    return (int)kesh_syscall1(SYS_VFS_DELETE, (int64_t)path);
}

static inline int kesh_vfs_rescan(void) {
    return (int)kesh_syscall0(SYS_VFS_RESCAN);
}

static inline int kesh_thread_create(void (*entry)(void)) {
    return (int)kesh_syscall1(SYS_THREAD_CREATE, (int64_t)entry);
}

static inline void kesh_thread_exit(void) {
    kesh_syscall0(SYS_THREAD_EXIT);
    for (;;) { }
}

static inline int kesh_getrandom(void *buffer, uint32_t bytes) {
    return (int)kesh_syscall2(SYS_GETRANDOM, (int64_t)buffer, bytes);
}

static inline int kesh_abi_info(kesh_abi_info_t *info) {
    return (int)kesh_syscall1(SYS_ABI_INFO, (int64_t)info);
}

static inline int kesh_service_heartbeat(void) {
    return (int)kesh_syscall0(SYS_SERVICE_HEARTBEAT);
}

static inline int kesh_permission_revoke(int target_pid, uint32_t mask) {
    return (int)kesh_syscall2(SYS_PERMISSION_REVOKE, target_pid, mask);
}

static inline int kesh_trust_store_reload(void) {
    return (int)kesh_syscall0(SYS_TRUST_STORE_RELOAD);
}

static inline int kesh_cpu_info(kesh_cpu_info_t *info) {
    return (int)kesh_syscall1(SYS_CPU_INFO, (int64_t)info);
}

static inline int kesh_exec(const char *path) {
    return (int)kesh_syscall1(SYS_EXEC, (int64_t)path);
}

static inline int kesh_get_procs(kesh_proc_info_t *table, int max_entries) {
    return (int)kesh_syscall2(SYS_GET_PROCS, (int64_t)table, max_entries);
}

static inline int kesh_kill(int pid) {
    return (int)kesh_syscall1(SYS_KILL, pid);
}

static inline int kesh_get_sysinfo(kesh_sysinfo_t *info) {
    return (int)kesh_syscall1(SYS_GET_SYSINFO, (int64_t)info);
}

static inline int kesh_get_settings(kesh_settings_t *out_settings) {
    return (int)kesh_syscall1(SYS_GET_SETTINGS, (int64_t)out_settings);
}

static inline int kesh_set_settings(const kesh_settings_t *settings) {
    return (int)kesh_syscall1(SYS_SET_SETTINGS, (int64_t)settings);
}

static inline void kesh_panic(const char *msg) {
    kesh_syscall1(SYS_PANIC, (int64_t)msg);
}

static inline int kesh_net_info(kesh_net_info_t *out_info) {
    return (int)kesh_syscall1(SYS_NET_INFO, (int64_t)out_info);
}

static inline int kesh_net_fetch(const char *url, char *buffer, int max_bytes) {
    return (int)kesh_syscall3(SYS_NET_FETCH, (int64_t)url, (int64_t)buffer, (int64_t)max_bytes);
}

static inline int kesh_socket(int protocol) { return (int)kesh_syscall1(SYS_SOCKET, protocol); }
static inline int kesh_socket_connect(int socket, uint32_t address, uint16_t port) { return (int)kesh_syscall3(SYS_SOCKET_CONNECT, socket, address, port); }
static inline int kesh_socket_bind(int socket, uint32_t address, uint16_t port) { return (int)kesh_syscall3(SYS_SOCKET_BIND, socket, address, port); }
static inline int kesh_socket_sendto(int socket, uint32_t address, uint16_t port, const void *data, uint16_t length) {
    kesh_datagram_t request = {address, port, length, (uint64_t)data};
    return (int)kesh_syscall2(SYS_SOCKET_SENDTO, socket, (int64_t)&request);
}
static inline int kesh_socket_recvfrom(int socket, uint32_t *address, uint16_t *port, void *data, uint16_t length) {
    kesh_datagram_t request = {0, 0, length, (uint64_t)data};
    int result = (int)kesh_syscall2(SYS_SOCKET_RECVFROM, socket, (int64_t)&request);
    if (result > 0) { if (address) *address = request.address; if (port) *port = request.port; }
    return result;
}
static inline int kesh_socket_poll(int socket, uint32_t events) { return (int)kesh_syscall2(SYS_SOCKET_POLL, socket, events); }
static inline int kesh_audio_device(void) { return (int)kesh_syscall0(SYS_AUDIO_DEVICE); }
static inline int kesh_audio_set_volume(uint8_t volume) { return (int)kesh_syscall1(SYS_AUDIO_VOLUME, volume); }
static inline int kesh_audio_play_pcm16(const int16_t *samples, uint32_t sample_count) {
    if (sample_count > 32768U) sample_count = 32768U;
    return (int)kesh_syscall2(SYS_AUDIO_PLAY, (int64_t)samples, (int64_t)sample_count * 2);
}
static inline int kesh_socket_send(int socket, const void *data, uint16_t length) { return (int)kesh_syscall3(SYS_SOCKET_SEND, socket, (int64_t)data, length); }
static inline int kesh_socket_recv(int socket, void *data, uint16_t length) { return (int)kesh_syscall3(SYS_SOCKET_RECV, socket, (int64_t)data, length); }
static inline int kesh_socket_close(int socket) { return (int)kesh_syscall1(SYS_SOCKET_CLOSE, socket); }

static inline int kesh_open(const char *path, uint32_t flags) { return (int)kesh_syscall2(SYS_FD_OPEN, (int64_t)path, flags); }
static inline int kesh_close(int fd) { return (int)kesh_syscall1(SYS_FD_CLOSE, fd); }
static inline int kesh_dup(int fd) { return (int)kesh_syscall1(SYS_FD_DUP, fd); }
static inline int kesh_dup2(int source_fd, int target_fd) { return (int)kesh_syscall2(SYS_FD_DUP2, source_fd, target_fd); }
static inline int kesh_read(int fd, void *data, uint32_t length) { return (int)kesh_syscall3(SYS_FD_READ, fd, (int64_t)data, length); }
static inline int kesh_write(int fd, const void *data, uint32_t length) { return (int)kesh_syscall3(SYS_FD_WRITE, fd, (int64_t)data, length); }
static inline int64_t kesh_seek(int fd, int64_t offset, int whence) { return kesh_syscall3(SYS_FD_SEEK, fd, offset, whence); }
static inline int kesh_readdir(int fd, kesh_dirent_t *entry) { return (int)kesh_syscall2(SYS_FD_READDIR, fd, (int64_t)entry); }
static inline int kesh_ipc_send(int target_pid, const void *data, uint32_t length) { return (int)kesh_syscall3(SYS_IPC_SEND, target_pid, (int64_t)data, length); }
static inline int kesh_ipc_receive(int sender_filter, void *data, uint32_t max_length) { return (int)kesh_syscall3(SYS_IPC_RECV, sender_filter, (int64_t)data, max_length); }
static inline int kesh_ipc_last_sender(void) { return (int)kesh_syscall0(SYS_IPC_LAST_SENDER); }
static inline int kesh_ipc_send_typed(int target_pid, uint16_t type, const void *data, uint32_t length) {
    kesh_ipc_request_t request = {(uint64_t)data, length, 0, type, 0};
    return (int)kesh_syscall2(SYS_IPC_SEND_TYPED, target_pid, (int64_t)&request);
}
static inline int kesh_ipc_receive_typed(int sender_filter, int type_filter, void *data, uint32_t max_length,
                                         int *out_sender, uint16_t *out_type) {
    kesh_ipc_request_t request = {(uint64_t)data, max_length, sender_filter, type_filter, 0};
    int result = (int)kesh_syscall1(SYS_IPC_RECV_TYPED, (int64_t)&request);
    if (result >= 0) {
        if (out_sender) *out_sender = request.sender;
        if (out_type) *out_type = (uint16_t)request.type;
    }
    return result;
}
static inline int kesh_get_credentials(kesh_credentials_t *out_credentials) { return (int)kesh_syscall1(SYS_GET_CREDENTIALS, (int64_t)out_credentials); }
static inline int kesh_diagnostics_read(void *buffer, uint32_t max_bytes) { return (int)kesh_syscall2(SYS_DIAGNOSTICS_READ, (int64_t)buffer, max_bytes); }
static inline int kesh_diagnostics_query(void *buffer, uint32_t max_bytes, int min_level, int max_level, const char *module) {
    kesh_diagnostics_query_t request = {(uint64_t)buffer, max_bytes, min_level, max_level, {0}};
    int i = 0;
    while (module && module[i] && i + 1 < (int)sizeof(request.module)) { request.module[i] = module[i]; i++; }
    return (int)kesh_syscall1(SYS_DIAGNOSTICS_QUERY, (int64_t)&request);
}
static inline int kesh_tty_create(void) { return (int)kesh_syscall0(SYS_TTY_CREATE); }
static inline int kesh_tty_close(int tty) { return (int)kesh_syscall1(SYS_TTY_CLOSE, tty); }
static inline int kesh_tty_read(int tty, void *data, uint32_t length) { return (int)kesh_syscall3(SYS_TTY_READ, tty, (int64_t)data, length); }
static inline int kesh_tty_write(int tty, const void *data, uint32_t length) { return (int)kesh_syscall3(SYS_TTY_WRITE, tty, (int64_t)data, length); }
static inline int kesh_tty_available(int tty) { return (int)kesh_syscall1(SYS_TTY_AVAILABLE, tty); }
static inline int kesh_pty_create(int handles[2]) { return (int)kesh_syscall1(SYS_PTY_CREATE, (int64_t)handles); }
static inline int kesh_pty_attach(int slave, int target_pid) { return (int)kesh_syscall2(SYS_PTY_ATTACH, slave, target_pid); }
static inline int kesh_pty_read(int handle, void *data, uint32_t length) { return (int)kesh_syscall3(SYS_PTY_READ, handle, (int64_t)data, length); }
static inline int kesh_pty_write(int handle, const void *data, uint32_t length) { return (int)kesh_syscall3(SYS_PTY_WRITE, handle, (int64_t)data, length); }
static inline int kesh_pty_available(int handle) { return (int)kesh_syscall1(SYS_PTY_AVAILABLE, handle); }
static inline int kesh_pty_set_mode(int master, uint32_t mode) { return (int)kesh_syscall2(SYS_PTY_SET_MODE, master, mode); }
static inline int kesh_pty_close(int handle) { return (int)kesh_syscall1(SYS_PTY_CLOSE, handle); }
static inline int kesh_pty_current(void) { return (int)kesh_syscall0(SYS_PTY_CURRENT); }
static inline int kesh_getpgid(void) { return (int)kesh_syscall0(SYS_GET_PGID); }
static inline int kesh_setpgid(int process_group) { return (int)kesh_syscall1(SYS_SET_PGID, process_group); }
static inline int kesh_pty_set_foreground(int master, int process_group) { return (int)kesh_syscall2(SYS_PTY_SET_FG, master, process_group); }
static inline int kesh_pty_get_foreground(int master) { return (int)kesh_syscall1(SYS_PTY_GET_FG, master); }
static inline int kesh_pty_signal(int master, int signal) { return (int)kesh_syscall2(SYS_PTY_SIGNAL, master, signal); }
static inline int kesh_pty_open_fd(uint32_t flags) { return (int)kesh_syscall1(SYS_PTY_OPEN_FD, flags); }

void kesh_clear(uint32_t *fb, int w, int h, uint32_t color);
void kesh_draw_rect(uint32_t *fb, int fb_w, int x, int y, int w, int h, uint32_t color);
void kesh_draw_rect_alpha(uint32_t *fb, int fb_w, int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void kesh_draw_rounded_rect(uint32_t *fb, int fb_w, int x, int y, int w, int h, int r, uint32_t color);
void kesh_draw_rounded_rect_alpha(uint32_t *fb, int fb_w, int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);

void draw_char(char c, int x, int y, uint32_t color, uint32_t* buf, uint32_t buf_width);
void draw_string(const char* str, int x, int y, uint32_t color, uint32_t* buf, uint32_t buf_width);
int font_text_width(const char* str);

void kesh_draw_folder_icon(uint32_t *fb, int fb_w, int x, int y, int size);
void kesh_draw_file_icon(uint32_t *fb, int fb_w, int x, int y, int size, uint32_t badge_color);

#ifdef __cplusplus
}
#endif

#endif
