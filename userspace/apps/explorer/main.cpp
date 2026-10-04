// проводник на c++
#include "kesh.h"
#include "kesh_ico.h"

inline void* operator new(size_t, void* p) noexcept { return p; }
inline void* operator new[](size_t, void* p) noexcept { return p; }
inline void operator delete(void*, void*) noexcept {}
inline void operator delete[](void*, void*) noexcept {}

namespace Kesh {

class ExplorerApp;
static ExplorerApp *volatile g_explorer_app = nullptr;

static int str_len(const char *s) {
    if (!s) return 0;
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void str_copy(char *dst, const char *src, int max_len) {
    if (!dst || max_len <= 0) return;
    int i = 0;
    if (src) {
        while (src[i] && i < max_len - 1) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static int str_cmp(const char *a, const char *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (int)(uint8_t)(*a) - (int)(uint8_t)(*b);
}

static int str_case_cmp(const char *a, const char *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return (int)(uint8_t)ca - (int)(uint8_t)cb;
        a++;
        b++;
    }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

static bool str_contains_case_insensitive(const char *haystack, const char *needle) {
    if (!needle || !needle[0]) return true;
    if (!haystack) return false;
    int nlen = str_len(needle);
    int hlen = str_len(haystack);
    if (nlen > hlen) return false;

    for (int i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (int j = 0; j < nlen; j++) {
            char ch = haystack[i + j];
            char cn = needle[j];
            if (ch >= 'A' && ch <= 'Z') ch += 32;
            if (cn >= 'A' && cn <= 'Z') cn += 32;
            if (ch != cn) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static void format_size(uint64_t bytes, char *out, int max_len) {
    if (!out || max_len <= 0) return;
    if (bytes >= 1024ULL * 1024 * 1024) {
        uint64_t gb = bytes / (1024ULL * 1024 * 1024);
        uint64_t rem = (bytes % (1024ULL * 1024 * 1024)) * 10 / (1024ULL * 1024 * 1024);
        char buf[32];
        int idx = 0;
        uint64_t tmp = gb;
        if (tmp == 0) buf[idx++] = '0';
        else {
            char rev[16]; int r = 0;
            while (tmp > 0) { rev[r++] = '0' + (tmp % 10); tmp /= 10; }
            while (r > 0) buf[idx++] = rev[--r];
        }
        buf[idx++] = '.';
        buf[idx++] = '0' + (char)rem;
        buf[idx++] = ' '; buf[idx++] = 'G'; buf[idx++] = 'B'; buf[idx] = '\0';
        str_copy(out, buf, max_len);
    } else if (bytes >= 1024ULL * 1024) {
        uint64_t mb = bytes / (1024ULL * 1024);
        uint64_t rem = (bytes % (1024ULL * 1024)) * 10 / (1024ULL * 1024);
        char buf[32];
        int idx = 0;
        uint64_t tmp = mb;
        if (tmp == 0) buf[idx++] = '0';
        else {
            char rev[16]; int r = 0;
            while (tmp > 0) { rev[r++] = '0' + (tmp % 10); tmp /= 10; }
            while (r > 0) buf[idx++] = rev[--r];
        }
        buf[idx++] = '.';
        buf[idx++] = '0' + (char)rem;
        buf[idx++] = ' '; buf[idx++] = 'M'; buf[idx++] = 'B'; buf[idx] = '\0';
        str_copy(out, buf, max_len);
    } else if (bytes >= 1024ULL) {
        uint64_t kb = bytes / 1024ULL;
        char buf[32];
        int idx = 0;
        uint64_t tmp = kb;
        if (tmp == 0) buf[idx++] = '0';
        else {
            char rev[16]; int r = 0;
            while (tmp > 0) { rev[r++] = '0' + (tmp % 10); tmp /= 10; }
            while (r > 0) buf[idx++] = rev[--r];
        }
        buf[idx++] = ' '; buf[idx++] = 'K'; buf[idx++] = 'B'; buf[idx] = '\0';
        str_copy(out, buf, max_len);
    } else {
        char buf[32];
        int idx = 0;
        uint64_t tmp = bytes;
        if (tmp == 0) buf[idx++] = '0';
        else {
            char rev[16]; int r = 0;
            while (tmp > 0) { rev[r++] = '0' + (tmp % 10); tmp /= 10; }
            while (r > 0) buf[idx++] = rev[--r];
        }
        buf[idx++] = ' '; buf[idx++] = 'B'; buf[idx] = '\0';
        str_copy(out, buf, max_len);
    }
}

static uint32_t get_file_badge_color(const char *name) {
    if (!name) return 0xFF9E9EA8;
    int len = str_len(name);
    if (len >= 4 && str_case_cmp(name + len - 4, ".kea") == 0) return 0xFF00C853; 
    if (len >= 4 && str_case_cmp(name + len - 4, ".elf") == 0) return 0xFF00E676; 
    if (len >= 4 && str_case_cmp(name + len - 4, ".txt") == 0) return 0xFF448AFF; 
    if (len >= 5 && str_case_cmp(name + len - 5, ".info") == 0) return 0xFF29B6F6; 
    if (len >= 4 && str_case_cmp(name + len - 4, ".bmp") == 0) return 0xFFFFAB00; 
    if (len >= 4 && str_case_cmp(name + len - 4, ".wav") == 0) return 0xFFE040FB; 
    if (len >= 4 && str_case_cmp(name + len - 4, ".wad") == 0) return 0xFFFF5252; 
    return 0xFF9E9EA8; 
}

struct FileItem {
    char name[64];
    bool is_dir;
    uint32_t size;
    uint32_t badge_color;
    char size_str[24];

    void init(const char *n, bool dir, uint32_t s) {
        str_copy(name, n, sizeof(name));
        is_dir = dir;
        size = s;
        badge_color = dir ? 0xFFFFC83B : get_file_badge_color(name);
        if (dir) {
            str_copy(size_str, "Folder", sizeof(size_str));
        } else {
            format_size(size, size_str, sizeof(size_str));
        }
    }
};

struct SidebarItem {
    char title[32];
    char path[128];
    char badge[16];
    uint32_t icon_color;
    bool is_device;
};

enum ViewMode {
    VIEW_GRID = 0,
    VIEW_LIST = 1
};

class DolphinRenderer {
public:
    static const int WIN_W = 800;
    static const int WIN_H = 530;
    static const int SIDEBAR_W = 190;
    static const int TOOLBAR_H = 50;
    static const int STATUSBAR_H = 28;

    inline static bool s_dark_mode = true;
    inline static int s_accent_color = 0;
    inline static uint8_t s_icon_data[3][10000];
    inline static kesh_ico_image_t s_icons[3];
    inline static int s_icon_ready[3];
    static void load_icons() {
        const char *paths[3] = { "/icons/explorer.ico", "/icons/folder.ico", "/icons/file.ico" };
        for (int i = 0; i < 3; ++i) s_icon_ready[i] = kesh_ico_load_vfs(paths[i], s_icon_data[i], sizeof(s_icon_data[i]), &s_icons[i]) == 0;
    }
    static void draw_icon(uint32_t *fb, int index, int x, int y, int size, uint32_t fallback) {
        if (index >= 0 && index < 3 && s_icon_ready[index]) kesh_ico_draw(fb, WIN_W, x, y, size, &s_icons[index]);
        else kesh_draw_rounded_rect(fb, WIN_W, x, y, size, size, size / 5, fallback);
    }
    static void set_dark_mode(bool dark) { s_dark_mode = dark; }
    static void set_accent_color(int acc) { s_accent_color = acc; }
    static uint32_t get_accent() {
        switch (s_accent_color) {
            case 1:  return 0xFF0A84FF;
            case 2:  return 0xFFFF453A;
            case 0:
            default: return 0xFFFF9F0A;
        }
    }
    static bool is_dark() { return s_dark_mode; }

    static void render_background(uint32_t *fb) {
        if (s_dark_mode) {
            kesh_draw_rect(fb, WIN_W, 0, 0, WIN_W, WIN_H, 0xFF1B1E24);
            kesh_draw_rect(fb, WIN_W, 0, 0, SIDEBAR_W, WIN_H, 0xFF23262E);
            kesh_draw_rect(fb, WIN_W, SIDEBAR_W - 1, 0, 1, WIN_H, 0xFF313642);
            kesh_draw_rect(fb, WIN_W, SIDEBAR_W, 0, WIN_W - SIDEBAR_W, TOOLBAR_H, 0xFF23262E);
            kesh_draw_rect(fb, WIN_W, SIDEBAR_W, TOOLBAR_H - 1, WIN_W - SIDEBAR_W, 1, 0xFF313642);
            kesh_draw_rect(fb, WIN_W, 0, WIN_H - STATUSBAR_H, WIN_W, STATUSBAR_H, 0xFF1F2229);
            kesh_draw_rect(fb, WIN_W, 0, WIN_H - STATUSBAR_H, WIN_W, 1, 0xFF313642);
        } else {
            kesh_draw_rect(fb, WIN_W, 0, 0, WIN_W, WIN_H, 0xFFFFFFFF);
            kesh_draw_rect(fb, WIN_W, 0, 0, SIDEBAR_W, WIN_H, 0xFFF0F2F6);
            kesh_draw_rect(fb, WIN_W, SIDEBAR_W - 1, 0, 1, WIN_H, 0xFFDFE2EB);
            kesh_draw_rect(fb, WIN_W, SIDEBAR_W, 0, WIN_W - SIDEBAR_W, TOOLBAR_H, 0xFFF7F8FA);
            kesh_draw_rect(fb, WIN_W, SIDEBAR_W, TOOLBAR_H - 1, WIN_W - SIDEBAR_W, 1, 0xFFDFE2EB);
            kesh_draw_rect(fb, WIN_W, 0, WIN_H - STATUSBAR_H, WIN_W, STATUSBAR_H, 0xFFF0F2F6);
            kesh_draw_rect(fb, WIN_W, 0, WIN_H - STATUSBAR_H, WIN_W, 1, 0xFFDFE2EB);
        }
    }

    static void render_sidebar_header(uint32_t *fb) {
        draw_icon(fb, 0, 14, 10, 30, get_accent());
        draw_string("Dolphin", 50, 13, s_dark_mode ? 0xFFFFFFFF : 0xFF1D1D1F, fb, WIN_W);
        draw_string("File Manager", 50, 26, s_dark_mode ? 0xFF7E8494 : 0xFF6E7382, fb, WIN_W);
    }

    static void render_sidebar(uint32_t *fb, const SidebarItem *items, int count, const char *curr_path, int hover_idx) {
        int y = 52;
        bool places_header_drawn = false;
        bool devices_header_drawn = false;

        uint32_t hdr_col = s_dark_mode ? 0xFF686F82 : 0xFF8A90A2;
        uint32_t div_col = s_dark_mode ? 0xFF2D323E : 0xFFDFE2EB;

        for (int i = 0; i < count; i++) {
            if (!items[i].is_device && !places_header_drawn) {
                draw_string("PLACES", 18, y + 4, hdr_col, fb, WIN_W);
                y += 24;
                places_header_drawn = true;
            } else if (items[i].is_device && !devices_header_drawn) {
                y += 10;
                kesh_draw_rect(fb, WIN_W, 16, y, SIDEBAR_W - 32, 1, div_col);
                y += 10;
                draw_string("DEVICES", 18, y + 4, hdr_col, fb, WIN_W);
                y += 24;
                devices_header_drawn = true;
            }

            bool is_active = (str_cmp(curr_path, items[i].path) == 0);
            bool is_hover = (hover_idx == i);

            if (is_active) {
                kesh_draw_rounded_rect(fb, WIN_W, 10, y, SIDEBAR_W - 20, 28, 5, s_dark_mode ? 0xFF2B4764 : 0xFFDFEBFF);
                kesh_draw_rect(fb, WIN_W, 10, y + 5, 3, 18, get_accent());
            } else if (is_hover) {
                kesh_draw_rounded_rect(fb, WIN_W, 10, y, SIDEBAR_W - 20, 28, 5, s_dark_mode ? 0xFF2B2F3A : 0xFFE8EEF8);
            }

            kesh_draw_rounded_rect(fb, WIN_W, 18, y + 6, 16, 16, 4, items[i].icon_color);
            uint32_t text_col = is_active ? (s_dark_mode ? 0xFFFFFFFF : get_accent()) : (is_hover ? (s_dark_mode ? 0xFFE0E3EC : 0xFF1D1D1F) : (s_dark_mode ? 0xFFA4A8B8 : 0xFF4A5060));
            draw_string(items[i].title, 42, y + 7, text_col, fb, WIN_W);

            if (items[i].badge[0] != '\0') {
                int bw = font_text_width(items[i].badge);
                draw_string(items[i].badge, SIDEBAR_W - 20 - bw, y + 7, s_dark_mode ? 0xFF6D7385 : 0xFF8A90A2, fb, WIN_W);
            }

            y += 32;
        }
    }

    static void render_toolbar(uint32_t *fb, const char *path, ViewMode view_mode, const char *search_query, bool search_active) {
        int x = SIDEBAR_W + 14;
        int y = 11;

        uint32_t btn_bg = s_dark_mode ? 0xFF2C313D : 0xFFFFFFFF;
        uint32_t btn_txt = s_dark_mode ? 0xFFDFE2EB : 0xFF1D1D1F;
        uint32_t field_bg = s_dark_mode ? 0xFF16181E : 0xFFFFFFFF;
        uint32_t field_border = s_dark_mode ? 0xFF1B1E24 : 0xFFD0D4DF;

        kesh_draw_rounded_rect(fb, WIN_W, x, y, 28, 28, 5, btn_bg);
        draw_string("<", x + 10, y + 7, btn_txt, fb, WIN_W);
        x += 34;

        kesh_draw_rounded_rect(fb, WIN_W, x, y, 28, 28, 5, btn_bg);
        draw_string("^", x + 10, y + 7, btn_txt, fb, WIN_W);
        x += 38;

        int breadcrumb_w = 280;
        kesh_draw_rounded_rect(fb, WIN_W, x, y, breadcrumb_w, 28, 5, field_border);
        kesh_draw_rounded_rect(fb, WIN_W, x + 1, y + 1, breadcrumb_w - 2, 26, 4, field_bg);

        char display_path[128];
        if (str_cmp(path, "/") == 0) {
            str_copy(display_path, "Root (/)", sizeof(display_path));
        } else if (str_cmp(path, "/hdd") == 0 || str_cmp(path, "/c") == 0) {
            str_copy(display_path, "Root > hdd", sizeof(display_path));
        } else if (str_cmp(path, "/cdrom") == 0) {
            str_copy(display_path, "Root > cdrom", sizeof(display_path));
        } else if (str_cmp(path, "/apps") == 0) {
            str_copy(display_path, "Root > apps", sizeof(display_path));
        } else if (str_cmp(path, "/bin") == 0) {
            str_copy(display_path, "Root > bin", sizeof(display_path));
        } else {
            str_copy(display_path, path, sizeof(display_path));
        }
        draw_string(display_path, x + 10, y + 7, s_dark_mode ? 0xFFE0E3EC : 0xFF1D1D1F, fb, WIN_W);
        x += breadcrumb_w + 14;

        int search_w = 150;
        kesh_draw_rounded_rect(fb, WIN_W, x, y, search_w, 28, 5, search_active ? 0xFF3DAEE9 : field_border);
        kesh_draw_rounded_rect(fb, WIN_W, x + 1, y + 1, search_w - 2, 26, 4, field_bg);

        if (search_query && search_query[0]) {
            draw_string(search_query, x + 10, y + 7, s_dark_mode ? 0xFFFFFFFF : 0xFF1D1D1F, fb, WIN_W);
        } else {
            draw_string("Filter...", x + 10, y + 7, s_dark_mode ? 0xFF5D6272 : 0xFF8A90A2, fb, WIN_W);
        }
        x += search_w + 14;

        kesh_draw_rounded_rect(fb, WIN_W, x, y, 62, 28, 5, btn_bg);
        if (view_mode == VIEW_GRID) {
            kesh_draw_rounded_rect(fb, WIN_W, x + 2, y + 2, 28, 24, 4, 0xFF3DAEE9);
            draw_string("::", x + 10, y + 7, 0xFFFFFFFF, fb, WIN_W);
            draw_string("=", x + 42, y + 7, s_dark_mode ? 0xFF8A90A2 : 0xFF686F82, fb, WIN_W);
        } else {
            kesh_draw_rounded_rect(fb, WIN_W, x + 32, y + 2, 28, 24, 4, 0xFF3DAEE9);
            draw_string("::", x + 10, y + 7, s_dark_mode ? 0xFF8A90A2 : 0xFF686F82, fb, WIN_W);
            draw_string("=", x + 42, y + 7, 0xFFFFFFFF, fb, WIN_W);
        }
    }
    static void render_grid_view(uint32_t *fb, const FileItem *items, int count, int selected_idx, int hover_idx) {
        int content_x = SIDEBAR_W + 16;
        int content_y = TOOLBAR_H + 16;
        int content_w = WIN_W - SIDEBAR_W - 32;

        int cols = 4;
        int card_w = (content_w - (cols - 1) * 12) / cols;
        int card_h = 92;

        if (count == 0) {
            draw_string("Folder is empty", content_x + 20, content_y + 40, s_dark_mode ? 0xFF6D7385 : 0xFF8A90A2, fb, WIN_W);
            return;
        }

        for (int i = 0; i < count; i++) {
            int row = i / cols;
            int col = i % cols;
            int x = content_x + col * (card_w + 12);
            int y = content_y + row * (card_h + 12);

            if (y + card_h > WIN_H - STATUSBAR_H) break;

            bool is_selected = (selected_idx == i);
            bool is_hover = (hover_idx == i);

            if (is_selected) {
                kesh_draw_rounded_rect(fb, WIN_W, x, y, card_w, card_h, 6, s_dark_mode ? 0xFF2C4362 : 0xFF0A84FF);
                kesh_draw_rounded_rect(fb, WIN_W, x + 1, y + 1, card_w - 2, card_h - 2, 5, s_dark_mode ? 0xFF1C2C40 : 0xFFE5EFFF);
            } else if (is_hover) {
                kesh_draw_rounded_rect(fb, WIN_W, x, y, card_w, card_h, 6, s_dark_mode ? 0xFF262A34 : 0xFFC8D6EC);
                kesh_draw_rounded_rect(fb, WIN_W, x + 1, y + 1, card_w - 2, card_h - 2, 5, s_dark_mode ? 0xFF20232B : 0xFFF0F4FA);
            } else {
                kesh_draw_rounded_rect(fb, WIN_W, x, y, card_w, card_h, 6, s_dark_mode ? 0xFF20232B : 0xFFE2E5ED);
                kesh_draw_rounded_rect(fb, WIN_W, x + 1, y + 1, card_w - 2, card_h - 2, 5, s_dark_mode ? 0xFF1A1C23 : 0xFFFFFFFF);
            }

            int icon_cx = x + card_w / 2;
            int icon_y = y + 12;

            if (items[i].is_dir) {
                draw_icon(fb, 1, icon_cx - 16, icon_y, 32, 0xFFE4BC65);
            } else {
                draw_icon(fb, 2, icon_cx - 14, icon_y, 28, items[i].badge_color);
            }

            int tw = font_text_width(items[i].name);
            int tx = icon_cx - tw / 2;
            if (tx < x + 6) tx = x + 6;
            uint32_t name_col = is_selected ? (s_dark_mode ? 0xFFFFFFFF : 0xFF0A84FF) : (s_dark_mode ? 0xFFDFE2EB : 0xFF1D1D1F);
            draw_string(items[i].name, tx, y + 54, name_col, fb, WIN_W);

            int sw = font_text_width(items[i].size_str);
            int sx = icon_cx - sw / 2;
            draw_string(items[i].size_str, sx, y + 70, s_dark_mode ? 0xFF6D7385 : 0xFF6E7382, fb, WIN_W);
        }
    }

    static void render_list_view(uint32_t *fb, const FileItem *items, int count, int selected_idx, int hover_idx) {
        int content_x = SIDEBAR_W + 16;
        int content_y = TOOLBAR_H + 12;
        int content_w = WIN_W - SIDEBAR_W - 32;

        uint32_t hdr_txt = s_dark_mode ? 0xFF686F82 : 0xFF8A90A2;
        draw_string("Name", content_x + 36, content_y + 4, hdr_txt, fb, WIN_W);
        draw_string("Type", content_x + 320, content_y + 4, hdr_txt, fb, WIN_W);
        draw_string("Size", content_x + 440, content_y + 4, hdr_txt, fb, WIN_W);
        kesh_draw_rect(fb, WIN_W, content_x, content_y + 20, content_w, 1, s_dark_mode ? 0xFF2D323E : 0xFFDFE2EB);

        content_y += 24;
        int row_h = 28;

        if (count == 0) {
            draw_string("Folder is empty", content_x + 20, content_y + 20, s_dark_mode ? 0xFF6D7385 : 0xFF8A90A2, fb, WIN_W);
            return;
        }

        for (int i = 0; i < count; i++) {
            int row_y = content_y + i * row_h;
            if (row_y + row_h > WIN_H - STATUSBAR_H) break;

            bool is_selected = (selected_idx == i);
            bool is_hover = (hover_idx == i);

            if (is_selected) {
                kesh_draw_rounded_rect(fb, WIN_W, content_x, row_y, content_w, row_h - 2, 4, s_dark_mode ? 0xFF2B4764 : 0xFFE2EAF8);
            } else if (is_hover) {
                kesh_draw_rounded_rect(fb, WIN_W, content_x, row_y, content_w, row_h - 2, 4, s_dark_mode ? 0xFF232731 : 0xFFF0F4FA);
            }

            if (items[i].is_dir) {
                draw_icon(fb, 1, content_x + 8, row_y + 4, 18, 0xFFE4BC65);
            } else {
                draw_icon(fb, 2, content_x + 8, row_y + 4, 16, items[i].badge_color);
            }

            uint32_t row_txt = is_selected ? (s_dark_mode ? 0xFFFFFFFF : 0xFF0A84FF) : (s_dark_mode ? 0xFFDFE2EB : 0xFF1D1D1F);
            uint32_t sub_txt = s_dark_mode ? 0xFF8F93A3 : 0xFF6E7382;
            draw_string(items[i].name, content_x + 36, row_y + 6, row_txt, fb, WIN_W);
            draw_string(items[i].is_dir ? "Folder" : "File", content_x + 320, row_y + 6, sub_txt, fb, WIN_W);
            draw_string(items[i].size_str, content_x + 440, row_y + 6, sub_txt, fb, WIN_W);
        }
    }

    static void render_statusbar(uint32_t *fb, int total_items, const FileItem *selected_item, uint64_t disk_total, uint64_t disk_free, const char *drive_name) {
        int y = WIN_H - STATUSBAR_H + 7;
        int x = 16;

        char buf[64];
        int idx = 0;
        int count = total_items;
        if (count == 0) buf[idx++] = '0';
        else {
            char rev[16]; int r = 0;
            while (count > 0) { rev[r++] = '0' + (count % 10); count /= 10; }
            while (r > 0) buf[idx++] = rev[--r];
        }
        const char *tail = " items";
        while (*tail) buf[idx++] = *tail++;
        buf[idx] = '\0';
        uint32_t sb_txt = s_dark_mode ? 0xFF7E8494 : 0xFF6E7382;
        draw_string(buf, x, y, sb_txt, fb, WIN_W);

        if (selected_item) {
            draw_string("|", x + 74, y, s_dark_mode ? 0xFF424856 : 0xFFB0B6C4, fb, WIN_W);
            draw_string(selected_item->name, x + 86, y, s_dark_mode ? 0xFFE0E3EC : 0xFF1D1D1F, fb, WIN_W);
        }

        if (disk_total > 0) {
            int slider_w = 100;
            int slider_h = 6;
            int slider_x = WIN_W - slider_w - 180;
            int slider_y = y + 4;

            kesh_draw_rounded_rect(fb, WIN_W, slider_x, slider_y, slider_w, slider_h, 3, s_dark_mode ? 0xFF2D323E : 0xFFDFE2EB);

            uint64_t used = (disk_total > disk_free) ? (disk_total - disk_free) : disk_total;
            int fill = (int)((used * (uint64_t)slider_w) / disk_total);
            if (fill < 2 && used > 0) fill = 2;
            if (fill > slider_w) fill = slider_w;
            if (fill > 0) {
                kesh_draw_rounded_rect(fb, WIN_W, slider_x, slider_y, fill, slider_h, 3, (disk_free == 0) ? 0xFFFFAB00 : 0xFF3DAEE9);
            }

            char free_str[32];
            format_size(disk_free, free_str, sizeof(free_str));
            char meter_txt[64];
            idx = 0;
            const char *dn = drive_name ? drive_name : "Free HDD";
            while (*dn) meter_txt[idx++] = *dn++;
            meter_txt[idx++] = ':'; meter_txt[idx++] = ' ';
            if (disk_free == 0) {
                const char *ro = "Read-Only";
                while (*ro) meter_txt[idx++] = *ro++;
            } else {
                const char *fp = free_str; while (*fp) meter_txt[idx++] = *fp++;
                const char *fr = " free"; while (*fr) meter_txt[idx++] = *fr++;
            }
            meter_txt[idx] = '\0';
            draw_string(meter_txt, slider_x + slider_w + 12, y, s_dark_mode ? 0xFF8A90A2 : 0xFF6E7382, fb, WIN_W);
        }
    }

    static void render_context_menu(uint32_t *fb, int menu_x, int menu_y, bool has_target, int hover_opt) {
        int w = 136;
        int h = has_target ? 78 : 54;
        uint32_t border_col = s_dark_mode ? 0xFF2A2E38 : 0xFFDFE2EB;
        uint32_t bg_col     = s_dark_mode ? 0xFF1E212A : 0xFFFFFFFF;
        uint32_t item_txt   = s_dark_mode ? 0xFFE0E3EC : 0xFF1D1D1F;
        uint32_t hov_bg     = s_dark_mode ? 0xFF3DAEE9 : 0xFFE5EFFF;
        uint32_t hov_txt    = s_dark_mode ? 0xFFFFFFFF : 0xFF0A84FF;

        kesh_draw_rounded_rect(fb, WIN_W, menu_x, menu_y, w, h, 6, border_col);
        kesh_draw_rounded_rect(fb, WIN_W, menu_x + 1, menu_y + 1, w - 2, h - 2, 5, bg_col);

        if (has_target) {
            if (hover_opt == 0) kesh_draw_rect(fb, WIN_W, menu_x + 2, menu_y + 4, w - 4, 22, hov_bg);
            draw_string("Open", menu_x + 12, menu_y + 8, (hover_opt == 0) ? hov_txt : item_txt, fb, WIN_W);

            if (hover_opt == 1) kesh_draw_rect(fb, WIN_W, menu_x + 2, menu_y + 27, w - 4, 22, hov_bg);
            draw_string("Delete", menu_x + 12, menu_y + 31, 0xFFFF5252, fb, WIN_W);

            if (hover_opt == 2) kesh_draw_rect(fb, WIN_W, menu_x + 2, menu_y + 50, w - 4, 22, hov_bg);
            draw_string("Refresh", menu_x + 12, menu_y + 54, (hover_opt == 2) ? hov_txt : item_txt, fb, WIN_W);
        } else {
            if (hover_opt == 0) kesh_draw_rect(fb, WIN_W, menu_x + 2, menu_y + 4, w - 4, 22, hov_bg);
            draw_string("New Folder", menu_x + 12, menu_y + 8, (hover_opt == 0) ? hov_txt : item_txt, fb, WIN_W);

            if (hover_opt == 1) kesh_draw_rect(fb, WIN_W, menu_x + 2, menu_y + 27, w - 4, 22, hov_bg);
            draw_string("Refresh", menu_x + 12, menu_y + 31, (hover_opt == 1) ? hov_txt : item_txt, fb, WIN_W);
        }
    }
};

class FileManager {
private:
    char m_current_path[128];
    FileItem m_items[128];
    int m_item_count;

    FileItem m_filtered_items[128];
    int m_filtered_count;

    char m_history[16][128];
    int m_history_count;

    char m_search_query[64];

public:
    FileManager() : m_item_count(0), m_filtered_count(0), m_history_count(0) {
        str_copy(m_current_path, "/", sizeof(m_current_path));
        m_search_query[0] = '\0';
    }

    const char* get_current_path() const { return m_current_path; }
    const char* get_search_query() const { return m_search_query; }
    int get_item_count() const { return m_filtered_count; }
    const FileItem* get_items() const { return m_filtered_items; }

    void refresh() {
        m_item_count = 0;
        m_filtered_count = 0;

        /* SYS_VFS_LIST has a bounded ABI buffer.  Asking for 128 made the
         * kernel reject the call and rendered every directory as empty. */
        static kesh_vfs_entry_t entries[64];
        int count = kesh_vfs_list(m_current_path, entries, 64);
        if (count < 0) count = 0;

        for (int i = 0; i < count && i < 64; i++) {
            m_items[m_item_count++].init(entries[i].name, entries[i].is_dir != 0, entries[i].size);
        }

        apply_filter();
    }

    void navigate_to(const char *path) {
        if (!path || !path[0]) return;
        if (str_cmp(m_current_path, path) == 0) return;

        if (m_history_count < 16) {
            str_copy(m_history[m_history_count++], m_current_path, sizeof(m_history[0]));
        }
        str_copy(m_current_path, path, sizeof(m_current_path));
        m_search_query[0] = '\0';
        refresh();
    }

    void navigate_back() {
        if (m_history_count > 0) {
            m_history_count--;
            str_copy(m_current_path, m_history[m_history_count], sizeof(m_current_path));
            m_search_query[0] = '\0';
            refresh();
        }
    }

    void navigate_up() {
        if (str_cmp(m_current_path, "/") == 0) return;
        int len = str_len(m_current_path);
        while (len > 0 && m_current_path[len - 1] == '/') len--;
        while (len > 0 && m_current_path[len - 1] != '/') len--;
        if (len <= 1) {
            navigate_to("/");
        } else {
            char parent[128];
            str_copy(parent, m_current_path, len);
            parent[len - 1] = '\0';
            navigate_to(parent);
        }
    }

    void set_search_query(const char *q) {
        str_copy(m_search_query, q, sizeof(m_search_query));
        apply_filter();
    }

    void open_item(int index) {
        if (index < 0 || index >= m_filtered_count) return;
        const FileItem &item = m_filtered_items[index];
        if (item.is_dir) {
            char new_path[128];
            int idx = 0;
            const char *p = m_current_path;
            while (*p) new_path[idx++] = *p++;
            if (idx > 0 && new_path[idx - 1] != '/') new_path[idx++] = '/';
            p = item.name;
            while (*p && idx < 127) new_path[idx++] = *p++;
            new_path[idx] = '\0';
            navigate_to(new_path);
        } else {
            int len = str_len(item.name);
            if ((len >= 4 && str_case_cmp(item.name + len - 4, ".kea") == 0) ||
                (len >= 4 && str_case_cmp(item.name + len - 4, ".elf") == 0)) {
                char full_path[128];
                int idx = 0;
                const char *p = m_current_path;
                while (*p) full_path[idx++] = *p++;
                if (idx > 0 && full_path[idx - 1] != '/') full_path[idx++] = '/';
                p = item.name;
                while (*p && idx < 127) full_path[idx++] = *p++;
                full_path[idx] = '\0';

                kesh_exec(full_path);
            }
        }
    }

    void create_folder(const char *base_name) {
        char full_path[128];
        int idx = 0;
        const char *p = m_current_path;
        while (*p) full_path[idx++] = *p++;
        if (idx > 0 && full_path[idx - 1] != '/') full_path[idx++] = '/';
        const char *bn = base_name ? base_name : "NewFolder";
        while (*bn && idx < 127) full_path[idx++] = *bn++;
        full_path[idx] = '\0';

        kesh_vfs_mkdir(full_path);
        refresh();
    }

    void delete_item(int index) {
        if (index < 0 || index >= m_filtered_count) return;
        const FileItem &item = m_filtered_items[index];
        char full_path[128];
        int idx = 0;
        const char *p = m_current_path;
        while (*p) full_path[idx++] = *p++;
        if (idx > 0 && full_path[idx - 1] != '/') full_path[idx++] = '/';
        const char *n = item.name;
        while (*n && idx < 127) full_path[idx++] = *n++;
        full_path[idx] = '\0';

        kesh_vfs_delete(full_path);
        refresh();
    }

private:
    void apply_filter() {
        m_filtered_count = 0;
        for (int i = 0; i < m_item_count; i++) {
            if (m_search_query[0] == '\0' || str_contains_case_insensitive(m_items[i].name, m_search_query)) {
                m_filtered_items[m_filtered_count++] = m_items[i];
            }
        }
    }
};

class ExplorerApp {
private:
    FileManager m_mgr;
    ViewMode m_view_mode;
    SidebarItem m_sidebar_items[16];
    int m_sidebar_count;

    int m_selected_idx;
    int m_hover_idx;
    int m_sidebar_hover_idx;

    bool m_search_focused;
    bool m_context_menu_open;
    int m_context_menu_x;
    int m_context_menu_y;
    int m_context_menu_hover;
    int m_context_target_idx;

    uint64_t m_disk_total;
    uint64_t m_disk_free;
    char m_active_drive_name[32];

public:
    ExplorerApp() :
        m_view_mode(VIEW_GRID),
        m_sidebar_count(0),
        m_selected_idx(-1),
        m_hover_idx(-1),
        m_sidebar_hover_idx(-1),
        m_search_focused(false),
        m_context_menu_open(false),
        m_context_menu_x(0),
        m_context_menu_y(0),
        m_context_menu_hover(-1),
        m_context_target_idx(-1),
        m_disk_total(0),
        m_disk_free(0)
    {
        str_copy(m_active_drive_name, "Free HDD", sizeof(m_active_drive_name));
    }

    void refresh_sidebar() {
        m_sidebar_count = 0;

        init_sidebar_item(m_sidebar_items[m_sidebar_count++], "Root (/)", "/", "", 0xFF3DAEE9, false);

        kesh_vfs_stat_t st;
        if (kesh_vfs_stat("/apps", &st) == 0 && st.is_dir) {
            init_sidebar_item(m_sidebar_items[m_sidebar_count++], "Applications", "/apps", "", 0xFF29B6F6, false);
        }
        if (kesh_vfs_stat("/bin", &st) == 0 && st.is_dir) {
            init_sidebar_item(m_sidebar_items[m_sidebar_count++], "Binaries", "/bin", "", 0xFFE040FB, false);
        }

        char hdd_badge[32];
        hdd_badge[0] = '\0';
        if (m_disk_total > 0) {
            format_size(m_disk_total, hdd_badge, sizeof(hdd_badge));
        }

        const char *hdd_label = (m_disk_free > 0 && m_disk_free != m_disk_total) ? "IDE Hard Disk" : "Free HDD";
        init_sidebar_item(m_sidebar_items[m_sidebar_count++], hdd_label, "/hdd", hdd_badge, 0xFF00E676, true);

        if (kesh_vfs_stat("/cdrom", &st) == 0 && st.is_dir) {
            init_sidebar_item(m_sidebar_items[m_sidebar_count++], "Optical Disc", "/cdrom", "ISO", 0xFFFFAB00, true);
        }
    }

    void do_full_refresh() {
        update_sysinfo();
        refresh_sidebar();
        m_mgr.refresh();
    }

    void run() {
        uint32_t *fb = kesh_create_window(DolphinRenderer::WIN_W, DolphinRenderer::WIN_H, "Dolphin");
        if (!fb) return;

        DolphinRenderer::load_icons();
        do_full_refresh();

        bool running = true;
        kesh_event_t ev;

        while (running) {
            ExplorerApp *app = g_explorer_app;
            app->render(fb);
            kesh_update_window(0);

            while (kesh_poll_event(0, &ev)) {
                if (ev.type == EVENT_CLOSE) {
                    running = false;
                    break;
                } else if (ev.type == EVENT_MOUSE_MOVE) {
                    app->handle_mouse_move(ev.mx, ev.my);
                } else if (ev.type == EVENT_MOUSE_DOWN) {
                    app->handle_mouse_down(ev.mx, ev.my, ev.btn);
                } else if (ev.type == EVENT_KEY_DOWN) {
                    app->handle_key_down(ev.key);
                }
            }

            kesh_sleep(16); 
        }
    }

private:
    void init_sidebar_item(SidebarItem &item, const char *title, const char *path, const char *badge, uint32_t icon_col, bool is_dev) {
        str_copy(item.title, title, sizeof(item.title));
        str_copy(item.path, path, sizeof(item.path));
        str_copy(item.badge, badge, sizeof(item.badge));
        item.icon_color = icon_col;
        item.is_device = is_dev;
    }

    void update_sysinfo() {
        kesh_sysinfo_t info;
        if (kesh_get_sysinfo(&info) == 0) {
            m_disk_total = info.disk_total_bytes;
            m_disk_free = info.disk_free_bytes;
            const char *curr = m_mgr.get_current_path();
            if (curr && str_cmp(curr, "/cdrom") == 0) {
                str_copy(m_active_drive_name, "Optical Disc", sizeof(m_active_drive_name));
            } else {
                str_copy(m_active_drive_name, "Free HDD", sizeof(m_active_drive_name));
            }
        }
    }

    void render(uint32_t *fb) {
        static int s_sett_tick = 0;
        if (s_sett_tick++ % 60 == 0) {
            kesh_settings_t st;
            if (kesh_get_settings(&st) == 0) {
                DolphinRenderer::set_dark_mode(st.dark_mode != 0);
                DolphinRenderer::set_accent_color(st.accent_color);
            }
        }

        DolphinRenderer::render_background(fb);
        DolphinRenderer::render_sidebar_header(fb);
        DolphinRenderer::render_sidebar(fb, m_sidebar_items, m_sidebar_count, m_mgr.get_current_path(), m_sidebar_hover_idx);
        DolphinRenderer::render_toolbar(fb, m_mgr.get_current_path(), m_view_mode, m_mgr.get_search_query(), m_search_focused);

        int count = m_mgr.get_item_count();
        const FileItem *items = m_mgr.get_items();

        if (m_view_mode == VIEW_GRID) {
            DolphinRenderer::render_grid_view(fb, items, count, m_selected_idx, m_hover_idx);
        } else {
            DolphinRenderer::render_list_view(fb, items, count, m_selected_idx, m_hover_idx);
        }

        const FileItem *sel_item = (m_selected_idx >= 0 && m_selected_idx < count) ? &items[m_selected_idx] : nullptr;
        DolphinRenderer::render_statusbar(fb, count, sel_item, m_disk_total, m_disk_free, m_active_drive_name);

        if (m_context_menu_open) {
            DolphinRenderer::render_context_menu(fb, m_context_menu_x, m_context_menu_y, m_context_target_idx >= 0, m_context_menu_hover);
        }
    }

    void handle_mouse_move(int mx, int my) {
        if (m_context_menu_open) {
            int mw = 136;
            int mh = (m_context_target_idx >= 0) ? 78 : 54;
            if (mx >= m_context_menu_x && mx < m_context_menu_x + mw &&
                my >= m_context_menu_y && my < m_context_menu_y + mh) {
                m_context_menu_hover = (my - m_context_menu_y - 4) / 23;
                return;
            } else {
                m_context_menu_hover = -1;
            }
        }

        m_sidebar_hover_idx = -1;
        if (mx >= 10 && mx < DolphinRenderer::SIDEBAR_W - 10) {
            int y = 52;
            bool places_header = false;
            bool devices_header = false;
            for (int i = 0; i < m_sidebar_count; i++) {
                if (!m_sidebar_items[i].is_device && !places_header) {
                    y += 24;
                    places_header = true;
                } else if (m_sidebar_items[i].is_device && !devices_header) {
                    y += 44; 
                    devices_header = true;
                }

                if (my >= y && my < y + 28) {
                    m_sidebar_hover_idx = i;
                    break;
                }
                y += 32;
            }
        }

        m_hover_idx = -1;
        int count = m_mgr.get_item_count();
        if (mx >= DolphinRenderer::SIDEBAR_W + 16 && mx < DolphinRenderer::WIN_W - 16 &&
            my >= DolphinRenderer::TOOLBAR_H + 12 && my < DolphinRenderer::WIN_H - DolphinRenderer::STATUSBAR_H) {
            if (m_view_mode == VIEW_GRID) {
                int content_x = DolphinRenderer::SIDEBAR_W + 16;
                int content_y = DolphinRenderer::TOOLBAR_H + 16;
                int content_w = DolphinRenderer::WIN_W - DolphinRenderer::SIDEBAR_W - 32;
                int cols = 4;
                int card_w = (content_w - (cols - 1) * 12) / cols;
                int card_h = 92;

                for (int i = 0; i < count; i++) {
                    int row = i / cols;
                    int col = i % cols;
                    int cx = content_x + col * (card_w + 12);
                    int cy = content_y + row * (card_h + 12);
                    if (mx >= cx && mx < cx + card_w && my >= cy && my < cy + card_h) {
                        m_hover_idx = i;
                        break;
                    }
                }
            } else {
                int content_y = DolphinRenderer::TOOLBAR_H + 12 + 24;
                int row_h = 28;
                for (int i = 0; i < count; i++) {
                    int ry = content_y + i * row_h;
                    if (my >= ry && my < ry + row_h) {
                        m_hover_idx = i;
                        break;
                    }
                }
            }
        }
    }

    void handle_mouse_down(int mx, int my, int btn) {
        if (m_context_menu_open) {
            if (m_context_menu_hover >= 0) {
                if (m_context_target_idx >= 0) {
                    if (m_context_menu_hover == 0) m_mgr.open_item(m_context_target_idx);
                    else if (m_context_menu_hover == 1) m_mgr.delete_item(m_context_target_idx);
                    else if (m_context_menu_hover == 2) do_full_refresh();
                } else {
                    if (m_context_menu_hover == 0) m_mgr.create_folder("NewFolder");
                    else if (m_context_menu_hover == 1) do_full_refresh();
                }
            }
            m_context_menu_open = false;
            return;
        }

        if (btn == 2) {
            m_context_menu_open = true;
            m_context_menu_x = mx;
            m_context_menu_y = my;
            m_context_menu_hover = -1;
            m_context_target_idx = m_hover_idx;
            return;
        }

        if (btn == 1) {

            if (m_sidebar_hover_idx >= 0 && m_sidebar_hover_idx < m_sidebar_count) {
                m_mgr.navigate_to(m_sidebar_items[m_sidebar_hover_idx].path);
                m_selected_idx = -1;
                update_sysinfo();
                return;
            }

            int nav_x = DolphinRenderer::SIDEBAR_W + 14;
            int nav_y = 11;

            if (mx >= nav_x && mx < nav_x + 28 && my >= nav_y && my < nav_y + 28) {
                m_mgr.navigate_back();
                m_selected_idx = -1;
                update_sysinfo();
                return;
            }
            nav_x += 34;

            if (mx >= nav_x && mx < nav_x + 28 && my >= nav_y && my < nav_y + 28) {
                m_mgr.navigate_up();
                m_selected_idx = -1;
                update_sysinfo();
                return;
            }
            nav_x += 38 + 280 + 14;

            int search_w = 150;
            if (mx >= nav_x && mx < nav_x + search_w && my >= nav_y && my < nav_y + 28) {
                m_search_focused = true;
                return;
            } else {
                m_search_focused = false;
            }
            nav_x += search_w + 14;

            if (mx >= nav_x && mx < nav_x + 62 && my >= nav_y && my < nav_y + 28) {
                m_view_mode = (m_view_mode == VIEW_GRID) ? VIEW_LIST : VIEW_GRID;
                return;
            }

            if (m_hover_idx >= 0) {
                if (m_selected_idx == m_hover_idx) {

                    m_mgr.open_item(m_hover_idx);
                    m_selected_idx = -1;
                    update_sysinfo();
                } else {
                    m_selected_idx = m_hover_idx;
                }
            } else {
                m_selected_idx = -1;
            }
        }
    }

    void handle_key_down(int key) {
        if (m_search_focused) {
            char query[64];
            str_copy(query, m_mgr.get_search_query(), sizeof(query));
            int len = str_len(query);

            if (key == 8) { 
                if (len > 0) {
                    query[len - 1] = '\0';
                    m_mgr.set_search_query(query);
                }
            } else if (key == 27 || key == '\n') { 
                m_search_focused = false;
            } else if (key >= 32 && key <= 126 && len < 62) {
                query[len] = (char)key;
                query[len + 1] = '\0';
                m_mgr.set_search_query(query);
            }
            return;
        }

        if (key == 8) { 
            m_mgr.navigate_back();
            m_selected_idx = -1;
            update_sysinfo();
        } else if (key == 27) { 
            m_selected_idx = -1;
            m_mgr.set_search_query("");
        } else if (key == '\n') { 
            if (m_selected_idx >= 0) {
                m_mgr.open_item(m_selected_idx);
                m_selected_idx = -1;
                update_sysinfo();
            }
        }
    }
};

} 

extern "C" int main(int argc, char **argv) {
    (void)argc; (void)argv;
    static Kesh::ExplorerApp app;
    Kesh::g_explorer_app = &app;
    app.run();
    return 0;
}
