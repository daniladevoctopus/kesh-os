#include "cmd_mode.h"
#include "vfs.h"
#include "memory.h"
#include "cpu.h"
#include "acpi.h"
#include "process.h"
#include "timer.h"
#include "log.h"
#include "gui/font.h"
#include "drivers/system/keyboard.h"

#include <stdint.h>
#include <stddef.h>

extern uint8_t* g_fb_vram;
extern uint32_t g_screen_w;
extern uint32_t g_screen_h;
extern uint32_t g_screen_pitch;
extern uint32_t g_screen_bpp;

/* Aesthetic Color Palette for CMD Mode */
#define CMD_BG_COLOR       0x000B0F17  /* Deep dark console background */
#define CMD_COLOR_PROMPT   0x007EE787  /* Vibrant terminal green */
#define CMD_COLOR_TEXT     0x00E6EDF3  /* Crisp light gray/white text */
#define CMD_COLOR_DIM      0x008B949E  /* Dim muted gray */
#define CMD_COLOR_CYAN     0x0058A6FF  /* Neon cyan-blue for headers/titles */
#define CMD_COLOR_ORANGE   0x00FFA657  /* Warm orange for warnings/tags */
#define CMD_COLOR_RED      0x00FF7B72  /* Crimson red for errors/reboot */
#define CMD_COLOR_GREEN    0x003FB950  /* Bright green for success */
#define CMD_COLOR_CURSOR   0x0058A6FF  /* Cyan cursor block */

#define CMD_MARGIN_X       18
#define CMD_MARGIN_Y       16
#define CMD_LINE_H         18

static int s_cur_x = CMD_MARGIN_X;
static int s_cur_y = CMD_MARGIN_Y;
static int s_cursor_visible = 0;

/* Internal String & Memory Helpers */
static size_t k_strlen(const char *s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

static int k_strcmp(const char *a, const char *b) {
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    if (n == 0) return 0;
    while (n-- > 1 && *a && (*a == *b)) {
        a++;
        b++;
    }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static void k_strcpy(char *dst, const char *src) {
    if (!dst || !src) return;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void console_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_fb_vram || w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > (int)g_screen_w) x1 = (int)g_screen_w;
    int y1 = y + h; if (y1 > (int)g_screen_h) y1 = (int)g_screen_h;
    uint32_t stride = g_screen_pitch / 4;

    for (int py = y0; py < y1; py++) {
        uint32_t *row = (uint32_t*)(g_fb_vram + ((uint32_t)py * g_screen_pitch));
        for (int px = x0; px < x1; px++) {
            row[px] = color;
        }
    }
}

static void console_scroll_up(void) {
    if (!g_fb_vram || g_screen_h <= CMD_LINE_H + CMD_MARGIN_Y) return;
    int start_y = CMD_MARGIN_Y;
    int end_y = (int)g_screen_h - CMD_LINE_H;

    for (int y = start_y; y < end_y; y++) {
        uint32_t *dst = (uint32_t*)(g_fb_vram + (uint32_t)y * g_screen_pitch);
        const uint32_t *src = (uint32_t*)(g_fb_vram + (uint32_t)(y + CMD_LINE_H) * g_screen_pitch);
        for (int x = 0; x < (int)g_screen_w; x++) {
            dst[x] = src[x];
        }
    }
    console_fill_rect(0, end_y, (int)g_screen_w, (int)g_screen_h - end_y, CMD_BG_COLOR);
}

static void console_check_scroll(void) {
    int max_y = (int)g_screen_h - CMD_LINE_H - 10;
    while (s_cur_y > max_y) {
        console_scroll_up();
        s_cur_y -= CMD_LINE_H;
    }
}

static void console_draw_cursor(int show) {
    if (show) {
        console_fill_rect(s_cur_x, s_cur_y + 13, 8, 3, CMD_COLOR_CURSOR);
    } else {
        console_fill_rect(s_cur_x, s_cur_y + 13, 8, 3, CMD_BG_COLOR);
    }
    s_cursor_visible = show;
}

static void console_newline(void) {
    console_draw_cursor(0);
    s_cur_x = CMD_MARGIN_X;
    s_cur_y += CMD_LINE_H;
    console_check_scroll();
}

static void console_putchar(char c, uint32_t color) {
    if (c == '\n') {
        console_newline();
        return;
    }
    if (c == '\r') {
        s_cur_x = CMD_MARGIN_X;
        return;
    }
    if (c == '\t') {
        int tab_size = 4 * 9;
        int next_tab = ((s_cur_x - CMD_MARGIN_X + tab_size) / tab_size) * tab_size + CMD_MARGIN_X;
        s_cur_x = next_tab;
        if (s_cur_x >= (int)g_screen_w - CMD_MARGIN_X) {
            console_newline();
        }
        return;
    }

    char tmp[2] = {c, '\0'};
    int w = font_text_width(tmp);
    if (w <= 0) w = 9;

    if (s_cur_x + w >= (int)g_screen_w - CMD_MARGIN_X) {
        console_newline();
    }

    console_fill_rect(s_cur_x, s_cur_y, w, CMD_LINE_H, CMD_BG_COLOR);
    draw_string(tmp, s_cur_x, s_cur_y, color, (uint32_t*)g_fb_vram, g_screen_pitch / 4);
    s_cur_x += w;
}

static void console_print(const char *str, uint32_t color) {
    if (!str) return;
    while (*str) {
        console_putchar(*str++, color);
    }
}

static void console_println(const char *str, uint32_t color) {
    console_print(str, color);
    console_newline();
}

static void console_print_u64(uint64_t val, uint32_t color) {
    char buf[32];
    int i = 0;
    if (val == 0) {
        console_putchar('0', color);
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        console_putchar(buf[j], color);
    }
}

static void console_print_hex(uint64_t val, uint32_t color) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[17];
    int i = 0;
    if (val == 0) {
        console_print("0x0", color);
        return;
    }
    while (val > 0) {
        buf[i++] = hex_chars[val & 0x0F];
        val >>= 4;
    }
    console_print("0x", color);
    for (int j = i - 1; j >= 0; j--) {
        console_putchar(buf[j], color);
    }
}

