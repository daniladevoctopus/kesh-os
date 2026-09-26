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

typedef struct {
    int pid;
    char name[32];
    int state;
    uint64_t memory_bytes;
} kesh_proc_info_t;

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
