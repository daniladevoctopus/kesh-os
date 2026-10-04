// рабочий стол, таскбар и отрисовка окон
#include "gui/desktop.h"
#include "gui/bmp_loader.h"
#include "gui/font.h"
#include "gui/wallpaper/wallpaper.h"
#include "gui/cursor/cursor.h"
#include "gui/apps/file/file_manager.h"
#include "gui/apps/music/music_app.h"
#include "gui/apps/about/about_app.h"
#include "gui/apps/calc/calc_app.h"
#include "gui/apps/terminal/terminal_app.h"
#include "gui/apps/settings/settings_app.h"
#include "drivers/system/keyboard.h"
#include "drivers/system/mouse.h"
#include "timer.h"
#include "gfx/gpu.h"
#include "uwindow.h"
#include "process.h"
#include "drm_fb.h"
#include "acpi.h"
#include "drivers/system/fat32.h"
#include "log.h"
#include "random.h"
#include "service.h"

extern void toggle_file_manager(void) __attribute__((weak));
extern void toggle_doom_app(void) __attribute__((weak));
extern void doom_app_close(void) __attribute__((weak));
extern void doom_app_feed_key(char key) __attribute__((weak));
extern void doom_app_feed_scancode(uint8_t sc, int ext, int pressed) __attribute__((weak));
extern int doom_app_is_open(void) __attribute__((weak));
extern void render_doom_app_window(uint32_t*, int, int, int, int, int, int) __attribute__((weak));

extern const uint8_t file_icon_bmp_start[] __attribute__((weak));
extern const uint8_t music_icon_bmp_start[] __attribute__((weak));
extern const uint8_t start_icon_bmp_start[] __attribute__((weak));
extern const uint8_t volume_bmp_start[] __attribute__((weak));
extern const uint8_t volume_mute_bmp_start[] __attribute__((weak));
extern const uint8_t volume_min_bmp_start[] __attribute__((weak));
extern const uint8_t volume_max_bmp_start[] __attribute__((weak));
extern const uint8_t wallpaper_bmp_start[] __attribute__((weak));
extern const uint8_t about_bmp_start[] __attribute__((weak));

extern const uint8_t doom_icon_bmp_start[] __attribute__((weak));
extern const uint8_t calc_icon_bmp_start[] __attribute__((weak));
extern const uint8_t notes_icon_bmp_start[] __attribute__((weak));
extern const uint8_t settings_icon_bmp_start[] __attribute__((weak));
extern const uint8_t terminal_icon_bmp_start[] __attribute__((weak));
extern const uint8_t kesh_icon_explorer_start[] __attribute__((weak));
extern const uint8_t kesh_icon_explorer_end[] __attribute__((weak));
extern const uint8_t kesh_icon_notepad_start[] __attribute__((weak));
extern const uint8_t kesh_icon_notepad_end[] __attribute__((weak));
extern const uint8_t kesh_icon_terminal_start[] __attribute__((weak));
extern const uint8_t kesh_icon_terminal_end[] __attribute__((weak));
extern const uint8_t kesh_icon_taskmgr_start[] __attribute__((weak));
extern const uint8_t kesh_icon_taskmgr_end[] __attribute__((weak));
extern const uint8_t kesh_icon_paint_start[] __attribute__((weak));
extern const uint8_t kesh_icon_paint_end[] __attribute__((weak));
extern const uint8_t kesh_icon_doom_start[] __attribute__((weak));
extern const uint8_t kesh_icon_doom_end[] __attribute__((weak));
extern const uint8_t kesh_icon_installer_start[] __attribute__((weak));
extern const uint8_t kesh_icon_installer_end[] __attribute__((weak));
extern const uint8_t kesh_icon_settings_start[] __attribute__((weak));
extern const uint8_t kesh_icon_settings_end[] __attribute__((weak));
extern const uint8_t kesh_icon_file_start[] __attribute__((weak));
extern const uint8_t kesh_icon_file_end[] __attribute__((weak));
extern const uint8_t kesh_icon_picture_start[] __attribute__((weak));
extern const uint8_t kesh_icon_picture_end[] __attribute__((weak));

#define TASKBAR_MARGIN_BOTTOM 10
#define TASKBAR_MARGIN_X      12
#define TASKBAR_HEIGHT        46
#define TASKBAR_CORNER_R      13

void blur_rect_buf(int x, int y, int w, int h, int radius);
void blur_rect_rounded_buf(int x, int y, int w, int h, int radius, int corner_r);
void render_layer_taskbar(int single_click);

extern const uint8_t wallpaper_day_bmp_start[] __attribute__((weak));
extern const uint8_t wallpaper_night_bmp_start[] __attribute__((weak));
int g_wallpaper_choice = 1;
int g_dark_mode = 1;
int g_accent_color = 0; // 0 = amber/orange (default), 1 = blue, 2 = red
/* Global UI preferences exposed to Ring 3 settings.  Themes intentionally use
   quiet paper/slate/linen palettes rather than neon desktop effects. */
int g_theme_id = 1;
int g_animations_enabled = 1;
int g_animation_speed = 65;
int g_interface_density = 1;
int g_show_seconds = 0;
int g_liquid_glass = 0;

uint32_t get_accent_color(void) {
    switch (g_accent_color) {
        case 1:  return 0x000A84FF; // Blue
        case 2:  return 0x00FF453A; // Red
        case 0:
        default: return 0x00FF9F0A; // Amber / Warm Orange
    }
}

int g_dock_mag_enabled = 1;
int g_dock_mag_level   = 55;  

const uint8_t* current_wallpaper_bmp(void) {
    switch (g_wallpaper_choice) {
        case 1: return wallpaper_day_bmp_start;
        case 2: return wallpaper_night_bmp_start;
        default: return wallpaper_bmp_start;
    }
}

static uint32_t scr_width  = 1280;
static uint32_t scr_height = 720;
static uint32_t scr_pitch  = 5120;
static uint32_t scr_bpp    = 32;

static uint8_t* frontbuffer = (uint8_t*)0xA0000;

#define MAX_WIDTH  2560
#define MAX_HEIGHT 1440

static uint32_t backbuffer[MAX_WIDTH * MAX_HEIGHT];
static uint32_t bg_buffer[MAX_WIDTH * MAX_HEIGHT];

static void desktop_swap_buffers(void);

#define COLOR_TOPBAR             0x00F6F6F6
#define COLOR_TOPBAR_BORDER     0x00D1D1D6
#define COLOR_BLACK             0x00000000
#define COLOR_WHITE             0x00FFFFFF
#define COLOR_DOCK_BG           0x00FFFFFF
#define COLOR_DOCK_BORDER       0x00E5E5EA
#define COLOR_TOOLTIP_BG        0x001C1C1E
#define COLOR_MENU_BG           0x00F0F0F3
#define COLOR_ACCENT            0x00007AFF
#define COLOR_MENU_ITEM_HOVER   0x00E1E1E6
#define COLOR_MENU_ITEM_PRESSED 0x00CACACF

typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} __attribute__((packed)) bmp_header_t;

typedef struct {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pixels_per_m;
    int32_t  y_pixels_per_m;
    uint32_t colors_used;
    uint32_t colors_important;
} __attribute__((packed)) bmp_info_header_t;

static void draw_scaled_ico(const uint8_t *data, const uint8_t *end, int x, int y, int size);

typedef struct {
    const char* title;
    const uint8_t* bmp_data;
    const uint8_t* ico_start;
    const uint8_t* ico_end;
    uint32_t fallback_color;
} dock_app_t;

static dock_app_t dock_apps[] = {
    {"Files", file_icon_bmp_start, kesh_icon_file_start, kesh_icon_file_end, 0x00007AFF},
    {"Terminal", terminal_icon_bmp_start, kesh_icon_file_start, kesh_icon_file_end, 0x001C1C1E},
    {"DOOM", doom_icon_bmp_start, kesh_icon_file_start, kesh_icon_file_end, 0x00B22222},
    {"Calculator", calc_icon_bmp_start, kesh_icon_file_start, kesh_icon_file_end, 0x00FF9500},
    {"Settings", settings_icon_bmp_start, kesh_icon_settings_start, kesh_icon_settings_end, 0x008E8E93},
    {"Music", music_icon_bmp_start, kesh_icon_picture_start, kesh_icon_picture_end, 0x00FF2D55},
    {"Notepad", notes_icon_bmp_start, kesh_icon_file_start, kesh_icon_file_end, 0x0034C759},
    {"TaskMgr", NULL, kesh_icon_file_start, kesh_icon_file_end, 0x00107C41},
    {"Paint", NULL, kesh_icon_file_start, kesh_icon_file_end, 0x00AF52DE}
};

static int app_count =
    sizeof(dock_apps) / sizeof(dock_app_t);

static int prev_mouse_left = 0;

static int g_drag_claimed_this_press = 0;

#define WIN_COUNT 8
enum { WIN_ID_FILE = 0, WIN_ID_MUSIC, WIN_ID_ABOUT, WIN_ID_CALC, WIN_ID_TERMINAL, WIN_ID_SETTINGS, WIN_ID_DOOM, WIN_ID_USER_APP };

static int g_win_z_order[WIN_COUNT] = { WIN_ID_FILE, WIN_ID_MUSIC, WIN_ID_ABOUT, WIN_ID_CALC, WIN_ID_TERMINAL, WIN_ID_SETTINGS, WIN_ID_DOOM, WIN_ID_USER_APP };
static int g_current_render_id = -1;

void win_bring_to_front(int id)
{
    int pos = -1;
    for (int i = 0; i < WIN_COUNT; i++) {
        if (g_win_z_order[i] == id) { pos = i; break; }
    }
    if (pos < 0) return;
    if (pos != WIN_COUNT - 1) {
        for (int i = pos; i < WIN_COUNT - 1; i++) g_win_z_order[i] = g_win_z_order[i + 1];
        g_win_z_order[WIN_COUNT - 1] = id;
    }
    win_set_focused(id);
    if (id == WIN_ID_USER_APP) uwindow_set_focused(1);
}

extern const uint8_t notepad_elf_start[] __attribute__((weak));
extern const uint8_t notepad_elf_end[] __attribute__((weak));
extern const uint8_t explorer_elf_start[] __attribute__((weak));
extern const uint8_t explorer_elf_end[] __attribute__((weak));

static user_window_t* find_uwindow_by_title_substr(const char *sub) {
    if (!sub) return NULL;
    int slen = 0;
    while (sub[slen]) slen++;
    for (int i = 0; i < MAX_USER_WINDOWS; i++) {
        user_window_t *w = uwindow_get(i);
        if (w && w->active && w->is_open) {
            for (int k = 0; w->title[k]; k++) {
                int match = 1;
                for (int m = 0; m < slen; m++) {
                    if (w->title[k + m] != sub[m]) { match = 0; break; }
                }
                if (match) return w;
            }
        }
    }
    return NULL;
}