static void console_clear(void) {
    console_fill_rect(0, 0, (int)g_screen_w, (int)g_screen_h, CMD_BG_COLOR);
    s_cur_x = CMD_MARGIN_X;
    s_cur_y = CMD_MARGIN_Y;
}

static void cmd_show_neofetch(void) {
    uint64_t total_ram = 0, used_ram = 0;
    pmm_get_stats(&total_ram, &used_ram);
    uint64_t uptime_sec = timer_millis() / 1000ULL;

    console_print("        /\\_/\\         ", CMD_COLOR_CYAN);
    console_println("keshos@recovery", CMD_COLOR_GREEN);

    console_print("       ( o.o )        ", CMD_COLOR_CYAN);
    console_println("---------------", CMD_COLOR_DIM);

    console_print("        > ^ <         ", CMD_COLOR_CYAN);
    console_print("OS: ", CMD_COLOR_CYAN);
    console_println("KeshOS 1.0 (Drop)", CMD_COLOR_TEXT);

    console_print("                      ", CMD_COLOR_DIM);
    console_print("Uptime: ", CMD_COLOR_CYAN);
    console_print_u64(uptime_sec, CMD_COLOR_TEXT);
    console_println("s", CMD_COLOR_TEXT);

    console_print("      [KESHOS]        ", CMD_COLOR_ORANGE);
    console_print("Memory: ", CMD_COLOR_CYAN);
    console_print_u64(used_ram / (1024 * 1024), CMD_COLOR_TEXT);
    console_print(" MB / ", CMD_COLOR_TEXT);
    console_print_u64(total_ram / (1024 * 1024), CMD_COLOR_TEXT);
    console_println(" MB", CMD_COLOR_TEXT);

    console_print("                      ", CMD_COLOR_DIM);
    console_print("Display: ", CMD_COLOR_CYAN);
    console_print_u64(g_screen_w, CMD_COLOR_TEXT);
    console_print("x", CMD_COLOR_TEXT);
    console_print_u64(g_screen_h, CMD_COLOR_TEXT);
    console_println(" (32 bpp)", CMD_COLOR_TEXT);

    console_print("                      ", CMD_COLOR_DIM);
    console_print("CPU: ", CMD_COLOR_CYAN);
    console_print_u64(cpu_count(), CMD_COLOR_TEXT);
    console_println(" Cores", CMD_COLOR_TEXT);

    console_newline();

    /* Neofetch color palette */
    uint32_t colors[8] = {
        0x00484F58, 0x00FF7B72, 0x007EE787, 0x00F2CC60,
        0x0058A6FF, 0x00BC8CFF, 0x0079C0FF, 0x00F0F6FC
    };
    int box_y = s_cur_y + 2;
    int box_w = 20;
    int box_h = 10;
    int box_start_x = CMD_MARGIN_X + 24;
    for (int i = 0; i < 8; i++) {
        console_fill_rect(box_start_x + i * (box_w + 4), box_y, box_w, box_h, colors[i]);
    }
    console_newline();
    console_newline();
}

