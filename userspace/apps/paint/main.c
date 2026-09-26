// рисовалка
#include "kesh.h"

#define WIN_W 640
#define WIN_H 430

#define TOOLBAR_H 54
#define CANVAS_Y TOOLBAR_H
#define CANVAS_W WIN_W
#define CANVAS_H (WIN_H - TOOLBAR_H)

static const uint32_t g_palette[] = {
    0xFF000000, 
    0xFFFFFFFF, 
    0xFFFF453A, 
    0xFFFF9F0A, 
    0xFFFFD60A, 
    0xFF30D158, 
    0xFF64D2FF, 
    0xFF0A84FF, 
    0xFFBF5AF2, 
    0xFFFF375F  
};
#define PALETTE_COUNT 10

static uint32_t g_selected_color = 0xFF0A84FF; 
static int g_brush_size = 2; 
static int g_is_eraser = 0;
static int g_is_mouse_down = 0;
static int g_last_draw_x = -1;
static int g_last_draw_y = -1;

static uint32_t g_canvas[CANVAS_W * CANVAS_H];

static void clear_canvas(uint32_t color) {
    for (int i = 0; i < CANVAS_W * CANVAS_H; i++) {
        g_canvas[i] = color;
    }
}

static void draw_brush_stamp(int cx, int cy, int size, uint32_t color) {
    if (cx < 0 || cx >= CANVAS_W || cy < 0 || cy >= CANVAS_H) return;
    int r = size;
    int r2 = r * r;

    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= CANVAS_H) continue;
        for (int dx = -r; dx <= r; dx++) {
            int px = cx + dx;
            if (px < 0 || px >= CANVAS_W) continue;
            if (dx * dx + dy * dy <= r2) {
                g_canvas[py * CANVAS_W + px] = color;
            }
        }
    }
}

