// старт ядра, инит всего железа по порядку
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "include/idt.h"
#include "gdt.h"
#include "syscall.h"
#include "memory.h"
#include "process.h"
#include "uwindow.h"
#include "vfs.h"
#include "timer.h"
#include "../src/drivers/pci/pci.h"
#include "../src/drivers/usb/usb.h"
#include "../src/gfx/gpu.h"

#include "../boot/loading/load_logo.h"
#include "../src/gui/font.h"

__attribute__((used, section(".requests")))
static volatile uint64_t limine_base_revision[3] = {
    0xf9562b2d5c95a6c8,
    0x6a7b384944536bdc,
    0
};

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

uint8_t* g_fb_vram = 0;
uint32_t g_screen_w = 0;
uint32_t g_screen_h = 0;
uint32_t g_screen_pitch = 0;
uint32_t g_screen_bpp = 32;

extern void init_mouse(void);
extern void mouse_set_bounds(uint32_t width, uint32_t height);
extern void desktop_init(uint8_t* vram, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp);
extern void desktop_run(void);
static kesh_gpu_caps_t g_gpu_caps;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static void serial_print(const char *str) {
    while (*str) outb(0x3F8, *str++);
}

void serial_print_hex(uint64_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        buf[2 + i] = hex_chars[(val >> ((15 - i) * 4)) & 0xF];
    }
    buf[18] = '\0';
    serial_print(buf);
}

void serial_print_dec(uint64_t val) {
    if (val == 0) {
        serial_print("0");
        return;
    }
    char buf[32];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        outb(0x3F8, buf[j]);
    }
}

static void klog_level(int level, const char *tag, const char *msg) {
    (void)level;
    uint64_t ms = timer_millis();
    serial_print("[");
    serial_print_dec(ms / 1000);
    serial_print(".");
    uint64_t frac = ms % 1000;
    if (frac < 100) serial_print("0");
    if (frac < 10) serial_print("0");
    serial_print_dec(frac);
    serial_print("] [");
    serial_print(tag);
    serial_print("] ");
    serial_print(msg);
}

#define KLOG_DEBUG(msg) klog_level(7, "DEBUG", msg)
#define KLOG_INFO(msg)  klog_level(6, "INFO ", msg)
#define KLOG_WARN(msg)  klog_level(4, "WARN ", msg)
#define KLOG_ERR(msg)   klog_level(3, "ERR  ", msg)

static void disable_pic(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

static void enable_sse(void) {
    uint64_t cr0, cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2);
    cr0 |= (1 << 1);
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);
    cr4 |= (1 << 10);
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
}

__attribute__((unused))
static void sleep_rtc(uint32_t seconds) {
    for (uint32_t s = 0; s < seconds; s++) {
        outb(0x70, 0x00);
        uint8_t last_sec = inb(0x71);
        while (1) {
            outb(0x70, 0x00);
            uint8_t curr_sec = inb(0x71);
            if (curr_sec != last_sec) break;
            __asm__ volatile ("pause");
        }
    }
}

__attribute__((noreturn))
static void halt(void) {
    serial_print("[HALT] System halted.\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void kernel_clear_screen(uint32_t color) {
    if (!g_fb_vram) return;
    uint32_t bytes_per_pixel = g_screen_bpp / 8;
    if (bytes_per_pixel == 0) bytes_per_pixel = 4;

    for (uint32_t y = 0; y < g_screen_h; y++) {
        uint8_t* row = g_fb_vram + (y * g_screen_pitch);
        for (uint32_t x = 0; x < g_screen_w; x++) {
            if (bytes_per_pixel == 4) {
                ((uint32_t*)row)[x] = color;
            } else if (bytes_per_pixel == 3) {
                row[x * 3 + 0] = (color) & 0xFF;
                row[x * 3 + 1] = (color >> 8) & 0xFF;
                row[x * 3 + 2] = (color >> 16) & 0xFF;
            }
        }
    }
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_fb_vram) return;
    uint32_t bytes_per_pixel = g_screen_bpp / 8;
    if (bytes_per_pixel == 0) bytes_per_pixel = 4;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > (int)g_screen_w) x1 = (int)g_screen_w;
    int y1 = y + h; if (y1 > (int)g_screen_h) y1 = (int)g_screen_h;

    for (int py = y0; py < y1; py++) {
        uint8_t* row = g_fb_vram + ((uint32_t)py * g_screen_pitch);
        for (int px = x0; px < x1; px++) {
            if (bytes_per_pixel == 4) {
                ((uint32_t*)row)[px] = color;
            } else if (bytes_per_pixel == 3) {
                row[px * 3 + 0] = color & 0xFF;
                row[px * 3 + 1] = (color >> 8) & 0xFF;
                row[px * 3 + 2] = (color >> 16) & 0xFF;
            }
        }
    }
}

static void spin_delay(uint32_t iterations) {
    for (volatile uint32_t i = 0; i < iterations; i++) {
        __asm__ volatile ("nop");
    }
}

