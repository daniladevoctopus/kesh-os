// настройки keshos на плюсах
#include "kesh.h"

inline void* operator new(size_t, void* p) noexcept { return p; }
inline void* operator new[](size_t, void* p) noexcept { return p; }
inline void operator delete(void*, void*) noexcept {}
inline void operator delete[](void*, void*) noexcept {}

namespace SettingsApp {

static int str_len(const char *s) {
    if (!s) return 0;
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void int_to_str(int v, char *out, int max_len) {
    if (max_len < 2) return;
    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    char tmp[16];
    int ti = 0;
    int sign = (v < 0);
    if (sign) v = -v;
    while (v > 0 && ti < 15) {
        tmp[ti++] = (char)('0' + (v % 10));
        v /= 10;
    }
    int oi = 0;
    if (sign && oi < max_len - 1) out[oi++] = '-';
    while (ti > 0 && oi < max_len - 1) {
        out[oi++] = tmp[--ti];
    }
    out[oi] = '\0';
}



static void draw_circle(uint32_t *fb, int fb_w, int cx, int cy, int radius, uint32_t color) {
    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                kesh_draw_rect(fb, fb_w, cx + dx, cy + dy, 1, 1, color);
            }
        }
    }
}

enum Tab {
    TAB_APPEARANCE = 0,
    TAB_PERSONALIZATION,
    TAB_WINDOW_CONTROLS,
    TAB_DOCK,
    TAB_COUNT
};

struct TabInfo {
    const char *name;
    const char *badge;
};

static const TabInfo g_tabs[TAB_COUNT] = {
    { "Appearance", "Art" },
    { "Personalization", "Clr" },
    { "Window Controls", "WM" },
    { "Dock", "Bar" }
};

class SettingsWindow {
private:
    int m_w;
    int m_h;
    uint32_t *m_fb;
    int m_current_tab;
    kesh_settings_t m_settings;
    kesh_sysinfo_t m_sysinfo;
    bool m_dragging_slider;
    int m_indicator_y;
    int m_target_indicator_y;
    int m_tab_anim_t;

    uint32_t get_accent() const {
        switch (m_settings.accent_color) {
            case 1:  return 0x000A84FF; // Blue
            case 2:  return 0x00FF453A; // Red
            case 0:
            default: return 0x00FF9F0A; // Amber / Orange
        }
    }

public:
    SettingsWindow(int w, int h)
        : m_w(w), m_h(h), m_fb(nullptr), m_current_tab(TAB_APPEARANCE), m_dragging_slider(false),
          m_indicator_y(50), m_target_indicator_y(50), m_tab_anim_t(10)
    {
        m_settings.wallpaper_choice = 1;
        m_settings.dock_mag_enabled = 1;
        m_settings.dock_mag_level = 65;
        m_settings.window_controls_align = 0;
        m_settings.dark_mode = 1;
        m_settings.accent_color = 0;
    }

    bool init() {
        m_fb = kesh_create_window(m_w, m_h, "Settings");
        if (!m_fb) return false;
        kesh_get_settings(&m_settings);
        kesh_get_sysinfo(&m_sysinfo);
        return true;
    }

    void refresh_data() {
        kesh_get_settings(&m_settings);
        kesh_get_sysinfo(&m_sysinfo);
    }