void toggle_notepad_user_app(void)
{
    user_window_t *win = find_uwindow_by_title_substr("Notepad");
    if (win) {
        win->minimized = !win->minimized;
        if (!win->minimized) {
            win->anim_state = 1;
            win->anim_t = 0;
            win_bring_to_front(WIN_ID_USER_APP);
        }
    } else {
        process_t *p = process_spawn_path("/apps/notepad.kea");
        if (!p && notepad_elf_start && notepad_elf_end) {
            size_t elf_size = (size_t)(notepad_elf_end - notepad_elf_start);
            process_spawn_elf("Notepad", notepad_elf_start, elf_size);
        }
        win_bring_to_front(WIN_ID_USER_APP);
    }
}

void toggle_explorer_user_app(void)
{
    user_window_t *win = find_uwindow_by_title_substr("Dolphin");
    if (win) {
        win->minimized = !win->minimized;
        if (!win->minimized) {
            win->anim_state = 1;
            win->anim_t = 0;
            win_bring_to_front(WIN_ID_USER_APP);
        }
    } else {
        process_t *p = process_spawn_path("/apps/explorer.kea");
        if (!p && explorer_elf_start && explorer_elf_end) {
            size_t elf_size = (size_t)(explorer_elf_end - explorer_elf_start);
            process_spawn_elf("Explorer", explorer_elf_start, elf_size);
        }
        win_bring_to_front(WIN_ID_USER_APP);
    }
}

void toggle_taskmgr_user_app(void)
{
    user_window_t *win = find_uwindow_by_title_substr("Task");
    if (win) {
        win->minimized = !win->minimized;
        if (!win->minimized) {
            win->anim_state = 1;
            win->anim_t = 0;
            win_bring_to_front(WIN_ID_USER_APP);
        }
    } else {
        process_spawn_path("/apps/taskmgr.kea");
        win_bring_to_front(WIN_ID_USER_APP);
    }
}

void toggle_browser_user_app(void)
{
    user_window_t *win = find_uwindow_by_title_substr("Kesh Browser");
    if (win) {
        win->minimized = !win->minimized;
        if (!win->minimized) {
            win->anim_state = 1;
            win->anim_t = 0;
            win_bring_to_front(WIN_ID_USER_APP);
        }
        return;
    }
    process_spawn_path("/apps/browser.kea");
    win_bring_to_front(WIN_ID_USER_APP);
}

void toggle_paint_user_app(void)
{
    user_window_t *win = find_uwindow_by_title_substr("Paint");
    if (win) {
        win->minimized = !win->minimized;
        if (!win->minimized) {
            win->anim_state = 1;
            win->anim_t = 0;
            win_bring_to_front(WIN_ID_USER_APP);
        }
    } else {
        process_spawn_path("/apps/paint.kea");
        win_bring_to_front(WIN_ID_USER_APP);
    }
}

void toggle_installer_user_app(void)
{
    user_window_t *win = find_uwindow_by_title_substr("Install KeshOS");
    if (win) {
        win->minimized = !win->minimized;
        if (!win->minimized) { win->anim_state = 1; win->anim_t = 0; win_bring_to_front(WIN_ID_USER_APP); }
        return;
    }
    process_spawn_path("/apps/installer.kea");
    win_bring_to_front(WIN_ID_USER_APP);
}

extern const uint8_t settings_elf_start[] __attribute__((weak));
extern const uint8_t settings_elf_end[] __attribute__((weak));

void toggle_settings_user_app(void)
{
    user_window_t *win = find_uwindow_by_title_substr("Settings");
    if (win) {
        win->minimized = !win->minimized;
        if (!win->minimized) {
            win->anim_state = 1;
            win->anim_t = 0;
            win_bring_to_front(WIN_ID_USER_APP);
        }
        return;
    }
    process_t *p = process_spawn_path("/apps/settings.kea");
    if (!p) toggle_settings_app();
    win_bring_to_front(p ? WIN_ID_USER_APP : WIN_ID_SETTINGS);
}

extern const uint8_t about_elf_start[] __attribute__((weak));
extern const uint8_t about_elf_end[] __attribute__((weak));

void toggle_about_user_app(void)
{
    user_window_t *win = find_uwindow_by_title_substr("About");
    if (win) {
        win->minimized = !win->minimized;
        if (!win->minimized) {
            win->anim_state = 1;
            win->anim_t = 0;
            win_bring_to_front(WIN_ID_USER_APP);
        }
        return;
    }
    process_t *p = process_spawn_path("/apps/about.kea");
    if (!p) p = process_spawn_path("/apps/about.elf");
    if (!p && about_elf_start && about_elf_end) {
        size_t elf_size = (size_t)(about_elf_end - about_elf_start);
        process_spawn_elf("About", about_elf_start, elf_size);
    }
    if (!p && (!about_elf_start || !about_elf_end)) {
        toggle_about_app();
        win_bring_to_front(WIN_ID_ABOUT);
    }
}

static void win_drag_frame_begin(int btn)
{
    if (!btn)
        g_drag_claimed_this_press = 0;
}

int win_drag_available(void)
{
    return !g_drag_claimed_this_press;
}

void win_drag_claim(void)
{
    g_drag_claimed_this_press = 1;
    if (g_current_render_id >= 0)
        win_bring_to_front(g_current_render_id);
}

typedef struct { int x, y, w, h, open; } win_rect_t;
static win_rect_t g_win_rect[WIN_COUNT];

void win_report_rect(int id, int x, int y, int w, int h, int is_open)
{
    if (id < 0 || id >= WIN_COUNT) return;
    g_win_rect[id].x = x;
    g_win_rect[id].y = y;
    g_win_rect[id].w = w;
    g_win_rect[id].h = h;
    g_win_rect[id].open = is_open;
}

int win_is_open(int id)
{
    if (id == WIN_ID_USER_APP) return uwindow_is_any_open();
    if (id < 0 || id >= WIN_COUNT) return 0;
    return g_win_rect[id].open;
}

static int g_focused_win = -1;

void win_set_focused(int id) { g_focused_win = id; }
int win_is_focused(int id) {
    if (id == WIN_ID_USER_APP) return uwindow_is_focused();
    return g_focused_win == id;
}

static int win_z_index_of(int id)
{
    for (int i = 0; i < WIN_COUNT; i++)
        if (g_win_z_order[i] == id) return i;
    return -1;
}

int win_click_occluded(int id, int mx, int my)
{
    int my_z = win_z_index_of(id);
    if (my_z < 0) return 0;

    for (int i = 0; i < WIN_COUNT; i++) {
        int other = g_win_z_order[i];
        if (other == id) continue;
        if (i <= my_z) continue; 
        if (!g_win_rect[other].open) continue;

        int ox = g_win_rect[other].x, oy = g_win_rect[other].y;
        int ow = g_win_rect[other].w, oh = g_win_rect[other].h;

        if (mx >= ox && mx < ox + ow && my >= oy && my < oy + oh)
            return 1; 
    }
    return 0;
}

static int current_hover_idx = -1;
static int tooltip_alpha = 0;

static int start_menu_open = 0;
static int volume_popup_open = 0;
static int current_volume = 70;
static int g_vol_btn_x = 0;
static int g_vol_btn_w = 56;

static const uint8_t* current_volume_bmp(void)
{
    if (current_volume <= 0) {
        return volume_mute_bmp_start ? volume_mute_bmp_start : volume_bmp_start;
    } else if (current_volume < 50) {
        return volume_min_bmp_start ? volume_min_bmp_start : volume_bmp_start;
    } else {
        return volume_max_bmp_start ? volume_max_bmp_start : volume_bmp_start;
    }
}

static int cached_h = 0;
static int cached_m = 0;
static int cached_s = 0;

static int cached_day = 1;
static int cached_month = 1;
static int cached_year = 2026;

static uint64_t next_rtc_update_ms = 0;

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(val), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;

    __asm__ __volatile__(
        "inb %1, %0"
        : "=a"(ret)
        : "Nd"(port)
    );

    return ret;
}

static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ __volatile__(
        "outw %0, %1"
        :
        : "a"(val), "Nd"(port)
    );
}

