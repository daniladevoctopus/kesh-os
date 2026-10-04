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
#include "vfs.h"
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

/* The remainder of this file is intentionally unchanged from the native
 * desktop implementation. The OzoneKesh development takeover is injected in
 * desktop_run() below. */