    void run() {
        render();
        kesh_update_window(0);

        kesh_event_t ev;
        while (true) {
            bool has_event = kesh_poll_event(0, &ev);
            if (has_event) {
                if (ev.type == EVENT_CLOSE) {
                    break;
                }
                handle_event(ev);
            }
            bool animating = (m_indicator_y != m_target_indicator_y) || (m_tab_anim_t < 10);
            if (has_event || animating) {
                render();
                kesh_update_window(0);
            }
            kesh_sleep(16);
        }
    }

private:
    void handle_event(const kesh_event_t &ev) {
        int mx = ev.mx;
        int my = ev.my;

        if (ev.type == EVENT_MOUSE_UP) {
            m_dragging_slider = false;
        }

        if (ev.type == EVENT_MOUSE_DOWN && ev.btn == 1) {
            int sidebar_w = 170;
            if (mx >= 10 && mx < sidebar_w - 10 && my >= 50 && my < 50 + TAB_COUNT * 40) {
                int clicked_tab = (my - 50) / 40;
                if (clicked_tab >= 0 && clicked_tab < TAB_COUNT) {
                    if (m_current_tab != clicked_tab) {
                        m_current_tab = clicked_tab;
                        m_target_indicator_y = 50 + clicked_tab * 40;
                        m_tab_anim_t = 0;
                    }
                    return;
                }
            }

            int content_x = sidebar_w + 24;
            int card_w = m_w - sidebar_w - 48;

            if (m_current_tab == TAB_APPEARANCE) {
                int card_y = 70;
                int btn_w = (card_w - 46) / 2, btn_h = 42;
                int btn_y = card_y + 46;
                if (mx >= content_x + 16 && mx <= content_x + 16 + btn_w && my >= btn_y && my <= btn_y + btn_h) {
                    m_settings.dark_mode = 0;
                    kesh_set_settings(&m_settings);
                }
                int dark_x = content_x + 16 + btn_w + 14;
                if (mx >= dark_x && mx <= dark_x + btn_w && my >= btn_y && my <= btn_y + btn_h) {
                    m_settings.dark_mode = 1;
                    kesh_set_settings(&m_settings);
                }
            }
            else if (m_current_tab == TAB_PERSONALIZATION) {
                int card_y = 70;
                int btn_y = card_y + 68;
                int gap = 16;
                int btn_w = (card_w - 32 - gap * 2) / 3, btn_h = 76;

                int c0_x = content_x + 16;
                if (mx >= c0_x && mx <= c0_x + btn_w && my >= btn_y && my <= btn_y + btn_h) {
                    m_settings.accent_color = 0;
                    kesh_set_settings(&m_settings);
                }
                int c1_x = c0_x + btn_w + gap;
                if (mx >= c1_x && mx <= c1_x + btn_w && my >= btn_y && my <= btn_y + btn_h) {
                    m_settings.accent_color = 1;
                    kesh_set_settings(&m_settings);
                }
                int c2_x = c1_x + btn_w + gap;
                if (mx >= c2_x && mx <= c2_x + btn_w && my >= btn_y && my <= btn_y + btn_h) {
                    m_settings.accent_color = 2;
                    kesh_set_settings(&m_settings);
                }
            }
            else if (m_current_tab == TAB_WINDOW_CONTROLS) {
                int card_y = 70;
                int seg_y = card_y + 65;
                int seg_w = (card_w - 46) / 2, seg_h = 38;
                if (mx >= content_x + 16 && mx <= content_x + 16 + seg_w && my >= seg_y && my <= seg_y + seg_h) {
                    m_settings.window_controls_align = 0;
                    kesh_set_settings(&m_settings);
                }
                int seg_r_x = content_x + 16 + seg_w + 14;
                if (mx >= seg_r_x && mx <= seg_r_x + seg_w && my >= seg_y && my <= seg_y + seg_h) {
                    m_settings.window_controls_align = 1;
                    kesh_set_settings(&m_settings);
                }
            }
            else if (m_current_tab == TAB_DOCK) {
                int card_y = 70;
                int sw_x = content_x + card_w - 68;
                int sw_y = card_y + 18;
                int sw_w = 48, sw_h = 26;
                if (mx >= sw_x && mx <= sw_x + sw_w && my >= sw_y && my <= sw_y + sw_h) {
                    m_settings.dock_mag_enabled = !m_settings.dock_mag_enabled;
                    kesh_set_settings(&m_settings);
                }

                int slider_x = content_x + 16;
                int slider_y = card_y + 105;
                int slider_w = card_w - 32;
                if (mx >= slider_x - 10 && mx <= slider_x + slider_w + 10 && my >= slider_y - 12 && my <= slider_y + 20) {
                    m_dragging_slider = true;
                    int rel = mx - slider_x;
                    if (rel < 0) rel = 0;
                    if (rel > slider_w) rel = slider_w;
                    m_settings.dock_mag_level = (rel * 100) / slider_w;
                    kesh_set_settings(&m_settings);
                }
            }
        }

        if (ev.type == EVENT_MOUSE_MOVE && m_dragging_slider) {
            int sidebar_w = 170;
            int content_x = sidebar_w + 24;
            int card_w = m_w - sidebar_w - 48;
            int slider_x = content_x + 16;
            int slider_w = card_w - 32;
            int rel = mx - slider_x;
            if (rel < 0) rel = 0;
            if (rel > slider_w) rel = slider_w;
            m_settings.dock_mag_level = (rel * 100) / slider_w;
            kesh_set_settings(&m_settings);
        }
    }