static void console_show_banner(void) {
    console_clear();
    console_println("KeshOS Recovery Mode", CMD_COLOR_CYAN);
    console_println("Type 'help' for commands, 'desktop' to launch GUI.", CMD_COLOR_DIM);
    console_newline();
    cmd_show_neofetch();
}

static void cmd_show_help(void) {
    console_println("Available Commands:", CMD_COLOR_CYAN);
    console_println("  neofetch        - Show system info & status", CMD_COLOR_TEXT);
    console_println("  ls [path]       - List files and directories", CMD_COLOR_TEXT);
    console_println("  cat <path>      - View contents of a file", CMD_COLOR_TEXT);
    console_println("  mkdir <path>    - Create a new directory", CMD_COLOR_TEXT);
    console_println("  rm <path>       - Delete a file or directory", CMD_COLOR_TEXT);
    console_println("  ps              - List active processes", CMD_COLOR_TEXT);
    console_println("  run <path>      - Execute an application", CMD_COLOR_TEXT);
    console_println("  hello / musl    - Run musl libc 1.2.5 POSIX test", CMD_COLOR_CYAN);
    console_println("  ipc             - Run Wayland IPC (memfd, SCM_RIGHTS, epoll) test", CMD_COLOR_CYAN);
    console_println("  wayland / wl    - Run Wayland architecture & libwayland test", CMD_COLOR_CYAN);
    console_println("  compositor / wc - Run Ring 3 Wayland compositor on /tmp/wayland-0", CMD_COLOR_CYAN);
    console_println("  gfx             - Run Graphics & Input (pixman, xkb, evdev, DRM) test", CMD_COLOR_CYAN);
    console_println("  fontdemo/plasma - Run FreeType2 Vector Font + Pixman UI demo", CMD_COLOR_CYAN);
    console_println("  qt              - Run Qt 6 linuxfb + evdev GUI demo", CMD_COLOR_CYAN);
    console_println("  qml             - Run Qt Quick/QML software-rendered GUI demo", CMD_COLOR_CYAN);
    console_println("  term            - Launch Terminal", CMD_COLOR_TEXT);
    console_println("  doom            - Launch Doom", CMD_COLOR_TEXT);
    console_println("  clear / cls     - Clear console screen", CMD_COLOR_TEXT);
    console_println("  echo <text>     - Print text to console", CMD_COLOR_TEXT);
    console_println("  reboot          - Reboot computer", CMD_COLOR_ORANGE);
    console_println("  start / shell   - Launch New KDE/QML KeshOS Desktop Shell", CMD_COLOR_GREEN);
    console_println("  desktop / exit  - Launch Graphical Desktop (Classic)", CMD_COLOR_GREEN);
}

static void cmd_do_ls(const char *arg) {
    const char *target = (arg && *arg) ? arg : "/";
    kesh_vfs_entry_t entries[32];
    int count = vfs_list(target, entries, 32);
    if (count < 0) {
        console_print("Error: Directory not found: ", CMD_COLOR_RED);
        console_println(target, CMD_COLOR_TEXT);
        return;
    }
    if (count == 0) {
        console_println("(directory is empty)", CMD_COLOR_DIM);
        return;
    }
    for (int i = 0; i < count; i++) {
        if (entries[i].is_dir) {
            console_print("  [DIR]   ", CMD_COLOR_CYAN);
            console_println(entries[i].name, CMD_COLOR_CYAN);
        } else {
            console_print("  [FILE]  ", CMD_COLOR_DIM);
            console_print(entries[i].name, CMD_COLOR_TEXT);
            console_print("  (", CMD_COLOR_DIM);
            console_print_u64(entries[i].size, CMD_COLOR_DIM);
            console_println(" bytes)", CMD_COLOR_DIM);
        }
    }
}

