// userspace/apps/shell/main.c — Полноценный модульный Userspace Desktop Shell для KeshOS
#include "kesh.h"
#include "kesh_ico.h"

#define DEFAULT_SCR_W 1280
#define DEFAULT_SCR_H 800

#define DOCK_H 46
#define NUM_ICONS 8

typedef struct {
    const char *name;
    const char *path;
    const char *icon_path;
    int x;
    int y;
    uint8_t icon_data[10000];
    kesh_ico_image_t icon;
    int icon_ready;
} shell_icon_t;

/* Ярлыки рабочего стола с точными координатами KeshOS */
static shell_icon_t g_icons[NUM_ICONS] = {
    { .name = "Explorer",  .path = "/apps/explorer.kea",  .icon_path = "/icons/explorer.ico",  .x = 28,  .y = 36 },
    { .name = "Notepad",   .path = "/apps/notepad.kea",   .icon_path = "/icons/notepad.ico",   .x = 124, .y = 36 },
    { .name = "Settings",  .path = "/apps/settings.kea",  .icon_path = "/icons/settings.ico",  .x = 220, .y = 36 },
    { .name = "Terminal",  .path = "terminal",            .icon_path = "/icons/terminal.ico",  .x = 28,  .y = 132 },
    { .name = "TaskMgr",   .path = "/apps/taskmgr.kea",   .icon_path = "/icons/taskmgr.ico",   .x = 124, .y = 132 },
    { .name = "Paint",     .path = "/apps/paint.kea",     .icon_path = "/icons/paint.ico",     .x = 220, .y = 132 },
    { .name = "Doom",      .path = "doom",                .icon_path = "/icons/doom.ico",      .x = 28,  .y = 228 },
    { .name = "Install",   .path = "/apps/installer.kea", .icon_path = "/icons/installer.ico", .x = 124, .y = 228 }
};

/* Приложения нижнего дока */
#define NUM_DOCK_APPS 9
typedef struct {
    const char *title;
    const char *path;
    const char *icon_path;
    uint32_t fallback_color;
    uint8_t icon_data[10000];
    kesh_ico_image_t icon;
    int icon_ready;
} dock_item_t;

static dock_item_t g_dock_items[NUM_DOCK_APPS] = {
    { "Files",     "/apps/explorer.kea",  "/icons/explorer.ico",  0x00007AFF },
    { "Terminal",  "terminal",            "/icons/terminal.ico",  0x001C1C1E },
    { "DOOM",      "doom",                "/icons/doom.ico",      0x00B22222 },
    { "Calculator","/apps/notepad.kea",   "/icons/file.ico",      0x00FF9500 },
    { "Settings",  "/apps/settings.kea",  "/icons/settings.ico",  0x008E8E93 },
    { "Music",     "/apps/explorer.kea",  "/icons/picture.ico",   0x00FF2D55 },
    { "Notepad",   "/apps/notepad.kea",   "/icons/notepad.ico",   0x0034C759 },
    { "TaskMgr",   "/apps/taskmgr.kea",   "/icons/taskmgr.ico",   0x00107C41 },
    { "Paint",     "/apps/paint.kea",     "/icons/paint.ico",     0x00AF52DE }
};

static int g_start_open = 0;
static int g_ctx_open = 0;
static int g_ctx_x = 0;
static int g_ctx_y = 0;
static int g_wallpaper_idx = 0;

typedef struct {
    const char *label;
    const char *tag;
    int id;
} shell_ctx_t;