static int compute_splash_logo_size(void) {
    int min_side = (int)((g_screen_w < g_screen_h) ? g_screen_w : g_screen_h);
    int target_size = (min_side * 22) / 100;
    if (target_size < 64) target_size = 64;
    if (target_size > 800) target_size = 800;
    return target_size;
}

static void draw_logo_to_screen_alpha(const unsigned char *logo, int src_w, int src_h, int alpha) {
    if (!g_fb_vram || !logo) return;
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    uint32_t bytes_per_pixel = g_screen_bpp / 8;
    if (bytes_per_pixel == 0) bytes_per_pixel = 4;

    int width = compute_splash_logo_size();
    int height = width;

    int start_x = ((int)g_screen_w - width) / 2;
    int start_y = ((int)g_screen_h - height) / 2 - 25;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int target_x = start_x + x;
            int target_y = start_y + y;

            if (target_x >= 0 && target_x < (int)g_screen_w &&
                target_y >= 0 && target_y < (int)g_screen_h) {

                int gx = (width  > 1) ? (x * (src_w - 1) * 256) / (width  - 1) : 0;
                int gy = (height > 1) ? (y * (src_h - 1) * 256) / (height - 1) : 0;
                int x0 = gx >> 8, y0 = gy >> 8;
                int x1 = (x0 < src_w - 1) ? x0 + 1 : x0;
                int y1 = (y0 < src_h - 1) ? y0 + 1 : y0;
                int fx = gx & 0xFF, fy = gy & 0xFF;

                int idx00 = (y0 * src_w + x0) * 3;
                int idx10 = (y0 * src_w + x1) * 3;
                int idx01 = (y1 * src_w + x0) * 3;
                int idx11 = (y1 * src_w + x1) * 3;

                uint8_t r = (uint8_t)((logo[idx00]     * (256 - fx) * (256 - fy) +
                                       logo[idx10]     * fx         * (256 - fy) +
                                       logo[idx01]     * (256 - fx) * fy +
                                       logo[idx11]     * fx         * fy) >> 16);
                uint8_t g = (uint8_t)((logo[idx00 + 1] * (256 - fx) * (256 - fy) +
                                       logo[idx10 + 1] * fx         * (256 - fy) +
                                       logo[idx01 + 1] * (256 - fx) * fy +
                                       logo[idx11 + 1] * fx         * fy) >> 16);
                uint8_t b = (uint8_t)((logo[idx00 + 2] * (256 - fx) * (256 - fy) +
                                       logo[idx10 + 2] * fx         * (256 - fy) +
                                       logo[idx01 + 2] * (256 - fx) * fy +
                                       logo[idx11 + 2] * fx         * fy) >> 16);

                if (alpha < 255) {
                    r = (uint8_t)((r * alpha) >> 8);
                    g = (uint8_t)((g * alpha) >> 8);
                    b = (uint8_t)((b * alpha) >> 8);
                }

                uint8_t* pixel_addr = g_fb_vram + (target_y * g_screen_pitch) + (target_x * bytes_per_pixel);

                if (bytes_per_pixel == 4) {
                    *(uint32_t*)pixel_addr = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
                } else if (bytes_per_pixel == 3) {
                    pixel_addr[0] = b;
                    pixel_addr[1] = g;
                    pixel_addr[2] = r;
                }
            }
        }
    }
}

__attribute__((unused))
static void draw_logo_to_screen(const unsigned char *logo, int src_w, int src_h) {
    draw_logo_to_screen_alpha(logo, src_w, src_h, 255);
}

static void fill_rounded_bar(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (w < 6) {
        fill_rect(x, y, w, h, color);
        return;
    }

    fill_rect(x + 2, y,     w - 4, 1, color);
    fill_rect(x + 1, y + 1, w - 2, 1, color);
    fill_rect(x,     y + 2, w,     2, color);
    fill_rect(x + 1, y + 4, w - 2, 1, color);
    fill_rect(x + 2, y + 5, w - 4, 1, color);
}

static void draw_mac_progress_bar(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    int logo_size = compute_splash_logo_size();
    int bar_w = 240;
    int bar_h = 6;
    int bar_x = ((int)g_screen_w - bar_w) / 2;
    int bar_y = ((int)g_screen_h - logo_size) / 2 - 25 + logo_size + 42;

    fill_rounded_bar(bar_x, bar_y, bar_w, bar_h, 0x00333336);

    int fill_w = (bar_w * percent) / 100;
    if (fill_w > 0) {
        fill_rounded_bar(bar_x, bar_y, fill_w, bar_h, 0x00F2F2F7);
    }
}

