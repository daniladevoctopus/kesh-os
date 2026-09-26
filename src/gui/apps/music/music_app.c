// плеер для wav треков
#include "gui/apps/music/music_app.h"
#include "gui/desktop.h"
#include "gui/font.h"
#include "gui/anim/genie_anim.h"
#include "drivers/system/sound_manager.h"

extern void draw_rounded_rect_buf(int x, int y, int w, int h, int r, uint32_t color);
extern void draw_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
extern void draw_rect_buf(int x, int y, int w, int h, uint32_t color);

static int is_open = 0;
static int is_playing = 0;
static int current_track = 0;
static int progress = 25;

static int win_x = 0;
static int win_y = 0;
static int win_w = 500;
static int win_h = 340;

static int positioned = 0;

static int dragging = 0;
static int drag_ox = 0;
static int drag_oy = 0;
static genie_state_t genie;

static const char* track_list[] = {
    "01. Track_One.mp3",
    "02. Ambient_Theme.mp3",
    "03. Cyberpunk_Beat.mp3"
};
static int track_count = 3;

void toggle_music_app(void) {
    int dx, dy;
    genie_dock_icon_point(5, &dx, &dy);

    if (genie_is_animating(&genie))
        genie_cancel(&genie);

    if (is_open) {
        genie_start_close(&genie, dx, dy);
        is_open = 0;
        dragging = 0;
        return;
    }

    is_open = 1;
    dragging = 0;
    genie_start_open(&genie, dx, dy);
}

void render_music_app_window(uint32_t* buf, int scr_w, int scr_h, int mx, int my, int btn, int click) {
    genie_tick(&genie);
    int mid_genie = genie_is_animating(&genie);
    if (!is_open && !mid_genie) return;

    if (!positioned) {
        win_x = (scr_w - win_w) / 2;
        win_y = (scr_h - win_h) / 2;
        positioned = 1;
    }

    int header_h = 32;
    int traffic_zone_w = 28; 

    if (btn && !dragging && !mid_genie && win_drag_available() &&
        mx >= win_x + traffic_zone_w && mx <= win_x + win_w &&
        my >= win_y && my <= win_y + header_h)
    {
        dragging = 1;
        drag_ox = mx - win_x;
        drag_oy = my - win_y;
        win_drag_claim();
    }
    if (!btn) dragging = 0;
    if (dragging) { win_x = mx - drag_ox; win_y = my - drag_oy; }

    win_report_rect(WIN_ID_MUSIC_, win_x, win_y, win_w, win_h, is_open && !mid_genie);
    if (win_click_occluded(WIN_ID_MUSIC_, mx, my)) click = 0;

    int draw_x, draw_y, draw_w, draw_h;
    genie_get_rect(&genie, win_x, win_y, win_w, win_h, &draw_x, &draw_y, &draw_w, &draw_h);

    draw_rounded_rect_alpha(draw_x + 4, draw_y + 4, draw_w, draw_h, 12, 0x00000000, 80);
    draw_rounded_rect_buf(draw_x, draw_y, draw_w, draw_h, 12, 0x001C1C1E);

    if (mid_genie) return;

    draw_rounded_rect_buf(win_x, win_y, win_w, 32, 12, 0x002C2C2E);
    draw_string("Music Player", win_x + win_w / 2 - 48, win_y + 8, 0x00FFFFFF, buf, scr_w);

    draw_rounded_rect_buf(win_x + 12, win_y + 10, 12, 12, 6, 0x00FF5F56);
    if (click && mx >= win_x + 12 && mx <= win_x + 24 && my >= win_y + 10 && my <= win_y + 22) {
        int dx, dy;
        genie_dock_icon_point(5, &dx, &dy);
        genie_start_close(&genie, dx, dy);
        is_open = 0;
        dragging = 0;
        return;
    }

    draw_rect_buf(win_x + 12, win_y + 44, 210, 220, 0x002C2C2E);
    for (int i = 0; i < track_count; i++) {
        uint32_t item_bg = (current_track == i) ? 0x00007AFF : 0x003A3A3C;
        draw_rounded_rect_buf(win_x + 16, win_y + 50 + i * 36, 202, 30, 6, item_bg);
        draw_string(track_list[i], win_x + 24, win_y + 58 + i * 36, 0x00FFFFFF, buf, scr_w);

        if (click && mx >= win_x + 16 && mx <= win_x + 218 && my >= win_y + 50 + i * 36 && my <= win_y + 80 + i * 36) {
            current_track = i;
            is_playing = 1;
        }
    }

    draw_rounded_rect_buf(win_x + 236, win_y + 44, 252, 160, 10, 0x00FF2D55);
    draw_string("Now Playing:", win_x + 236, win_y + 215, 0x008E8E93, buf, scr_w);
    draw_string(track_list[current_track], win_x + 236, win_y + 235, 0x00FFFFFF, buf, scr_w);

    draw_rounded_rect_buf(win_x + 12, win_y + 276, win_w - 24, 6, 3, 0x003A3A3C);
    int fill_w = ((win_w - 24) * progress) / 100;
    draw_rounded_rect_buf(win_x + 12, win_y + 276, fill_w, 6, 3, 0x00007AFF);

    uint32_t btn_color = is_playing ? 0x00FF9500 : 0x0034C759;
    const char* play_lbl = is_playing ? "PAUSE" : "PLAY";
    int play_w = font_text_width(play_lbl);
    int pbtn_w = play_w + 28;
    if (pbtn_w < 64) pbtn_w = 64;
    int pbtn_x = win_x + (win_w - pbtn_w) / 2;
    draw_rounded_rect_buf(pbtn_x, win_y + 292, pbtn_w, 32, 8, btn_color);
    draw_string(play_lbl, pbtn_x + (pbtn_w - play_w) / 2, win_y + 302, 0x00FFFFFF, buf, scr_w);

    if (click && mx >= pbtn_x && mx <= pbtn_x + pbtn_w && my >= win_y + 292 && my <= win_y + 324) {
        is_playing = !is_playing;
    }
}
