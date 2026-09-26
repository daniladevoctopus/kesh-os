// командный шелл
#include "kesh.h"

#define DEFAULT_SCR_W 1280
#define DEFAULT_SCR_H 800

#define TASKBAR_H 48
#define DOCK_H    42

typedef struct {
    const char *name;
    const char *tag;
    const char *path;
    uint32_t color;
    int x;
    int y;
} shell_icon_t;

static shell_icon_t g_icons[] = {
    { "Explorer", "[DIR]", "/apps/explorer.kea", 0xFF24527A, 28, 36 },
    { "Notepad",  "[TXT]", "/apps/notepad.kea",  0xFF2B3A4D, 28, 128 },
    { "TaskMgr",  "[TSK]", "/apps/taskmgr.kea",  0xFF1E3A2F, 28, 220 },
    { "Paint",    "[ART]", "/apps/paint.kea",    0xFF5E2B5A, 28, 312 },
    { "Terminal", "[CLI]", "terminal",           0xFF1B2028, 28, 404 },
    { "Doom",     "[WAD]", "doom",               0xFF7A2020, 118, 36 }
};
#define NUM_ICONS 6

static int g_start_open = 0;

static int g_ctx_open = 0;
static int g_ctx_x = 0;
static int g_ctx_y = 0;

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
    { "Refresh Shell",    "[REF]", 7 }
};
#define NUM_CTX 7

static int g_wallpaper_idx = 0;

static void int_to_str(int val, char *buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[16];
    int p = 0;
    while (val > 0) {
        tmp[p++] = '0' + (val % 10);
        val /= 10;
    }
    int dst = 0;
    while (p > 0) {
        buf[dst++] = tmp[--p];
    }
    buf[dst] = '\0';
}