static const shell_ctx_t g_ctx_items[] = {
    { "Open Explorer",    "[DIR]", 1 },
    { "Open Notepad",     "[TXT]", 2 },
    { "Task Manager",     "[TSK]", 3 },
    { "Kesh Paint",       "[ART]", 4 },
    { "Open Terminal",    "[CLI]", 5 },
    { "Next Wallpaper",   "[BG]",  6 },
    { "Settings",         "[CFG]", 7 }
};
#define NUM_CTX 7

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000U | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static uint32_t lerp_color(uint32_t c1, uint32_t c2, int t, int max_t) {
    if (t <= 0) return c1;
    if (t >= max_t) return c2;
    uint32_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint32_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    uint32_t r = r1 + ((r2 - r1) * t) / max_t;
    uint32_t g = g1 + ((g2 - g1) * t) / max_t;
    uint32_t b = b1 + ((b2 - b1) * t) / max_t;
    return rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

/* Процедурная отрисовка обоев в стиле KeshOS */
static void render_wallpaper(uint32_t *fb, int scr_w, int scr_h, int style) {
    if (style == 0) {
        /* KeshOS Desert Sunset — фирменный градиент неба и барханов */
        uint32_t c_top = rgb(135, 168, 205);    /* нежно-голубой */
        uint32_t c_mid1 = rgb(215, 195, 215);   /* лавандовый */
        uint32_t c_mid2 = rgb(248, 202, 190);   /* закатный розовый */
        uint32_t c_horiz = rgb(253, 230, 198);  /* теплый абрикосовый */
        uint32_t c_dune1 = rgb(218, 142, 75);   /* золотистая дюна */
        uint32_t c_dune2 = rgb(178, 86, 42);    /* терракотовая тень */
        uint32_t c_dune3 = rgb(136, 52, 28);    /* глубокий каньон */

        int h_sky = scr_h * 58 / 100;
        for (int y = 0; y < h_sky; y++) {
            uint32_t col;
            if (y < h_sky / 3) {
                col = lerp_color(c_top, c_mid1, y, h_sky / 3);
            } else if (y < h_sky * 2 / 3) {
                col = lerp_color(c_mid1, c_mid2, y - h_sky / 3, h_sky / 3);
            } else {
                col = lerp_color(c_mid2, c_horiz, y - h_sky * 2 / 3, h_sky - h_sky * 2 / 3);
            }
            uint32_t *row = &fb[y * scr_w];
            for (int x = 0; x < scr_w; x++) row[x] = col;
        }

        /* Дюны на переднем плане */
        for (int y = h_sky; y < scr_h; y++) {
            int dy = y - h_sky;
            int total_dune_h = scr_h - h_sky;
            uint32_t *row = &fb[y * scr_w];
            for (int x = 0; x < scr_w; x++) {
                int wave1 = (x * 3) / 4;
                int wave_y1 = h_sky + total_dune_h * 15 / 100 + ((wave1 * 37) % 23);
                int wave2 = (x * 5) / 3;
                int wave_y2 = h_sky + total_dune_h * 45 / 100 + ((wave2 * 19) % 29);

                uint32_t dcol;
                if (y < wave_y1) {
                    dcol = lerp_color(c_horiz, c_dune1, dy, total_dune_h);
                } else if (y < wave_y2) {
                    dcol = lerp_color(c_dune1, c_dune2, y - wave_y1, total_dune_h / 2);
                } else {
                    dcol = lerp_color(c_dune2, c_dune3, y - wave_y2, total_dune_h / 2);
                }
                row[x] = dcol;
            }
        }
    } else if (style == 1) {
        /* KeshOS Deep Night */
        uint32_t c_top = rgb(18, 22, 34);
        uint32_t c_bot = rgb(34, 42, 60);
        for (int y = 0; y < scr_h; y++) {
            uint32_t col = lerp_color(c_top, c_bot, y, scr_h);
            uint32_t *row = &fb[y * scr_w];
            for (int x = 0; x < scr_w; x++) row[x] = col;
        }
    } else if (style == 2) {
        /* Slate Dark */
        kesh_clear(fb, scr_w, scr_h, 0xFF202833);
        for (int y = 0; y < scr_h; y += 48) {
            kesh_draw_rect_alpha(fb, scr_w, 0, y, scr_w, 1, 0xFF354456, 30);
        }
    } else {
        /* Paper Light */
        kesh_clear(fb, scr_w, scr_h, 0xFFE9EDF0);
        for (int y = 0; y < scr_h; y += 48) {
            kesh_draw_rect_alpha(fb, scr_w, 0, y, scr_w, 1, 0xFF9FB2C4, 30);
        }
    }
}

static void int_to_str(int val, char *buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16]; int p = 0;
    while (val > 0) { tmp[p++] = (char)('0' + (val % 10)); val /= 10; }
    int dst = 0;
    while (p > 0) { buf[dst++] = tmp[--p]; }
    buf[dst] = '\0';
}