void sys_shutdown(void)
{
    KLOG_NOTICE("power", "clean shutdown requested; persisting diagnostics");
    (void)random_save_persistent_seed();
    (void)klog_persist();
    (void)fat32_shutdown_clean();
    uint32_t total_pixels =
        scr_width * scr_height;

    if (total_pixels >
        (uint32_t)(MAX_WIDTH * MAX_HEIGHT))
    {
        total_pixels =
            MAX_WIDTH * MAX_HEIGHT;
    }

    for (uint32_t i = 0; i < total_pixels; i++)
    {
        backbuffer[i] = 0x00000000;
    }

    const char* msg1 =
        "KeshOS has shut down.";

    const char* msg2 =
        "It is now safe to close this window.";

    draw_string(
        msg1,
        ((int)scr_width -
         font_text_width(msg1)) / 2,
        (int)scr_height / 2 - 14,
        0x00FFFFFF,
        backbuffer,
        scr_width
    );

    draw_string(
        msg2,
        ((int)scr_width -
         font_text_width(msg2)) / 2,
        (int)scr_height / 2 + 6,
        0x008E8E93,
        backbuffer,
        scr_width
    );

    desktop_swap_buffers();

    (void)acpi_poweroff();

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    __asm__ __volatile__("cli");

    for (;;)
    {
        __asm__ __volatile__("hlt");
    }
}

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static uint8_t get_rtc_register(int reg)
{
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static int is_rtc_updating(void)
{
    outb(CMOS_ADDRESS, 0x0A);

    return (inb(CMOS_DATA) & 0x80);
}

static uint8_t bcd2bin(uint8_t val)
{
    return ((val / 16) * 10) +
           (val % 16);
}

static void update_rtc_cache(void)
{
    if (is_rtc_updating())
        return;

    uint8_t sec =
        get_rtc_register(0x00);

    uint8_t min =
        get_rtc_register(0x02);

    uint8_t hour =
        get_rtc_register(0x04);

    uint8_t d =
        get_rtc_register(0x07);

    uint8_t mo =
        get_rtc_register(0x08);

    uint8_t yr =
        get_rtc_register(0x09);

    uint8_t regB =
        get_rtc_register(0x0B);

    if (!(regB & 0x04))
    {
        sec  = bcd2bin(sec);
        min  = bcd2bin(min);
        hour = bcd2bin(hour);
        d    = bcd2bin(d);
        mo   = bcd2bin(mo);
        yr   = bcd2bin(yr);
    }

    cached_h = hour;
    cached_m = min;
    cached_s = sec;

    cached_day = d;
    cached_month = mo;
    cached_year = 2000 + yr;
}

uint32_t blend_colors(
    uint32_t bg,
    uint32_t fg,
    uint8_t alpha
)
{
    uint8_t r_bg =
        (bg >> 16) & 0xFF;

    uint8_t g_bg =
        (bg >> 8) & 0xFF;

    uint8_t b_bg =
        bg & 0xFF;

    uint8_t r_fg =
        (fg >> 16) & 0xFF;

    uint8_t g_fg =
        (fg >> 8) & 0xFF;

    uint8_t b_fg =
        fg & 0xFF;

    uint8_t r =
        (r_fg * alpha +
         r_bg * (255 - alpha)) / 255;

    uint8_t g =
        (g_fg * alpha +
         g_bg * (255 - alpha)) / 255;

    uint8_t b =
        (b_fg * alpha +
         b_bg * (255 - alpha)) / 255;

    return
        (r << 16) |
        (g << 8) |
        b;
}

void draw_pixel_buf(
    int x,
    int y,
    uint32_t color
)
{
    if (x < 0 ||
        x >= (int)scr_width ||
        y < 0 ||
        y >= (int)scr_height)
    {
        return;
    }

    uint32_t idx =
        y * scr_width + x;

    if (idx <
        (MAX_WIDTH * MAX_HEIGHT))
    {
        backbuffer[idx] = color;
    }
}

static void draw_pixel_blend(int x, int y, uint32_t color, uint8_t alpha) {
    if (x < 0 || x >= (int)scr_width || y < 0 || y >= (int)scr_height) return;
    uint32_t idx = y * scr_width + x;
    if (idx < (MAX_WIDTH * MAX_HEIGHT)) {
        backbuffer[idx] = blend_colors(backbuffer[idx], color, alpha);
    }
}

void draw_rect_buf(
    int x,
    int y,
    int w,
    int h,
    uint32_t color
)
{
    if (w <= 0 || h <= 0)
        return;

    for (int i = 0; i < h; i++)
    {
        for (int j = 0; j < w; j++)
        {
            draw_pixel_buf(
                x + j,
                y + i,
                color
            );
        }
    }
}

void draw_rounded_rect_buf(
    int x,
    int y,
    int w,
    int h,
    int r,
    uint32_t color
)
{
    if (w <= 0 || h <= 0)
        return;

    if (r < 1)
        r = 1;

    if (r * 2 > w)
        r = w / 2;

    if (r * 2 > h)
        r = h / 2;

    for (int i = 0; i < h; i++)
    {
        for (int j = 0; j < w; j++)
        {
            int rx = -1;
            int ry = -1;

            if (j < r && i < r)
            {
                rx = r - j - 1;
                ry = r - i - 1;
            }
            else if (
                j >= w - r &&
                i < r
            )
            {
                rx = j - (w - r);
                ry = r - i - 1;
            }
            else if (
                j < r &&
                i >= h - r
            )
            {
                rx = r - j - 1;
                ry = i - (h - r);
            }
            else if (
                j >= w - r &&
                i >= h - r
            )
            {
                rx = j - (w - r);
                ry = i - (h - r);
            }

            if (rx != -1 &&
                ry != -1)
            {
                int dist_sq = rx * rx + ry * ry;
                int r_sq = r * r;
                if (dist_sq <= r_sq)
                {
                    draw_pixel_buf(
                        x + j,
                        y + i,
                        color
                    );
                }
                else
                {

                    int r_outer_sq = (r + 1) * (r + 1);
                    if (dist_sq <= r_outer_sq)
                    {
                        draw_pixel_blend(x + j, y + i, color, 110);
                    }
                }
            }
            else
            {
                draw_pixel_buf(
                    x + j,
                    y + i,
                    color
                );
            }
        }
    }
}

void draw_rounded_rect_alpha(
    int x,
    int y,
    int w,
    int h,
    int r,
    uint32_t color,
    uint8_t alpha
)
{
    if (w <= 0 || h <= 0)
        return;

    for (int i = 0; i < h; i++)
    {
        for (int j = 0; j < w; j++)
        {
            int px = x + j;
            int py = y + i;

            if (
                px < 0 ||
                px >= (int)scr_width ||
                py < 0 ||
                py >= (int)scr_height
            )
            {
                continue;
            }

            int rx = -1;
            int ry = -1;

            if (j < r && i < r)
            {
                rx = r - j - 1;
                ry = r - i - 1;
            }
            else if (
                j >= w - r &&
                i < r
            )
            {
                rx = j - (w - r);
                ry = r - i - 1;
            }
            else if (
                j < r &&
                i >= h - r
            )
            {
                rx = r - j - 1;
                ry = i - (h - r);
            }
            else if (
                j >= w - r &&
                i >= h - r
            )
            {
                rx = j - (w - r);
                ry = i - (h - r);
            }

            int dist_sq = -1;
            if (rx != -1 && ry != -1) dist_sq = rx * rx + ry * ry;

            uint8_t pixel_alpha = alpha;
            if (dist_sq != -1)
            {
                int r_sq = r * r;
                if (dist_sq > r_sq)
                {
                    int r_outer_sq = (r + 1) * (r + 1);
                    if (dist_sq > r_outer_sq)
                    {
                        continue; 
                    }
                    pixel_alpha = (uint8_t)(alpha / 2); 
                }
            }

            uint32_t idx =
                py * scr_width + px;

            if (
                idx <
                (MAX_WIDTH * MAX_HEIGHT)
            )
            {
                backbuffer[idx] =
                    blend_colors(
                        backbuffer[idx],
                        color,
                        pixel_alpha
                    );
            }
        }
    }
}

void draw_scaled_bmp_rounded(
    const uint8_t* bmp_data,
    int x,
    int y,
    int target_w,
    int target_h,
    int r
)
{

    if (!bmp_data || target_w <= 0 || target_h <= 0)
        return;

    bmp_header_t* header = (bmp_header_t*)bmp_data;
    if (header->type != 0x4D42)
        return;

    bmp_info_header_t* info =
        (bmp_info_header_t*)(bmp_data + sizeof(bmp_header_t));

    int img_w = info->width;
    int img_h = info->height < 0 ? -info->height : info->height;
    if (img_w <= 0 || img_h <= 0)
        return;

    int is_bottom_up = info->height > 0;
    uint16_t bpp = info->bpp;
    const uint8_t* pixels = bmp_data + header->offset;
    int bytes_pp = bpp / 8;
    if (bytes_pp < 3 || bytes_pp > 4)
        return;

    int row_stride = ((img_w * bytes_pp + 3) / 4) * 4;
    if (r < 0) r = 0;
    if (r * 2 > target_w) r = target_w / 2;
    if (r * 2 > target_h) r = target_h / 2;

    for (int cy = 0; cy < target_h; cy++) {
        int src_y = (cy * img_h) / target_h;
        if (src_y >= img_h) src_y = img_h - 1;
        int py = is_bottom_up ? (img_h - 1 - src_y) : src_y;
        const uint8_t* row = pixels + py * row_stride;

        for (int cx = 0; cx < target_w; cx++) {
            if (r > 0) {
                int rx = -1, ry = -1;
                if (cx < r && cy < r) { rx = r - cx - 1; ry = r - cy - 1; }
                else if (cx >= target_w - r && cy < r) { rx = cx - (target_w - r); ry = r - cy - 1; }
                else if (cx < r && cy >= target_h - r) { rx = r - cx - 1; ry = cy - (target_h - r); }
                else if (cx >= target_w - r && cy >= target_h - r) { rx = cx - (target_w - r); ry = cy - (target_h - r); }
                if (rx >= 0 && ry >= 0 && rx * rx + ry * ry > r * r)
                    continue;
            }

            int src_x = (cx * img_w) / target_w;
            if (src_x >= img_w) src_x = img_w - 1;
            const uint8_t* p = row + src_x * bytes_pp;
            uint8_t b = p[0], g = p[1], rv = p[2];
            if (rv > 240 && g < 15 && b > 240) continue;
            if (bpp == 32) {
                if (p[3] < 8) continue;
                draw_pixel_blend(x + cx, y + cy, ((uint32_t)rv << 16) | ((uint32_t)g << 8) | b, p[3]);
            } else {
                draw_pixel_buf(x + cx, y + cy, ((uint32_t)rv << 16) | ((uint32_t)g << 8) | b);
            }
        }
    }
}

static void desktop_swap_buffers(void)
{
    uint32_t bytes_per_pixel = scr_bpp / 8;
    if (bytes_per_pixel == 0) bytes_per_pixel = 4;

    uint32_t copy_height = scr_height;
    uint32_t copy_width = scr_width;
    if (copy_height > MAX_HEIGHT) copy_height = MAX_HEIGHT;
    if (copy_width > MAX_WIDTH) copy_width = MAX_WIDTH;

    if (bytes_per_pixel == 4) {
        for (uint32_t row = 0; row < copy_height; ++row) {
            uint8_t *dst = frontbuffer + (uint64_t)row * scr_pitch;
            const uint32_t *src = backbuffer + (uint64_t)row * scr_width;
            kesh_gpu_copy(dst, src, (size_t)copy_width * sizeof(uint32_t));
        }
    } else if (bytes_per_pixel == 3) {
        for (uint32_t row = 0; row < copy_height; ++row) {
            uint8_t *dst = frontbuffer + (uint64_t)row * scr_pitch;
            const uint32_t *src = backbuffer + (uint64_t)row * scr_width;
            for (uint32_t col = 0; col < copy_width; ++col) {
                uint32_t px = src[col];
                dst[col * 3 + 0] = (uint8_t)(px & 0xFF);
                dst[col * 3 + 1] = (uint8_t)((px >> 8) & 0xFF);
                dst[col * 3 + 2] = (uint8_t)((px >> 16) & 0xFF);
            }
        }
    }
}

void render_layer_background(void)
{
    uint32_t total_pixels = scr_width * scr_height;
    if (total_pixels > MAX_WIDTH * MAX_HEIGHT)
        total_pixels = MAX_WIDTH * MAX_HEIGHT;

    uint32_t qwords = total_pixels / 2;
    uint64_t *dst64 = (uint64_t *)backbuffer;
    const uint64_t *src64 = (const uint64_t *)bg_buffer;
    for (uint32_t i = 0; i < qwords; ++i)
        dst64[i] = src64[i];

    if (total_pixels & 1U)
        backbuffer[total_pixels - 1] = bg_buffer[total_pixels - 1];
}

typedef struct {
    const char *title;
    const char *path;
    const uint8_t *ico_start;
    const uint8_t *ico_end;
    int x;
    int y;
} dicon_t;

static dicon_t s_desktop_icons[] = {
    { "Explorer", "/apps/explorer.kea", kesh_icon_file_start,      kesh_icon_file_end,      28,  36 },
    { "Notepad",  "/apps/notepad.kea",  kesh_icon_file_start,      kesh_icon_file_end,      124, 36 },
    { "Settings", "/apps/settings.kea", kesh_icon_settings_start,  kesh_icon_settings_end,  220, 36 },
    { "Terminal", "terminal",           kesh_icon_file_start,      kesh_icon_file_end,      28,  132 },
    { "TaskMgr",  "/apps/taskmgr.kea",  kesh_icon_file_start,      kesh_icon_file_end,      124, 132 },
    { "Paint",    "/apps/paint.kea",    kesh_icon_file_start,      kesh_icon_file_end,      220, 132 },
    { "Doom",     "doom",               kesh_icon_file_start,      kesh_icon_file_end,      28,  228 },
    { "Install",  "/apps/installer.kea",kesh_icon_installer_start, kesh_icon_installer_end, 124, 228 }
};
#define NUM_DESKTOP_ICONS 8

static uint16_t ico_u16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t ico_u32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static void draw_scaled_ico(const uint8_t *data, const uint8_t *end, int x, int y, int size) {
    if (!data || !end || end <= data || (uint64_t)(end - data) < 62 || ico_u16(data) || ico_u16(data + 2) != 1 || ico_u16(data + 4) < 1) return;
    uint32_t total = (uint32_t)(end - data), offset = ico_u32(data + 18), image_size = ico_u32(data + 14);
    if (offset > total || image_size > total - offset || image_size < 40) return;
    const uint8_t *dib = data + offset;
    int width = (int)ico_u32(dib + 4), height = (int)ico_u32(dib + 8) / 2;
    if (width < 1 || height < 1 || width > 256 || height > 256 || ico_u16(dib + 14) != 32 || ico_u32(dib + 16) != 0 || (uint32_t)width * (uint32_t)height * 4U > image_size - 40U) return;
    const uint8_t *pixels = dib + 40;
    for (int dy = 0; dy < size; ++dy) for (int dx = 0; dx < size; ++dx) {
        int sx = dx * width / size, sy = height - 1 - dy * height / size;
        const uint8_t *src = pixels + ((uint32_t)sy * (uint32_t)width + (uint32_t)sx) * 4U;
        uint32_t a = src[3]; if (!a) continue;
        uint32_t *dst = &backbuffer[(y + dy) * scr_width + x + dx];
        uint32_t bg = *dst, inv = 255U - a;
        uint32_t r = ((uint32_t)src[2] * a + ((bg >> 16) & 255U) * inv) / 255U;
        uint32_t g = ((uint32_t)src[1] * a + ((bg >> 8) & 255U) * inv) / 255U;
        uint32_t b = ((uint32_t)src[0] * a + (bg & 255U) * inv) / 255U;
        *dst = (r << 16) | (g << 8) | b;
    }
}

void render_desktop_icons(int single_click)
{

    for (int i = 0; i < NUM_DESKTOP_ICONS; i++) {
        dicon_t *di = &s_desktop_icons[i];
        int w = 82;
        int h = 84;
        int hover = (mouse_x >= di->x && mouse_x < di->x + w &&
                     mouse_y >= di->y && mouse_y < di->y + h);

        if (hover) {
            draw_rounded_rect_alpha(di->x, di->y, w, h, 10, 0x00FFFFFF, 35);
        }

        int tx = di->x + (w - 48) / 2;
        int ty = di->y + 4;
        draw_rounded_rect_alpha(tx - 2, ty + 2, 48, 48, 10, 0x00000000, 45);
        draw_scaled_ico(di->ico_start, di->ico_end, tx, ty, 48);

        int name_len = 0;
        while (di->title[name_len]) name_len++;
        int name_x = di->x + (w - name_len * 8) / 2;
        draw_string(di->title, name_x + 1, di->y + 61, 0x00000000, backbuffer, scr_width);
        draw_string(di->title, name_x, di->y + 60, 0x00FFFFFF, backbuffer, scr_width);

        if (hover && single_click) {
            if (i == 0) {
                toggle_explorer_user_app();
                win_bring_to_front(WIN_ID_USER_APP);
            } else if (i == 1) {
                toggle_notepad_user_app();
                win_bring_to_front(WIN_ID_USER_APP);
            } else if (i == 2) {
                toggle_settings_user_app();
            } else if (i == 3) {
                toggle_terminal_app();
                win_bring_to_front(WIN_ID_TERMINAL);
            } else if (i == 4) {
                toggle_taskmgr_user_app();
                win_bring_to_front(WIN_ID_USER_APP);
            } else if (i == 5) {
                toggle_paint_user_app();
                win_bring_to_front(WIN_ID_USER_APP);
            } else if (i == 6 && toggle_doom_app) {
                toggle_doom_app();
                win_bring_to_front(WIN_ID_DOOM);
            } else if (i == 7) {
                toggle_installer_user_app();
            }
        }
    }
}

static int s_ctx_open = 0;
static int s_ctx_x = 0;
static int s_ctx_y = 0;
static int s_prev_mouse_right = 0;

typedef struct {
    const char *label;
    const char *tag;
    int id;
} ctx_entry_t;

static const ctx_entry_t s_ctx_entries[] = {
    { "Open Explorer",    "[DIR]", 1 },
    { "Open Notepad",     "[TXT]", 2 },
    { "Task Manager",     "[TSK]", 6 },
    { "Kesh Paint",       "[ART]", 7 },
    { "Open Terminal",    "[CLI]", 3 },
    { "Next Wallpaper",   "[BG]",  4 },
    { "Settings",         "[CFG]", 5 }
};
#define NUM_CTX_ENTRIES 7

void render_desktop_context_menu(int single_click)
{
    int right_event = (mouse_right_clicked && !s_prev_mouse_right);
    s_prev_mouse_right = mouse_right_clicked;

    if (right_event) {
        if (mouse_y < (int)scr_height - 60) {
            s_ctx_open = 1;
            s_ctx_x = mouse_x;
            s_ctx_y = mouse_y;
            if (s_ctx_x + 160 > (int)scr_width) s_ctx_x = (int)scr_width - 165;
            if (s_ctx_y + 150 > (int)scr_height) s_ctx_y = (int)scr_height - 155;
            start_menu_open = 0;
        }
    }

    if (!s_ctx_open) return;

    int mw = 160;
    int mh = NUM_CTX_ENTRIES * 26 + 12;

    if (single_click) {
        if (mouse_x < s_ctx_x || mouse_x >= s_ctx_x + mw ||
            mouse_y < s_ctx_y || mouse_y >= s_ctx_y + mh) {
            s_ctx_open = 0;
            return;
        }
    }

    int is_dark = g_dark_mode;
    draw_rounded_rect_alpha(s_ctx_x - 3, s_ctx_y + 3, mw + 6, mh + 6, 10, 0x00000000, 60);
    draw_rounded_rect_buf(s_ctx_x, s_ctx_y, mw, mh, 8, is_dark ? 0x001B1E26 : 0x00F5F6F8);
    draw_rounded_rect_alpha(s_ctx_x, s_ctx_y, mw, mh, 8, is_dark ? 0x00FFFFFF : 0x00000000, is_dark ? 20 : 15);

    for (int i = 0; i < NUM_CTX_ENTRIES; i++) {
        int iy = s_ctx_y + 6 + i * 26;
        int hover = (mouse_x >= s_ctx_x + 4 && mouse_x < s_ctx_x + mw - 4 &&
                     mouse_y >= iy && mouse_y < iy + 24);

        if (hover) {
            draw_rounded_rect_alpha(s_ctx_x + 4, iy, mw - 8, 24, 6, is_dark ? 0x003A4D6B : 0x00007AFF, is_dark ? 180 : 50);
        }

        draw_string(s_ctx_entries[i].tag, s_ctx_x + 8, iy + 6, 0x0058A6FF, backbuffer, scr_width);
        draw_string(s_ctx_entries[i].label, s_ctx_x + 46, iy + 6, is_dark ? 0x00E5E5EA : 0x001D1D1F, backbuffer, scr_width);

        if (hover && single_click) {
            s_ctx_open = 0;
            int act = s_ctx_entries[i].id;
            if (act == 1) {
                toggle_explorer_user_app();
                win_bring_to_front(WIN_ID_USER_APP);
            } else if (act == 2) {
                toggle_notepad_user_app();
                win_bring_to_front(WIN_ID_USER_APP);
            } else if (act == 3) {
                toggle_terminal_app();
                win_bring_to_front(WIN_ID_TERMINAL);
            } else if (act == 4) {
                g_wallpaper_choice = (g_wallpaper_choice + 1) % 4;
            } else if (act == 5) {
                toggle_settings_user_app();
            } else if (act == 6) {
                toggle_taskmgr_user_app();
                win_bring_to_front(WIN_ID_USER_APP);
            } else if (act == 7) {
                toggle_paint_user_app();
                win_bring_to_front(WIN_ID_USER_APP);
            }
        }
    }
}

static int s_menu_anim_t = 0;

void render_start_menu(int single_click)
{
    if (start_menu_open) {
        if (s_menu_anim_t < 10) s_menu_anim_t++;
    } else {
        if (s_menu_anim_t > 0) s_menu_anim_t--;
    }

    if (s_menu_anim_t == 0)
        return;

    int p = (s_menu_anim_t * 256) / 10;
    int inv = 256 - p;
    int ease_p = 256 - ((inv * inv) >> 8);

    typedef struct {
        const char *name;
        uint32_t color;
        int action_id;
    } menu_item_t;

    static const menu_item_t items[] = {
        {"About KeshOS",   0x00FFFFFF, 0},
        {"File Manager",   0x00FFFFFF, 1},
        {"Terminal",       0x00FFFFFF, 2},
        {"Task Manager",   0x00FFFFFF, 7},
        {"Kesh Paint",     0x00FFFFFF, 8},
        {"DOOM",           0x00FFFFFF, 3},
        {"Calculator",     0x00FFFFFF, 4},
        {"Settings",       0x00FFFFFF, 5},
        {"Kesh Browser",   0x0064D2FF, 9},
        {"Shut Down...",   0x00FF453A, 6}
    };
    int item_count = sizeof(items) / sizeof(items[0]);

    int max_item_w = 0;
    for (int i = 0; i < item_count; i++) {
        int w = font_text_width(items[i].name);
        if (w > max_item_w) max_item_w = w;
    }
    int title_w = font_text_width("KeshOS 1.0 Drop");
    int sub_w = font_text_width("Build 950.x86_64");
    int header_text_w = (title_w > sub_w ? title_w : sub_w) + 50 + 20;
    int req_w = max_item_w + 36;
    if (header_text_w > req_w) req_w = header_text_w;
    int menu_w = (req_w > 240) ? req_w : 240;

    int bar_y = (int)scr_height - TASKBAR_HEIGHT - TASKBAR_MARGIN_BOTTOM;
    if (g_theme_id == 0) bar_y = 0;
    else if (g_theme_id == 2) bar_y = (int)scr_height - TASKBAR_HEIGHT - 18;
    int menu_h = 56 + item_count * 27 + 10;
    int menu_x = TASKBAR_MARGIN_X + 4;
    int base_menu_y = g_theme_id == 0 ? bar_y + TASKBAR_HEIGHT + 8 : bar_y - menu_h - 8;
    int slide_y = ((256 - ease_p) * 16) >> 8;
    int menu_y = g_theme_id == 0 ? base_menu_y - slide_y : base_menu_y + slide_y;

    if (s_menu_anim_t < 8 || !start_menu_open) single_click = 0;

    draw_rounded_rect_alpha(menu_x - 3, menu_y + 3, menu_w + 6, menu_h + 6, 14, 0x00000000, (50 * ease_p) >> 8);
    draw_rounded_rect_alpha(menu_x - 6, menu_y + 6, menu_w + 12, menu_h + 10, 16, 0x00000000, (30 * ease_p) >> 8);

    int is_dark = g_dark_mode;
    blur_rect_rounded_buf(menu_x, menu_y, menu_w, menu_h, 8, 12);

    if (is_dark) {
        draw_rounded_rect_alpha(menu_x, menu_y, menu_w, menu_h, 12, 0x0014161C, (220 * ease_p) >> 8);
        draw_rounded_rect_alpha(menu_x, menu_y, menu_w, menu_h, 12, 0x00FFFFFF, (25 * ease_p) >> 8);
        draw_rounded_rect_alpha(menu_x + 4, menu_y + 1, menu_w - 8, 1, 0, 0x00FFFFFF, (45 * ease_p) >> 8);
    } else {
        draw_rounded_rect_alpha(menu_x, menu_y, menu_w, menu_h, 12, 0x00F5F6F8, (225 * ease_p) >> 8);
        draw_rounded_rect_alpha(menu_x, menu_y, menu_w, menu_h, 12, 0x00FFFFFF, (130 * ease_p) >> 8);
        draw_rounded_rect_alpha(menu_x + 4, menu_y + 1, menu_w - 8, 1, 0, 0x00FFFFFF, (200 * ease_p) >> 8);
    }

    if (about_bmp_start)
    {
        draw_scaled_bmp_rounded(about_bmp_start, menu_x + 12, menu_y + 10, 30, 30, 0);
    }
    else
    {
        draw_rounded_rect_buf(menu_x + 12, menu_y + 10, 30, 30, 6, 0x00007AFF);
    }

    draw_string("KeshOS 1.0 Drop", menu_x + 50, menu_y + 10, is_dark ? COLOR_WHITE : 0x001D1D1F, backbuffer, scr_width);
    draw_string("Build 950.x86_64", menu_x + 50, menu_y + 24, is_dark ? 0x008E8E93 : 0x006E6E73, backbuffer, scr_width);

    draw_rect_buf(menu_x + 10, menu_y + 46, menu_w - 20, 1, is_dark ? 0x002D313B : 0x00D0D4DF);

    int item_x = menu_x + 8;
    int item_w = menu_w - 16;
    int item_h = 24;
    int start_y = menu_y + 52;
    int gap = 3;

    for (int i = 0; i < item_count; i++)
    {
        int cur_y = start_y + i * (item_h + gap);
        int hover = (mouse_x >= item_x && mouse_x < item_x + item_w &&
                     mouse_y >= cur_y && mouse_y < cur_y + item_h);

        if (hover)
        {
            uint8_t a = mouse_left_clicked ? 65 : 35;
            draw_rounded_rect_alpha(item_x, cur_y, item_w, item_h, 6, 0x00388BFD, a);
            draw_rounded_rect_alpha(item_x, cur_y, item_w, item_h, 6, 0x00FFFFFF, a / 2);
        }

        uint32_t it_col = (items[i].color == 0x00FFFFFF) ? (is_dark ? COLOR_WHITE : 0x001D1D1F) : items[i].color;
        draw_string((char*)items[i].name, item_x + 10, cur_y + 5, it_col, backbuffer, scr_width);

        if (single_click && hover)
        {
            start_menu_open = 0;
            switch (items[i].action_id)
            {
                case 0:
                    toggle_about_user_app();
                    win_bring_to_front(WIN_ID_USER_APP);
                    break;
                case 1:
                    toggle_explorer_user_app();
                    win_bring_to_front(WIN_ID_USER_APP);
                    break;
                case 2:
                    toggle_terminal_app();
                    win_bring_to_front(WIN_ID_TERMINAL);
                    break;
                case 3:
                    if (toggle_doom_app) {
                        toggle_doom_app();
                        win_bring_to_front(WIN_ID_DOOM);
                    }
                    break;
                case 4:
                    toggle_calc_app();
                    win_bring_to_front(WIN_ID_CALC);
                    break;
                case 5:
                    toggle_settings_user_app();
                    break;
                case 6:
                    sys_shutdown();
                    break;
                case 7:
                    toggle_taskmgr_user_app();
                    win_bring_to_front(WIN_ID_USER_APP);
                    break;
                case 8:
                    toggle_paint_user_app();
                    win_bring_to_front(WIN_ID_USER_APP);
                    break;
                case 9:
                    toggle_browser_user_app();
                    win_bring_to_front(WIN_ID_USER_APP);
                    break;
            }
        }
    }
}

void render_volume_popup(void)
{
    if (!volume_popup_open)
        return;

    char vol_str[16];
    int vi = 0;
    const char *vlabel = "Volume: ";
    for (int k = 0; vlabel[k]; k++) vol_str[vi++] = vlabel[k];
    if (current_volume >= 100) {
        vol_str[vi++] = '1'; vol_str[vi++] = '0'; vol_str[vi++] = '0';
    } else {
        if (current_volume >= 10) vol_str[vi++] = '0' + (current_volume / 10);
        vol_str[vi++] = '0' + (current_volume % 10);
    }
    vol_str[vi++] = '%';
    vol_str[vi] = 0;

    int text_w = font_text_width(vol_str);
    int pop_min_w = 12 + 16 + 8 + text_w + 16;
    int pop_w = (pop_min_w > 190) ? pop_min_w : 190;
    int pop_h = 56;
    int bar_y = (int)scr_height - TASKBAR_HEIGHT - TASKBAR_MARGIN_BOTTOM;
    int pop_y = bar_y - pop_h - 8;

    int center_anchor = (g_vol_btn_w > 0) ? (g_vol_btn_x + g_vol_btn_w / 2) : ((int)scr_width - 100);
    int pop_x = center_anchor - pop_w / 2;
    if (pop_x + pop_w > (int)scr_width - 12)
        pop_x = (int)scr_width - 12 - pop_w;
    if (pop_x < 12)
        pop_x = 12;

    draw_rounded_rect_alpha(pop_x - 3, pop_y + 3, pop_w + 6, pop_h + 6, 12, 0x00000000, 50);

    blur_rect_rounded_buf(pop_x, pop_y, pop_w, pop_h, 8, 10);

    int is_dark = g_dark_mode;
    if (is_dark) {
        draw_rounded_rect_alpha(pop_x, pop_y, pop_w, pop_h, 10, 0x0014161C, 220);
        draw_rounded_rect_alpha(pop_x, pop_y, pop_w, pop_h, 10, 0x00FFFFFF, 25);
        draw_rounded_rect_alpha(pop_x + 3, pop_y + 1, pop_w - 6, 1, 0, 0x00FFFFFF, 45);
    } else {
        draw_rounded_rect_alpha(pop_x, pop_y, pop_w, pop_h, 10, 0x00F5F6F8, 225);
        draw_rounded_rect_alpha(pop_x, pop_y, pop_w, pop_h, 10, 0x00FFFFFF, 130);
        draw_rounded_rect_alpha(pop_x + 3, pop_y + 1, pop_w - 6, 1, 0, 0x00FFFFFF, 200);
    }

    const uint8_t* vicon = current_volume_bmp();
    if (vicon) {
        draw_scaled_bmp_rounded(vicon, pop_x + 12, pop_y + 8, 16, 16, 3);
    }
    draw_string(vol_str, pop_x + 32, pop_y + 9, is_dark ? COLOR_WHITE : 0x001D1D1F, backbuffer, scr_width);

    int track_x = pop_x + 12;
    int track_y = pop_y + 30;
    int track_w = pop_w - 24;
    int track_h = 6;

    draw_rounded_rect_buf(track_x, track_y, track_w, track_h, 3, is_dark ? 0x00333844 : 0x00D8DCE5);

    int fill_w = (track_w * current_volume) / 100;
    if (fill_w > 0)
    {
        draw_rounded_rect_buf(track_x, track_y, fill_w, track_h, 3, get_accent_color());
    }

    int thumb_x = track_x + fill_w - 6;
    draw_rounded_rect_buf(thumb_x, track_y - 4, 12, 14, 4, COLOR_WHITE);

    if (mouse_left_clicked)
    {
        if (mouse_x >= track_x - 8 && mouse_x <= track_x + track_w + 8 &&
            mouse_y >= pop_y + 15 && mouse_y <= pop_y + pop_h)
        {
            int new_vol = ((mouse_x - track_x) * 100) / track_w;
            if (new_vol < 0) new_vol = 0;
            if (new_vol > 100) new_vol = 100;
            current_volume = new_vol;
        }
    }
}

void render_layer_topbar(int single_click)
{
    (void)single_click;
}

void blur_rect_buf(
    int x,
    int y,
    int w,
    int h,
    int radius
)
{
    if (!g_liquid_glass) return;
    if (radius <= 0)
        return;

    if (x < 0)
    {
        w += x;
        x = 0;
    }

    if (y < 0)
    {
        h += y;
        y = 0;
    }

    if (
        x + w >
        (int)scr_width
    )
    {
        w =
            (int)scr_width - x;
    }

    if (
        y + h >
        (int)scr_height
    )
    {
        h =
            (int)scr_height - y;
    }

    if (
        w <= 0 ||
        h <= 0 ||
        w > MAX_WIDTH ||
        h > MAX_HEIGHT
    )
    {
        return;
    }

    static uint32_t scratch[
        MAX_WIDTH
    ];

    for (int row = 0; row < h; row++)
    {
        uint32_t* line = &backbuffer[(y + row) * scr_width + x];
        int r = 0, g = 0, b = 0, cnt = 0;

        int init_max = (radius < w) ? radius : (w - 1);
        for (int k = 0; k <= init_max; k++)
        {
            uint32_t p = line[k];
            r += (p >> 16) & 0xFF;
            g += (p >> 8) & 0xFF;
            b += p & 0xFF;
            cnt++;
        }

        for (int col = 0; col < w; col++)
        {
            if (col > 0)
            {
                int add_idx = col + radius;
                if (add_idx < w)
                {
                    uint32_t p = line[add_idx];
                    r += (p >> 16) & 0xFF;
                    g += (p >> 8) & 0xFF;
                    b += p & 0xFF;
                    cnt++;
                }
                int sub_idx = col - 1 - radius;
                if (sub_idx >= 0)
                {
                    uint32_t p = line[sub_idx];
                    r -= (p >> 16) & 0xFF;
                    g -= (p >> 8) & 0xFF;
                    b -= p & 0xFF;
                    cnt--;
                }
            }

            scratch[col] =
                ((uint32_t)(r / cnt) << 16) |
                ((uint32_t)(g / cnt) << 8) |
                (uint32_t)(b / cnt);
        }

        for (int col = 0; col < w; col++)
        {
            line[col] = scratch[col];
        }
    }

    for (int col = 0; col < w; col++)
    {
        for (int row = 0; row < h; row++)
        {
            scratch[row] = backbuffer[(y + row) * scr_width + x + col];
        }

        int r = 0, g = 0, b = 0, cnt = 0;
        int init_max = (radius < h) ? radius : (h - 1);
        for (int k = 0; k <= init_max; k++)
        {
            uint32_t p = scratch[k];
            r += (p >> 16) & 0xFF;
            g += (p >> 8) & 0xFF;
            b += p & 0xFF;
            cnt++;
        }

        for (int row = 0; row < h; row++)
        {
            if (row > 0)
            {
                int add_idx = row + radius;
                if (add_idx < h)
                {
                    uint32_t p = scratch[add_idx];
                    r += (p >> 16) & 0xFF;
                    g += (p >> 8) & 0xFF;
                    b += p & 0xFF;
                    cnt++;
                }
                int sub_idx = row - 1 - radius;
                if (sub_idx >= 0)
                {
                    uint32_t p = scratch[sub_idx];
                    r -= (p >> 16) & 0xFF;
                    g -= (p >> 8) & 0xFF;
                    b -= p & 0xFF;
                    cnt--;
                }
            }

            backbuffer[(y + row) * scr_width + x + col] =
                ((uint32_t)(r / cnt) << 16) |
                ((uint32_t)(g / cnt) << 8) |
                (uint32_t)(b / cnt);
        }
    }
}

void blur_rect_rounded_buf(
    int x,
    int y,
    int w,
    int h,
    int radius,
    int corner_r
)
{
    if (w <= 0 || h <= 0)
        return;

    if (corner_r < 0)
        corner_r = 0;
    if (corner_r * 2 > w)
        corner_r = w / 2;
    if (corner_r * 2 > h)
        corner_r = h / 2;

    static uint32_t corner_backup[4][160 * 160];
    int cr = corner_r;
    if (cr > 160) cr = 160; 

    int x0 = x, y0 = y;
    int x1 = x + w, y1 = y + h;

    for (int cy = 0; cy < cr; cy++) {
        for (int cx = 0; cx < cr; cx++) {
            int px_tl = x0 + cx, py_tl = y0 + cy;
            int px_tr = x1 - cr + cx, py_tr = y0 + cy;
            int px_bl = x0 + cx, py_bl = y1 - cr + cy;
            int px_br = x1 - cr + cx, py_br = y1 - cr + cy;

            if (px_tl >= 0 && px_tl < (int)scr_width && py_tl >= 0 && py_tl < (int)scr_height)
                corner_backup[0][cy * 160 + cx] = backbuffer[py_tl * scr_width + px_tl];
            if (px_tr >= 0 && px_tr < (int)scr_width && py_tr >= 0 && py_tr < (int)scr_height)
                corner_backup[1][cy * 160 + cx] = backbuffer[py_tr * scr_width + px_tr];
            if (px_bl >= 0 && px_bl < (int)scr_width && py_bl >= 0 && py_bl < (int)scr_height)
                corner_backup[2][cy * 160 + cx] = backbuffer[py_bl * scr_width + px_bl];
            if (px_br >= 0 && px_br < (int)scr_width && py_br >= 0 && py_br < (int)scr_height)
                corner_backup[3][cy * 160 + cx] = backbuffer[py_br * scr_width + px_br];
        }
    }

    blur_rect_buf(x, y, w, h, radius);

    for (int cy = 0; cy < cr; cy++) {
        for (int cx = 0; cx < cr; cx++) {

            int dx = cr - cx - 1;
            int dy = cr - cy - 1;
            int outside = (dx * dx + dy * dy) > (cr * cr);

            if (!outside)
                continue;

            int px_tl = x0 + cx, py_tl = y0 + cy;
            int px_tr = x1 - cr + cx, py_tr = y0 + cy;
            int px_bl = x0 + cx, py_bl = y1 - cr + cy;
            int px_br = x1 - cr + cx, py_br = y1 - cr + cy;

            if (px_tl >= 0 && px_tl < (int)scr_width && py_tl >= 0 && py_tl < (int)scr_height)
                backbuffer[py_tl * scr_width + px_tl] = corner_backup[0][cy * 160 + cx];
            if (px_tr >= 0 && px_tr < (int)scr_width && py_tr >= 0 && py_tr < (int)scr_height)
                backbuffer[py_tr * scr_width + px_tr] = corner_backup[1][cy * 160 + cx];
            if (px_bl >= 0 && px_bl < (int)scr_width && py_bl >= 0 && py_bl < (int)scr_height)
                backbuffer[py_bl * scr_width + px_bl] = corner_backup[2][cy * 160 + cx];
            if (px_br >= 0 && px_br < (int)scr_width && py_br >= 0 && py_br < (int)scr_height)
                backbuffer[py_br * scr_width + px_br] = corner_backup[3][cy * 160 + cx];
        }
    }
}

#define DOCK_BASE_ICON   36
#define DOCK_MAX_ICON_CAP 72   
#define DOCK_INFLUENCE   80
#define DOCK_PAD_X       12
#define DOCK_PAD_Y        5
#define DOCK_SPACING     10

static int dock_iabs(int v) { return v < 0 ? -v : v; }

static int dock_max_icon_now(void) {
    if (!g_dock_mag_enabled) return DOCK_BASE_ICON;
    int lvl = g_dock_mag_level;
    if (lvl < 0) lvl = 0;
    if (lvl > 100) lvl = 100;

    return DOCK_BASE_ICON + ((DOCK_MAX_ICON_CAP - DOCK_BASE_ICON) * lvl) / 100;
}

static int dock_icon_size_for_dist(int dist) {
    int base = DOCK_BASE_ICON;
    int max_i = dock_max_icon_now();
    if (max_i <= base) return base;
    if (dist >= DOCK_INFLUENCE) return base;
    int t = DOCK_INFLUENCE - dist;
    int bump = (t * t) / DOCK_INFLUENCE;
    int size = base + ((max_i - base) * bump) / DOCK_INFLUENCE;
    if (size > max_i) size = max_i;
    if (size < base) size = base;
    return size;
}

static int g_dock_icon_x[16];
static int g_dock_icon_y[16];
static int g_dock_icon_size[16];
static int g_dock_icon_count = 0;
static int g_dock_icon_y_top = 0;

void genie_dock_icon_point(int dock_index, int *out_x, int *out_y)
{
    if (dock_index < 0 || dock_index >= g_dock_icon_count || g_dock_icon_count == 0) {
        *out_x = 40;
        *out_y = (int)scr_height - 30;
        return;
    }
    *out_x = g_dock_icon_x[dock_index] + g_dock_icon_size[dock_index] / 2;
    *out_y = g_dock_icon_y_top;
}

void render_layer_taskbar(int single_click)
{
    int bar_margin_bottom = TASKBAR_MARGIN_BOTTOM;
    int bar_margin_x = TASKBAR_MARGIN_X;
    int bar_h = TASKBAR_HEIGHT;
    int bar_w = (int)scr_width - bar_margin_x * 2;
    int bar_x = bar_margin_x;
    int bar_y = (int)scr_height - bar_h - bar_margin_bottom;
    int bar_r = TASKBAR_CORNER_R;

    /* Three actual layouts: Paper uses a quiet top bar, Slate keeps the
       floating dock, Linen narrows it into a centred shelf. */
    if (g_theme_id == 0) {
        bar_margin_bottom = 0; bar_margin_x = 0; bar_h = 40;
        bar_w = (int)scr_width; bar_x = 0; bar_y = 0; bar_r = 0;
    } else if (g_theme_id == 2) {
        bar_margin_x = (int)scr_width / 5;
        bar_w = (int)scr_width - bar_margin_x * 2;
        bar_x = bar_margin_x;
        bar_y = (int)scr_height - bar_h - 18;
        bar_r = 18;
    }

    int is_dark = g_dark_mode;

    draw_rounded_rect_alpha(bar_x - 3, bar_y + 3, bar_w + 6, bar_h + 4, bar_r + 2, 0x00000000, 35);
    draw_rounded_rect_alpha(bar_x - 6, bar_y + 6, bar_w + 12, bar_h + 8, bar_r + 4, 0x00000000, 20);

    blur_rect_rounded_buf(bar_x, bar_y, bar_w, bar_h, 8, bar_r);

    if (is_dark) {
        draw_rounded_rect_alpha(bar_x, bar_y, bar_w, bar_h, bar_r, 0x0014161C, 200);
        draw_rounded_rect_alpha(bar_x, bar_y, bar_w, bar_h, bar_r, 0x00FFFFFF, 22);
        draw_rounded_rect_alpha(bar_x + 4, bar_y + 1, bar_w - 8, 1, 0, 0x00FFFFFF, 45);
    } else if (g_theme_id == 2) {
        draw_rounded_rect_alpha(bar_x, bar_y, bar_w, bar_h, bar_r, 0x00FFF8EF, 238);
        draw_rounded_rect_alpha(bar_x, bar_y, bar_w, bar_h, bar_r, 0x00A98262, 32);
    } else {
        draw_rounded_rect_alpha(bar_x, bar_y, bar_w, bar_h, bar_r, 0x00F0F2F6, 195);
        draw_rounded_rect_alpha(bar_x, bar_y, bar_w, bar_h, bar_r, 0x00FFFFFF, 120);
        draw_rounded_rect_alpha(bar_x + 4, bar_y + 1, bar_w - 8, 1, 0, 0x00FFFFFF, 200);
    }

    int start_btn_x = bar_x + 6;
    int start_btn_w = 40;
    int start_btn_h = 36;
    int start_btn_y = bar_y + (bar_h - start_btn_h) / 2;
    int start_hover = (mouse_x >= start_btn_x && mouse_x < start_btn_x + start_btn_w &&
                       mouse_y >= start_btn_y && mouse_y < start_btn_y + start_btn_h);

    if (start_menu_open || s_menu_anim_t > 0) {
        draw_rounded_rect_alpha(start_btn_x, start_btn_y, start_btn_w, start_btn_h, 8, is_dark ? 0x003A4050 : 0x00D0D8E8, 140);
        draw_rounded_rect_alpha(start_btn_x, start_btn_y, start_btn_w, start_btn_h, 8, 0x00007AFF, 80);
    } else if (start_hover) {
        draw_rounded_rect_alpha(start_btn_x, start_btn_y, start_btn_w, start_btn_h, 8, is_dark ? 0x00FFFFFF : 0x00000000, is_dark ? 35 : 20);
    }

    if (about_bmp_start) {
        draw_scaled_bmp_rounded(about_bmp_start, start_btn_x + 7, start_btn_y + 5, 26, 26, 0);
    } else {
        draw_rounded_rect_buf(start_btn_x + 9, start_btn_y + 7, 22, 22, 5, 0x00007AFF);
    }

    if (single_click && start_hover) {
        start_menu_open = !start_menu_open;
        volume_popup_open = 0;
    }

    int div_x = start_btn_x + start_btn_w + 6;
    draw_rect_buf(div_x, bar_y + 11, 1, bar_h - 22, is_dark ? 0x00323640 : 0x00D0D4DE);

    static const int dock_to_win[] = {
        WIN_ID_FILE, WIN_ID_TERMINAL, WIN_ID_DOOM, WIN_ID_CALC, WIN_ID_SETTINGS, WIN_ID_MUSIC, WIN_ID_USER_APP, WIN_ID_USER_APP, WIN_ID_USER_APP
    };

    int tile_w = 40;
    int tile_h = 36;
    int tile_y = bar_y + (bar_h - tile_h) / 2;
    int apps_start_x = div_x + 8;
    int icon_inner_size = 28;

    int hovered_app_idx = -1;

    for (int i = 0; i < app_count && i < 16; i++) {
        int cur_x = apps_start_x + i * (tile_w + 4);
        int cur_hover = (mouse_x >= cur_x && mouse_x < cur_x + tile_w &&
                         mouse_y >= tile_y && mouse_y < tile_y + tile_h);

        int wid = (i < 9) ? dock_to_win[i] : -1;
        int is_open = 0;
        int is_focused = 0;

        if (i >= 6 && i < 9) {
            static const char *uapp_title[] = { "Notepad", "Task", "Paint" };
            user_window_t *uw = find_uwindow_by_title_substr(uapp_title[i - 6]);
            is_open = (uw != NULL);
            if (uw) {
                int idx = ((char*)uw - (char*)uwindow_get(0)) / sizeof(user_window_t);
                is_focused = uwindow_is_focused_idx(idx);
            }
        } else {
            is_open = (wid >= 0 && wid < WIN_COUNT) ? win_is_open(wid) : 0;
            is_focused = (wid >= 0 && wid < WIN_COUNT) ? win_is_focused(wid) : 0;
        }

        g_dock_icon_x[i] = cur_x + (tile_w - icon_inner_size) / 2;
        g_dock_icon_y[i] = tile_y + (tile_h - icon_inner_size) / 2;
        g_dock_icon_size[i] = icon_inner_size;

        if (is_focused && is_open) {
            draw_rounded_rect_alpha(cur_x, tile_y, tile_w, tile_h, 8, is_dark ? 0x003B4252 : 0x00D0D8E8, 140);
            draw_rounded_rect_alpha(cur_x, tile_y, tile_w, tile_h, 8, get_accent_color(), is_dark ? 30 : 45);
        } else if (cur_hover) {
            draw_rounded_rect_alpha(cur_x, tile_y, tile_w, tile_h, 8, is_dark ? 0x00FFFFFF : 0x00000000, is_dark ? 35 : 20);
        } else if (is_open) {
            draw_rounded_rect_alpha(cur_x, tile_y, tile_w, tile_h, 8, is_dark ? 0x00FFFFFF : 0x00000000, is_dark ? 15 : 12);
        }

        int icon_draw_x = cur_x + (tile_w - icon_inner_size) / 2;
        int icon_draw_y = tile_y + (tile_h - icon_inner_size) / 2;

        if (dock_apps[i].ico_start && dock_apps[i].ico_end) {
            draw_scaled_ico(dock_apps[i].ico_start, dock_apps[i].ico_end, icon_draw_x, icon_draw_y, icon_inner_size);
        } else if (dock_apps[i].bmp_data) {
            draw_scaled_bmp_rounded(dock_apps[i].bmp_data, icon_draw_x, icon_draw_y,
                                    icon_inner_size, icon_inner_size, 6);
        } else {
            draw_rounded_rect_buf(icon_draw_x, icon_draw_y,
                                  icon_inner_size, icon_inner_size, 6,
                                  dock_apps[i].fallback_color);
        }

        if (is_open) {
            int ind_w = is_focused ? 18 : 8;
            int ind_x = cur_x + (tile_w - ind_w) / 2;
            int ind_y = tile_y + tile_h - 3;
            uint32_t ind_col = is_focused ? get_accent_color() : (is_dark ? 0x00A0A5B4 : 0x006E6E73);
            draw_rounded_rect_buf(ind_x, ind_y, ind_w, 2, 1, ind_col);
        }

        if (cur_hover) {
            hovered_app_idx = i;
            if (single_click) {
                if (i == 0) {
                    toggle_explorer_user_app();
                    win_bring_to_front(WIN_ID_USER_APP);
                } else if (i == 1) {
                    toggle_terminal_app();
                    win_bring_to_front(WIN_ID_TERMINAL);
                } else if (i == 2 && toggle_doom_app) {
                    toggle_doom_app();
                    win_bring_to_front(WIN_ID_DOOM);
                } else if (i == 3) {
                    toggle_calc_app();
                    win_bring_to_front(WIN_ID_CALC);
                } else if (i == 4) {
                    toggle_settings_user_app();
                } else if (i == 5) {
                    toggle_music_app();
                    win_bring_to_front(WIN_ID_MUSIC);
                } else if (i == 6) {
                    toggle_notepad_user_app();
                    win_bring_to_front(WIN_ID_USER_APP);
                } else if (i == 7) {
                    toggle_taskmgr_user_app();
                    win_bring_to_front(WIN_ID_USER_APP);
                } else if (i == 8) {
                    toggle_paint_user_app();
                    win_bring_to_front(WIN_ID_USER_APP);
                }
            }
        }
    }

    g_dock_icon_count = app_count;
    g_dock_icon_y_top = tile_y + tile_h / 2;

    int desk_sliver_w = 6;
    int desk_sliver_x = bar_x + bar_w - desk_sliver_w - 4;
    int desk_sliver_y = bar_y + (bar_h - 24) / 2;
    int desk_hover = (mouse_x >= desk_sliver_x && mouse_x < desk_sliver_x + desk_sliver_w &&
                      mouse_y >= desk_sliver_y && mouse_y < desk_sliver_y + 24);

    if (desk_hover) {
        draw_rounded_rect_alpha(desk_sliver_x - 1, desk_sliver_y, desk_sliver_w + 2, 24, 3, is_dark ? 0x00FFFFFF : 0x00000000, is_dark ? 50 : 25);
    }
    draw_rounded_rect_buf(desk_sliver_x + 1, desk_sliver_y + 3, 2, 18, 1, is_dark ? 0x004A5060 : 0x00C8CCD6);

    uint64_t now_ms = timer_millis();
    if (now_ms >= next_rtc_update_ms) {
        update_rtc_cache();
        next_rtc_update_ms = now_ms + 1000ULL;
    }

    char time_str[10];
    time_str[0] = '0' + (cached_h / 10);
    time_str[1] = '0' + (cached_h % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (cached_m / 10);
    time_str[4] = '0' + (cached_m % 10);
    time_str[5] = 0;

    char date_str[12];
    date_str[0] = '0' + (cached_day / 10);
    date_str[1] = '0' + (cached_day % 10);
    date_str[2] = '.';
    date_str[3] = '0' + (cached_month / 10);
    date_str[4] = '0' + (cached_month % 10);
    date_str[5] = '.';
    date_str[6] = '0' + ((cached_year / 10) % 10);
    date_str[7] = '0' + (cached_year % 10);
    date_str[8] = 0;

    int time_w = font_text_width(time_str);
    int date_w = font_text_width(date_str);
    int clock_text_w = (time_w > date_w) ? time_w : date_w;
    int clock_w = clock_text_w + 18;
    if (clock_w < 66) clock_w = 66;
    int clock_h = 36;
    int clock_x = desk_sliver_x - clock_w - 6;
    int clock_y = bar_y + (bar_h - clock_h) / 2;
    int clock_hover = (mouse_x >= clock_x && mouse_x < clock_x + clock_w &&
                       mouse_y >= clock_y && mouse_y < clock_y + clock_h);

    uint32_t col_primary = is_dark ? COLOR_WHITE : 0x001D1D1F;
    uint32_t col_sec = is_dark ? 0x00A0A5B4 : 0x006E6E73;

    if (clock_hover) {
        draw_rounded_rect_alpha(clock_x, clock_y, clock_w, clock_h, 6, 0x00388BFD, 50);
        draw_rounded_rect_alpha(clock_x, clock_y, clock_w, clock_h, 6, is_dark ? 0x00FFFFFF : 0x00000000, 25);
    }
    draw_string(time_str, clock_x + (clock_w - time_w) / 2, clock_y + 4,
                col_primary, backbuffer, scr_width);
    draw_string(date_str, clock_x + (clock_w - date_w) / 2, clock_y + 19,
                col_sec, backbuffer, scr_width);

    char vol_str[8];
    int vi = 0;
    if (current_volume >= 100) {
        vol_str[vi++] = '1'; vol_str[vi++] = '0'; vol_str[vi++] = '0';
    } else {
        if (current_volume >= 10) vol_str[vi++] = '0' + (current_volume / 10);
        vol_str[vi++] = '0' + (current_volume % 10);
    }
    vol_str[vi++] = '%';
    vol_str[vi] = 0;

    int vol_text_w = font_text_width(vol_str);
    int vol_icon_sz = 18;
    int vol_pad_x = 7;
    int vol_gap = 5;
    int vol_btn_w = vol_pad_x + vol_icon_sz + vol_gap + vol_text_w + vol_pad_x;
    if (vol_btn_w < 56) vol_btn_w = 56;
    int vol_btn_h = 36;
    int vol_btn_x = clock_x - vol_btn_w - 6;
    int vol_btn_y = bar_y + (bar_h - vol_btn_h) / 2;
    int vol_hover = (mouse_x >= vol_btn_x && mouse_x < vol_btn_x + vol_btn_w &&
                     mouse_y >= vol_btn_y && mouse_y < vol_btn_y + vol_btn_h);

    g_vol_btn_x = vol_btn_x;
    g_vol_btn_w = vol_btn_w;

    if (volume_popup_open) {
        draw_rounded_rect_alpha(vol_btn_x, vol_btn_y, vol_btn_w, vol_btn_h, 6, 0x00388BFD, 80);
        draw_rounded_rect_alpha(vol_btn_x, vol_btn_y, vol_btn_w, vol_btn_h, 6, 0x00FFFFFF, 25);
    } else if (vol_hover) {
        draw_rounded_rect_alpha(vol_btn_x, vol_btn_y, vol_btn_w, vol_btn_h, 6, 0x00388BFD, 60);
        draw_rounded_rect_alpha(vol_btn_x, vol_btn_y, vol_btn_w, vol_btn_h, 6, 0x00FFFFFF, 25);
    }

    int icon_draw_x = vol_btn_x + vol_pad_x;
    int icon_draw_y = vol_btn_y + (vol_btn_h - vol_icon_sz) / 2;
    const uint8_t* vol_icon = current_volume_bmp();
    if (vol_icon) {
        draw_scaled_bmp_rounded(vol_icon, icon_draw_x, icon_draw_y, vol_icon_sz, vol_icon_sz, 4);
    } else {
        draw_rounded_rect_buf(icon_draw_x, icon_draw_y + 2, 14, 14, 3, 0x008E8E93);
    }

    int text_draw_x = icon_draw_x + vol_icon_sz + vol_gap;
    int text_draw_y = vol_btn_y + 12;
    draw_string(vol_str, text_draw_x, text_draw_y, col_primary, backbuffer, scr_width);

    if (single_click && vol_hover) {
        volume_popup_open = !volume_popup_open;
        start_menu_open = 0;
    }

    if (hovered_app_idx != -1) {
        if (current_hover_idx != hovered_app_idx) {
            current_hover_idx = hovered_app_idx;
            tooltip_alpha = 0;
        }
        if (tooltip_alpha < 255) tooltip_alpha += 45;
        if (tooltip_alpha > 255) tooltip_alpha = 255;
    } else {
        if (tooltip_alpha > 0) tooltip_alpha -= 45;
        if (tooltip_alpha < 0) tooltip_alpha = 0;
    }

    if (tooltip_alpha > 10 && current_hover_idx >= 0 && current_hover_idx < app_count) {
        const char* label = dock_apps[current_hover_idx].title;
        int tip_w = font_text_width(label) + 16;
        int tip_h = 22;
        int target_x = apps_start_x + current_hover_idx * (tile_w + 4);
        int tip_x = target_x + tile_w / 2 - tip_w / 2;
        int tip_y = bar_y - tip_h - 6;

        if (tip_x < 8) tip_x = 8;
        if (tip_x + tip_w > (int)scr_width - 8) tip_x = (int)scr_width - 8 - tip_w;

        draw_rounded_rect_alpha(tip_x, tip_y, tip_w, tip_h, 6, is_dark ? 0x0014161C : 0x00F0F2F6, (uint8_t)tooltip_alpha);
        draw_rounded_rect_alpha(tip_x, tip_y, tip_w, tip_h, 6, is_dark ? 0x00FFFFFF : 0x00000000, (uint8_t)(tooltip_alpha / (is_dark ? 6 : 10)));
        draw_string((char*)label, tip_x + 8, tip_y + 4, col_primary, backbuffer, scr_width);
    }

    if (single_click) {
        int menu_w = 240;
        int menu_h = 310;
        int menu_x = bar_x + 4;
        int menu_y = bar_y - menu_h - 8;
        if (start_menu_open && !start_hover &&
            (mouse_x < menu_x || mouse_x >= menu_x + menu_w ||
             mouse_y < menu_y || mouse_y >= menu_y + menu_h)) {
            start_menu_open = 0;
        }

        int pop_min_w = 12 + 16 + 8 + font_text_width(vol_str) + 16;
        int pop_w = (pop_min_w > 190) ? pop_min_w : 190;
        int pop_h = 56;
        int pop_y = bar_y - pop_h - 8;
        int center_anchor = (g_vol_btn_w > 0) ? (g_vol_btn_x + g_vol_btn_w / 2) : (clock_x - 30);
        int pop_x = center_anchor - pop_w / 2;
        if (pop_x + pop_w > (int)scr_width - 12) pop_x = (int)scr_width - 12 - pop_w;
        if (pop_x < 12) pop_x = 12;

        if (volume_popup_open && !vol_hover &&
            (mouse_x < pop_x || mouse_x >= pop_x + pop_w ||
             mouse_y < pop_y || mouse_y >= pop_y + pop_h)) {
            volume_popup_open = 0;
        }
    }
}

void render_layer_dock(int single_click)
{
    render_layer_taskbar(single_click);
}

static int g_wallpaper_drawn_choice = -1;

void refresh_wallpaper(void)
{
    if (g_wallpaper_choice == g_wallpaper_drawn_choice)
        return;

    if (current_wallpaper_bmp())
    {
        draw_bmp_stretched(
            current_wallpaper_bmp(),
            (int)scr_width,
            (int)scr_height,
            bg_buffer
        );
    }
    else
    {
        uint32_t total_pixels = scr_width * scr_height;
        if (total_pixels > MAX_WIDTH * MAX_HEIGHT)
            total_pixels = MAX_WIDTH * MAX_HEIGHT;

        for (uint32_t i = 0; i < total_pixels; i++)
            bg_buffer[i] = 0x001E1E22;
    }

    g_wallpaper_drawn_choice = g_wallpaper_choice;
}

void desktop_init(
    uint8_t* vram,
    uint32_t width,
    uint32_t height,
    uint32_t pitch,
    uint32_t bpp
)
{
    if (vram)
        frontbuffer = vram;

    if (width > 0) {
        scr_width = (width > MAX_WIDTH) ? MAX_WIDTH : width;
    }

    if (height > 0) {
        scr_height = (height > MAX_HEIGHT) ? MAX_HEIGHT : height;
    }

    scr_pitch =
        pitch
            ? pitch
            : scr_width * 4;

    scr_bpp =
        bpp
            ? bpp
            : 32;

    init_keyboard();

    mouse_set_bounds(scr_width, scr_height);

    update_rtc_cache();
    next_rtc_update_ms = timer_millis() + 1000ULL;

    g_wallpaper_drawn_choice = -1;
    refresh_wallpaper();
}

static void serial_write_desktop(const char *s) {
    if (!s) return;
    while (*s) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)*s++), "Nd"((uint16_t)0x3F8));
    }
}

