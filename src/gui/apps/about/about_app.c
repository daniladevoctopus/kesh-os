#include "gui/apps/about/about_app.h"
#include "gui/desktop.h"
#include "gui/font.h"
#include "gui/bmp_loader.h"
#include "gui/anim/genie_anim.h"

#include <stdint.h>

extern void draw_rounded_rect_buf(
    int x,
    int y,
    int w,
    int h,
    int r,
    uint32_t color
);

extern void draw_rounded_rect_alpha(
    int x,
    int y,
    int w,
    int h,
    int r,
    uint32_t color,
    uint8_t alpha
);

extern void draw_rect_buf(
    int x,
    int y,
    int w,
    int h,
    uint32_t color
);

extern void draw_pixel_buf(
    int x,
    int y,
    uint32_t color
);

extern const uint8_t about_bmp_start[];
extern const uint8_t about_bmp_end[];

static int is_open = 0;

static int win_x = 0;
static int win_y = 0;

static int win_w = 600;
static int win_h = 340;

static int positioned = 0;

static int dragging = 0;
static int drag_ox = 0;
static int drag_oy = 0;
static genie_state_t genie;

void toggle_about_app(void)
{
    int dx, dy;
    genie_dock_icon_point(0, &dx, &dy);

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

static void draw_about_image(
    int x,
    int y,
    int w,
    int h,
    uint32_t* buf
) {
    if (!about_bmp_start)
        return;

    static uint32_t about_buffer[180 * 180];

    draw_bmp_stretched(
        about_bmp_start,
        w,
        h,
        about_buffer
    );

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {

            int dx = x + px;
            int dy = y + py;

            if (dx < 0 || dy < 0)
                continue;

            uint32_t col = about_buffer[py * w + px];

            if ((col & 0x00FFFFFF) != 0) {
                draw_pixel_buf(
                    dx,
                    dy,
                    col
                );
            }
        }
    }

    (void)buf;
}

