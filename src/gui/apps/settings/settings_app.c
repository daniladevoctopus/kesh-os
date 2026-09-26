// настройки keshos
#include "settings_app.h"
#include "gui/desktop.h"
#include "gui/font.h"
#include "gui/anim/genie_anim.h"
#include "gui/anim/win_chrome.h"
#include "gui/wm_config.h"

#include <stdint.h>

extern void draw_rounded_rect_buf(int x, int y, int w, int h, int r, uint32_t color);
extern void draw_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
extern void draw_rect_buf(int x, int y, int w, int h, uint32_t color);
extern void blur_rect_rounded_buf(int x, int y, int w, int h, int radius, int corner_r);
extern void pmm_get_stats(uint64_t *total, uint64_t *used);

extern const uint8_t about_bmp_start[];
extern void draw_scaled_bmp_rounded(const uint8_t* bmp_data, int x, int y, int target_w, int target_h, int r);

extern int g_wallpaper_choice;
extern int g_dark_mode;
extern int g_accent_color;
extern uint32_t get_accent_color(void);

extern int g_dock_mag_enabled;
extern int g_dock_mag_level;
extern int g_window_controls_align;

#define SETTINGS_DOCK_INDEX 4

static int is_open = 0;
static int minimized = 0;
static int win_x = 0, win_y = 0;
static int win_w = 690, win_h = 430;
static int positioned = 0;
static int dragging = 0;
static int drag_ox = 0, drag_oy = 0;
static genie_state_t genie;

static int current_tab = 0; // 0=Appearance, 1=Personalization, 2=Window Controls, 3=Dock, 4=About
static int dragging_slider = 0;
static int s_indicator_y = 50;
static int s_target_indicator_y = 50;
static int s_tab_anim_t = 10;

static void get_cpu_brand(char *brand, int max_len) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000));
    if (eax >= 0x80000004) {
        char buf[49];
        uint32_t *p = (uint32_t*)buf;
        for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
            __asm__ volatile ("cpuid" : "=a"(*p), "=b"(*(p+1)), "=c"(*(p+2)), "=d"(*(p+3)) : "a"(leaf));
            p += 4;
        }
        buf[48] = '\0';
        char *s = buf;
        while (*s == ' ') s++;
        int i = 0;
        while (s[i] && i < max_len - 1) {
            brand[i] = s[i];
            i++;
        }
        brand[i] = '\0';
        if (i > 0) return;
    }

    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    char vendor[13];
    *(uint32_t*)&vendor[0] = ebx;
    *(uint32_t*)&vendor[4] = edx;
    *(uint32_t*)&vendor[8] = ecx;
    vendor[12] = '\0';
    int i = 0;
    while (vendor[i] && i < max_len - 1) {
        brand[i] = vendor[i];
        i++;
    }
    brand[i] = '\0';
}

static void draw_filled_circle(int cx, int cy, int radius, uint32_t color) {
    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= r2) {
                draw_rect_buf(cx + dx, cy + dy, 1, 1, color);
            }
        }
    }
}

void toggle_settings_app(void)
{
    int dx, dy;
    genie_dock_icon_point(SETTINGS_DOCK_INDEX, &dx, &dy);

    if (genie_is_animating(&genie))
        genie_cancel(&genie);

    if (is_open && minimized) {
        genie_start_open(&genie, dx, dy);
        minimized = 0;
        dragging = 0;
        dragging_slider = 0;
        return;
    }

    if (is_open) {
        genie_start_close(&genie, dx, dy);
        is_open = 0;
        minimized = 0;
        dragging = 0;
        dragging_slider = 0;
        return;
    }

    is_open = 1;
    minimized = 0;
    dragging = 0;
    dragging_slider = 0;
    genie_start_open(&genie, dx, dy);
}

