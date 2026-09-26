// стейт рабочего стола
#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  color_info[6];
} __attribute__((packed)) multiboot_info_t;

void kernel_main(uint32_t magic, multiboot_info_t* mb_info);
void desktop_init(uint8_t* vram, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp);
void desktop_run(void);
void refresh_wallpaper(void);

int win_drag_available(void);
void win_drag_claim(void);

#define WIN_ID_FILE_     0
#define WIN_ID_MUSIC_    1
#define WIN_ID_ABOUT_    2
#define WIN_ID_CALC_     3
#define WIN_ID_TERMINAL_ 4
#define WIN_ID_SETTINGS_ 5
#define WIN_ID_DOOM_     6
#define WIN_ID_USER_APP_ 7

void win_report_rect(int id, int x, int y, int w, int h, int is_open);
int win_click_occluded(int id, int mx, int my);
int win_is_open(int id);
int win_is_focused(int id);
void win_set_focused(int id);

void draw_pixel_buf(int x, int y, uint32_t color);
void draw_rect_buf(int x, int y, int w, int h, uint32_t color);
void draw_rounded_rect_buf(int x, int y, int w, int h, int r, uint32_t color);
void draw_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
uint32_t blend_colors(uint32_t bg, uint32_t fg, uint8_t alpha);

void read_rtc_datetime(int *h, int *m, int *s, int *day, int *month, int *year);

#endif