    void render() {
        bool is_dark = (m_settings.dark_mode != 0);
        uint32_t bg_col = is_dark ? 0x001B1E28 : 0x00F5F6F8;
        kesh_clear(m_fb, m_w, m_h, bg_col);

        int sidebar_w = 170;
        uint32_t side_col = is_dark ? 0x0014161F : 0x00E6EBF5;
        uint32_t div_col  = is_dark ? 0x002B3042 : 0x00D0D5E2;
        kesh_draw_rect(m_fb, m_w, 0, 0, sidebar_w, m_h, side_col);
        kesh_draw_rect(m_fb, m_w, sidebar_w, 0, 1, m_h, div_col);

        uint32_t accent = get_accent();
        uint32_t text_primary = is_dark ? 0xFFFFFFFF : 0xFF1D1D1F;

        draw_string("Settings", 22, 18, text_primary, m_fb, (uint32_t)m_w);

        int diff = m_target_indicator_y - m_indicator_y;
        if (diff != 0) {
            int step = diff / 2;
            if (step == 0) step = (diff > 0) ? 1 : -1;
            m_indicator_y += step;
        }
        if (m_tab_anim_t < 10) m_tab_anim_t++;

        kesh_draw_rounded_rect(m_fb, m_w, 10, m_indicator_y, sidebar_w - 20, 32, 8, accent);

        for (int i = 0; i < TAB_COUNT; i++) {
            int tab_y = 50 + i * 40;
            int dist = m_indicator_y - tab_y;
            if (dist < 0) dist = -dist;
            bool active = (dist < 18);

            uint32_t text_col = active ? 0xFFFFFFFF : (is_dark ? 0xFFB0B6C8 : 0xFF555A66);
            draw_string(g_tabs[i].name, 22, tab_y + 8, text_col, m_fb, (uint32_t)m_w);
        }

        int content_x = sidebar_w + 24;
        int slide_y = ((10 - m_tab_anim_t) * 12) / 10;
        int content_y = 20 + slide_y;

        switch (m_current_tab) {
            case TAB_APPEARANCE:
                render_tab_appearance(content_x, content_y);
                break;
            case TAB_PERSONALIZATION:
                render_tab_personalization(content_x, content_y);
                break;
            case TAB_WINDOW_CONTROLS:
                render_tab_window_controls(content_x, content_y);
                break;
            case TAB_DOCK:
                render_tab_dock(content_x, content_y);
                break;
            default:
                break;
        }
    }

    void render_tab_appearance(int ox, int oy) {
        bool is_dark = (m_settings.dark_mode != 0);
        uint32_t text_primary = is_dark ? 0xFFFFFFFF : 0xFF1D1D1F;
        uint32_t text_sec     = is_dark ? 0xFF8E95A5 : 0xFF6E6E73;
        uint32_t text_label   = is_dark ? 0xFFE0E5F0 : 0xFF2C3038;
        uint32_t accent       = get_accent();

        draw_string("Appearance", ox, oy, text_primary, m_fb, (uint32_t)m_w);
        draw_string("System Appearance and Color Mode", ox, oy + 22, text_sec, m_fb, (uint32_t)m_w);

        int card_y = oy + 55;
        int card_w = m_w - (ox + 24);
        kesh_draw_rounded_rect(m_fb, m_w, ox, card_y, card_w, 130, 12, is_dark ? 0x00232736 : 0x00FFFFFF);
        kesh_draw_rect(m_fb, m_w, ox + 6, card_y, card_w - 12, 1, is_dark ? 0x003D445C : 0x00D0D4DF);

        draw_string("System Appearance Mode", ox + 16, card_y + 16, text_label, m_fb, (uint32_t)m_w);
        draw_string("Switches Taskbar, Explorer and all apps between Light and Dark", ox + 16, card_y + 36, text_sec, m_fb, (uint32_t)m_w);

        int btn_w = (card_w - 46) / 2, btn_h = 42;
        int btn_y = card_y + 66;

        // Light mode button
        uint32_t col_light_btn = !is_dark ? accent : (is_dark ? 0x002D3346 : 0x00E4E7EE);
        kesh_draw_rounded_rect(m_fb, m_w, ox + 16, btn_y, btn_w, btn_h, 8, col_light_btn);
        draw_string("Light (White)", ox + 16 + (btn_w - font_text_width("Light (White)")) / 2, btn_y + 13, !is_dark ? 0xFFFFFFFF : text_primary, m_fb, (uint32_t)m_w);

        // Dark mode button
        int dark_x = ox + 16 + btn_w + 14;
        uint32_t col_dark_btn = is_dark ? accent : (is_dark ? 0x002D3346 : 0x00E4E7EE);
        kesh_draw_rounded_rect(m_fb, m_w, dark_x, btn_y, btn_w, btn_h, 8, col_dark_btn);
        draw_string("Dark (Black)", dark_x + (btn_w - font_text_width("Dark (Black)")) / 2, btn_y + 13, is_dark ? 0xFFFFFFFF : text_primary, m_fb, (uint32_t)m_w);

        draw_string("Themes apply across all native components instantly.", ox, card_y + 160, text_sec, m_fb, (uint32_t)m_w);
    }

