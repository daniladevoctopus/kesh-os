// быстрые сисколлы через msr
#include "syscall.h"
#include "timer.h"
#include "vfs.h"
#include "process.h"
#include "memory.h"
#include <stddef.h>
#include "../src/drivers/net/net_stack.h"
#include "../src/drivers/net/netdev.h"
#include "../src/drivers/net/tls/kesh_tls.h"

#define MSR_EFER  0xC0000080
#define MSR_STAR  0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084

extern void syscall_entry_stub(void);

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)(val & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static void serial_write_char(char c) {
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)c), "Nd"((uint16_t)0x3F8));
}

static void serial_write_str(const char *s) {
    if (!s) return;
    while (*s) serial_write_char(*s++);
}

static uint8_t g_syscall_stack[16384] __attribute__((aligned(16)));
uint64_t g_syscall_kernel_stack_top = 0;

void syscall_init(void) {
    g_syscall_kernel_stack_top = (uint64_t)&g_syscall_stack[sizeof(g_syscall_stack) - 16];

    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1ULL; 
    wrmsr(MSR_EFER, efer);

    uint64_t star = ((uint64_t)0x30 << 48) | ((uint64_t)0x28 << 32);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry_stub);

    wrmsr(MSR_FMASK, (1ULL << 9) | (1ULL << 8));

    serial_write_str("[SYSCALL] Fast system calls (syscall/sysretq) initialized.\n");
}

#include "uwindow.h"

extern void process_return_to_kernel(int exit_code) __attribute__((weak));
extern uint8_t g_syscall_should_yield;
extern uint64_t g_active_user_context;
extern void process_sleep_current(uint32_t ms);