static void draw_wallpaper(uint32_t *fb, int w, int h) {
    if (g_wallpaper_idx == 0) {

        for (int y = 0; y < h; y++) {
            uint32_t r = 12 + (y * 18) / h;
            uint32_t g = 14 + (y * 22) / h;
            uint32_t b = 28 + (y * 50) / h;
            uint32_t col = 0xFF000000 | (r << 16) | (g << 8) | b;
            for (int x = 0; x < w; x++) {
                fb[y * w + x] = col;
            }
        }

        for (int y = 0; y < h; y += 40) {
            kesh_draw_rect_alpha(fb, w, 0, y, w, 1, 0xFF64D2FF, 18);
        }
        for (int x = 0; x < w; x += 40) {
            kesh_draw_rect_alpha(fb, w, x, 0, 1, h, 0xFF64D2FF, 18);
        }
    } else if (g_wallpaper_idx == 1) {

        for (int y = 0; y < h; y++) {
            uint32_t val = 18 + (y * 12) / h;
            uint32_t col = 0xFF000000 | (val << 16) | (val << 8) | (val + 4);
            for (int x = 0; x < w; x++) {
                fb[y * w + x] = col;
            }
        }
    } else if (g_wallpaper_idx == 2) {

        for (int y = 0; y < h; y++) {
            uint32_t r = 24 + (y * 30) / h;
            uint32_t g = 10 + (y * 15) / h;
            uint32_t b = 38 + (y * 35) / h;
            uint32_t col = 0xFF000000 | (r << 16) | (g << 8) | b;
            for (int x = 0; x < w; x++) {
                fb[y * w + x] = col;
            }
        }
    } else {

        kesh_clear(fb, w, h, 0xFF141519);
    }
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
    kesh_print("[SHELL] Userspace Desktop Surface created successfully!\n");

    uint32_t *fb = g_shell_fb;
    kesh_proc_info_t procs[16];
    int proc_count = 0;
    int tick = 0;
    int last_proc_poll = 0;

    int cur_mx = 0;
    int cur_my = 0;

    while (1) {
        tick++;

        if (tick - last_proc_poll > 20 || last_proc_poll == 0) {
            last_proc_poll = tick;
            kesh_get_sysinfo(&sysinfo);
            proc_count = kesh_get_procs(procs, 16);
        }

        draw_wallpaper(fb, scr_w, scr_h);

        kesh_draw_rounded_rect_alpha(fb, scr_w, 24, 14, 210, 22, 6, 0xFF000000, 110);
        draw_string("KeshOS 1.0", 34, 19, 0xFF30D158, fb, scr_w);
        draw_string("Ring 3 Shell", 110, 19, 0xFF8E8E93, fb, scr_w);

        for (int i = 0; i < NUM_ICONS; i++) {
            shell_icon_t *icon = &g_icons[i];
            int iw = 76;
            int ih = 82;
            int hover = (cur_mx >= icon->x && cur_mx < icon->x + iw &&
                         cur_my >= icon->y && cur_my < icon->y + ih);

            if (hover) {
                kesh_draw_rounded_rect_alpha(fb, scr_w, icon->x, icon->y, iw, ih, 10, 0xFFFFFFFF, 40);
            }

            int tx = icon->x + (iw - 44) / 2;
            int ty = icon->y + 6;
            kesh_draw_rounded_rect_alpha(fb, scr_w, tx - 2, ty + 2, 48, 48, 10, 0xFF000000, 60);
            kesh_draw_rounded_rect(fb, scr_w, tx, ty, 44, 44, 10, icon->color);
            kesh_draw_rounded_rect_alpha(fb, scr_w, tx, ty, 44, 20, 10, 0xFFFFFFFF, 40);

            int tag_len = 0;
            while (icon->tag[tag_len]) tag_len++;
            int tag_x = tx + (44 - tag_len * 8) / 2;
            draw_string(icon->tag, tag_x, ty + 16, 0xFFFFFFFF, fb, scr_w);

            int name_len = 0;
            while (icon->name[name_len]) name_len++;
            int name_x = icon->x + (iw - name_len * 8) / 2;
            draw_string(icon->name, name_x + 1, icon->y + 61, 0xFF000000, fb, scr_w);
            draw_string(icon->name, name_x, icon->y + 60, 0xFFFFFFFF, fb, scr_w);
        }

        int dock_w = scr_w - 32;
        int dock_x = 16;
        int dock_y = scr_h - DOCK_H - 12;

        kesh_draw_rounded_rect_alpha(fb, scr_w, dock_x, dock_y + 2, dock_w, DOCK_H, 12, 0xFF000000, 70);
        kesh_draw_rounded_rect_alpha(fb, scr_w, dock_x, dock_y, dock_w, DOCK_H, 12, 0xFF1B1D24, 235);
        kesh_draw_rounded_rect_alpha(fb, scr_w, dock_x, dock_y, dock_w, 18, 12, 0xFFFFFFFF, 20);

        int btn_x = dock_x + 8;
        int btn_y = dock_y + 6;
        int btn_w = 40;
        int btn_h = 30;
        uint32_t start_col = g_start_open ? 0xFF0A84FF : 0xFF2C2F3A;
        kesh_draw_rounded_rect(fb, scr_w, btn_x, btn_y, btn_w, btn_h, 8, start_col);
        kesh_draw_rounded_rect_alpha(fb, scr_w, btn_x - 1, btn_y - 1, btn_w + 2, btn_h + 2, 8, 0xFF64D2FF, 120);
        draw_string("K", btn_x + 16, btn_y + 8, 0xFFFFFFFF, fb, scr_w);

        int tab_x = btn_x + btn_w + 14;
        for (int p = 0; p < proc_count && p < 6; p++) {
            if (procs[p].state == 0) continue;
            int tw = 100;
            int th = 28;
            int ty = dock_y + 7;
            kesh_draw_rounded_rect_alpha(fb, scr_w, tab_x, ty, tw, th, 6, 0xFF282B36, 210);

            uint32_t dot_col = (procs[p].state == 2) ? 0xFF30D158 : 0xFF64D2FF;
            kesh_draw_rounded_rect(fb, scr_w, tab_x + 8, ty + 10, 6, 6, 3, dot_col);

            char name_buf[12];
            int ni = 0;
            while (procs[p].name[ni] && ni < 9) {
                name_buf[ni] = procs[p].name[ni];
                ni++;
            }
            name_buf[ni] = '\0';
            draw_string(name_buf, tab_x + 20, ty + 7, 0xFFE5E5EA, fb, scr_w);

            tab_x += tw + 8;
        }

        int tray_x = dock_x + dock_w - 200;

        uint32_t total_mb = (uint32_t)(sysinfo.total_ram_bytes / (1024 * 1024));
        uint32_t used_mb = (uint32_t)((sysinfo.total_ram_bytes - sysinfo.free_ram_bytes) / (1024 * 1024));
        if (total_mb == 0) { total_mb = 256; used_mb = 42; }

        kesh_draw_rounded_rect_alpha(fb, scr_w, tray_x, dock_y + 8, 70, 26, 6, 0xFF252732, 220);
        char u_str[16];
        int_to_str((int)used_mb, u_str);
        draw_string(u_str, tray_x + 8, dock_y + 13, 0xFF0A84FF, fb, scr_w);
        draw_string("M", tray_x + 28, dock_y + 13, 0xFF8E8E93, fb, scr_w);

        int clock_x = tray_x + 80;
        kesh_draw_rounded_rect_alpha(fb, scr_w, clock_x, dock_y + 8, 105, 26, 6, 0xFF252732, 220);

        uint64_t total_sec = sysinfo.uptime_ms / 1000;
        int sec = (int)(total_sec % 60);
        int min = (int)((total_sec / 60) % 60);
        int hr  = (int)((total_sec / 3600) % 24);

        char time_str[16];
        time_str[0] = '0' + (hr / 10);
        time_str[1] = '0' + (hr % 10);
        time_str[2] = ':';
        time_str[3] = '0' + (min / 10);
        time_str[4] = '0' + (min % 10);
        time_str[5] = ':';
        time_str[6] = '0' + (sec / 10);
        time_str[7] = '0' + (sec % 10);
        time_str[8] = '\0';
        draw_string(time_str, clock_x + 16, dock_y + 13, 0xFFFFFFFF, fb, scr_w);

        if (g_start_open) {
            int sm_w = 260;
            int sm_h = 240;
            int sm_x = dock_x + 8;
            int sm_y = dock_y - sm_h - 10;

            kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x - 4, sm_y + 4, sm_w + 8, sm_h + 8, 12, 0xFF000000, 80);

            kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x, sm_y, sm_w, sm_h, 10, 0xFF1C1E26, 245);
            kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x, sm_y, sm_w, 36, 10, 0xFFFFFFFF, 25);

            draw_string("KESHOS MENU", sm_x + 14, sm_y + 11, 0xFF64D2FF, fb, scr_w);
            kesh_draw_rect(fb, scr_w, sm_x, sm_y + 36, sm_w, 1, 0xFF2F323F);

            const char *menu_names[] = {
                "File Explorer",
                "Text Editor (Notepad)",
                "Task Manager",
                "Kesh Paint",
                "Terminal Console",
                "Next Wallpaper"
            };
            const char *menu_tags[] = { "[DIR]", "[TXT]", "[TSK]", "[ART]", "[CLI]", "[BG]" };

            for (int m = 0; m < 6; m++) {
                int iy = sm_y + 42 + m * 30;
                int hover = (cur_mx >= sm_x + 6 && cur_mx < sm_x + sm_w - 6 &&
                             cur_my >= iy && cur_my < iy + 26);
                if (hover) {
                    kesh_draw_rounded_rect_alpha(fb, scr_w, sm_x + 6, iy, sm_w - 12, 26, 6, 0xFF0A84FF, 180);
                }
                draw_string(menu_tags[m], sm_x + 14, iy + 6, 0xFF30D158, fb, scr_w);
                draw_string(menu_names[m], sm_x + 55, iy + 6, 0xFFFFFFFF, fb, scr_w);
            }
        }

        if (g_ctx_open) {
            int cm_w = 170;
            int cm_h = NUM_CTX * 26 + 12;

            kesh_draw_rounded_rect_alpha(fb, scr_w, g_ctx_x - 3, g_ctx_y + 3, cm_w + 6, cm_h + 6, 10, 0xFF000000, 80);
            kesh_draw_rounded_rect_alpha(fb, scr_w, g_ctx_x, g_ctx_y, cm_w, cm_h, 8, 0xFF191B22, 245);
            kesh_draw_rounded_rect_alpha(fb, scr_w, g_ctx_x, g_ctx_y, cm_w, cm_h, 8, 0xFFFFFFFF, 20);

            for (int c = 0; c < NUM_CTX; c++) {
                int iy = g_ctx_y + 6 + c * 26;
                int hover = (cur_mx >= g_ctx_x + 4 && cur_mx < g_ctx_x + cm_w - 4 &&
                             cur_my >= iy && cur_my < iy + 24);
                if (hover) {
                    kesh_draw_rounded_rect_alpha(fb, scr_w, g_ctx_x + 4, iy, cm_w - 8, 24, 6, 0xFF3A4D6B, 200);
                }
                draw_string(g_ctx_items[c].tag, g_ctx_x + 8, iy + 6, 0xFF58A6FF, fb, scr_w);
                draw_string(g_ctx_items[c].label, g_ctx_x + 46, iy + 6, 0xFFE5E5EA, fb, scr_w);
            }
        }

        kesh_update_window(0);

        kesh_event_t ev;
        while (kesh_poll_event(0, &ev)) {
            if (ev.type == EVENT_MOUSE_MOVE) {
                cur_mx = ev.mx;
                cur_my = ev.my;
            } else if (ev.type == EVENT_MOUSE_DOWN) {
                cur_mx = ev.mx;
                cur_my = ev.my;

                if (ev.btn == 2) {
                    g_ctx_open = 1;
                    g_ctx_x = ev.mx;
                    g_ctx_y = ev.my;
                    if (g_ctx_x + 180 > scr_w) g_ctx_x = scr_w - 185;
                    if (g_ctx_y + 200 > scr_h) g_ctx_y = scr_h - 205;
                    g_start_open = 0;
                    continue;
                }

                if (ev.btn == 1) {

                    if (g_ctx_open) {
                        int cm_w = 170;
                        int cm_h = NUM_CTX * 26 + 12;
                        if (ev.mx >= g_ctx_x && ev.mx <= g_ctx_x + cm_w &&
                            ev.my >= g_ctx_y && ev.my <= g_ctx_y + cm_h) {
                            int clicked_idx = (ev.my - (g_ctx_y + 6)) / 26;
                            if (clicked_idx >= 0 && clicked_idx < NUM_CTX) {
                                int act = g_ctx_items[clicked_idx].id;
                                if (act == 1) kesh_exec("/apps/explorer.kea");
                                else if (act == 2) kesh_exec("/apps/notepad.kea");
                                else if (act == 3) kesh_exec("/apps/taskmgr.kea");
                                else if (act == 4) kesh_exec("/apps/paint.kea");
                                else if (act == 5) kesh_exec("terminal");
                                else if (act == 6) g_wallpaper_idx = (g_wallpaper_idx + 1) % 4;
                            }
                        }
                        g_ctx_open = 0;
                        continue;
                    }

                    if (g_start_open) {
                        int sm_w = 260;
                        int sm_h = 240;
                        int sm_x = dock_x + 8;
                        int sm_y = dock_y - sm_h - 10;
                        if (ev.mx >= sm_x && ev.mx <= sm_x + sm_w &&
                            ev.my >= sm_y && ev.my <= sm_y + sm_h) {
                            int item = (ev.my - (sm_y + 42)) / 30;
                            if (item == 0) kesh_exec("/apps/explorer.kea");
                            else if (item == 1) kesh_exec("/apps/notepad.kea");
                            else if (item == 2) kesh_exec("/apps/taskmgr.kea");
                            else if (item == 3) kesh_exec("/apps/paint.kea");
                            else if (item == 4) kesh_exec("terminal");
                            else if (item == 5) g_wallpaper_idx = (g_wallpaper_idx + 1) % 4;
                            g_start_open = 0;
                            continue;
                        }
                        g_start_open = 0;
                    }

                    if (ev.mx >= btn_x && ev.mx <= btn_x + btn_w &&
                        ev.my >= btn_y && ev.my <= btn_y + btn_h) {
                        g_start_open = !g_start_open;
                        continue;
                    }

                    for (int i = 0; i < NUM_ICONS; i++) {
                        shell_icon_t *icon = &g_icons[i];
                        int iw = 76;
                        int ih = 82;
                        if (ev.mx >= icon->x && ev.mx < icon->x + iw &&
                            ev.my >= icon->y && ev.my < icon->y + ih) {
                            kesh_print("[SHELL] Spawning selected desktop application: ");
                            kesh_print(icon->path);
                            kesh_print("\n");
                            kesh_exec(icon->path);
                            break;
                        }
                    }
                }
            }
        }

        kesh_yield();
    }

    return 0;
}