    void render_tab_personalization(int ox, int oy) {
        bool is_dark = (m_settings.dark_mode != 0);
        uint32_t text_primary = is_dark ? 0xFFFFFFFF : 0xFF1D1D1F;
        uint32_t text_sec     = is_dark ? 0xFF8E95A5 : 0xFF6E6E73;
        uint32_t text_label   = is_dark ? 0xFFE0E5F0 : 0xFF2C3038;

        draw_string("Personalization", ox, oy, text_primary, m_fb, (uint32_t)m_w);
        draw_string("Accent color tint and system highlight styling", ox, oy + 22, text_sec, m_fb, (uint32_t)m_w);

        int card_y = oy + 55;
        int card_w = m_w - (ox + 24);
        kesh_draw_rounded_rect(m_fb, m_w, ox, card_y, card_w, 180, 12, is_dark ? 0x00232736 : 0x00FFFFFF);
        kesh_draw_rect(m_fb, m_w, ox + 6, card_y, card_w - 12, 1, is_dark ? 0x003D445C : 0x00D0D4DF);

        draw_string("System Accent Color", ox + 16, card_y + 16, text_label, m_fb, (uint32_t)m_w);
        draw_string("Applied to selection, active tabs, volume and window glow", ox + 16, card_y + 36, text_sec, m_fb, (uint32_t)m_w);

        int btn_y = card_y + 70;
        int gap = 16;
        int btn_w = (card_w - 32 - gap * 2) / 3, btn_h = 80;

        // 1. Amber / Orange
        int c0_x = ox + 16;
        bool c0_sel = (m_settings.accent_color == 0);
        kesh_draw_rounded_rect(m_fb, m_w, c0_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x002D3346 : 0x00EBF0F8);
        if (c0_sel) {
            kesh_draw_rounded_rect(m_fb, m_w, c0_x - 2, btn_y - 2, btn_w + 4, btn_h + 4, 12, 0x00FF9F0A);
            kesh_draw_rounded_rect(m_fb, m_w, c0_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x001B1E28 : 0x00FFFFFF);
        }
        draw_circle(m_fb, m_w, c0_x + btn_w / 2, btn_y + 28, 14, 0x00FF9F0A);
        draw_string("Amber", c0_x + (btn_w - font_text_width("Amber")) / 2, btn_y + 54, text_primary, m_fb, (uint32_t)m_w);

        // 2. Electric Blue
        int c1_x = c0_x + btn_w + gap;
        bool c1_sel = (m_settings.accent_color == 1);
        kesh_draw_rounded_rect(m_fb, m_w, c1_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x002D3346 : 0x00EBF0F8);
        if (c1_sel) {
            kesh_draw_rounded_rect(m_fb, m_w, c1_x - 2, btn_y - 2, btn_w + 4, btn_h + 4, 12, 0x000A84FF);
            kesh_draw_rounded_rect(m_fb, m_w, c1_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x001B1E28 : 0x00FFFFFF);
        }
        draw_circle(m_fb, m_w, c1_x + btn_w / 2, btn_y + 28, 14, 0x000A84FF);
        draw_string("Blue", c1_x + (btn_w - font_text_width("Blue")) / 2, btn_y + 54, text_primary, m_fb, (uint32_t)m_w);

        // 3. Crimson Red
        int c2_x = c1_x + btn_w + gap;
        bool c2_sel = (m_settings.accent_color == 2);
        kesh_draw_rounded_rect(m_fb, m_w, c2_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x002D3346 : 0x00EBF0F8);
        if (c2_sel) {
            kesh_draw_rounded_rect(m_fb, m_w, c2_x - 2, btn_y - 2, btn_w + 4, btn_h + 4, 12, 0x00FF453A);
            kesh_draw_rounded_rect(m_fb, m_w, c2_x, btn_y, btn_w, btn_h, 10, is_dark ? 0x001B1E28 : 0x00FFFFFF);
        }
        draw_circle(m_fb, m_w, c2_x + btn_w / 2, btn_y + 28, 14, 0x00FF453A);
        draw_string("Red", c2_x + (btn_w - font_text_width("Red")) / 2, btn_y + 54, text_primary, m_fb, (uint32_t)m_w);
    }