void render_settings_app_window(
    uint32_t* buf,
    int scr_w,
    int scr_h,
    int mx,
    int my,
    int btn,
    int click
) {
    genie_tick(&genie);

    if (!is_open && !genie_is_animating(&genie))
        return;

    if (minimized && !genie_is_animating(&genie))
        return;

    if (!positioned) {
        win_x = (scr_w - win_w) / 2;
        win_y = (scr_h - win_h) / 2;
        positioned = 1;
    }

    int mid_genie = genie_is_animating(&genie);
    int header_h = 36;
    int traffic_zone_w = 76;

    int drag_start_x = (g_window_controls_align == 1) ? (win_x + 90) : (win_x + traffic_zone_w + 65);
    int drag_end_x   = (g_window_controls_align == 1) ? (win_x + win_w - traffic_zone_w) : (win_x + win_w);

    if (!mid_genie && btn && !dragging && win_drag_available() &&
        mx >= drag_start_x && mx <= drag_end_x &&
        my >= win_y && my <= win_y + header_h)
    {
        dragging = 1;
        drag_ox = mx - win_x;
        drag_oy = my - win_y;
        win_drag_claim();
        win_set_focused(WIN_ID_SETTINGS_);
    }
    if (!btn) dragging = 0;
    if (dragging) { win_x = mx - drag_ox; win_y = my - drag_oy; }

    win_report_rect(WIN_ID_SETTINGS_, win_x, win_y, win_w, win_h, is_open && !mid_genie);
    int occluded = win_click_occluded(WIN_ID_SETTINGS_, mx, my);

    int draw_x, draw_y, draw_w, draw_h;
    genie_get_rect(&genie, win_x, win_y, win_w, win_h, &draw_x, &draw_y, &draw_w, &draw_h);

    int tl_size = 12, tl_gap = 7;
    int tl_y = win_y + (header_h - tl_size) / 2;
    int close_x, minimize_x, zoom_x;

    if (g_window_controls_align == 1) {
        close_x = win_x + win_w - 14 - tl_size;
        minimize_x = close_x - tl_gap - tl_size;
        zoom_x = minimize_x - tl_gap - tl_size;
    } else {
        close_x = win_x + 14;
        minimize_x = close_x + tl_size + tl_gap;
        zoom_x = minimize_x + tl_size + tl_gap;
    }

    int hit_padding = 6;
    int hover_close = !mid_genie &&
        mx >= (close_x - hit_padding) && mx <= (close_x + tl_size + hit_padding) &&
        my >= (tl_y - hit_padding) && my <= (tl_y + tl_size + hit_padding);
    int hover_minimize = !mid_genie &&
        mx >= (minimize_x - hit_padding) && mx <= (minimize_x + tl_size + hit_padding) &&
        my >= (tl_y - hit_padding) && my <= (tl_y + tl_size + hit_padding);
    int hover_zoom = !mid_genie &&
        mx >= (zoom_x - hit_padding) && mx <= (zoom_x + tl_size + hit_padding) &&
        my >= (tl_y - hit_padding) && my <= (tl_y + tl_size + hit_padding);

    // тени окна
    static const struct { int spread; int drop; uint8_t alpha; } shadows[] = {
        {16, 20, 10}, {12, 16, 16}, {8, 12, 22}, {4, 8, 30}, {2, 4, 40},
    };
    for (unsigned i = 0; i < sizeof(shadows) / sizeof(shadows[0]); i++) {
        int sp = shadows[i].spread;
        draw_rounded_rect_alpha(
            draw_x - sp / 2, draw_y - sp / 2 + shadows[i].drop,
            draw_w + sp, draw_h + sp, 18 + sp / 2, 0x00000000, shadows[i].alpha
        );
    }

    // стеклянный блюр для обеих тем
    blur_rect_rounded_buf(draw_x, draw_y, draw_w, draw_h, 8, 16);

    int is_dark = g_dark_mode;
    uint32_t accent = get_accent_color();

    uint32_t text_primary = is_dark ? 0x00FFFFFF : 0x001D1D1F;
    uint32_t text_sec     = is_dark ? 0x008E95A5 : 0x006E6E73;
    uint32_t text_label   = is_dark ? 0x00E0E5F0 : 0x002C3038;

    if (is_dark) {
        draw_rounded_rect_alpha(draw_x, draw_y, draw_w, draw_h, 16, 0x00141622, 195);
        draw_rounded_rect_alpha(draw_x, draw_y, draw_w, 1, 16, 0x00FFFFFF, 55);
    } else {
        draw_rounded_rect_alpha(draw_x, draw_y, draw_w, draw_h, 16, 0x00FFFFFF, 175);
        draw_rounded_rect_alpha(draw_x, draw_y, draw_w, draw_h, 16, 0x00EDF1F7, 50);
        draw_rounded_rect_alpha(draw_x, draw_y, draw_w, 1, 16, 0x00FFFFFF, 220);
        draw_rounded_rect_alpha(draw_x, draw_y + draw_h - 1, draw_w, 1, 16, 0x00D0D5E0, 100);
    }

    if (mid_genie) {
        dragging = 0;
        dragging_slider = 0;
        return;
    }

    // светофор
    draw_rounded_rect_buf(close_x, tl_y, tl_size, tl_size, 6,
        hover_close ? 0x00FF453A : 0x00FF5F57);
    draw_rounded_rect_buf(minimize_x, tl_y, tl_size, tl_size, 6,
        hover_minimize ? 0x00FFD60A : 0x00FFBD2E);
    draw_rounded_rect_buf(zoom_x, tl_y, tl_size, tl_size, 6,
        hover_zoom ? 0x0030D158 : 0x0028C93F);

    if (click && hover_close && !occluded) {
        int dx, dy;
        genie_dock_icon_point(SETTINGS_DOCK_INDEX, &dx, &dy);
        genie_start_close(&genie, dx, dy);
        is_open = 0;
        dragging = 0;
        return;
    }

    if (click && hover_minimize && !occluded) {
        int dx, dy;
        genie_dock_icon_point(SETTINGS_DOCK_INDEX, &dx, &dy);
        genie_start_minimize(&genie, dx, dy);
        minimized = 1;
        dragging = 0;
        return;
    }

    // сайдбар
    int sidebar_w = 168;
    if (is_dark) {
        draw_rounded_rect_alpha(win_x, win_y, sidebar_w, win_h, 16, 0x000A0D14, 85);
        draw_rect_buf(win_x + sidebar_w, win_y, 1, win_h, 0x002B2E3E);
    } else {
        draw_rounded_rect_alpha(win_x, win_y, sidebar_w, win_h, 16, 0x00E6EBF5, 140);
        draw_rect_buf(win_x + sidebar_w, win_y, 1, win_h, 0x00D0D5E2);
    }

    // адаптивное позиционирование заголовка Settings
    int settings_title_x = (g_window_controls_align == 0) ? (win_x + 72) : (win_x + 18);
    draw_string("Settings", settings_title_x, win_y + 14, text_primary, buf, (uint32_t)scr_w);

    const char *tab_titles[] = { "Appearance", "Personalization", "Window Controls", "Dock" };
    int tab_count = 4;

    int idiff = s_target_indicator_y - s_indicator_y;
    if (idiff != 0) {
        int step = idiff / 2;
        if (step == 0) step = (idiff > 0) ? 1 : -1;
        s_indicator_y += step;
    }
    if (s_tab_anim_t < 10) s_tab_anim_t++;

    draw_rounded_rect_alpha(win_x + 10, win_y + s_indicator_y, sidebar_w - 20, 30, 8, accent, 190);
    draw_rounded_rect_alpha(win_x + 10, win_y + s_indicator_y, sidebar_w - 20, 1, 8, 0x00FFFFFF, 70);

    for (int t = 0; t < tab_count; t++) {
        int ty = win_y + 50 + t * 38;
        int dist = (win_y + s_indicator_y) - ty;
        if (dist < 0) dist = -dist;
        int active = (dist < 18);
        uint32_t tab_col = active ? 0x00FFFFFF : (is_dark ? 0x00A5ACB8 : 0x00555A66);
        draw_string(tab_titles[t], win_x + 18, ty + 7, tab_col, buf, (uint32_t)scr_w);

        if (click && !occluded && mx >= win_x + 10 && mx <= win_x + sidebar_w - 10 && my >= ty && my <= ty + 30) {
            if (current_tab != t) {
                current_tab = t;
                s_target_indicator_y = 50 + t * 38;
                s_tab_anim_t = 0;
            }
        }
    }

    int slide_oy = ((10 - s_tab_anim_t) * 12) / 10;
    int tab_base_y = win_y + slide_oy;

    int content_x = win_x + sidebar_w + 24;
    int card_w = win_w - sidebar_w - 48;

    // Вкладка 0: Appearance
    if (current_tab == 0) {
        draw_string("Appearance", content_x, win_y + 14, text_primary, buf, (uint32_t)scr_w);
        draw_string("System Appearance and Color Mode", content_x, win_y + 36, text_sec, buf, (uint32_t)scr_w);

        int card1_y = win_y + 70;
        if (is_dark) {
            draw_rounded_rect_alpha(content_x, card1_y, card_w, 120, 12, 0x001C202E, 130);
            draw_rounded_rect_alpha(content_x, card1_y, card_w, 1, 12, 0x00FFFFFF, 30);
        } else {
            draw_rounded_rect_alpha(content_x, card1_y, card_w, 120, 12, 0x00FFFFFF, 190);
            draw_rounded_rect_alpha(content_x, card1_y, card_w, 1, 12, 0x00D0D4DF, 120);
        }
        draw_string("System Appearance Mode", content_x + 16, card1_y + 16, text_label, buf, (uint32_t)scr_w);

        int tbtn_y = card1_y + 46;
        int tbtn_w = (card_w - 46) / 2, tbtn_h = 42;
        int light_sel = (!g_dark_mode);
        int dark_sel = (g_dark_mode);

        // Light Theme
        draw_rounded_rect_alpha(content_x + 16, tbtn_y, tbtn_w, tbtn_h, 8, light_sel ? accent : (is_dark ? 0x00272B3B : 0x00E4E7EE), light_sel ? 210 : 160);
        draw_string("Light (White)", content_x + 16 + (tbtn_w - font_text_width("Light (White)")) / 2, tbtn_y + 13, light_sel ? 0x00FFFFFF : text_primary, buf, (uint32_t)scr_w);

        // Dark Theme
        int dark_btn_x = content_x + 16 + tbtn_w + 14;
        draw_rounded_rect_alpha(dark_btn_x, tbtn_y, tbtn_w, tbtn_h, 8, dark_sel ? accent : (is_dark ? 0x00272B3B : 0x00E4E7EE), dark_sel ? 210 : 160);
        draw_string("Dark (Black)", dark_btn_x + (tbtn_w - font_text_width("Dark (Black)")) / 2, tbtn_y + 13, dark_sel ? 0x00FFFFFF : text_primary, buf, (uint32_t)scr_w);

        if (click && !occluded) {
            if (mx >= content_x + 16 && mx <= content_x + 16 + tbtn_w && my >= tbtn_y && my <= tbtn_y + tbtn_h) {
                g_dark_mode = 0;
            }
            if (mx >= dark_btn_x && mx <= dark_btn_x + tbtn_w && my >= tbtn_y && my <= tbtn_y + tbtn_h) {
                g_dark_mode = 1;
            }
        }

        draw_string("Themes apply across all native components instantly.", content_x, card1_y + 146, text_sec, buf, (uint32_t)scr_w);
    }

    // Вкладка 1: Personalization (цветовой акцент)
    else if (current_tab == 1) {
        draw_string("Personalization", content_x, win_y + 14, text_primary, buf, (uint32_t)scr_w);
        draw_string("Accent color tint and system highlight styling", content_x, win_y + 36, text_sec, buf, (uint32_t)scr_w);

        int card_y = win_y + 70;
        if (is_dark) {
            draw_rounded_rect_alpha(content_x, card_y, card_w, 170, 12, 0x001C202E, 130);
            draw_rounded_rect_alpha(content_x, card_y, card_w, 1, 12, 0x00FFFFFF, 30);
        } else {
            draw_rounded_rect_alpha(content_x, card_y, card_w, 170, 12, 0x00FFFFFF, 190);
            draw_rounded_rect_alpha(content_x, card_y, card_w, 1, 12, 0x00D0D4DF, 120);
        }

        draw_string("System Accent Color", content_x + 16, card_y + 16, text_label, buf, (uint32_t)scr_w);
        draw_string("Applied to selection, active tabs, volume and window glow", content_x + 16, card_y + 36, text_sec, buf, (uint32_t)scr_w);

        int btn_y = card_y + 68;
        int gap = 16;
        int btn_w = (card_w - 32 - gap * 2) / 3, btn_h = 76;

        // 1. Amber / Orange
        int c0_x = content_x + 16;
        int c0_sel = (g_accent_color == 0);
        draw_rounded_rect_alpha(c0_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x00272B3B : 0x00EBF0F8, 160);
        if (c0_sel) {
            draw_rounded_rect_alpha(c0_x - 2, btn_y - 2, btn_w + 4, btn_h + 4, 12, 0x00FF9F0A, 255);
            draw_rounded_rect_alpha(c0_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x001A1D28 : 0x00FFFFFF, 255);
        }
        draw_filled_circle(c0_x + btn_w / 2, btn_y + 26, 14, 0x00FF9F0A);
        draw_string("Amber", c0_x + (btn_w - font_text_width("Amber")) / 2, btn_y + 50, text_primary, buf, (uint32_t)scr_w);

        // 2. Electric Blue
        int c1_x = c0_x + btn_w + gap;
        int c1_sel = (g_accent_color == 1);
        draw_rounded_rect_alpha(c1_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x00272B3B : 0x00EBF0F8, 160);
        if (c1_sel) {
            draw_rounded_rect_alpha(c1_x - 2, btn_y - 2, btn_w + 4, btn_h + 4, 12, 0x000A84FF, 255);
            draw_rounded_rect_alpha(c1_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x001A1D28 : 0x00FFFFFF, 255);
        }
        draw_filled_circle(c1_x + btn_w / 2, btn_y + 26, 14, 0x000A84FF);
        draw_string("Blue", c1_x + (btn_w - font_text_width("Blue")) / 2, btn_y + 50, text_primary, buf, (uint32_t)scr_w);

        // 3. Crimson Red
        int c2_x = c1_x + btn_w + gap;
        int c2_sel = (g_accent_color == 2);
        draw_rounded_rect_alpha(c2_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x00272B3B : 0x00EBF0F8, 160);
        if (c2_sel) {
            draw_rounded_rect_alpha(c2_x - 2, btn_y - 2, btn_w + 4, btn_h + 4, 12, 0x00FF453A, 255);
            draw_rounded_rect_alpha(c2_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x001A1D28 : 0x00FFFFFF, 255);
        }
        draw_filled_circle(c2_x + btn_w / 2, btn_y + 26, 14, 0x00FF453A);
        draw_string("Red", c2_x + (btn_w - font_text_width("Red")) / 2, btn_y + 50, text_primary, buf, (uint32_t)scr_w);

        if (click && !occluded) {
            if (mx >= c0_x && mx <= c0_x + btn_w && my >= btn_y && my <= btn_y + btn_h) {
                g_accent_color = 0;
            }
            if (mx >= c1_x && mx <= c1_x + btn_w && my >= btn_y && my <= btn_y + btn_h) {
                g_accent_color = 1;
            }
            if (mx >= c2_x && mx <= c2_x + btn_w && my >= btn_y && my <= btn_y + btn_h) {
                g_accent_color = 2;
            }
        }
    }

    // Вкладка 2: Window Controls
    else if (current_tab == 2) {
        draw_string("Window Controls", content_x, win_y + 14, text_primary, buf, (uint32_t)scr_w);
        draw_string("Consistent traffic lights position across all windows", content_x, win_y + 36, text_sec, buf, (uint32_t)scr_w);

        int card_y = win_y + 70;
        if (is_dark) {
            draw_rounded_rect_alpha(content_x, card_y, card_w, 230, 12, 0x001C202E, 130);
            draw_rounded_rect_alpha(content_x, card_y, card_w, 1, 12, 0x00FFFFFF, 30);
        } else {
            draw_rounded_rect_alpha(content_x, card_y, card_w, 230, 12, 0x00FFFFFF, 190);
            draw_rounded_rect_alpha(content_x, card_y, card_w, 1, 12, 0x00D0D4DF, 120);
        }

        draw_string("Titlebar Button Alignment", content_x + 16, card_y + 16, text_label, buf, (uint32_t)scr_w);
        draw_string("Choose where close, minimize and zoom appear:", content_x + 16, card_y + 36, text_sec, buf, (uint32_t)scr_w);

        int seg_y = card_y + 65;
        int seg_w = (card_w - 46) / 2, seg_h = 36;
        int is_left = (g_window_controls_align == 0);
        int is_right = (g_window_controls_align == 1);

        // Кнопка Слева
        draw_rounded_rect_alpha(content_x + 16, seg_y, seg_w, seg_h, 8, is_left ? accent : (is_dark ? 0x00272B3B : 0x00E4E7EE), is_left ? 210 : 160);
        draw_string("Left", content_x + 16 + (seg_w - font_text_width("Left")) / 2, seg_y + 10, is_left ? 0x00FFFFFF : text_primary, buf, (uint32_t)scr_w);

        // Кнопка Справа
        int seg_r_x = content_x + 16 + seg_w + 14;
        draw_rounded_rect_alpha(seg_r_x, seg_y, seg_w, seg_h, 8, is_right ? accent : (is_dark ? 0x00272B3B : 0x00E4E7EE), is_right ? 210 : 160);
        draw_string("Right", seg_r_x + (seg_w - font_text_width("Right")) / 2, seg_y + 10, is_right ? 0x00FFFFFF : text_primary, buf, (uint32_t)scr_w);

        if (click && !occluded) {
            if (mx >= content_x + 16 && mx <= content_x + 16 + seg_w && my >= seg_y && my <= seg_y + seg_h) {
                g_window_controls_align = 0;
            }
            if (mx >= seg_r_x && mx <= seg_r_x + seg_w && my >= seg_y && my <= seg_y + seg_h) {
                g_window_controls_align = 1;
            }
        }

        // Превью
        int prev_y = seg_y + 52;
        int prev_w = card_w - 32, prev_h = 60;
        draw_rounded_rect_alpha(content_x + 16, prev_y, prev_w, prev_h, 10, is_dark ? 0x00141620 : 0x00F0F2F6, 200);
        draw_rounded_rect_alpha(content_x + 16, prev_y, prev_w, 24, 10, is_dark ? 0x00242838 : 0x00E0E4EC, 180);
        draw_rect_buf(content_x + 16, prev_y + 24, prev_w, 1, is_dark ? 0x002D3244 : 0x00D0D4DF);

        if (is_left) {
            draw_rounded_rect_buf(content_x + 28, prev_y + 7, 10, 10, 5, 0x00FF5F56);
            draw_rounded_rect_buf(content_x + 43, prev_y + 7, 10, 10, 5, 0x00FFBD2E);
            draw_rounded_rect_buf(content_x + 58, prev_y + 7, 10, 10, 5, 0x0027C93F);
        } else {
            draw_rounded_rect_buf(content_x + 16 + prev_w - 22, prev_y + 7, 10, 10, 5, 0x00FF5F56);
            draw_rounded_rect_buf(content_x + 16 + prev_w - 37, prev_y + 7, 10, 10, 5, 0x00FFBD2E);
            draw_rounded_rect_buf(content_x + 16 + prev_w - 52, prev_y + 7, 10, 10, 5, 0x0027C93F);
        }
        draw_string("Preview Window", content_x + 16 + prev_w / 2 - 45, prev_y + 5, text_sec, buf, (uint32_t)scr_w);

        const char *cfg_lbl = "Saved to /wm.conf: WINDOW_CONTROLS_ALIGN = ";
        draw_string(cfg_lbl, content_x + 16, prev_y + prev_h + 12, text_sec, buf, (uint32_t)scr_w);
        draw_string(is_left ? "LEFT" : "RIGHT", content_x + 16 + font_text_width(cfg_lbl), prev_y + prev_h + 12, accent, buf, (uint32_t)scr_w);
    }

    // Вкладка 3: Dock
    else if (current_tab == 3) {
        draw_string("Dock & Navigation Bar", content_x, win_y + 14, text_primary, buf, (uint32_t)scr_w);
        draw_string("Magnification and shelf animations", content_x, win_y + 36, text_sec, buf, (uint32_t)scr_w);

        int card_y = win_y + 70;
        if (is_dark) {
            draw_rounded_rect_alpha(content_x, card_y, card_w, 170, 12, 0x001C202E, 130);
            draw_rounded_rect_alpha(content_x, card_y, card_w, 1, 12, 0x00FFFFFF, 30);
        } else {
            draw_rounded_rect_alpha(content_x, card_y, card_w, 170, 12, 0x00FFFFFF, 190);
            draw_rounded_rect_alpha(content_x, card_y, card_w, 1, 12, 0x00D0D4DF, 120);
        }

        draw_string("Icon Magnification", content_x + 16, card_y + 18, text_label, buf, (uint32_t)scr_w);
        draw_string("Expand icons on mouse hover", content_x + 16, card_y + 36, text_sec, buf, (uint32_t)scr_w);

        int sw_x = content_x + card_w - 68;
        int sw_y = card_y + 18;
        int sw_w = 48, sw_h = 26;
        uint32_t sw_col = g_dock_mag_enabled ? accent : (is_dark ? 0x00353C4E : 0x00D0D4DE);
        draw_rounded_rect_buf(sw_x, sw_y, sw_w, sw_h, sw_h / 2, sw_col);
        int knob_d = 20;
        int knob_x = g_dock_mag_enabled ? (sw_x + sw_w - knob_d - 3) : (sw_x + 3);
        int knob_y = sw_y + (sw_h - knob_d) / 2;
        draw_rounded_rect_buf(knob_x, knob_y, knob_d, knob_d, knob_d / 2, 0x00FFFFFF);

        if (click && !occluded && mx >= sw_x && mx <= sw_x + sw_w && my >= sw_y && my <= sw_y + sw_h) {
            g_dock_mag_enabled = !g_dock_mag_enabled;
        }

        draw_string("Magnification Strength", content_x + 16, card_y + 75, text_label, buf, (uint32_t)scr_w);
        int slider_x = content_x + 16;
        int slider_y = card_y + 105;
        int slider_w = card_w - 32;
        int slider_h = 6;
        draw_rounded_rect_buf(slider_x, slider_y, slider_w, slider_h, 3, is_dark ? 0x00282E40 : 0x00D8DCE5);

        int fill_w = (slider_w * g_dock_mag_level) / 100;
        if (fill_w > slider_w) fill_w = slider_w;
        draw_rounded_rect_buf(slider_x, slider_y, fill_w, slider_h, 3, g_dock_mag_enabled ? accent : 0x00656D80);
        int kx = slider_x + fill_w - 9;
        int ky = slider_y + 3 - 9;
        draw_rounded_rect_buf(kx, ky, 18, 18, 9, 0x00FFFFFF);

        if (btn && !dragging && !occluded && mx >= slider_x - 10 && mx <= slider_x + slider_w + 10 && my >= slider_y - 12 && my <= slider_y + 20) {
            dragging_slider = 1;
            int rel = mx - slider_x;
            if (rel < 0) rel = 0;
            if (rel > slider_w) rel = slider_w;
            g_dock_mag_level = (rel * 100) / slider_w;
        }
        if (!btn) dragging_slider = 0;
    }
}