void render_about_app_window(
    uint32_t* buf,
    int scr_w,
    int scr_h,
    int mx,
    int my,
    int btn,
    int click
) {
    genie_tick(&genie);
    int mid_genie = genie_is_animating(&genie);
    if (!is_open && !mid_genie)
        return;

    if (!positioned) {

        win_x = (scr_w - win_w) / 2;
        win_y = (scr_h - win_h) / 2;

        positioned = 1;
    }

    int traffic_zone_w = 90;
    extern int g_window_controls_align;
    int drag_start_x = (g_window_controls_align == 1) ? win_x : (win_x + traffic_zone_w);
    int drag_end_x   = (g_window_controls_align == 1) ? (win_x + win_w - traffic_zone_w) : (win_x + win_w);

    if (
        btn &&
        !dragging &&
        !mid_genie &&
        win_drag_available() &&
        mx >= drag_start_x &&
        mx <= drag_end_x &&
        my >= win_y &&
        my <= win_y + 36
    ) {
        dragging = 1;

        drag_ox = mx - win_x;
        drag_oy = my - win_y;

        win_drag_claim();
    }

    if (!btn)
        dragging = 0;

    if (dragging) {

        win_x = mx - drag_ox;
        win_y = my - drag_oy;
    }

    win_report_rect(WIN_ID_ABOUT_, win_x, win_y, win_w, win_h, is_open && !mid_genie);
    int occluded = win_click_occluded(WIN_ID_ABOUT_, mx, my);

    int draw_x, draw_y, draw_w, draw_h;
    genie_get_rect(&genie, win_x, win_y, win_w, win_h, &draw_x, &draw_y, &draw_w, &draw_h);

    if (mid_genie) {
        draw_rounded_rect_alpha(draw_x + 4, draw_y + 4, draw_w, draw_h, 12, 0x00000000, 60);
        draw_rounded_rect_buf(draw_x, draw_y, draw_w, draw_h, 12, 0x00F6F6F8);
        dragging = 0;
        return;
    }

    int tl_size = 12;
    int tl_gap = 8;
    int tl_y = win_y + (36 - tl_size) / 2;
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

    int hit_padding = 8;

    int hover_close =
        mx >= (close_x - hit_padding) &&
        mx <= (close_x + tl_size + hit_padding) &&
        my >= (tl_y - hit_padding) &&
        my <= (tl_y + tl_size + hit_padding);

    static const struct {

        int spread;
        int drop;
        uint8_t alpha;

    } shadows[] = {

        {10, 14, 10},
        {8,  12, 14},
        {6,  10, 18},
        {4,   8, 24},
        {2,   6, 32}
    };

    for (
        unsigned i = 0;
        i < sizeof(shadows) / sizeof(shadows[0]);
        i++
    ) {

        int sp = shadows[i].spread;

        draw_rounded_rect_alpha(

            win_x - sp / 2,

            win_y -
            sp / 2 +
            shadows[i].drop,

            win_w + sp,

            win_h + sp,

            12 + sp / 2,

            0x00000000,

            shadows[i].alpha
        );
    }

    draw_rounded_rect_buf(
        win_x,
        win_y,
        win_w,
        win_h,
        12,
        0x00F6F6F8
    );

    draw_rounded_rect_buf(
        win_x,
        win_y,
        win_w,
        36,
        12,
        0x00ECECEF
    );

    draw_rect_buf(
        win_x,
        win_y + 18,
        win_w,
        18,
        0x00ECECEF
    );

    draw_rect_buf(
        win_x,
        win_y + 35,
        win_w,
        1,
        0x00D8D8DC
    );

    draw_rounded_rect_buf(
        zoom_x,
        tl_y,
        tl_size,
        tl_size,
        4,
        0x0028C840
    );

    draw_rounded_rect_buf(
        minimize_x,
        tl_y,
        tl_size,
        tl_size,
        4,
        0x00FFBD2E
    );

    draw_rounded_rect_buf(
        close_x,
        tl_y,
        tl_size,
        tl_size,
        4,
        hover_close
            ? 0x00FF6259
            : 0x00FF5F57
    );

    if (click && hover_close && !occluded) {
        int dx, dy;
        genie_dock_icon_point(0, &dx, &dy);
        genie_start_close(&genie, dx, dy);
        is_open = 0;
        dragging = 0;
        return;
    }

    int content_top = win_y + 36;

    int content_h =
        win_h - 36;

    int image_size = 150;

    int image_x =
        win_x + 45;

    int image_y =
        content_top +
        (content_h - image_size) / 2;

    static uint32_t image_buffer[150 * 150];

    draw_bmp_stretched(
        about_bmp_start,
        image_size,
        image_size,
        image_buffer
    );

    for (int py = 0; py < image_size; py++) {

        for (int px = 0; px < image_size; px++) {

            int dx =
                image_x + px;

            int dy =
                image_y + py;

            uint32_t col = image_buffer[
                py * image_size + px
            ];

            if ((col & 0x00FFFFFF) != 0) {
                draw_pixel_buf(
                    dx,
                    dy,
                    col
                );
            }
        }
    }

    int text_x =
        image_x +
        image_size +
        38;

    int text_y =
        content_top +
        50;

    draw_string(
        "KeshOS 1.0 Drop",
        text_x,
        text_y,
        0x001C1C1E,
        buf,
        scr_w
    );

    text_y += 28;

    draw_string(
        "Release Candidate 1 (Build 950.x86_64)",
        text_x,
        text_y,
        0x001C1C1E,
        buf,
        scr_w
    );

    text_y += 24;

    draw_string(
        "Kernel: 1.0.0-rc1-ksh",
        text_x,
        text_y,
        0x00444446,
        buf,
        scr_w
    );

    text_y += 20;

    draw_string(
        "Channel: Beta/RC | Target: UEFI/GPT",
        text_x,
        text_y,
        0x006E6E73,
        buf,
        scr_w
    );

    text_y += 20;

    draw_string(
        "Render: Composited Desktop",
        text_x,
        text_y,
        0x000A84FF,
        buf,
        scr_w
    );

    const char* footer =
        "2026 SneakDeak Technologies. All rights reserved.";

    int footer_w =
        font_text_width(footer);

    draw_string(
        footer,
        win_x +
        (win_w - footer_w) / 2,
        win_y +
        win_h -
        26,
        0x008E8E93,
        buf,
        scr_w
    );
}