    void render_tab_window_controls(int ox, int oy) {
        bool is_dark = (m_settings.dark_mode != 0);
        uint32_t text_primary = is_dark ? 0xFFFFFFFF : 0xFF1D1D1F;
        uint32_t text_sec     = is_dark ? 0xFF8E95A5 : 0xFF6E6E73;
        uint32_t text_label   = is_dark ? 0xFFE0E5F0 : 0xFF2C3038;
        uint32_t accent       = get_accent();

        draw_string("Window Controls", ox, oy, text_primary, m_fb, (uint32_t)m_w);
        draw_string("Consistent traffic lights position across all windows", ox, oy + 22, text_sec, m_fb, (uint32_t)m_w);

        int card_y = oy + 55;
        int card_w = m_w - (ox + 24);
        kesh_draw_rounded_rect(m_fb, m_w, ox, card_y, card_w, 240, 12, is_dark ? 0x00232736 : 0x00FFFFFF);
        kesh_draw_rect(m_fb, m_w, ox + 6, card_y, card_w - 12, 1, is_dark ? 0x003D445C : 0x00D0D4DF);

        draw_string("Titlebar Button Alignment", ox + 16, card_y + 16, text_label, m_fb, (uint32_t)m_w);
        draw_string("Choose where close, minimize, and zoom buttons appear:", ox + 16, card_y + 36, text_sec, m_fb, (uint32_t)m_w);

        int seg_y = card_y + 65;
        int seg_w = (card_w - 46) / 2, seg_h = 38;

        bool is_left = (m_settings.window_controls_align == 0);
        bool is_right = (m_settings.window_controls_align == 1);

        // Кнопка Слева
        uint32_t col_left_bg = is_left ? accent : (is_dark ? 0x002D3346 : 0x00E4E7EE);
        kesh_draw_rounded_rect(m_fb, m_w, ox + 16, seg_y, seg_w, seg_h, 8, col_left_bg);
        draw_string("Left", ox + 16 + (seg_w - font_text_width("Left")) / 2, seg_y + 11, is_left ? 0xFFFFFFFF : text_primary, m_fb, (uint32_t)m_w);

        // Кнопка Справа
        int seg_r_x = ox + 16 + seg_w + 14;
        uint32_t col_right_bg = is_right ? accent : (is_dark ? 0x002D3346 : 0x00E4E7EE);
        kesh_draw_rounded_rect(m_fb, m_w, seg_r_x, seg_y, seg_w, seg_h, 8, col_right_bg);
        draw_string("Right", seg_r_x + (seg_w - font_text_width("Right")) / 2, seg_y + 11, is_right ? 0xFFFFFFFF : text_primary, m_fb, (uint32_t)m_w);

        // Интерактивное превью окна
        int prev_y = seg_y + 55;
        int prev_w = card_w - 32, prev_h = 65;
        kesh_draw_rounded_rect(m_fb, m_w, ox + 16, prev_y, prev_w, prev_h, 10, is_dark ? 0x00141620 : 0x00F0F2F6);
        kesh_draw_rounded_rect(m_fb, m_w, ox + 16, prev_y, prev_w, 24, 10, is_dark ? 0x002A2E40 : 0x00E0E4EC);
        kesh_draw_rect(m_fb, m_w, ox + 16, prev_y + 24, prev_w, 1, is_dark ? 0x00353C50 : 0x00D0D4DF);

        if (is_left) {
            kesh_draw_rounded_rect(m_fb, m_w, ox + 28, prev_y + 7, 10, 10, 5, 0xFFFF5F56);
            kesh_draw_rounded_rect(m_fb, m_w, ox + 43, prev_y + 7, 10, 10, 5, 0xFFFFBD2E);
            kesh_draw_rounded_rect(m_fb, m_w, ox + 58, prev_y + 7, 10, 10, 5, 0xFF27C93F);
        } else {
            kesh_draw_rounded_rect(m_fb, m_w, ox + 16 + prev_w - 22, prev_y + 7, 10, 10, 5, 0xFFFF5F56);
            kesh_draw_rounded_rect(m_fb, m_w, ox + 16 + prev_w - 37, prev_y + 7, 10, 10, 5, 0xFFFFBD2E);
            kesh_draw_rounded_rect(m_fb, m_w, ox + 16 + prev_w - 52, prev_y + 7, 10, 10, 5, 0xFF27C93F);
        }
        draw_string("Preview Window", ox + 16 + prev_w / 2 - 45, prev_y + 5, text_sec, m_fb, (uint32_t)m_w);

        const char *cfg_lbl = "Saved to /wm.conf: WINDOW_CONTROLS_ALIGN = ";
        draw_string(cfg_lbl, ox + 16, prev_y + prev_h + 12, text_sec, m_fb, (uint32_t)m_w);
        draw_string(is_left ? "LEFT" : "RIGHT", ox + 16 + font_text_width(cfg_lbl), prev_y + prev_h + 12, accent, m_fb, (uint32_t)m_w);
    }