void kernel_main(void) {
    enable_sse();

    KLOG_INFO("Kernel main reached. CPU SSE enabled.\n");

    KLOG_INFO("[0/6] Initializing GDT & TSS (Ring 0 & Ring 3 segments)...\n");
    gdt_init();
    KLOG_DEBUG("GDT initialized with Kernel CS 0x28, SS 0x30, User SS 0x3B, CS 0x43, TSS 0x48.\n");

    syscall_init();
    KLOG_DEBUG("IA32_EFER.SCE enabled, STAR MSR and LSTAR (syscall_entry_stub) loaded.\n");

    idt_init();
    KLOG_DEBUG("IDT loaded, hardware interrupts unmasked.\n");
    timer_init(250);
    KLOG_DEBUG("PIT timer channel 0 configured for 250 Hz ticks.\n");

    pmm_init();
    KLOG_DEBUG("PMM/VMM initialized. HHDM offset: ");
    serial_print_hex(g_hhdm_offset);
    serial_print("\n");

    pci_device_t pci_devices[16];
    int pci_count = pci_enumerate(pci_devices, 16);
    KLOG_INFO("[PCI] Enumerated ");
    serial_print_dec((uint64_t)pci_count);
    serial_print(" PCI functions.\n");
    usb_init();
    KLOG_INFO("[USB] Host-controller discovery completed.\n");
    kesh_gpu_init(&g_gpu_caps);
    KLOG_INFO("[GFX] CPU graphics backend initialized (SSE2=");
    serial_print_dec(g_gpu_caps.sse2 ? 1 : 0);
    serial_print(", AVX2=");
    serial_print_dec(g_gpu_caps.avx2 ? 1 : 0);
    serial_print(").\n");

    process_init();
    uwindow_init();
    vfs_init();
    extern void wm_config_init(void);
    wm_config_init();
    extern void net_init(void);
    net_init();
    KLOG_DEBUG("Process table, user window compositor, VFS and network stack initialized.\n");

    KLOG_INFO("[RING3] Starting isolated user mode payload verification...\n");
    process_run_test_ring3();
    KLOG_INFO("[RING3] User mode verification passed, returned safely to Kernel Mode!\n");

    KLOG_INFO("[1/6] Checking Limine framebuffer...\n");
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        KLOG_ERR("Limine framebuffer response is NULL or empty!\n");
        halt();
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    g_fb_vram = (uint8_t*)fb->address;
    g_screen_w = (uint32_t)fb->width;
    g_screen_h = (uint32_t)fb->height;
    g_screen_pitch = (uint32_t)fb->pitch;
    g_screen_bpp = (uint32_t)fb->bpp;
    if (g_screen_bpp == 32) kesh_gpu_set_target((uint32_t*)g_fb_vram, g_screen_w, g_screen_h, g_screen_pitch / 4);

    KLOG_INFO("[2/6] Framebuffer acquired: ");
    serial_print_dec(g_screen_w);
    serial_print("x");
    serial_print_dec(g_screen_h);
    serial_print(" @ ");
    serial_print_dec(g_screen_bpp);
    serial_print("bpp, pitch=");
    serial_print_dec(g_screen_pitch);
    serial_print(", vram=");
    serial_print_hex((uint64_t)g_fb_vram);
    serial_print("\n");

    KLOG_INFO("[3/6] Clearing screen to black background...\n");
    kernel_clear_screen(0x00000000);

    KLOG_INFO("[4/6] Fading in Apple-style boot logo...\n");
    for (int a = 0; a <= 255; a += 15) {
        draw_logo_to_screen_alpha(load_logo, LOGO_WIDTH, LOGO_HEIGHT, a);
        timer_wait_ms(20);
    }
    draw_logo_to_screen_alpha(load_logo, LOGO_WIDTH, LOGO_HEIGHT, 255);

    KLOG_INFO("[BOOT] Starting Apple-style boot progress bar (smooth 0-100%)...\n");
    draw_mac_progress_bar(0);
    timer_wait_ms(150);

    int mouse_initialized = 0;
    uint64_t boot_duration_ms = 3500ULL; 
    uint64_t boot_start = timer_millis();

    while (1) {
        uint64_t now = timer_millis();
        uint64_t elapsed = (now >= boot_start) ? (now - boot_start) : 0;
        if (elapsed >= boot_duration_ms) break;

        int percent = (int)((elapsed * 100ULL) / boot_duration_ms);
        if (percent > 100) percent = 100;

        if (percent >= 50 && !mouse_initialized) {
            KLOG_INFO("[5/6] Initializing mouse drivers...\n");
            mouse_set_bounds(g_screen_w, g_screen_h);
            init_mouse();
            mouse_initialized = 1;
            KLOG_DEBUG("PS/2 mouse initialized with display bounds.\n");
        }

        draw_mac_progress_bar(percent);
        timer_wait_ms(25);
    }

    if (!mouse_initialized) {
        KLOG_INFO("[5/6] Initializing mouse drivers...\n");
        mouse_set_bounds(g_screen_w, g_screen_h);
        init_mouse();
        mouse_initialized = 1;
    }

    draw_mac_progress_bar(100);
    timer_wait_ms(250);

    KLOG_INFO("[6/6] Initializing desktop compositor...\n");
    desktop_init(g_fb_vram, g_screen_w, g_screen_h, g_screen_pitch, g_screen_bpp);
    KLOG_INFO("[DESKTOP] Launching desktop_run main event loop!\n");
    desktop_run();

    halt();
}

__attribute__((noreturn))
void _start(void) {
    __asm__ volatile ("cli");
    disable_pic();
    serial_init();

    serial_print("\n===============================\n");
    serial_print("       KERNEL BOOT START       \n");
    serial_print("===============================\n");

    kernel_main();
    halt();
}