static void cmd_do_cat(const char *arg) {
    if (!arg || !*arg) {
        console_println("Usage: cat <file_path>", CMD_COLOR_ORANGE);
        return;
    }
    static char file_buf[4096];
    int bytes = vfs_read(arg, file_buf, sizeof(file_buf) - 1);
    if (bytes < 0) {
        console_print("Error: Could not read file: ", CMD_COLOR_RED);
        console_println(arg, CMD_COLOR_TEXT);
        return;
    }
    file_buf[bytes] = '\0';
    console_println(file_buf, CMD_COLOR_TEXT);
}

static void cmd_do_ps(void) {
    kesh_proc_info_t procs[16];
    int count = process_get_table(procs, 16);
    if (count <= 0) {
        console_println("No active userspace processes.", CMD_COLOR_DIM);
        return;
    }
    console_println("PID   STATE     MEMORY(KB)   NAME", CMD_COLOR_CYAN);
    for (int i = 0; i < count; i++) {
        console_print_u64(procs[i].pid, CMD_COLOR_TEXT);
        console_print("     ", CMD_COLOR_DIM);
        const char *st_str = "READY  ";
        if (procs[i].state == 1) st_str = "RUNNING";
        else if (procs[i].state == 2) st_str = "SLEEP  ";
        else if (procs[i].state == 3) st_str = "BLOCKED";
        else if (procs[i].state == 4) st_str = "ZOMBIE ";
        console_print(st_str, CMD_COLOR_GREEN);
        console_print("   ", CMD_COLOR_DIM);
        console_print_u64(procs[i].memory_bytes / 1024, CMD_COLOR_TEXT);
        console_print(" KB        ", CMD_COLOR_DIM);
        console_println(procs[i].name, CMD_COLOR_TEXT);
    }
}

static void cmd_do_run(const char *arg) {
    if (!arg || !*arg) {
        console_println("Usage: run <binary_path>", CMD_COLOR_ORANGE);
        return;
    }
    process_t *p = process_spawn_path(arg);
    if (!p && arg[0] != '/') {
        char full[128] = "/cdrom/boot/apps/";
        int fi = 17;
        for (int i = 0; arg[i] && fi < 127; i++) full[fi++] = arg[i];
        full[fi] = '\0';
        p = process_spawn_path(full);
        if (!p) {
            char full2[128] = "/apps/";
            int fi2 = 6;
            for (int i = 0; arg[i] && fi2 < 127; i++) full2[fi2++] = arg[i];
            full2[fi2] = '\0';
            p = process_spawn_path(full2);
        }
    }

    if (p) {
        console_print("Running '", CMD_COLOR_GREEN);
        console_print(arg, CMD_COLOR_TEXT);
        console_println("'...", CMD_COLOR_GREEN);
        while (process_has_active()) {
            process_step_active();
        }
        console_println("Process finished.", CMD_COLOR_DIM);
    } else {
        console_print("Failed to execute: ", CMD_COLOR_RED);
        console_println(arg, CMD_COLOR_TEXT);
    }
}

