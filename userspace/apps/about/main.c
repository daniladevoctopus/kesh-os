// окно о системе в юзерспейсе
#include "kesh.h"
#include "logo_data.h"

#define WIN_W 560
#define WIN_H 310

static void get_cpu_brand(char *brand, int max_len) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000));
    if (eax >= 0x80000004) {
        char buf[49];
        uint32_t *p = (uint32_t*)buf;
        for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
            __asm__ volatile ("cpuid" : "=a"(*p), "=b"(*(p+1)), "=c"(*(p+2)), "=d"(*(p+3)) : "a"(leaf));
            p += 4;
        }
        buf[48] = '\0';
        char *s = buf;
        while (*s == ' ') s++;
        int i = 0;
        while (s[i] && i < max_len - 1) {
            brand[i] = s[i];
            i++;
        }
        brand[i] = '\0';
    } else {
        const char *def_c = "x86_64 Processor";
        int i = 0;
        while (def_c[i] && i < max_len - 1) {
            brand[i] = def_c[i];
            i++;
        }
        brand[i] = '\0';
    }
}

static void int_to_str(int val, char *buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[16];
    int p = 0;
    while (val > 0) {
        tmp[p++] = '0' + (val % 10);
        val /= 10;
    }
    int dst = 0;
    while (p > 0) {
        buf[dst++] = tmp[--p];
    }
    buf[dst] = '\0';
}

static inline void blend_pixel(uint32_t *fb, int x, int y, uint32_t color, uint8_t alpha) {
    if (alpha == 0) return;
    int idx = y * WIN_W + x;
    uint32_t bg = fb[idx];
    if (alpha >= 255) {
        fb[idx] = (255 << 24) | (color & 0x00FFFFFF);
        return;
    }
    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = color & 0xFF;
    uint32_t bg_r = (bg >> 16) & 0xFF;
    uint32_t bg_g = (bg >> 8) & 0xFF;
    uint32_t bg_b = bg & 0xFF;
    uint32_t out_r = (r * alpha + bg_r * (255 - alpha)) / 255;
    uint32_t out_g = (g * alpha + bg_g * (255 - alpha)) / 255;
    uint32_t out_b = (b * alpha + bg_b * (255 - alpha)) / 255;
    fb[idx] = (255 << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

int main(void) {
    uint32_t *fb = kesh_create_window(WIN_W, WIN_H, "About KeshOS");
    if (!fb) return 1;

    char cpu_name[56];
    get_cpu_brand(cpu_name, sizeof(cpu_name));

    kesh_settings_t st;
    kesh_sysinfo_t si;

    while (1) {
        kesh_event_t ev;
        while (kesh_poll_event(0, &ev)) {
            if (ev.type == EVENT_CLOSE) {
                kesh_exit(0);
                return 0;
            }
        }

        kesh_get_settings(&st);
        kesh_get_sysinfo(&si);

        int is_dark = st.dark_mode;
        uint32_t bg_color = is_dark ? ((135 << 24) | 0x1E2230) : ((140 << 24) | 0xF6F8FA);
        uint32_t text_primary = is_dark ? 0xFFFFFFFF : 0xFF1D1D1F;
        uint32_t text_sec     = is_dark ? 0xFF9BA3B5 : 0xFF636366;
        uint32_t text_muted   = is_dark ? 0xFF727B8E : 0xFF8E8E93;

        kesh_clear(fb, WIN_W, WIN_H, bg_color);

        // чистый логотип без кругов и рамок
        int logo_x = 35;
        int logo_y = (WIN_H - LOGO_HEIGHT) / 2 - 8;

        for (int y = 0; y < LOGO_HEIGHT; y++) {
            for (int x = 0; x < LOGO_WIDTH; x++) {
                uint32_t px = g_kesh_logo[y * LOGO_WIDTH + x];
                uint8_t a = (px >> 24) & 0xFF;
                if (a > 0) {
                    blend_pixel(fb, logo_x + x, logo_y + y, px & 0x00FFFFFF, a);
                }
            }
        }

        // текст информации о системе
        int text_x = logo_x + LOGO_WIDTH + 34;
        int cur_y = 36;

        draw_string("KeshOS 1.0 Drop", text_x, cur_y, text_primary, fb, WIN_W);
        cur_y += 22;

        draw_string("Release Candidate 1 (Build 950.x86_64)", text_x, cur_y, text_sec, fb, WIN_W);
        cur_y += 24;

        kesh_draw_rect(fb, WIN_W, text_x, cur_y, WIN_W - text_x - 30, 1, is_dark ? 0x0032384A : 0x00D6DAE2);
        cur_y += 18;

        int label_w = 72;
        int max_val_w = (WIN_W - 20) - (text_x + label_w);

        // система
        draw_string("System", text_x, cur_y, text_sec, fb, WIN_W);
        draw_string("PC (x86_64, UEFI/GPT)", text_x + label_w, cur_y, text_primary, fb, WIN_W);
        cur_y += 22;

        // процессор
        draw_string("Chip", text_x, cur_y, text_sec, fb, WIN_W);
        if (font_text_width(cpu_name) > max_val_w && max_val_w > 60) {
            char fit[56];
            int ci = 0;
            while (cpu_name[ci] && ci < 46) {
                fit[ci] = cpu_name[ci];
                fit[ci + 1] = '\0';
                if (font_text_width(fit) + 16 > max_val_w) {
                    if (ci > 2) {
                        fit[ci - 2] = '.';
                        fit[ci - 1] = '.';
                        fit[ci] = '.';
                        fit[ci + 1] = '\0';
                    }
                    break;
                }
                ci++;
            }
            draw_string(fit, text_x + label_w, cur_y, text_primary, fb, WIN_W);
        } else {
            draw_string(cpu_name, text_x + label_w, cur_y, text_primary, fb, WIN_W);
        }
        cur_y += 22;

        // память
        draw_string("Memory", text_x, cur_y, text_sec, fb, WIN_W);
        int ram_mb = (int)(si.total_ram_bytes / (1024 * 1024));
        if (ram_mb <= 0) ram_mb = 2048;
        char ram_buf[32];
        char num_buf[16];
        int_to_str(ram_mb, num_buf);
        int ni = 0;
        while (num_buf[ni]) { ram_buf[ni] = num_buf[ni]; ni++; }
        ram_buf[ni++] = ' ';
        ram_buf[ni++] = 'M';
        ram_buf[ni++] = 'B';
        ram_buf[ni] = '\0';
        draw_string(ram_buf, text_x + label_w, cur_y, text_primary, fb, WIN_W);
        cur_y += 22;

        // графика
        draw_string("Graphics", text_x, cur_y, text_sec, fb, WIN_W);
        draw_string("VESA VBE 1024x768 32-bit", text_x + label_w, cur_y, text_primary, fb, WIN_W);
        cur_y += 22;

        // ядро
        draw_string("Kernel", text_x, cur_y, text_sec, fb, WIN_W);
        draw_string("1.0.0-rc1-ksh", text_x + label_w, cur_y, text_primary, fb, WIN_W);

        // копирайт внизу
        const char *copy_txt = "SneakDeak Technologies. All rights reserved.";
        int copy_w = font_text_width(copy_txt);
        draw_string(copy_txt, (WIN_W - copy_w) / 2, WIN_H - 20, text_muted, fb, WIN_W);

        kesh_update_window(0);
        kesh_sleep(25);
    }

    return 0;
}