void desktop_run(void)
{
    serial_write_desktop("[DESKTOP] desktop_run entered. Starting 60 FPS compositor loop...\n");

    uint64_t next_frame_tick = timer_ticks();
    int frame_log_count = 0;

    while (1)
    {
        if (frame_log_count < 3) {
            serial_write_desktop("[DESKTOP] Rendering desktop frame...\n");
            frame_log_count++;
        }

        poll_mouse();
        extern void net_poll(void);
        net_poll();
        service_poll();

        int single_click =
            (
                mouse_left_clicked &&
                !prev_mouse_left
            );

        prev_mouse_left =
            mouse_left_clicked;

        kbd_event_t kev;
        int doom_has_focus = doom_app_is_open && doom_app_feed_scancode &&
                             doom_app_is_open() && win_is_focused(WIN_ID_DOOM);

        if (keyboard_consume_alt_f4()) {

            int fid = -1;

            for (int wi = 0; wi < WIN_COUNT; wi++) {
                if (win_is_focused(wi) && win_is_open(wi)) { fid = wi; break; }
            }
            if (fid < 0) {

                for (int zi = WIN_COUNT - 1; zi >= 0; zi--) {
                    int id = g_win_z_order[zi];
                    if (win_is_open(id)) { fid = id; break; }
                }
            }
            if (fid == WIN_ID_FILE && toggle_file_manager) toggle_file_manager();
            else if (fid == WIN_ID_TERMINAL) toggle_terminal_app();
            else if (fid == WIN_ID_CALC) toggle_calc_app();
            else if (fid == WIN_ID_SETTINGS) toggle_settings_user_app();
            else if (fid == WIN_ID_MUSIC) toggle_music_app();
            else if (fid == WIN_ID_ABOUT) toggle_about_user_app();
            else if (fid == WIN_ID_DOOM && doom_app_close) doom_app_close();
        }

        while (keyboard_poll_event(&kev)) {
            if (doom_has_focus) {
                doom_app_feed_scancode(kev.scancode, kev.extended, kev.pressed);
                continue;
            }
            char key = keyboard_event_to_char(&kev);
            if (key) {
                calc_app_feed_key(key);
                terminal_app_feed_key(key);
                uwindow_feed_key(key);
            }

        }

        /* /dev/fb0 is an exclusive direct-scanout lease. When a Ring 3
         * compositor such as OzoneKesh owns it, keep input, network, services
         * and process scheduling alive but stop the legacy desktop from
         * overwriting userspace pixels every frame. */
        if (drm_fb_userspace_owned()) {
            if (process_has_active()) process_step_active();
            timer_wait_ticks(1);
            next_frame_tick = timer_ticks();
            continue;
        }

        int has_user_shell = uwindow_has_desktop_surface();

        if (!has_user_shell) {
            refresh_wallpaper();
        }

        win_drag_frame_begin(mouse_left_clicked);

        if (!has_user_shell) {
            render_layer_background();
            render_desktop_icons(single_click);
        }

        if (process_has_active()) {
            process_step_active();
        }

        /* The process may have acquired /dev/fb0 during the step above. Do not
         * swap one last legacy frame over its first Chromium frame. */
        if (drm_fb_userspace_owned()) {
            next_frame_tick = timer_ticks();
            continue;
        }

        for (int __wz = 0; __wz < WIN_COUNT; __wz++) {
            g_current_render_id = g_win_z_order[__wz];
            switch (g_current_render_id) {
                case WIN_ID_FILE:
                    render_file_manager_window(backbuffer, scr_width, scr_height, mouse_x, mouse_y, mouse_left_clicked, single_click);
                    break;
                case WIN_ID_MUSIC:
                    render_music_app_window(backbuffer, scr_width, scr_height, mouse_x, mouse_y, mouse_left_clicked, single_click);
                    break;
                case WIN_ID_ABOUT:
                    render_about_app_window(backbuffer, scr_width, scr_height, mouse_x, mouse_y, mouse_left_clicked, single_click);
                    break;
                case WIN_ID_CALC:
                    render_calc_app_window(backbuffer, scr_width, scr_height, mouse_x, mouse_y, mouse_left_clicked, single_click);
                    break;
                case WIN_ID_TERMINAL:
                    render_terminal_app_window(backbuffer, scr_width, scr_height, mouse_x, mouse_y, mouse_left_clicked, single_click);
                    break;
                case WIN_ID_SETTINGS:
                    render_settings_app_window(backbuffer, scr_width, scr_height, mouse_x, mouse_y, mouse_left_clicked, single_click);
                    break;
                case WIN_ID_DOOM:
                    if (render_doom_app_window)
                        render_doom_app_window(backbuffer, scr_width, scr_height, mouse_x, mouse_y, mouse_left_clicked, single_click);
                    break;
                case WIN_ID_USER_APP:
                    uwindow_render_all(backbuffer, scr_width, scr_height, mouse_x, mouse_y, mouse_left_clicked, single_click);
                    break;
            }
        }
        g_current_render_id = -1;

        if (!has_user_shell) {
            render_layer_taskbar(
                single_click
            );

            render_start_menu(
                single_click
            );

            render_volume_popup();
            render_desktop_context_menu(single_click);
        }

        poll_mouse();

        draw_cursor_bmp(
            mouse_x,
            mouse_y,
            backbuffer,
            scr_width,
            scr_height
        );

        desktop_swap_buffers();

        next_frame_tick += 4;
        uint64_t now_tick = timer_ticks();
        if (now_tick < next_frame_tick) {
            timer_wait_ticks(next_frame_tick - now_tick);
        } else if (now_tick > next_frame_tick + 8) {

            next_frame_tick = now_tick;
        }
    }
}