static int execute_command(char *cmd_line) {
    /* Skip leading spaces */
    while (*cmd_line == ' ') cmd_line++;
    if (!*cmd_line) return 0;

    /* Extract command and argument */
    char cmd[64];
    int i = 0;
    while (*cmd_line && *cmd_line != ' ' && i < 63) {
        cmd[i++] = *cmd_line++;
    }
    cmd[i] = '\0';

    while (*cmd_line == ' ') cmd_line++;
    char *arg = cmd_line;

    if (k_strcmp(cmd, "help") == 0 || k_strcmp(cmd, "?") == 0) {
        cmd_show_help();
    } else if (k_strcmp(cmd, "neofetch") == 0 || k_strcmp(cmd, "fetch") == 0 ||
               k_strcmp(cmd, "sysinfo") == 0 || k_strcmp(cmd, "info") == 0) {
        cmd_show_neofetch();
    } else if (k_strcmp(cmd, "ls") == 0 || k_strcmp(cmd, "dir") == 0) {
        cmd_do_ls(arg);
    } else if (k_strcmp(cmd, "cat") == 0 || k_strcmp(cmd, "type") == 0) {
        cmd_do_cat(arg);
    } else if (k_strcmp(cmd, "mkdir") == 0) {
        if (!arg || !*arg) {
            console_println("Usage: mkdir <path>", CMD_COLOR_ORANGE);
        } else {
            int r = vfs_mkdir(arg);
            if (r == 0) console_println("Directory created.", CMD_COLOR_GREEN);
            else console_println("Failed to create directory.", CMD_COLOR_RED);
        }
    } else if (k_strcmp(cmd, "rm") == 0) {
        if (!arg || !*arg) {
            console_println("Usage: rm <path>", CMD_COLOR_ORANGE);
        } else {
            int r = vfs_delete(arg);
            if (r == 0) console_println("Deleted.", CMD_COLOR_GREEN);
            else console_println("Failed to delete.", CMD_COLOR_RED);
        }
    } else if (k_strcmp(cmd, "ps") == 0 || k_strcmp(cmd, "tasks") == 0) {
        cmd_do_ps();
    } else if (k_strcmp(cmd, "run") == 0 || k_strcmp(cmd, "exec") == 0) {
        cmd_do_run(arg);
    } else if (k_strcmp(cmd, "term") == 0) {
        cmd_do_run("/apps/term.kea");
    } else if (k_strcmp(cmd, "hello") == 0 || k_strcmp(cmd, "musl") == 0) {
        cmd_do_run("/cdrom/boot/apps/hello.elf");
    } else if (k_strcmp(cmd, "ipc") == 0) {
        cmd_do_run("/cdrom/boot/apps/test_ipc.elf");
    } else if (k_strcmp(cmd, "wayland") == 0 || k_strcmp(cmd, "wl") == 0) {
        cmd_do_run("/cdrom/boot/apps/wayland_demo.elf");
    } else if (k_strcmp(cmd, "compositor") == 0 || k_strcmp(cmd, "wc") == 0) {
        cmd_do_run("/cdrom/boot/apps/wc.elf");
    } else if (k_strcmp(cmd, "gfx") == 0 || k_strcmp(cmd, "input") == 0) {
        cmd_do_run("/cdrom/boot/apps/gfx_demo.elf");
    } else if (k_strcmp(cmd, "fontdemo") == 0 || k_strcmp(cmd, "plasma") == 0) {
        cmd_do_run("/cdrom/boot/apps/font_demo.elf");
    } else if (k_strcmp(cmd, "qt") == 0 || k_strcmp(cmd, "qtdemo") == 0) {
        extern void init_mouse(void);
        extern void mouse_set_bounds(uint32_t width, uint32_t height);
        extern void pic_clear_mask(uint8_t irq_line);
        mouse_set_bounds(g_screen_w, g_screen_h);
        init_mouse();
        pic_clear_mask(2);
        pic_clear_mask(12);
        cmd_do_run("/cdrom/boot/apps/qt_demo.elf");
    } else if (k_strcmp(cmd, "qml") == 0 || k_strcmp(cmd, "qmldemo") == 0) {
        extern void init_mouse(void);
        extern void mouse_set_bounds(uint32_t width, uint32_t height);
        extern void pic_clear_mask(uint8_t irq_line);
        mouse_set_bounds(g_screen_w, g_screen_h);
        init_mouse();
        pic_clear_mask(2);
        pic_clear_mask(12);
        cmd_do_run("/cdrom/boot/apps/qml_demo.elf");
    } else if (k_strcmp(cmd, "start") == 0 || k_strcmp(cmd, "shell") == 0) {
        extern void init_mouse(void);
        extern void mouse_set_bounds(uint32_t width, uint32_t height);
        extern void pic_clear_mask(uint8_t irq_line);
        mouse_set_bounds(g_screen_w, g_screen_h);
        init_mouse();
        pic_clear_mask(2);
        pic_clear_mask(12);
        cmd_do_run("/cdrom/boot/apps/shell.elf");
    } else if (k_strcmp(cmd, "doom") == 0) {
        cmd_do_run("/apps/doom.kea");
    } else if (k_strcmp(cmd, "echo") == 0) {
        console_println(arg, CMD_COLOR_TEXT);
    } else if (k_strcmp(cmd, "clear") == 0 || k_strcmp(cmd, "cls") == 0) {
        console_show_banner();
    } else if (k_strcmp(cmd, "reboot") == 0) {
        console_println("Rebooting...", CMD_COLOR_RED);
        timer_wait_ms(250);
        acpi_reboot();
    } else if (k_strcmp(cmd, "shutdown") == 0 || k_strcmp(cmd, "poweroff") == 0) {
        console_println("Shutting down...", CMD_COLOR_RED);
        timer_wait_ms(250);
        acpi_poweroff();
    } else if (k_strcmp(cmd, "desktop") == 0 || k_strcmp(cmd, "gui") == 0 || k_strcmp(cmd, "exit") == 0) {
        console_println("Starting desktop...", CMD_COLOR_GREEN);
        timer_wait_ms(300);
        return 1; /* Signal to exit CMD mode */
    } else {
        console_print("Unknown command: '", CMD_COLOR_RED);
        console_print(cmd, CMD_COLOR_TEXT);
        console_println("'. Type 'help' for commands.", CMD_COLOR_RED);
    }
    return 0;
}

