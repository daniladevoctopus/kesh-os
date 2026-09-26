// дескрипторы окон
#ifndef UWINDOW_H
#define UWINDOW_H

#include <stdint.h>
#include <stddef.h>

#define MAX_USER_WINDOWS 8
#define USER_WINDOW_HEADER_H 30
#define USER_WINDOW_FB_VADDR 0x50000000ULL

#define EVENT_NONE       0
#define EVENT_MOUSE_MOVE 1
#define EVENT_MOUSE_DOWN 2
#define EVENT_MOUSE_UP   3
#define EVENT_KEY_DOWN   4
#define EVENT_CLOSE      5

typedef struct {
    int type;
    int mx;
    int my;
    int btn;
    int key;
} uevent_t;

typedef struct user_window {
    int active;
    int win_id;
    int is_desktop;          
    int is_glass;
    int x, y;
    int w, h;
    char title[64];
    uint32_t *framebuffer;   
    uint64_t user_fb_vaddr;  
    uint64_t pml4_phys;
    int is_open;
    int minimized;
    int dragging;
    int drag_ox, drag_oy;
    int dirty;
    int anim_state;
    int anim_t;

    uevent_t events[32];
    int ev_head;
    int ev_tail;
} user_window_t;

void uwindow_init(void);
user_window_t* uwindow_create(int w, int h, const char *title, uint64_t pml4_phys);
void uwindow_destroy(int win_id);
user_window_t* uwindow_get(int win_id);
int uwindow_push_event(int win_id, uevent_t ev);
int uwindow_pop_event(int win_id, uevent_t *out_ev);
void uwindow_feed_key(char c);

void uwindow_render_all(uint32_t *backbuffer, int scr_w, int scr_h, int mx, int my, int click, int single_click);
int uwindow_is_any_open(void);
int uwindow_has_desktop_surface(void);
int uwindow_is_focused(void);
void uwindow_set_focused(int focused);
int uwindow_is_focused_idx(int idx);

#endif