int64_t syscall_dispatcher(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
    switch (num) {
        case SYS_EXIT:
            serial_write_str("[SYSCALL] SYS_EXIT called by user process.\n");
            g_syscall_should_yield = 2; 
            if (!g_active_user_context && process_return_to_kernel) {
                g_syscall_should_yield = 0;
                process_return_to_kernel((int)a1);
            }
            return 0;

        case SYS_PRINT:
            serial_write_str((const char*)a1);
            return 0;

        case SYS_YIELD:
            g_syscall_should_yield = 1; 
            return 0;

        case SYS_GET_TIME:
            return (int64_t)timer_millis();

        case SYS_TEST:
            serial_write_str("[SYSCALL] SYS_TEST received from Ring 3! Argument: ");
            char buf[32];
            int i = 0;
            uint64_t val = a1;
            if (val == 0) buf[i++] = '0';
            while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
            while (i > 0) serial_write_char(buf[--i]);
            serial_write_str(" [SUCCESS]\n");
            return (int64_t)(a1 * 2);

        case SYS_CREATE_WINDOW: {
            uint64_t user_cr3;
            __asm__ volatile ("mov %%cr3, %0" : "=r"(user_cr3));
            user_window_t *win = uwindow_create((int)a1, (int)a2, (const char*)a3, user_cr3);
            if (!win) {
                serial_write_str("[SYSCALL] Failed to create user window!\n");
                return 0;
            }
            serial_write_str("[SYSCALL] User window created successfully! Framebuffer at: 0x50000000\n");
            return (int64_t)win->user_fb_vaddr;
        }

        case SYS_UPDATE_WINDOW: {
            user_window_t *win = uwindow_get((int)a1);
            if (win) win->dirty = 1;
            g_syscall_should_yield = 1; 
            return 0;
        }

        case SYS_POLL_EVENT: {
            uevent_t ev;
            int has_ev = uwindow_pop_event((int)a1, &ev);
            if (has_ev && a2) {
                uevent_t *dst = (uevent_t*)a2;
                *dst = ev;
                return 1;
            }
            return 0;
        }

        case SYS_SLEEP:
            process_sleep_current((uint32_t)a1);
            g_syscall_should_yield = 1;
            return 0;

        case SYS_VFS_LIST:
            return vfs_list((const char*)a1, (kesh_vfs_entry_t*)a2, (int)a3);

        case SYS_VFS_READ:
            return vfs_read((const char*)a1, (void*)a2, (int)a3);

        case SYS_VFS_WRITE:
            return vfs_write((const char*)a1, (const void*)a2, (int)a3);

        case SYS_NET_INFO: {
            if (!a1) return -1;
            typedef struct {
                uint32_t ip;
                uint32_t netmask;
                uint32_t gateway;
                uint32_t dns;
                uint8_t mac[6];
                int link_up;
                char driver[16];
            } net_info_t;
            net_info_t *info = (net_info_t*)a1;
            const netdev_info_t *dev = netdev_get_info();
            info->ip = net_get_my_ip();
            extern uint32_t net_get_netmask(void);
            extern uint32_t net_get_gateway(void);
            extern uint32_t net_get_dns_server(void);
            info->netmask = net_get_netmask();
            info->gateway = net_get_gateway();
            info->dns = net_get_dns_server();
            const uint8_t *mac = net_get_my_mac();
            for (int i = 0; i < 6; i++) info->mac[i] = mac[i];
            info->link_up = dev ? (dev->link_up ? 1 : 0) : 0;
            int i = 0;
            if (dev && dev->name) {
                while (dev->name[i] && i < 15) { info->driver[i] = dev->name[i]; i++; }
            }
            info->driver[i] = '\0';
            return 0;
        }

        case SYS_NET_FETCH: {
            if (!a1 || !a2 || a3 < 2 || a3 > 65535) return -1;
            return kesh_http_fetch((const char*)a1, (char*)a2, (size_t)a3, NULL);
        }

        case SYS_VFS_STAT:
            return vfs_stat((const char*)a1, (kesh_vfs_stat_t*)a2);

        case SYS_EXEC: {
            extern process_t* process_spawn_path(const char *path);
            process_t *p = process_spawn_path((const char*)a1);
            if (p) {
                g_syscall_should_yield = 1;
                return p->id;
            }
            return -1;
        }

        case SYS_GET_PROCS:
            return process_get_table((kesh_proc_info_t*)a1, (int)a2);

        case SYS_KILL:
            return process_kill((int)a1);

        case SYS_GET_SYSINFO: {
            kesh_sysinfo_t *info = (kesh_sysinfo_t*)a1;
            if (!info) return -1;
            uint64_t total = 0, used = 0;
            pmm_get_stats(&total, &used);
            info->total_ram_bytes = total;
            info->free_ram_bytes = (total > used) ? (total - used) : 0;
            info->uptime_ms = timer_millis();
            extern uint32_t g_screen_w, g_screen_h;
            info->screen_w = g_screen_w;
            info->screen_h = g_screen_h;
            kesh_proc_info_t dummy[16];
            info->active_procs = (uint32_t)process_get_table(dummy, 16);
            vfs_get_disk_stats(&info->disk_total_bytes, &info->disk_free_bytes);
            return 0;
        }

        case SYS_VFS_MKDIR:
            return vfs_mkdir((const char*)a1);

        case SYS_VFS_DELETE:
            return vfs_delete((const char*)a1);

        case SYS_VFS_RESCAN:
            vfs_rescan_drives();
            return 0;

        case SYS_GET_SETTINGS: {
            kesh_settings_t *st = (kesh_settings_t*)a1;
            if (!st) return -1;
            extern int g_wallpaper_choice;
            extern int g_dark_mode;
            extern int g_dock_mag_enabled;
            extern int g_dock_mag_level;
            extern int g_window_controls_align;
            extern int g_accent_color;
            st->wallpaper_choice = g_wallpaper_choice;
            st->dock_mag_enabled = g_dock_mag_enabled;
            st->dock_mag_level = g_dock_mag_level;
            st->window_controls_align = g_window_controls_align;
            st->dark_mode = g_dark_mode;
            st->accent_color = g_accent_color;
            return 0;
        }

        case SYS_SET_SETTINGS: {
            const kesh_settings_t *st = (const kesh_settings_t*)a1;
            if (!st) return -1;
            extern int g_wallpaper_choice;
            extern int g_dark_mode;
            extern int g_dock_mag_enabled;
            extern int g_dock_mag_level;
            extern int g_window_controls_align;
            extern int g_accent_color;
            if (st->wallpaper_choice >= 1 && st->wallpaper_choice <= 2) {
                g_wallpaper_choice = st->wallpaper_choice;
            }
            g_dark_mode = st->dark_mode ? 1 : 0;
            g_dock_mag_enabled = st->dock_mag_enabled ? 1 : 0;
            if (st->dock_mag_level >= 0 && st->dock_mag_level <= 100) {
                g_dock_mag_level = st->dock_mag_level;
            }
            if (st->window_controls_align == 0 || st->window_controls_align == 1) {
                g_window_controls_align = st->window_controls_align;
            }
            if (st->accent_color >= 0 && st->accent_color <= 2) {
                g_accent_color = st->accent_color;
            }
            return 0;
        }

        case 21: {
            extern void kernel_panic(const char *msg);
            kernel_panic(a1 ? (const char*)a1 : "Kernel panic triggered from userspace application");
            return 0;
        }

        default:
            serial_write_str("[SYSCALL] Unknown syscall number!\n");
            return -1;
    }
}
