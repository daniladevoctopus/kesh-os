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
#define SYS_VFS_WRITE      22
#define SYS_NET_INFO       23
#define SYS_NET_FETCH      24

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
