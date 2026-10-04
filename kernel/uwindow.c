// композитор окон, рисуем буферы
#include "uwindow.h"
#include "memory.h"
#include "gui/desktop.h"
#include "gui/font.h"
#include "gui/anim/win_chrome.h"
#include <stddef.h>

#define WIN_ID_USER_BASE 7

static user_window_t g_windows[MAX_USER_WINDOWS];
static int g_focused_win_id = -1;
static int g_prev_click = 0;
static int g_last_mx = 0, g_last_my = 0;

extern void draw_rounded_rect_buf(int x, int y, int w, int h, int r, uint32_t color);
extern void draw_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
extern void draw_rect_buf(int x, int y, int w, int h, uint32_t color);

void uwindow_init(void) {
    for (int i = 0; i < MAX_USER_WINDOWS; i++) {
        g_windows[i].active = 0;
        g_windows[i].is_open = 0;
        g_windows[i].ev_head = 0;
        g_windows[i].ev_tail = 0;
    }
}

user_window_t* uwindow_create(int w, int h, const char *title, uint64_t pml4_phys) {
    int is_desk = 0;
    if (title && title[0] == '_' && title[1] == '_' && title[2] == 'd') {
        is_desk = 1;
    }

    if (w < 100) w = 100;
    if (w > 1920) w = 1920;
    if (h < 50) h = 50;
    if (h > 1200) h = 1200;

    int slot = -1;
    for (int i = 0; i < MAX_USER_WINDOWS; i++) {
        if (!g_windows[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return NULL;

    user_window_t *win = &g_windows[slot];
    win->win_id = slot;
    win->is_desktop = is_desk;
    win->w = w;
    win->h = h;
    if (is_desk) {
        win->x = 0;
        win->y = 0;
    } else {
        win->x = 220 + slot * 30;
        win->y = 120 + slot * 30;
    }
    win->is_glass = 0;
    if (title) {
        for (int si = 0; title[si]; si++) {
            if ((title[si] == 'S' || title[si] == 's') && (title[si+1] == 'e') && (title[si+2] == 't')) {
                win->is_glass = 1; break;
            }
            if ((title[si] == 'G' || title[si] == 'g') && (title[si+1] == 'l') && (title[si+2] == 'a') && (title[si+3] == 's') && (title[si+4] == 's')) {
                win->is_glass = 1; break;
            }
            if ((title[si] == 'A' || title[si] == 'a') && (title[si+1] == 'b') && (title[si+2] == 'o') && (title[si+3] == 'u') && (title[si+4] == 't')) {
                win->is_glass = 1; break;
            }
        }
    }
    win->is_open = 1;
    win->minimized = 0;
    win->dragging = 0;
    win->drag_ox = 0;
    win->drag_oy = 0;
    win->dirty = 1;
    win->anim_state = is_desk ? 0 : 1;
    win->anim_t = 0;
    win->pml4_phys = pml4_phys;
    win->user_fb_vaddr = USER_WINDOW_FB_VADDR + (uint64_t)slot * USER_WINDOW_FB_STRIDE;
    win->ev_head = 0;
    win->ev_tail = 0;

    int t = 0;
    if (title) {
        while (title[t] && t < 63) {
            win->title[t] = title[t];
            t++;
        }
    }
    win->title[t] = '\0';

    uint64_t total_bytes = (uint64_t)w * (uint64_t)h * 4ULL;
    uint64_t num_pages = ((total_bytes + PAGE_SIZE - 1ULL) / PAGE_SIZE) + 4ULL;
    if (num_pages * PAGE_SIZE > USER_WINDOW_FB_STRIDE) return NULL;

    uint64_t fb_phys = pmm_alloc_pages(num_pages);
    if (!fb_phys) return NULL;

    for (uint64_t p = 0; p < num_pages; p++) {
        if (vmm_map_page(pml4_phys, win->user_fb_vaddr + p * PAGE_SIZE,
                         fb_phys + p * PAGE_SIZE, PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE) != 0) {
            while (p > 0) {
                --p;
                vmm_unmap_page(pml4_phys, win->user_fb_vaddr + p * PAGE_SIZE, 0);
            }
            pmm_free_pages(fb_phys, num_pages);
            return NULL;
        }
    }

    vmm_switch_pml4(pml4_phys);

    win->framebuffer = (uint32_t*)(fb_phys + g_hhdm_offset);

    for (uint64_t i = 0; i < (uint64_t)w * h; i++) {
        win->framebuffer[i] = 0xFF1E1E22;
    }

    win->active = 1;
    g_focused_win_id = slot;
    return win;
}

void uwindow_destroy(int win_id) {
    if (win_id < 0 || win_id >= MAX_USER_WINDOWS) return;
    g_windows[win_id].active = 0;
    g_windows[win_id].is_open = 0;
    if (g_focused_win_id == win_id) g_focused_win_id = -1;
}

void uwindow_destroy_process(uint64_t pml4_phys) {
    for (int i = 0; i < MAX_USER_WINDOWS; i++) {
        if (!g_windows[i].active || g_windows[i].pml4_phys != pml4_phys) continue;
        uwindow_destroy(i);
    }
}

user_window_t* uwindow_get(int win_id) {
    if (win_id < 0 || win_id >= MAX_USER_WINDOWS) return NULL;
    if (!g_windows[win_id].active) return NULL;
    return &g_windows[win_id];
}

user_window_t* uwindow_get_process_window(uint64_t pml4_phys) {
    if (!pml4_phys) return NULL;
    for (int i = 0; i < MAX_USER_WINDOWS; ++i) {
        if (g_windows[i].active && g_windows[i].pml4_phys == pml4_phys) return &g_windows[i];
    }
    return NULL;
}

int uwindow_push_event(int win_id, uevent_t ev) {
    user_window_t *win = uwindow_get(win_id);
    if (!win) return 0;

    int next = (win->ev_head + 1) % 32;
    if (next == win->ev_tail) {

        win->ev_tail = (win->ev_tail + 1) % 32;
    }
    win->events[win->ev_head] = ev;
    win->ev_head = next;
    return 1;
}

int uwindow_pop_event(int win_id, uevent_t *out_ev) {
    user_window_t *win = uwindow_get(win_id);
    if (!win || !out_ev) return 0;
    if (win->ev_head == win->ev_tail) return 0;

    *out_ev = win->events[win->ev_tail];
    win->ev_tail = (win->ev_tail + 1) % 32;
    return 1;
}

void uwindow_feed_key(char c) {
    if (g_focused_win_id >= 0 && g_focused_win_id < MAX_USER_WINDOWS) {
        uevent_t ev;
        ev.type = EVENT_KEY_DOWN;
        ev.mx = 0;
        ev.my = 0;
        ev.btn = 0;
        ev.key = (int)c;
        uwindow_push_event(g_focused_win_id, ev);
    }
}

int uwindow_is_any_open(void) {
    for (int i = 0; i < MAX_USER_WINDOWS; i++) {
        if (g_windows[i].active && g_windows[i].is_open && !g_windows[i].minimized && !g_windows[i].is_desktop)
            return 1;
    }
    return 0;
}

int uwindow_has_desktop_surface(void) {
    for (int i = 0; i < MAX_USER_WINDOWS; i++) {
        if (g_windows[i].active && g_windows[i].is_open && g_windows[i].is_desktop)
            return 1;
    }
    return 0;
}

int uwindow_is_focused(void) {
    return (g_focused_win_id >= 0 && g_windows[g_focused_win_id].active && g_windows[g_focused_win_id].is_open);
}

void uwindow_set_focused(int focused) {
    if (!focused) g_focused_win_id = -1;
}

int uwindow_is_focused_idx(int idx) {
    return (g_focused_win_id == idx);
}

void uwindow_render_all(uint32_t *backbuffer, int scr_w, int scr_h, int mx, int my, int click, int single_click) {
    (void)single_click;

    int desktop_slot = -1;
    for (int i = 0; i < MAX_USER_WINDOWS; i++) {
        if (g_windows[i].active && g_windows[i].is_open && g_windows[i].is_desktop) {
            desktop_slot = i;
            break;
        }
    }

    if (desktop_slot >= 0) {
        user_window_t *dwin = &g_windows[desktop_slot];
        if (dwin->framebuffer) {
            int copy_w = (dwin->w < scr_w) ? dwin->w : scr_w;
            int copy_h = (dwin->h < scr_h) ? dwin->h : scr_h;
            for (int r = 0; r < copy_h; r++) {
                uint32_t *dst = &backbuffer[r * scr_w];
                const uint32_t *src = &dwin->framebuffer[r * dwin->w];
                for (int c = 0; c < copy_w; c++) {
                    dst[c] = src[c];
                }
            }
        }
    }

    int captured_by_client = 0;

    for (int i = 0; i < MAX_USER_WINDOWS; i++) {
        user_window_t *win = &g_windows[i];
        if (!win->active || !win->is_open || win->minimized || win->is_desktop) continue;

        if (win->anim_state == 1) {
            win->anim_t++;
            if (win->anim_t >= 10) {
                win->anim_state = 0;
            }
        } else if (win->anim_state == 2) {
            win->anim_t++;
            if (win->anim_t >= 8) {
                uevent_t cev;
                cev.type = EVENT_CLOSE;
                cev.mx = 0; cev.my = 0; cev.btn = 0; cev.key = 0;
                uwindow_push_event(i, cev);
                win->is_open = 0;
                win->anim_state = 0;
                continue;
            }
        }

        int header_h = USER_WINDOW_HEADER_H;
        int total_w = win->w;
        int total_h = win->h + header_h;

        if (win->anim_state == 0) {
            if (win->dragging) {
                captured_by_client = 1;
                if (!click) {
                    win->dragging = 0;
                } else {
                    win->x = mx - win->drag_ox;
                    win->y = my - win->drag_oy;
                    if (win->y < 28) win->y = 28;
                    if (win->y > scr_h - 40) win->y = scr_h - 40;
                    if (win->x < -win->w + 80) win->x = -win->w + 80;
                    if (win->x > scr_w - 80) win->x = scr_w - 80;
                }
            }
        }

        int draw_x = win->x;
        int draw_y = win->y;
        int draw_w = total_w;
        int draw_h = total_h;
        int cur_hdr_h = header_h;
        int ease_p = 256;

        if (win->anim_state == 1) {
            int p = (win->anim_t * 256) / 10;
            int inv = 256 - p;
            ease_p = 256 - ((inv * inv) >> 8);
            int scale = 218 + ((256 - 218) * ease_p) / 256;
            draw_w = (total_w * scale) / 256;
            draw_h = (total_h * scale) / 256;
            draw_x = win->x + (total_w - draw_w) / 2;
            draw_y = win->y + (total_h - draw_h) / 2;
            cur_hdr_h = (header_h * scale) / 256;
            if (cur_hdr_h < 18) cur_hdr_h = 18;
        } else if (win->anim_state == 2) {
            int p = (win->anim_t * 256) / 8;
            ease_p = 256 - ((p * p) >> 8);
            int scale = 256 - ((256 - 200) * p) / 256;
            if (scale < 200) scale = 200;
            draw_w = (total_w * scale) / 256;
            draw_h = (total_h * scale) / 256;
            draw_x = win->x + (total_w - draw_w) / 2;
            draw_y = win->y + (total_h - draw_h) / 2;
            cur_hdr_h = (header_h * scale) / 256;
            if (cur_hdr_h < 14) cur_hdr_h = 14;
        }

        extern int g_window_controls_align;
        int tl_size = 12, tl_gap = 6;
        int close_x, min_x, zoom_x;
        int tl_y = draw_y + (cur_hdr_h - tl_size) / 2;

        if (g_window_controls_align == 1) {
            close_x = draw_x + draw_w - 20;
            min_x   = close_x - tl_gap - tl_size;
            zoom_x  = min_x - tl_gap - tl_size;
        } else {
            close_x = draw_x + 12;
            min_x   = close_x + tl_size + tl_gap;
            zoom_x  = min_x + tl_size + tl_gap;
        }

        int close_hover = (win->anim_state == 0) && (mx >= close_x - 3 && mx <= close_x + tl_size + 3 && my >= tl_y - 3 && my <= tl_y + tl_size + 3);
        int min_hover   = (win->anim_state == 0) && (mx >= min_x - 3 && mx <= min_x + tl_size + 3 && my >= tl_y - 3 && my <= tl_y + tl_size + 3);
        int zoom_hover  = (win->anim_state == 0) && (mx >= zoom_x - 3 && mx <= zoom_x + tl_size + 3 && my >= tl_y - 3 && my <= tl_y + tl_size + 3);

        if (win->anim_state == 0 && !win->dragging && click && !g_prev_click) {
            int in_traffic = close_hover || min_hover || zoom_hover;
            if (!in_traffic && mx >= win->x && mx < win->x + win->w &&
                my >= win->y && my < win->y + header_h) {
                if (win_drag_available()) {
                    win_drag_claim();
                    win->dragging = 1;
                    win->drag_ox = mx - win->x;
                    win->drag_oy = my - win->y;
                    g_focused_win_id = i;
                    captured_by_client = 1;
                }
            } else if (mx >= win->x && mx < win->x + win->w &&
                       my >= win->y + header_h && my < win->y + total_h) {
                g_focused_win_id = i;
                captured_by_client = 1;
            }
        }

        draw_rounded_rect_alpha(draw_x - 4, draw_y - 4, draw_w + 8, draw_h + 8, 14, 0x00000000, (45 * ease_p) >> 8);
        draw_rounded_rect_alpha(draw_x - 2, draw_y - 2, draw_w + 4, draw_h + 4, 12, 0x00000000, (70 * ease_p) >> 8);

        extern int g_dark_mode;
        int is_dark = g_dark_mode;

        if (win->is_glass) {
            extern void blur_rect_rounded_buf(int x, int y, int w, int h, int radius, int corner_r);
            blur_rect_rounded_buf(draw_x, draw_y, draw_w, draw_h, 8, 14);
            if (is_dark) {
                draw_rounded_rect_alpha(draw_x, draw_y, draw_w, draw_h, 14, 0x00141620, (180 * ease_p) >> 8);
                draw_rounded_rect_alpha(draw_x, draw_y, draw_w, cur_hdr_h, 14, 0x00FFFFFF, (15 * ease_p) >> 8);
                draw_rounded_rect_alpha(draw_x, draw_y, draw_w, 1, 14, 0x00FFFFFF, (50 * ease_p) >> 8);
            } else {
                draw_rounded_rect_alpha(draw_x, draw_y, draw_w, draw_h, 14, 0x00F5F6F8, (195 * ease_p) >> 8);
                draw_rounded_rect_alpha(draw_x, draw_y, draw_w, cur_hdr_h, 14, 0x00FFFFFF, (120 * ease_p) >> 8);
                draw_rounded_rect_alpha(draw_x, draw_y, draw_w, 1, 14, 0x00FFFFFF, (180 * ease_p) >> 8);
            }
        } else {
            uint32_t bg_col = is_dark ? 0x001B1C20 : 0x00FFFFFF;
            uint32_t hdr_col = is_dark ? 0x00282930 : 0x00E8EAEE;
            draw_rounded_rect_buf(draw_x, draw_y, draw_w, draw_h, 10, bg_col);
            draw_rounded_rect_buf(draw_x, draw_y, draw_w, cur_hdr_h + 4, 10, hdr_col);
            draw_rect_buf(draw_x, draw_y + cur_hdr_h, draw_w, draw_h - cur_hdr_h, bg_col);
        }

        uint32_t col_close = close_hover ? 0xFFFF453A : 0xFFFF5F56;
        uint32_t col_min   = min_hover   ? 0xFFFFD60A : 0xFFFFBD2E;
        uint32_t col_zoom  = zoom_hover  ? 0xFF30D158 : 0xFF27C93F;

        draw_rounded_rect_buf(close_x, tl_y, tl_size, tl_size, 6, col_close);
        draw_rounded_rect_buf(min_x,   tl_y, tl_size, tl_size, 6, col_min);
        draw_rounded_rect_buf(zoom_x,  tl_y, tl_size, tl_size, 6, col_zoom);

        if (win->anim_state == 0 && click && !g_prev_click) {
            if (close_hover) {
                win->anim_state = 2;
                win->anim_t = 0;
                captured_by_client = 1;
                continue;
            } else if (min_hover) {
                win->minimized = 1;
                captured_by_client = 1;
                continue;
            }
        }

        int title_w = font_text_width(win->title);
        int title_x = draw_x + (draw_w - title_w) / 2;
        if (g_window_controls_align == 0) {
            if (title_x < draw_x + 65) title_x = draw_x + 65;
        } else {
            if (title_x + title_w > draw_x + draw_w - 65) title_x = draw_x + draw_w - 65 - title_w;
        }
        int title_y = draw_y + (cur_hdr_h - 16) / 2;
        if (title_y < draw_y + 4) title_y = draw_y + 4;
        draw_string(win->title, title_x, title_y, is_dark ? 0xFFE5E5EA : 0xFF1D1D1F, backbuffer, scr_w);

        int client_h = draw_h - cur_hdr_h;
        if (win->framebuffer && client_h > 0 && draw_w > 0) {
            if (win->anim_state == 0) {
                int clip_y_start = (win->y + header_h < 0) ? -(win->y + header_h) : 0;
                int clip_y_end = win->h;
                if (win->y + header_h + clip_y_end > scr_h) {
                    clip_y_end = scr_h - (win->y + header_h);
                }

                int clip_x_start = (win->x < 0) ? -win->x : 0;
                int clip_x_end = win->w;
                if (win->x + clip_x_end > scr_w) {
                    clip_x_end = scr_w - win->x;
                }

                for (int r = clip_y_start; r < clip_y_end; r++) {
                    int dst_y = win->y + header_h + r;
                    uint32_t *dst_row = &backbuffer[dst_y * scr_w + win->x + clip_x_start];
                    const uint32_t *src_row = &win->framebuffer[r * win->w + clip_x_start];
                    int copy_count = clip_x_end - clip_x_start;
                    if (copy_count > 0) {
                        if (win->is_glass) {
                            for (int c = 0; c < copy_count; c++) {
                                int actual_c = clip_x_start + c;
                                int corner_r = 14;
                                int dy_corner = (win->h - 1) - r;
                                if (dy_corner < corner_r) {
                                    int dy = corner_r - dy_corner;
                                    if (actual_c < corner_r) {
                                        int dx = corner_r - actual_c;
                                        if (dx * dx + dy * dy > corner_r * corner_r) continue;
                                    } else if (actual_c >= win->w - corner_r) {
                                        int dx = actual_c - (win->w - corner_r);
                                        if (dx * dx + dy * dy > corner_r * corner_r) continue;
                                    }
                                }
                                uint32_t sc = src_row[c];
                                uint8_t a = (sc >> 24) & 0xFF;
                                if (a == 0 || a == 255) {
                                    dst_row[c] = sc;
                                } else {
                                    uint32_t dc = dst_row[c];
                                    uint32_t sr = (sc >> 16) & 0xFF;
                                    uint32_t sg = (sc >> 8) & 0xFF;
                                    uint32_t sb = sc & 0xFF;
                                    uint32_t dr = (dc >> 16) & 0xFF;
                                    uint32_t dg = (dc >> 8) & 0xFF;
                                    uint32_t db = dc & 0xFF;
                                    uint32_t or_val = (sr * a + dr * (255 - a)) / 255;
                                    uint32_t og_val = (sg * a + dg * (255 - a)) / 255;
                                    uint32_t ob_val = (sb * a + db * (255 - a)) / 255;
                                    dst_row[c] = (or_val << 16) | (og_val << 8) | ob_val;
                                }
                            }
                        } else {
                            for (int c = 0; c < copy_count; c++) {
                                dst_row[c] = src_row[c];
                            }
                        }
                    }
                }
            } else {
                int clip_y_start = (draw_y + cur_hdr_h < 0) ? -(draw_y + cur_hdr_h) : 0;
                int clip_y_end = client_h;
                if (draw_y + cur_hdr_h + clip_y_end > scr_h) {
                    clip_y_end = scr_h - (draw_y + cur_hdr_h);
                }
                int clip_x_start = (draw_x < 0) ? -draw_x : 0;
                int clip_x_end = draw_w;
                if (draw_x + clip_x_end > scr_w) {
                    clip_x_end = scr_w - draw_x;
                }

                for (int r = clip_y_start; r < clip_y_end; r++) {
                    int dst_y = draw_y + cur_hdr_h + r;
                    uint32_t *dst_row = &backbuffer[dst_y * scr_w + draw_x + clip_x_start];
                    int src_y = (r * win->h) / client_h;
                    if (src_y >= win->h) src_y = win->h - 1;
                    const uint32_t *src_row = &win->framebuffer[src_y * win->w];
                    int copy_count = clip_x_end - clip_x_start;
                    for (int c = 0; c < copy_count; c++) {
                        int actual_c = clip_x_start + c;
                        int src_x = (actual_c * win->w) / draw_w;
                        if (src_x >= win->w) src_x = win->w - 1;
                        uint32_t sc = src_row[src_x];
                        uint8_t a = (sc >> 24) & 0xFF;
                        if (a == 0 || a == 255) {
                            dst_row[c] = sc;
                        } else {
                            uint32_t dc = dst_row[c];
                            uint32_t sr = (sc >> 16) & 0xFF;
                            uint32_t sg = (sc >> 8) & 0xFF;
                            uint32_t sb = sc & 0xFF;
                            uint32_t dr = (dc >> 16) & 0xFF;
                            uint32_t dg = (dc >> 8) & 0xFF;
                            uint32_t db = dc & 0xFF;
                            uint32_t or_val = (sr * a + dr * (255 - a)) / 255;
                            uint32_t og_val = (sg * a + dg * (255 - a)) / 255;
                            uint32_t ob_val = (sb * a + db * (255 - a)) / 255;
                            dst_row[c] = (or_val << 16) | (og_val << 8) | ob_val;
                        }
                    }
                }
            }
        }

        draw_rect_buf(draw_x, draw_y + cur_hdr_h, draw_w, 1, is_dark ? (win->is_glass ? 0x002A2D3A : 0x003A3A40) : 0x00D0D4DF);

        if (win->anim_state == 0) {
            int in_client = (mx >= win->x && mx < win->x + win->w &&
                             my >= win->y + header_h && my < win->y + total_h);

            if (in_client) {
                captured_by_client = 1;
                int rel_x = mx - win->x;
                int rel_y = my - (win->y + header_h);

                if (click && !g_prev_click) {
                    uevent_t ev = { .type = EVENT_MOUSE_DOWN, .mx = rel_x, .my = rel_y, .btn = 1, .key = 0 };
                    uwindow_push_event(i, ev);
                } else if (!click && g_prev_click) {
                    uevent_t ev = { .type = EVENT_MOUSE_UP, .mx = rel_x, .my = rel_y, .btn = 0, .key = 0 };
                    uwindow_push_event(i, ev);
                } else if (mx != g_last_mx || my != g_last_my) {
                    uevent_t ev = { .type = EVENT_MOUSE_MOVE, .mx = rel_x, .my = rel_y, .btn = click, .key = 0 };
                    uwindow_push_event(i, ev);
                }
            }
        }
    }

    extern int mouse_right_clicked;
    static int s_prev_right = 0;
    int right_click = mouse_right_clicked;

    if (!captured_by_client && desktop_slot >= 0) {
        if (click && !g_prev_click) {
            uevent_t ev = { .type = EVENT_MOUSE_DOWN, .mx = mx, .my = my, .btn = 1, .key = 0 };
            uwindow_push_event(desktop_slot, ev);
        } else if (!click && g_prev_click) {
            uevent_t ev = { .type = EVENT_MOUSE_UP, .mx = mx, .my = my, .btn = 0, .key = 0 };
            uwindow_push_event(desktop_slot, ev);
        } else if (right_click && !s_prev_right) {
            uevent_t ev = { .type = EVENT_MOUSE_DOWN, .mx = mx, .my = my, .btn = 2, .key = 0 };
            uwindow_push_event(desktop_slot, ev);
        } else if (!right_click && s_prev_right) {
            uevent_t ev = { .type = EVENT_MOUSE_UP, .mx = mx, .my = my, .btn = 0, .key = 0 };
            uwindow_push_event(desktop_slot, ev);
        } else if (mx != g_last_mx || my != g_last_my) {
            uevent_t ev = { .type = EVENT_MOUSE_MOVE, .mx = mx, .my = my, .btn = click ? 1 : (right_click ? 2 : 0), .key = 0 };
            uwindow_push_event(desktop_slot, ev);
        }
    }
    s_prev_right = right_click;

    g_prev_click = click;
    g_last_mx = mx;
    g_last_my = my;
}