    void render_tab_dock(int ox, int oy) {
        bool is_dark = (m_settings.dark_mode != 0);
        uint32_t text_primary = is_dark ? 0xFFFFFFFF : 0xFF1D1D1F;
        uint32_t text_sec     = is_dark ? 0xFF8E95A5 : 0xFF6E6E73;
        uint32_t text_label   = is_dark ? 0xFFE0E5F0 : 0xFF2C3038;
        uint32_t accent       = get_accent();

        draw_string("Dock & Navigation Bar", ox, oy, text_primary, m_fb, (uint32_t)m_w);
        draw_string("Magnification and shelf animations", ox, oy + 22, text_sec, m_fb, (uint32_t)m_w);

        int card_y = oy + 55;
        int card_w = m_w - (ox + 24);
        kesh_draw_rounded_rect(m_fb, m_w, ox, card_y, card_w, 180, 12, is_dark ? 0x00232736 : 0x00FFFFFF);
        kesh_draw_rect(m_fb, m_w, ox + 6, card_y, card_w - 12, 1, is_dark ? 0x003D445C : 0x00D0D4DF);

        draw_string("Icon Magnification", ox + 16, card_y + 18, text_label, m_fb, (uint32_t)m_w);
        draw_string("Expand icons on mouse hover", ox + 16, card_y + 36, text_sec, m_fb, (uint32_t)m_w);

        int sw_x = ox + card_w - 68;
        int sw_y = card_y + 18;
        int sw_w = 48, sw_h = 26;
        uint32_t sw_col = m_settings.dock_mag_enabled ? accent : (is_dark ? 0xFF353C4E : 0xFFD0D4DE);
        kesh_draw_rounded_rect(m_fb, m_w, sw_x, sw_y, sw_w, sw_h, sw_h / 2, sw_col);
        int knob_d = 20;
        int knob_x = m_settings.dock_mag_enabled ? (sw_x + sw_w - knob_d - 3) : (sw_x + 3);
        int knob_y = sw_y + (sw_h - knob_d) / 2;
        kesh_draw_rounded_rect(m_fb, m_w, knob_x, knob_y, knob_d, knob_d, knob_d / 2, 0xFFFFFFFF);

        draw_string("Magnification Strength", ox + 16, card_y + 75, text_label, m_fb, (uint32_t)m_w);

        char pct_buf[8];
        int_to_str(m_settings.dock_mag_level, pct_buf, sizeof(pct_buf));
        int plen = str_len(pct_buf);
        pct_buf[plen] = '%';
        pct_buf[plen + 1] = '\0';
        draw_string(pct_buf, ox + card_w - 65, card_y + 75, accent, m_fb, (uint32_t)m_w);

        int slider_x = ox + 16;
        int slider_y = card_y + 105;
        int slider_w = card_w - 32;
        int slider_h = 6;
        kesh_draw_rounded_rect(m_fb, m_w, slider_x, slider_y, slider_w, slider_h, 3, is_dark ? 0xFF141620 : 0xFFD8DCE5);

        int fill_w = (slider_w * m_settings.dock_mag_level) / 100;
        if (fill_w > slider_w) fill_w = slider_w;
        kesh_draw_rounded_rect(m_fb, m_w, slider_x, slider_y, fill_w, slider_h, 3,
            m_settings.dock_mag_enabled ? accent : 0xFF656D80);

        int kx = slider_x + fill_w - 9;
        int ky = slider_y + 3 - 9;
        kesh_draw_rounded_rect(m_fb, m_w, kx, ky, 18, 18, 9, 0xFFFFFFFF);
    }
};

} // namespace SettingsApp

extern "C" int main(void) {
    SettingsApp::SettingsWindow app(690, 430);
    if (!app.init()) {
        return 1;
    }
    app.run();
    return 0;
}
