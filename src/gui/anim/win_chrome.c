// заголовок окна и кнопки закрытия
#include "win_chrome.h"
#include "gui/desktop.h"

extern void draw_rounded_rect_buf(int x, int y, int w, int h, int r, uint32_t color);

static int g_focused = -1;

void win_chrome_set_focused(int win_id) { g_focused = win_id; }
int  win_chrome_is_focused(int win_id)  { return g_focused == win_id; }

int win_chrome_dock_to_win_id(int dock_index)
{
    switch (dock_index) {
        case 0: return WIN_ID_FILE_;
        case 1: return WIN_ID_TERMINAL_;
        case 2: return WIN_ID_DOOM_;
        case 3: return WIN_ID_CALC_;
        case 4: return WIN_ID_SETTINGS_;
        case 5: return WIN_ID_MUSIC_;
        default: return -1;
    }
}

int win_chrome_traffic_lights(
    int win_id,
    int win_x, int win_y, int win_w,
    int header_h,
    int mx, int my, int click, int occluded,
    int *out_minimize_clicked,
    int *out_zoom_clicked
) {
    if (out_minimize_clicked) *out_minimize_clicked = 0;
    if (out_zoom_clicked) *out_zoom_clicked = 0;

    int tl = 12, gap = 8, margin = 14;
    int ty = win_y + (header_h - tl) / 2;
    int close_x, min_x, zoom_x;

    extern int g_window_controls_align;
    if (g_window_controls_align == 1) {
        close_x = win_x + win_w - margin - tl;
        min_x   = close_x - gap - tl;
        zoom_x  = min_x - gap - tl;
    } else {
        close_x = win_x + margin;
        min_x   = close_x + tl + gap;
        zoom_x  = min_x + tl + gap;
    }
    int pad = 6;

    int focused = win_chrome_is_focused(win_id);
    int h_close = mx >= close_x - pad && mx <= close_x + tl + pad &&
                  my >= ty - pad && my <= ty + tl + pad;
    int h_min   = mx >= min_x - pad && mx <= min_x + tl + pad &&
                  my >= ty - pad && my <= ty + tl + pad;
    int h_zoom  = mx >= zoom_x - pad && mx <= zoom_x + tl + pad &&
                  my >= ty - pad && my <= ty + tl + pad;

    uint32_t c_close = (focused || h_close) ? (h_close ? 0x00FF3B30 : 0x00FF5F57) : 0x00C8C8CC;
    uint32_t c_min   = (focused || h_min)   ? (h_min   ? 0x00FF9500 : 0x00FFBD2E) : 0x00C8C8CC;
    uint32_t c_zoom  = (focused || h_zoom)  ? (h_zoom  ? 0x0028CD41 : 0x0028C840) : 0x00C8C8CC;

    draw_rounded_rect_buf(close_x, ty, tl, tl, 6, c_close);
    draw_rounded_rect_buf(min_x,   ty, tl, tl, 6, c_min);
    draw_rounded_rect_buf(zoom_x,  ty, tl, tl, 6, c_zoom);

    if (click && !occluded) {
        if (h_close) return 1;
        if (h_min && out_minimize_clicked) *out_minimize_clicked = 1;
        if (h_zoom && out_zoom_clicked)    *out_zoom_clicked = 1;
    }
    return 0;
}
