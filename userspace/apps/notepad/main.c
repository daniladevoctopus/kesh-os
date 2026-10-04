// блокнот для заметок
#include "kesh.h"
#include "kesh_ico.h"

#define WIN_W 500
#define WIN_H 320
#define MAX_TEXT 4096
#define SAVE_PATH "/hdd/NOTEPAD.TXT"
#define FALLBACK_SAVE_PATH "/NOTEPAD.TXT"

static char g_text[MAX_TEXT];
static int g_text_len = 0;
static char g_status_msg[64] = "Ready | Ctrl+S to save";
static uint32_t g_status_color = 0xFF8E8E93;
static int g_status_timer = 0;

static int save_text(void) {
    int r = kesh_vfs_write(SAVE_PATH, g_text, g_text_len);
    if (r < 0) {
        r = kesh_vfs_write(FALLBACK_SAVE_PATH, g_text, g_text_len);
    }
    if (r >= 0) {
        int idx = 0;
        const char *m = "Saved: NOTEPAD.TXT";
        while (m[idx]) { g_status_msg[idx] = m[idx]; idx++; }
        g_status_msg[idx] = '\0';
        g_status_color = 0xFF30D158;
        g_status_timer = 120;
        return 0;
    }
    int idx = 0;
    const char *m = "Save failed";
    while (m[idx]) { g_status_msg[idx] = m[idx]; idx++; }
    g_status_msg[idx] = '\0';
    g_status_color = 0xFFFF453A;
    g_status_timer = 120;
    return -1;
}

static void load_initial_text(void) {
    int r = kesh_vfs_read(SAVE_PATH, g_text, MAX_TEXT - 1);
    if (r <= 0) {
        r = kesh_vfs_read(FALLBACK_SAVE_PATH, g_text, MAX_TEXT - 1);
    }
    if (r > 0) {
        g_text_len = r;
        g_text[g_text_len] = '\0';
    } else {
        const char *init_msg = "Welcome to Notepad.\nStart typing here...";
        int i = 0;
        while (init_msg[i] && i < MAX_TEXT - 1) {
            g_text[i] = init_msg[i];
            i++;
        }
        g_text_len = i;
        g_text[g_text_len] = '\0';
    }
}

int main(void) {
    uint32_t *fb = kesh_create_window(WIN_W, WIN_H, "Notepad");
    if (!fb) return 1;

    static uint8_t icon_data[10000];
    kesh_ico_image_t icon;
    int icon_ready = kesh_ico_load_vfs("/icons/notepad.ico", icon_data, sizeof(icon_data), &icon) == 0;

    load_initial_text();

    int cursor_tick = 0;
    int save_btn_x = WIN_W - 146;
    int save_btn_y = 5;
    int save_btn_w = 66;
    int save_btn_h = 22;

    int clear_btn_x = WIN_W - 74;
    int clear_btn_y = 5;
    int clear_btn_w = 66;
    int clear_btn_h = 22;

    while (1) {
        kesh_clear(fb, WIN_W, WIN_H, 0xFF1C1D22);

        kesh_draw_rect(fb, WIN_W, 0, 0, WIN_W, 32, 0xFF24262E);
        kesh_draw_rect(fb, WIN_W, 0, 32, WIN_W, 1, 0xFF323540);

        if (icon_ready) kesh_ico_draw(fb, WIN_W, 10, 6, 20, &icon);
        else kesh_draw_rounded_rect(fb, WIN_W, 12, 8, 16, 16, 4, 0xFF9CA7B4);
        draw_string("Notepad", 34, 10, 0xFFFFFFFF, fb, WIN_W);

        kesh_draw_rounded_rect(fb, WIN_W, save_btn_x, save_btn_y, save_btn_w, save_btn_h, 4, 0xFF0A84FF);
        draw_string("Save", save_btn_x + 18, save_btn_y + 4, 0xFFFFFFFF, fb, WIN_W);

        kesh_draw_rounded_rect(fb, WIN_W, clear_btn_x, clear_btn_y, clear_btn_w, clear_btn_h, 4, 0xFF353945);
        draw_string("Clear", clear_btn_x + 14, clear_btn_y + 4, 0xFFE5E5EA, fb, WIN_W);

        int text_area_x = 16;
        int text_area_y = 44;
        int text_line_h = 16;
        int cur_x = text_area_x;
        int cur_y = text_area_y;

        for (int i = 0; i < g_text_len; i++) {
            char c = g_text[i];
            if (c == '\n') {
                cur_x = text_area_x;
                cur_y += text_line_h;
            } else {
                if (cur_y + text_line_h < WIN_H - 24) {
                    draw_char(c, cur_x, cur_y, 0xFFF2F2F7, fb, WIN_W);
                }
                cur_x += 8;
                if (cur_x > WIN_W - 24) {
                    cur_x = text_area_x;
                    cur_y += text_line_h;
                }
            }
        }

        cursor_tick++;
        if ((cursor_tick / 16) % 2 == 0 && cur_y + text_line_h < WIN_H - 20) {
            kesh_draw_rect(fb, WIN_W, cur_x, cur_y + 1, 2, 13, 0xFF0A84FF);
        }

        int status_y = WIN_H - 24;
        kesh_draw_rect(fb, WIN_W, 0, status_y, WIN_W, 24, 0xFF24262E);
        kesh_draw_rect(fb, WIN_W, 0, status_y, WIN_W, 1, 0xFF323540);

        if (g_status_timer > 0) {
            g_status_timer--;
            if (g_status_timer == 0) {
                const char *r_msg = "Ready | Ctrl+S to save";
                int idx = 0;
                while (r_msg[idx]) { g_status_msg[idx] = r_msg[idx]; idx++; }
                g_status_msg[idx] = '\0';
                g_status_color = 0xFF8E8E93;
            }
        }

        draw_string(g_status_msg, 14, status_y + 5, g_status_color, fb, WIN_W);

        kesh_update_window(0);

        kesh_event_t ev;
        while (kesh_poll_event(0, &ev)) {
            if (ev.type == EVENT_CLOSE) {
                kesh_exit(0);
            } else if (ev.type == EVENT_MOUSE_DOWN) {
                if (ev.btn == 1) {
                    if (ev.mx >= save_btn_x && ev.mx <= save_btn_x + save_btn_w &&
                        ev.my >= save_btn_y && ev.my <= save_btn_y + save_btn_h) {
                        save_text();
                    } else if (ev.mx >= clear_btn_x && ev.mx <= clear_btn_x + clear_btn_w &&
                               ev.my >= clear_btn_y && ev.my <= clear_btn_y + clear_btn_h) {
                        g_text_len = 0;
                        g_text[0] = '\0';
                    }
                }
            } else if (ev.type == EVENT_KEY_DOWN) {
                char k = (char)ev.key;
                if (k == 19) {
                    save_text();
                    continue;
                }
                if (k == '\b' || k == 127) {
                    if (g_text_len > 0) {
                        g_text_len--;
                        g_text[g_text_len] = '\0';
                    }
                } else if (k == '\n' || k == '\r') {
                    if (g_text_len < MAX_TEXT - 1) {
                        g_text[g_text_len++] = '\n';
                        g_text[g_text_len] = '\0';
                    }
                } else if (k >= 32 && k <= 126) {
                    if (g_text_len < MAX_TEXT - 1) {
                        g_text[g_text_len++] = k;
                        g_text[g_text_len] = '\0';
                    }
                }
            }
        }

        kesh_sleep(16);
    }

    return 0;
}