static uint32_t *g_shell_fb = NULL;

int main(void) {
    kesh_print("[SHELL] Starting Userspace Desktop Shell (Ring 3)...\n");

    kesh_sysinfo_t sysinfo;
    kesh_get_sysinfo(&sysinfo);

    int scr_w = (int)sysinfo.screen_w;
    int scr_h = (int)sysinfo.screen_h;
    if (scr_w <= 0 || scr_w > 1920) scr_w = DEFAULT_SCR_W;
    if (scr_h <= 0 || scr_h > 1200) scr_h = DEFAULT_SCR_H;

    g_shell_fb = kesh_create_window(scr_w, scr_h, "__desktop__");
    if (!g_shell_fb) {
        kesh_print("[SHELL] Failed to create desktop surface window!\n");
        return 1;
    }
    kesh_print("[SHELL] Userspace Desktop Surface active. Rendering desktop...\n");

    /* Загрузка иконок рабочего стола из VFS /icons/*.ico */
    for (int i = 0; i < NUM_ICONS; ++i) {
        g_icons[i].icon_ready = (kesh_ico_load_vfs(g_icons[i].icon_path, g_icons[i].icon_data,
                                                    sizeof(g_icons[i].icon_data), &g_icons[i].icon) == 0);
    }

    /* Загрузка иконок дока из VFS */
    for (int i = 0; i < NUM_DOCK_APPS; ++i) {
        g_dock_items[i].icon_ready = (kesh_ico_load_vfs(g_dock_items[i].icon_path, g_dock_items[i].icon_data,
                                                        sizeof(g_dock_items[i].icon_data), &g_dock_items[i].icon) == 0);
    }

    uint32_t *fb = g_shell_fb;
    int cur_mx = 0, cur_my = 0;
    int tick = 0;
    int last_poll_tick = 0;

    kesh_proc_info_t procs[16];
    int proc_count = 0;

    while (1) {
        tick++;

        if (tick - last_poll_tick > 25 || last_poll_tick == 0) {
            last_poll_tick = tick;
            kesh_get_sysinfo(&sysinfo);
            proc_count = kesh_get_procs(procs, 16);
        }

        /* 1. Отрисовка обоев */
        render_wallpaper(fb, scr_w, scr_h, g_wallpaper_idx);

        /* 2. Отрисовка ярлыков на рабочем столе */
        for (int i = 0; i < NUM_ICONS; i++) {
            shell_icon_t *icon = &g_icons[i];
            int w = 82;
            int h = 84;
            int hover = (cur_mx >= icon->x && cur_mx < icon->x + w &&
                         cur_my >= icon->y && cur_my < icon->y + h);

            if (hover) {
                kesh_draw_rounded_rect_alpha(fb, scr_w, icon->x, icon->y, w, h, 10, 0x00FFFFFF, 40);
            }

            int tx = icon->x + (w - 48) / 2;
            int ty = icon->y + 4;
            kesh_draw_rounded_rect_alpha(fb, scr_w, tx - 2, ty + 2, 48, 48, 10, 0x00000000, 45);

            if (icon->icon_ready) {
                kesh_ico_draw(fb, scr_w, tx, ty, 48, &icon->icon);
            } else {
                kesh_draw_rounded_rect(fb, scr_w, tx, ty, 48, 48, 10, 0xFFE0E0E6);
            }

            int name_len = 0;
            while (icon->name[name_len]) name_len++;
            int name_x = icon->x + (w - name_len * 8) / 2;
            draw_string(icon->name, name_x + 1, icon->y + 61, 0x00000000, fb, scr_w);
            draw_string(icon->name, name_x, icon->y + 60, 0x00FFFFFF, fb, scr_w);
        }

        /* 3. Отрисовка нижнего дока (Taskbar / Dock) */
        int dock_w = scr_w - 24;
        int dock_x = 12;
        int dock_y = scr_h - DOCK_H - 10;

        /* Фон дока — glassmorphism */
        kesh_draw_rounded_rect_alpha(fb, scr_w, dock_x, dock_y + 2, dock_w, DOCK_H, 13, 0x00000000, 70);
        kesh_draw_rounded_rect_alpha(fb, scr_w, dock_x, dock_y, dock_w, DOCK_H, 13, 0x0020222B, 215);
        kesh_draw_rounded_rect_alpha(fb, scr_w, dock_x, dock_y, dock_w, 20, 13, 0x00FFFFFF, 20);

        /* Кнопка KeshOS (Пуск) */
        int btn_x = dock_x + 8;
        int btn_y = dock_y + 6;
        int btn_w = 42;
        int btn_h = 34;
        int btn_hover = (cur_mx >= btn_x && cur_mx < btn_x + btn_w &&
                         cur_my >= btn_y && cur_my < btn_y + btn_h);

        uint32_t btn_bg = g_start_open ? 0x00007AFF : (btn_hover ? 0x003B4252 : 0x00282C37);
        kesh_draw_rounded_rect_alpha(fb, scr_w, btn_x, btn_y, btn_w, btn_h, 9, btn_bg, 230);
        kesh_draw_rounded_rect_alpha(fb, scr_w, btn_x - 1, btn_y - 1, btn_w + 2, btn_h + 2, 9, 0x005AC8FA, 70);
        draw_string("K", btn_x + 17, btn_y + 9, 0x00FFFFFF, fb, scr_w);

        /* Иконки приложений в доке */
        int item_start_x = btn_x + btn_w + 14;
        int item_w = 40;
        int item_h = 36;
        int item_y = dock_y + 5;

        for (int d = 0; d < NUM_DOCK_APPS; d++) {
            int ix = item_start_x + d * (item_w + 6);
            int hover = (cur_mx >= ix && cur_mx < ix + item_w &&
                         cur_my >= item_y && cur_my < item_y + item_h);

            if (hover) {
                kesh_draw_rounded_rect_alpha(fb, scr_w, ix, item_y, item_w, item_h, 8, 0x00FFFFFF, 40);
            }

            int icon_draw_x = ix + (item_w - 28) / 2;
            int icon_draw_y = item_y + (item_h - 28) / 2 - 1;

            if (g_dock_items[d].icon_ready) {
                kesh_ico_draw(fb, scr_w, icon_draw_x, icon_draw_y, 28, &g_dock_items[d].icon);
            } else {
                kesh_draw_rounded_rect(fb, scr_w, icon_draw_x, icon_draw_y, 28, 28, 6, g_dock_items[d].fallback_color);
            }

            /* Проверка запущен ли процесс приложения */
            int is_running = 0;
            for (int p = 0; p < proc_count; p++) {
                if (procs[p].state > 0 && procs[p].name[0]) {
                    if (g_dock_items[d].title[0] == procs[p].name[0] &&
                        g_dock_items[d].title[1] == procs[p].name[1]) {
                        is_running = 1;
                        break;
                    }
                }
            }
            if (is_running) {
                int dot_w = 12;
                int dot_x = ix + (item_w - dot_w) / 2;
                kesh_draw_rounded_rect(fb, scr_w, dot_x, dock_y + DOCK_H - 4, dot_w, 2, 1, 0x00007AFF);
            }
        }

        /* Системный трей справа: RAM, звук, время и дата */
        int tray_w = 210;
        int tray_x = dock_x + dock_w - tray_w - 8;

        /* RAM status */
        uint32_t total_mb = (uint32_t)(sysinfo.total_ram_bytes / (1024 * 1024));
        uint32_t free_mb = (uint32_t)(sysinfo.free_ram_bytes / (1024 * 1024));
        uint32_t used_mb = (total_mb >= free_mb) ? (total_mb - free_mb) : 0;
        if (total_mb == 0) { total_mb = 256; used_mb = 42; }

        kesh_draw_rounded_rect_alpha(fb, scr_w, tray_x, dock_y + 9, 62, 28, 6, 0x001C1E26, 180);
        char ram_str[16];
        int_to_str((int)used_mb, ram_str);
        draw_string(ram_str, tray_x + 6, dock_y + 15, 0x005AC8FA, fb, scr_w);
        draw_string("M", tray_x + 38, dock_y + 15, 0x00A0A5B4, fb, scr_w);

        /* Индикатор звука 70% */
        kesh_draw_rounded_rect_alpha(fb, scr_w, tray_x + 68, dock_y + 9, 44, 28, 6, 0x001C1E26, 180);
        draw_string("70%", tray_x + 74, dock_y + 15, 0x00D0D8E8, fb, scr_w);

        /* Цифровые часы и дата */
        int clock_x = tray_x + 118;
        kesh_draw_rounded_rect_alpha(fb, scr_w, clock_x, dock_y + 7, 86, 32, 6, 0x001C1E26, 200);

        uint64_t total_sec = sysinfo.uptime_ms / 1000;
        int sec = (int)(total_sec % 60);
        int min = (int)((total_sec / 60) % 60);
        int hr  = (int)((total_sec / 3600) % 24);

        char time_str[10];
        time_str[0] = (char)('0' + (hr / 10));
        time_str[1] = (char)('0' + (hr % 10));
        time_str[2] = ':';
        time_str[3] = (char)('0' + (min / 10));
        time_str[4] = (char)('0' + (min % 10));
        time_str[5] = ':';
        time_str[6] = (char)('0' + (sec / 10));
        time_str[7] = (char)('0' + (sec % 10));
        time_str[8] = '\0';
        draw_string(time_str, clock_x + 11, dock_y + 10, 0x00FFFFFF, fb, scr_w);
        draw_string("03.10.26", clock_x + 11, dock_y + 22, 0x00A0A5B4, fb, scr_w);

        /* 4. Всплывающее меню Start (Пуск) */
        if (g_start_open) {
            int sm_w = 270;
            int sm_h = 310;
            int sm_x = dock_x + 8;
            int sm_y = dock_y - sm_h - 10;

            kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x - 3, sm_y + 3, sm_w + 6, sm_h + 6, 13, 0x00000000, 80);
            kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x, sm_y, sm_w, sm_h, 12, 0x001C1E26, 248);
            kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x, sm_y, sm_w, 38, 12, 0x00007AFF, 80);

            draw_string("KESHOS MENU", sm_x + 16, sm_y + 12, 0x00FFFFFF, fb, scr_w);
            kesh_draw_rect(fb, scr_w, sm_x, sm_y + 38, sm_w, 1, 0x00353A47);

            const char *menu_names[] = {
                "File Explorer",
                "Text Editor (Notepad)",
                "System Settings",
                "Terminal Console",
                "Task Manager",
                "Kesh Paint",
                "DOOM Game",
                "Next Wallpaper"
            };

            for (int m = 0; m < 8; m++) {
                int iy = sm_y + 44 + m * 28;
                int hover = (cur_mx >= sm_x + 6 && cur_mx < sm_x + sm_w - 6 &&
                             cur_my >= iy && cur_my < iy + 25);
                if (hover) {
                    kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x + 6, iy, sm_w - 12, 25, 6, 0x00007AFF, 120);
                }
                draw_string(menu_names[m], sm_x + 16, iy + 5, hover ? 0x00FFFFFF : 0x00D0D8E8, fb, scr_w);
            }

            /* Кнопки Power внизу меню Пуск */
            int pw_y = sm_y + sm_h - 34;
            kesh_draw_rect(fb, scr_w, sm_x, pw_y - 4, sm_w, 1, 0x00353A47);

            int rb_hover = (cur_mx >= sm_x + 12 && cur_mx < sm_x + 120 && cur_my >= pw_y && cur_my < pw_y + 26);
            if (rb_hover) kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x + 10, pw_y, 115, 26, 5, 0x00FF9500, 100);
            draw_string("Restart", sm_x + 22, pw_y + 6, 0x00FF9500, fb, scr_w);

            int sd_hover = (cur_mx >= sm_x + 140 && cur_mx < sm_x + 255 && cur_my >= pw_y && cur_my < pw_y + 26);
            if (sd_hover) kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x + 138, pw_y, 120, 26, 5, 0x00FF3B30, 100);
            draw_string("Shut Down", sm_x + 152, pw_y + 6, 0x00FF3B30, fb, scr_w);
        }

        /* 5. Контекстное меню (ПКМ) */
        if (g_ctx_open) {
            int cm_w = 180;
            int cm_h = NUM_CTX * 26 + 12;
            kesh_draw_rounded_rect_alpha(fb, scr_w, g_ctx_x - 2, g_ctx_y + 2, cm_w + 4, cm_h + 4, 10, 0x00000000, 75);
            kesh_draw_rounded_rect_alpha(fb, scr_w, g_ctx_x, g_ctx_y, cm_w, cm_h, 9, 0x001C1E26, 245);

            for (int c = 0; c < NUM_CTX; c++) {
                int iy = g_ctx_y + 6 + c * 26;
                int hover = (cur_mx >= g_ctx_x + 4 && cur_mx < g_ctx_x + cm_w - 4 &&
                             cur_my >= iy && cur_my < iy + 24);
                if (hover) {
                    kesh_draw_rounded_rect_alpha(fb, scr_w, g_ctx_x + 4, iy, cm_w - 8, 24, 6, 0x00007AFF, 120);
                }
                draw_string(g_ctx_items[c].tag, g_ctx_x + 8, iy + 5, 0x005AC8FA, fb, scr_w);
                draw_string(g_ctx_items[c].label, g_ctx_x + 50, iy + 5, hover ? 0x00FFFFFF : 0x00D0D8E8, fb, scr_w);
            }
        }

        /* Уведомить ядро о завершении кадра десктопа */
        kesh_update_window(0);

        /* 6. Обработка событий ввода мыши от ядра */
        kesh_event_t ev;
        while (kesh_poll_event(0, &ev)) {
            if (ev.type == EVENT_MOUSE_MOVE) {
                cur_mx = ev.mx;
                cur_my = ev.my;
            } else if (ev.type == EVENT_MOUSE_DOWN) {
                cur_mx = ev.mx;
                cur_my = ev.my;

                /* Правая кнопка мыши — контекстное меню */
                if (ev.btn == 2) {
                    g_ctx_open = 1;
                    g_ctx_x = ev.mx;
                    g_ctx_y = ev.my;
                    if (g_ctx_x + 190 > scr_w) g_ctx_x = scr_w - 195;
                    if (g_ctx_y + 210 > scr_h) g_ctx_y = scr_h - 215;
                    g_start_open = 0;
                    continue;
                }

                /* Левая кнопка мыши */
                if (ev.btn == 1) {
                    /* Клик по контекстному меню */
                    if (g_ctx_open) {
                        int cm_w = 180;
                        int cm_h = NUM_CTX * 26 + 12;
                        if (ev.mx >= g_ctx_x && ev.mx <= g_ctx_x + cm_w &&
                            ev.my >= g_ctx_y && ev.my <= g_ctx_y + cm_h) {
                            int clicked = (ev.my - (g_ctx_y + 6)) / 26;
                            if (clicked >= 0 && clicked < NUM_CTX) {
                                int act = g_ctx_items[clicked].id;
                                if (act == 1) kesh_exec("/apps/explorer.kea");
                                else if (act == 2) kesh_exec("/apps/notepad.kea");
                                else if (act == 3) kesh_exec("/apps/taskmgr.kea");
                                else if (act == 4) kesh_exec("/apps/paint.kea");
                                else if (act == 5) kesh_exec("terminal");
                                else if (act == 6) g_wallpaper_idx = (g_wallpaper_idx + 1) % 4;
                                else if (act == 7) kesh_exec("/apps/settings.kea");
                            }
                        }
                        g_ctx_open = 0;
                        continue;
                    }

                    /* Клик по меню Start */
                    if (g_start_open) {
                        int sm_w = 270;
                        int sm_h = 310;
                        int sm_x = dock_x + 8;
                        int sm_y = dock_y - sm_h - 10;
                        if (ev.mx >= sm_x && ev.mx <= sm_x + sm_w &&
                            ev.my >= sm_y && ev.my <= sm_y + sm_h) {
                            int pw_y = sm_y + sm_h - 34;
                            if (ev.my >= pw_y && ev.my < pw_y + 26) {
                                if (ev.mx < sm_x + 130) {
                                    kesh_exec("reboot");
                                } else {
                                    kesh_exec("shutdown");
                                }
                                g_start_open = 0;
                                continue;
                            }
                            int item = (ev.my - (sm_y + 44)) / 28;
                            if (item == 0) kesh_exec("/apps/explorer.kea");
                            else if (item == 1) kesh_exec("/apps/notepad.kea");
                            else if (item == 2) kesh_exec("/apps/settings.kea");
                            else if (item == 3) kesh_exec("terminal");
                            else if (item == 4) kesh_exec("/apps/taskmgr.kea");
                            else if (item == 5) kesh_exec("/apps/paint.kea");
                            else if (item == 6) kesh_exec("doom");
                            else if (item == 7) g_wallpaper_idx = (g_wallpaper_idx + 1) % 4;
                            g_start_open = 0;
                            continue;
                        }
                        g_start_open = 0;
                    }

                    /* Клик по кнопке Start */
                    if (ev.mx >= btn_x && ev.mx <= btn_x + btn_w &&
                        ev.my >= btn_y && ev.my <= btn_y + btn_h) {
                        g_start_open = !g_start_open;
                        continue;
                    }

                    /* Клик по иконкам дока */
                    int clicked_dock = 0;
                    for (int d = 0; d < NUM_DOCK_APPS; d++) {
                        int ix = item_start_x + d * (item_w + 6);
                        if (ev.mx >= ix && ev.mx < ix + item_w &&
                            ev.my >= item_y && ev.my < item_y + item_h) {
                            kesh_exec(g_dock_items[d].path);
                            clicked_dock = 1;
                            break;
                        }
                    }
                    if (clicked_dock) continue;

                    /* Клик по ярлыкам на рабочем столе */
                    for (int i = 0; i < NUM_ICONS; i++) {
                        shell_icon_t *icon = &g_icons[i];
                        int iw = 82;
                        int ih = 84;
                        if (ev.mx >= icon->x && ev.mx < icon->x + iw &&
                            ev.my >= icon->y && ev.my < icon->y + ih) {
                            kesh_print("[SHELL] Spawning desktop app: ");
                            kesh_print(icon->path);
                            kesh_print("\n");
                            kesh_exec(icon->path);
                            break;
                        }
                    }
                }
            }
        }

        /* 60 FPS пауза / yield */
        kesh_sleep(16);
    }

    return 0;
}