static void draw_line(int x0, int y0, int x1, int y1, int size, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        draw_brush_stamp(x0, y0, size, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

int main(void) {
    kesh_print("[PAINT] Starting Kesh Paint (Ring 3)...\n");

    uint32_t *fb = kesh_create_window(WIN_W, WIN_H, "Kesh Paint (Ring 3)");
    if (!fb) {
        kesh_print("[PAINT] Failed to create window!\n");
        return 1;
    }

    clear_canvas(0xFFFFFFFF); 

    while (1) {

        kesh_draw_rect(fb, WIN_W, 0, 0, WIN_W, TOOLBAR_H, 0xFF1C1D24);
        kesh_draw_rect(fb, WIN_W, 0, TOOLBAR_H - 1, WIN_W, 1, 0xFF2F313D);

        draw_string("KESH PAINT", 14, 18, 0xFFE5E5EA, fb, WIN_W);

        int btn_y = 12;
        int btn_h = 30;

        int pen_x = 100;
        int pen_w = 46;
        uint32_t pen_col = (!g_is_eraser && g_brush_size == 1) ? 0xFF0A84FF : 0xFF2C2D38;
        kesh_draw_rounded_rect(fb, WIN_W, pen_x, btn_y, pen_w, btn_h, 5, pen_col);
        draw_string("1px", pen_x + 12, btn_y + 8, 0xFFFFFFFF, fb, WIN_W);

        int br_x = 152;
        int br_w = 46;
        uint32_t br_col = (!g_is_eraser && g_brush_size == 3) ? 0xFF0A84FF : 0xFF2C2D38;
        kesh_draw_rounded_rect(fb, WIN_W, br_x, btn_y, br_w, btn_h, 5, br_col);
        draw_string("3px", br_x + 12, btn_y + 8, 0xFFFFFFFF, fb, WIN_W);

        int th_x = 204;
        int th_w = 46;
        uint32_t th_col = (!g_is_eraser && g_brush_size == 6) ? 0xFF0A84FF : 0xFF2C2D38;
        kesh_draw_rounded_rect(fb, WIN_W, th_x, btn_y, th_w, btn_h, 5, th_col);
        draw_string("6px", th_x + 12, btn_y + 8, 0xFFFFFFFF, fb, WIN_W);

        int er_x = 256;
        int er_w = 60;
        uint32_t er_col = (g_is_eraser) ? 0xFFFF453A : 0xFF2C2D38;
        kesh_draw_rounded_rect(fb, WIN_W, er_x, btn_y, er_w, btn_h, 5, er_col);
        draw_string("Eraser", er_x + 10, btn_y + 8, 0xFFFFFFFF, fb, WIN_W);

        kesh_draw_rect(fb, WIN_W, 324, 12, 1, 30, 0xFF353744);

        int sw_x = 336;
        int sw_size = 20;
        for (int i = 0; i < PALETTE_COUNT; i++) {
            int cx = sw_x + (i * 24);
            int cy = 16;
            kesh_draw_rounded_rect(fb, WIN_W, cx, cy, sw_size, sw_size, 4, g_palette[i]);
            if (g_palette[i] == g_selected_color && !g_is_eraser) {

                kesh_draw_rect(fb, WIN_W, cx - 2, cy - 2, sw_size + 4, 2, 0xFFFFFFFF);
                kesh_draw_rect(fb, WIN_W, cx - 2, cy + sw_size, sw_size + 4, 2, 0xFFFFFFFF);
                kesh_draw_rect(fb, WIN_W, cx - 2, cy, 2, sw_size, 0xFFFFFFFF);
                kesh_draw_rect(fb, WIN_W, cx + sw_size, cy, 2, sw_size, 0xFFFFFFFF);
            }
        }

        int clr_x = WIN_W - 62;
        int clr_w = 50;
        kesh_draw_rounded_rect(fb, WIN_W, clr_x, btn_y, clr_w, btn_h, 5, 0xFF3A3D4A);
        draw_string("Clear", clr_x + 8, btn_y + 8, 0xFFFF453A, fb, WIN_W);

        for (int cy = 0; cy < CANVAS_H; cy++) {
            int py = CANVAS_Y + cy;
            uint32_t *dst = &fb[py * WIN_W];
            uint32_t *src = &g_canvas[cy * CANVAS_W];
            for (int cx = 0; cx < CANVAS_W; cx++) {
                dst[cx] = src[cx];
            }
        }

        kesh_update_window(0);

        kesh_event_t ev;
        while (kesh_poll_event(0, &ev)) {
            if (ev.type == EVENT_CLOSE) {
                kesh_print("[PAINT] Close requested. Exiting...\n");
                kesh_exit(0);
            } else if (ev.type == EVENT_MOUSE_DOWN) {

                if (ev.my < TOOLBAR_H) {
                    if (ev.mx >= pen_x && ev.mx <= pen_x + pen_w) {
                        g_brush_size = 1;
                        g_is_eraser = 0;
                    } else if (ev.mx >= br_x && ev.mx <= br_x + br_w) {
                        g_brush_size = 3;
                        g_is_eraser = 0;
                    } else if (ev.mx >= th_x && ev.mx <= th_x + th_w) {
                        g_brush_size = 6;
                        g_is_eraser = 0;
                    } else if (ev.mx >= er_x && ev.mx <= er_x + er_w) {
                        g_is_eraser = 1;
                    } else if (ev.mx >= clr_x && ev.mx <= clr_x + clr_w) {
                        clear_canvas(0xFFFFFFFF);
                    } else {

                        for (int i = 0; i < PALETTE_COUNT; i++) {
                            int cx = sw_x + (i * 24);
                            int cy = 16;
                            if (ev.mx >= cx && ev.mx <= cx + sw_size && ev.my >= cy && ev.my <= cy + sw_size) {
                                g_selected_color = g_palette[i];
                                g_is_eraser = 0;
                                break;
                            }
                        }
                    }
                } else {

                    g_is_mouse_down = 1;
                    int canvas_x = ev.mx;
                    int canvas_y = ev.my - CANVAS_Y;
                    uint32_t col = g_is_eraser ? 0xFFFFFFFF : g_selected_color;
                    draw_brush_stamp(canvas_x, canvas_y, g_brush_size, col);
                    g_last_draw_x = canvas_x;
                    g_last_draw_y = canvas_y;
                }
            } else if (ev.type == EVENT_MOUSE_UP) {
                g_is_mouse_down = 0;
                g_last_draw_x = -1;
                g_last_draw_y = -1;
            } else if (ev.type == EVENT_MOUSE_MOVE) {
                if (g_is_mouse_down && ev.my >= CANVAS_Y) {
                    int canvas_x = ev.mx;
                    int canvas_y = ev.my - CANVAS_Y;
                    uint32_t col = g_is_eraser ? 0xFFFFFFFF : g_selected_color;

                    if (g_last_draw_x >= 0 && g_last_draw_y >= 0) {
                        draw_line(g_last_draw_x, g_last_draw_y, canvas_x, canvas_y, g_brush_size, col);
                    } else {
                        draw_brush_stamp(canvas_x, canvas_y, g_brush_size, col);
                    }
                    g_last_draw_x = canvas_x;
                    g_last_draw_y = canvas_y;
                }
            }
        }

        kesh_yield();
    }

    return 0;
}
