// номера сисколлов
#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

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
#define SYS_PANIC           21
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
#define SYS_IPC_SEND_TYPED    77
#define SYS_IPC_RECV_TYPED    78
#define SYS_ABI_INFO          79
#define SYS_DIAGNOSTICS_QUERY 80
#define SYS_SERVICE_HEARTBEAT 81
#define SYS_PERMISSION_REVOKE 82
#define SYS_TRUST_STORE_RELOAD 83
#define SYS_CPU_INFO           84

#define KESH_ABI_VERSION 1U
#define KESH_ABI_FEATURE_USER_COPY (1ULL << 0)
#define KESH_ABI_FEATURE_WX        (1ULL << 1)
#define KESH_ABI_FEATURE_TYPED_IPC (1ULL << 2)
#define KESH_ABI_FEATURE_FD        (1ULL << 3)
#define KESH_ABI_FEATURE_CA_UPDATE (1ULL << 4)
#define KESH_ABI_FEATURE_CPU_INFO  (1ULL << 5)

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

#define KESH_CLOCK_MONOTONIC 1
#define KESH_CLOCK_REALTIME  2
#define KESH_PROT_READ       0x01U
#define KESH_PROT_WRITE      0x02U
#define KESH_PROT_EXEC       0x04U

typedef struct {
    uint64_t seconds;
    uint32_t nanoseconds;
    uint32_t clock_id;
} kesh_timespec_t;

#define ACCENT_AMBER 0
#define ACCENT_BLUE  1
#define ACCENT_RED   2

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

void syscall_init(void);

static inline int64_t user_syscall(int64_t num, int64_t arg1, int64_t arg2, int64_t arg3) {
    int64_t ret;
    register int64_t r10 __asm__("r10") = arg3;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#endif