void kernel_cmd_mode(void) {
    KLOG_INFO("cmd_mode", "entering recovery mode");
    console_show_banner();

    char line[256];
    int line_len = 0;
    int char_widths[256];
    char last_cmd[256] = {0};

    const char *prompt_str = "recovery> ";
    console_print(prompt_str, CMD_COLOR_PROMPT);

    uint64_t last_blink = timer_millis();
    int cursor_state = 1;
    console_draw_cursor(1);

    while (1) {
        /* Cursor blinking */
        uint64_t now = timer_millis();
        if (now - last_blink >= 450) {
            cursor_state = !cursor_state;
            console_draw_cursor(cursor_state);
            last_blink = now;
        }

        kbd_event_t ev;
        if (!keyboard_poll_event(&ev)) {
            timer_wait_ms(10);
            continue;
        }

        if (!ev.pressed) continue;

        /* Enter Key */
        if (ev.scancode == 0x1C) {
            console_draw_cursor(0);
            line[line_len] = '\0';
            console_newline();

            if (line_len > 0) {
                k_strcpy(last_cmd, line);
                int should_exit = execute_command(line);
                if (should_exit) {
                    KLOG_INFO("cmd_mode", "user requested exit to desktop");
                    return;
                }
            }
            line_len = 0;
            console_newline();
            console_print(prompt_str, CMD_COLOR_PROMPT);
            cursor_state = 1;
            console_draw_cursor(1);
            last_blink = timer_millis();
            continue;
        }

        /* Backspace Key */
        if (ev.scancode == 0x0E) {
            if (line_len > 0) {
                console_draw_cursor(0);
                line_len--;
                int w = char_widths[line_len];
                s_cur_x -= w;
                console_fill_rect(s_cur_x, s_cur_y, w + 4, CMD_LINE_H, CMD_BG_COLOR);
                line[line_len] = '\0';
                console_draw_cursor(1);
            }
            continue;
        }

        /* Up arrow: recall last command */
        if (ev.extended && ev.scancode == 0x48) {
            if (last_cmd[0]) {
                console_draw_cursor(0);
                /* Erase current line */
                while (line_len > 0) {
                    line_len--;
                    int w = char_widths[line_len];
                    s_cur_x -= w;
                    console_fill_rect(s_cur_x, s_cur_y, w + 4, CMD_LINE_H, CMD_BG_COLOR);
                }
                /* Print last_cmd */
                for (size_t k = 0; last_cmd[k] && line_len < 250; k++) {
                    char c = last_cmd[k];
                    char tmp[2] = {c, '\0'};
                    int w = font_text_width(tmp);
                    if (w <= 0) w = 9;
                    char_widths[line_len] = w;
                    line[line_len++] = c;
                    draw_string(tmp, s_cur_x, s_cur_y, CMD_COLOR_TEXT, (uint32_t*)g_fb_vram, g_screen_pitch / 4);
                    s_cur_x += w;
                }
                line[line_len] = '\0';
                console_draw_cursor(1);
            }
            continue;
        }

        /* Printable Characters */
        char ch = keyboard_event_to_char(&ev);
        if (ch >= 32 && ch <= 126 && line_len < 250) {
            console_draw_cursor(0);
            char tmp[2] = {ch, '\0'};
            int w = font_text_width(tmp);
            if (w <= 0) w = 9;

            if (s_cur_x + w < (int)g_screen_w - CMD_MARGIN_X) {
                char_widths[line_len] = w;
                line[line_len++] = ch;
                line[line_len] = '\0';
                console_fill_rect(s_cur_x, s_cur_y, w, CMD_LINE_H, CMD_BG_COLOR);
                draw_string(tmp, s_cur_x, s_cur_y, CMD_COLOR_TEXT, (uint32_t*)g_fb_vram, g_screen_pitch / 4);
                s_cur_x += w;
            }
            console_draw_cursor(1);
        }
    }
}
