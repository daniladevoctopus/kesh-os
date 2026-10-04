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
#include "cpu.h"
#include "apic.h"
#include "log.h"
#include "serial.h"
#include "uwindow.h"
#include "vfs.h"
#include "timer.h"
#include "random.h"
#include "service.h"
#include "tty.h"
#include "block.h"
#include "gpt.h"
#include "acpi.h"
#include "input.h"
#include "socket.h"
#include "fd.h"
#include "ipc.h"
#include "display.h"
#include "../src/drivers/pci/pci.h"
#include "../src/drivers/usb/usb.h"
#include "../src/gfx/gpu.h"
#include "../src/drivers/system/sound_manager.h"

#include "../boot/loading/load_logo.h"
#include "../src/gui/font.h"
#include "cmd_mode.h"
#include "../src/drivers/system/keyboard.h"

extern void kernel_panic(const char *msg);

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
static const char keshos_build_id[] = "KESHOS-2026-09-29-BLACKSCREEN-V4";
uint32_t g_screen_w = 0;
uint32_t g_screen_h = 0;
uint32_t g_screen_pitch = 0;
uint32_t g_screen_bpp = 32;

extern void init_mouse(void);
extern void mouse_set_bounds(uint32_t width, uint32_t height);
extern void desktop_init(uint8_t* vram, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp);
extern void desktop_run(void);
extern uint8_t pic_get_mask(uint8_t irq_line);
static kesh_gpu_caps_t g_gpu_caps;

extern void wm_config_init(void);
extern void net_init(void);
static int start_process_service(void) { process_init(); return 0; }
static int start_window_service(void) { uwindow_init(); return 0; }
static int start_vfs_service(void) { vfs_init(); return 0; }
static int start_wm_config_service(void) { wm_config_init(); return 0; }
static int start_network_service(void) { net_init(); return 0; }

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int memory_security_selftest(void) {
    uint64_t first = pmm_alloc_page();
    if (!first || pmm_free_page(first) != 0) return -1;
    uint64_t reused = pmm_alloc_page();
    if (reused != first || pmm_free_page(reused) != 0) return -2;
    uint64_t run = pmm_alloc_pages(3);
    if (!run || pmm_free_pages(run, 3) != 0) return -11;
    if (pmm_alloc_pages(3) != run) return -12;
    if (pmm_free_pages(run, 3) != 0) return -13;

    uint64_t pml4 = vmm_create_user_pml4();
    uint64_t page = pml4 ? pmm_alloc_page() : 0;
    if (!pml4 || !page) {
        if (page) pmm_free_page(page);
        if (pml4) vmm_destroy_user_pml4(pml4);
        return -3;
    }
    const uint64_t test_va = 0x41000000ULL;
    if (vmm_map_page(pml4, test_va, page, PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE) != 0) {
        pmm_free_page(page);
        vmm_destroy_user_pml4(pml4);
        return -4;
    }
    if (vmm_map_page(pml4, test_va, page, PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE) == 0 ||
        vmm_map_page(pml4, USER_VA_LIMIT, page, PTE_USER | PTE_WRITABLE | PTE_NO_EXECUTE) == 0) {
        vmm_destroy_user_pml4(pml4);
        return -10;
    }
    uint64_t mapped_flags = vmm_get_page_flags(pml4, test_va);
    if (!(mapped_flags & PTE_USER) || !(mapped_flags & PTE_WRITABLE) || !(mapped_flags & PTE_NO_EXECUTE)) {
        vmm_destroy_user_pml4(pml4);
        return -5;
    }
    uint64_t wx_page = pmm_alloc_page();
    if (!wx_page) {
        vmm_destroy_user_pml4(pml4);
        return -14;
    }
    if (vmm_map_page(pml4, test_va + PAGE_SIZE, wx_page, PTE_USER | PTE_WRITABLE) == 0) {
        vmm_destroy_user_pml4(pml4);
        return -14;
    }
    pmm_free_page(wx_page);
    uint64_t kernel_cr3 = vmm_get_kernel_pml4();
    vmm_switch_pml4(pml4);
    volatile uint8_t *user_page = (volatile uint8_t *)test_va;
    user_page[0] = 'K';
    user_page[1] = 'S';
    char copied[2];
    int ok = copy_from_user(copied, (const void *)test_va, sizeof(copied)) == 0 && copied[0] == 'K' && copied[1] == 'S';
    vmm_switch_pml4(kernel_cr3);
    if (!ok || user_range_valid(USER_VA_LIMIT - 1ULL, 2, USER_ACCESS_READ)) {
        vmm_destroy_user_pml4(pml4);
        return -6;
    }

    if (vmm_set_page_flags(pml4, test_va, PTE_USER | PTE_NO_EXECUTE) != 0) {
        vmm_destroy_user_pml4(pml4);
        return -7;
    }
    mapped_flags = vmm_get_page_flags(pml4, test_va);
    if ((mapped_flags & PTE_WRITABLE) || !(mapped_flags & PTE_NO_EXECUTE)) {
        vmm_destroy_user_pml4(pml4);
        return -8;
    }

    if (vmm_destroy_user_pml4(pml4) != 0) return -9;
    return 0;
}

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
    KLOG_FATAL("boot", "CPU entering permanent halt");
    serial_print("[HALT] System halted.\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void kernel_clear_screen(uint32_t color) {
    KLOG_ENTER("framebuffer");
    if (!g_fb_vram) { KLOG_LEAVE("framebuffer", -1); return; }
    /* All Limine outputs are cleared before the primary desktop takes over.
       This avoids stale firmware contents on a secondary monitor. */
    display_clear(color);
    KLOG_LEAVE("framebuffer", 0);
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
    KLOG_ENTER("splash");
    if (!g_fb_vram || !logo) { KLOG_LEAVE("splash", -1); return; }
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
                    *(volatile uint32_t*)pixel_addr = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
                } else if (bytes_per_pixel == 3) {
                    pixel_addr[0] = b;
                    pixel_addr[1] = g;
                    pixel_addr[2] = r;
                }
            }
        }
    }
    KLOG_LEAVE("splash", 0);
}

__attribute__((unused))
static void draw_logo_to_screen(const unsigned char *logo, int src_w, int src_h) {
    draw_logo_to_screen_alpha(logo, src_w, src_h, 255);
}

static void framebuffer_probe_pattern(void) {
    KLOG_TRACE("framebuffer", "probe begin vram=%p size=%ux%u pitch=%u bpp=%u",
               (void*)g_fb_vram, (unsigned)g_screen_w, (unsigned)g_screen_h,
               (unsigned)g_screen_pitch, (unsigned)g_screen_bpp);
    if (!g_fb_vram || g_screen_w < 16 || g_screen_h < 16) {
        KLOG_ERROR("framebuffer", "probe skipped: invalid framebuffer geometry");
        return;
    }
    fill_rect(0, 0, 16, 16, 0x00FFFFFF);
    fill_rect((int)g_screen_w - 16, 0, 16, 16, 0x0000FFFF);
    fill_rect(0, (int)g_screen_h - 16, 16, 16, 0x00FF00FF);
    fill_rect((int)g_screen_w - 16, (int)g_screen_h - 16, 16, 16, 0x00FF0000);
    KLOG_TRACE("framebuffer", "probe complete corner_markers=4");
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

    /* Prompt for F9 Recovery Mode */
    const char *hint = "[ F9 ] KeshOS Recovery Mode";
    int hint_w = font_text_width(hint);
    int hint_x = ((int)g_screen_w - hint_w) / 2;
    int hint_y = bar_y + 26;

    fill_rect(hint_x - 6, hint_y - 2, hint_w + 12, 20, 0x00000000);
    draw_string(hint, hint_x, hint_y, 0x009E9EA3, (uint32_t*)g_fb_vram, g_screen_pitch / 4);
}

void kernel_main(void) {
    enable_sse();

    KLOG_INFO("boot", "kernel_main entered; SSE path enabled");

    KLOG_INFO("gdt", "initializing GDT/TSS ring0/ring3");
    gdt_init();
    KLOG_DEBUG("gdt", "selectors kcs=0x28 kss=0x30 uss=0x3B ucs=0x43 tss=0x48");

    syscall_init();
    KLOG_DEBUG("syscall", "SCE=1 STAR/LSTAR configured");

    cpu_init_bsp();
    idt_init();
    KLOG_DEBUG("idt", "IDT loaded; external IRQs still masked");
    timer_init(250);
    KLOG_DEBUG("timer", "PIT channel0 configured frequency=250Hz irq0_mask=%u", (unsigned)pic_get_mask(0));

    pmm_init();
    random_init();
    tty_init();
    int tty_test = tty_selftest();
    KLOG_INFO("tty", "ownership self-test result=%d", tty_test);
    if (tty_test != 0) kernel_panic("TTY ownership self-test failed");
    input_init();
    socket_init();
    fd_init();
    extern void unix_ipc_init(void);
    unix_ipc_init();
    extern void evdev_init(void);
    evdev_init();
    extern void drm_fb_init(void);
    drm_fb_init();
    ipc_init();
    if (acpi_init() != 0) KLOG_WARN("acpi", "firmware tables unavailable or invalid; using legacy discovery only");
    else (void)timer_calibrate_from_acpi();
    int memory_test = memory_security_selftest();
    KLOG_INFO("memory", "security self-test=%s", memory_test == 0 ? "PASS" : "FAIL");
    KLOG_DEBUG("memory", "HHDM offset=%llx", (unsigned long long)g_hhdm_offset);
    serial_print("[MEM] hhdm=");
    serial_print_hex(g_hhdm_offset);
    serial_print("\n");

    int apic_result = apic_init();
    int apic_irq_mode = apic_result == 0 && apic_enable_irq_routing() == 0;
    KLOG_NOTICE("apic", "bring-up result=%d controller=%s timer=PIT", apic_result,
                apic_irq_mode ? "IOAPIC" : "PIC");
    cpu_smp_init();
    serial_print("[CPU] online=");
    serial_print_dec(cpu_online_count());
    serial_print(" total=");
    serial_print_dec(cpu_count());
    KLOG_DEBUG("cpu", "topology online=%u total=%u bsp_apic_id=%u",
               (unsigned)cpu_online_count(), (unsigned)cpu_count(),
               (unsigned)apic_current_lapic_id());
    __asm__ volatile("sti" : : : "memory");
    if (apic_irq_mode) {
        uint64_t irq_before = timer_irq_count();
        for (uint32_t wait = 0; wait < 5000000U && timer_irq_count() == irq_before; ++wait) __asm__ volatile("pause");
        if (timer_irq_count() == irq_before) {
            apic_disable_irq_routing();
            apic_irq_mode = 0;
            KLOG_ERROR("apic", "IOAPIC timer route inactive; restored legacy PIC routing");
        }
    }
    uint64_t boot_rflags = 0;
    __asm__ volatile("pushfq; popq %0" : "=r"(boot_rflags));
    KLOG_NOTICE("irq", "interrupts enabled IF=%u vector_irq0=32 controller=%s timer=PIT",
                (unsigned)((boot_rflags >> 9) & 1ULL), apic_irq_mode ? "IOAPIC" : "PIC");

    int ecam_regions = pci_init();
    KLOG_INFO("pci", "configuration=%s ecam_regions=%d", pci_uses_ecam() ? "ECAM" : "legacy-io", ecam_regions);
    pci_device_t pci_devices[16];
    int pci_count = pci_enumerate(pci_devices, 16);
    serial_print("[PCI] functions=");
    serial_print_dec((uint64_t)pci_count);
    serial_print("\n");
    usb_init();
    KLOG_INFO("usb", "host-controller discovery completed");
    int audio_result = sound_init();
    KLOG_INFO("audio", "controller=%d playback_ready=%d init_result=%d",
              (int)sound_device(), sound_is_ready(), audio_result);
    kesh_gpu_init(&g_gpu_caps);
    serial_print("[GFX] backend=cpu SSE2=");
    serial_print_dec(g_gpu_caps.sse2 ? 1 : 0);
    serial_print(" AVX2=");
    serial_print_dec(g_gpu_caps.avx2 ? 1 : 0);
    serial_print("\n");

    block_init();
    for (int i = 0; i < block_device_count(); ++i) {
        const block_device_t *device = block_get_device(i);
        if (device && device->block_size == 512) (void)gpt_scan(i);
    }
    service_manager_init();
    int process_service = service_register("process", SERVICE_NO_DEPENDENCY, start_process_service);
    int window_service = service_register("window", process_service, start_window_service);
    int vfs_service = service_register("vfs", SERVICE_NO_DEPENDENCY, start_vfs_service);
    (void)service_register("wm-config", window_service, start_wm_config_service);
    (void)service_register("network", vfs_service, start_network_service);
    int service_result = service_start_all();
    int manifest_services = service_load_manifest("/hdd/etc/services.conf");
    if (manifest_services > 0) service_result = service_start_all();
    int seed_result = random_load_persistent_seed();
    int previous_log_size = klog_load_persisted();
    KLOG_INFO("service", "boot services ready=%d result=%d", service_ready_count(), service_result);
    KLOG_INFO("service", "manifest userspace services=%d", manifest_services > 0 ? manifest_services : 0);
    KLOG_INFO("random", "persistent boot seed result=%d", seed_result);
    KLOG_INFO("diagnostics", "previous boot log bytes=%d", previous_log_size > 0 ? previous_log_size : 0);
    uint64_t pre_ring3_flags = 0;
    __asm__ volatile("pushfq; popq %0" : "=r"(pre_ring3_flags));
    KLOG_TRACE("timer", "pre-ring3 ticks=%llu irq0=%llu mask0=%u IF=%u",
               (unsigned long long)timer_ticks(), (unsigned long long)timer_irq_count(),
               (unsigned)pic_get_mask(0), (unsigned)((pre_ring3_flags >> 9) & 1ULL));

    KLOG_INFO("process", "starting ring3 payload verification");
    process_run_test_ring3();
    KLOG_INFO("process", "ring3 payload returned to kernel mode");
    uint64_t post_ring3_flags = 0;
    __asm__ volatile("pushfq; popq %0" : "=r"(post_ring3_flags));
    KLOG_TRACE("timer", "post-ring3 ticks=%llu irq0=%llu IF=%u",
               (unsigned long long)timer_ticks(), (unsigned long long)timer_irq_count(),
               (unsigned)((post_ring3_flags >> 9) & 1ULL));

    KLOG_INFO("framebuffer", "checking Limine framebuffer response");
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        KLOG_FATAL("framebuffer", "Limine framebuffer response is NULL or empty");
        halt();
    }

    if (display_init(framebuffer_request.response->framebuffers, framebuffer_request.response->framebuffer_count) != 0) {
        KLOG_FATAL("display", "no valid framebuffer outputs discovered");
        halt();
    }
    const display_info_t *primary = display_get(0);
    g_fb_vram = (uint8_t *)(uint64_t)primary->address;
    g_screen_w = primary->width;
    g_screen_h = primary->height;
    g_screen_pitch = primary->pitch;
    g_screen_bpp = primary->bpp;
    KLOG_INFO("display", "outputs=%u primary=%ux%u scale=%u", (unsigned)display_count(),
              (unsigned)g_screen_w, (unsigned)g_screen_h, (unsigned)primary->scale_milli);
    if (g_screen_bpp == 32) kesh_gpu_set_target((uint32_t*)g_fb_vram, g_screen_w, g_screen_h, g_screen_pitch / 4);

    KLOG_INFO("framebuffer", "Limine FB=%p %ux%u bpp=%u pitch=%u",
              (void*)g_fb_vram, (unsigned)g_screen_w, (unsigned)g_screen_h,
              (unsigned)g_screen_bpp, (unsigned)g_screen_pitch);

    KLOG_INFO("framebuffer", "clear start color=0x00000000");
    kernel_clear_screen(0x00000000);

    framebuffer_probe_pattern();

    KLOG_INFO("splash", "safe boot splash start; timer-free rendering path enabled");
    KLOG_TRACE("splash", "logo source=%p pixels=%u bytes=%u",
               (void*)load_logo, (unsigned)(LOGO_WIDTH * LOGO_HEIGHT),
               (unsigned)(LOGO_WIDTH * LOGO_HEIGHT * 3));
    draw_logo_to_screen_alpha(load_logo, LOGO_WIDTH, LOGO_HEIGHT, 255);
    KLOG_TRACE("splash", "logo render returned successfully");

    /* The splash must never be able to block the kernel on an interrupt-driven
       timer. Use a bounded CPU delay only for the visual pause, then continue. */
    spin_delay(2500000U);

    KLOG_INFO("splash", "progress bar start range=0..100");
    draw_mac_progress_bar(0);
    /* Initialize keyboard driver early to capture F9 hotkey during boot */
    init_keyboard();

    int mouse_initialized = 0;
    uint64_t boot_duration_ms = 3500ULL; 
    uint64_t boot_start = timer_millis();
    int boot_to_cmd = 0;

    while (1) {
        uint64_t now = timer_millis();
        uint64_t elapsed = (now >= boot_start) ? (now - boot_start) : 0;
        if (elapsed >= boot_duration_ms) break;

        /* Poll keyboard for F9 during boot splash */
        kbd_event_t kev;
        while (keyboard_poll_event(&kev)) {
            if (kev.pressed && !kev.extended && kev.scancode == 0x43) { /* F9 key */
                boot_to_cmd = 1;
                break;
            }
        }
        if (boot_to_cmd) break;

        int percent = (int)((elapsed * 100ULL) / boot_duration_ms);
        if (percent > 100) percent = 100;

        if (percent >= 50 && !mouse_initialized) {
            KLOG_INFO("mouse", "PS/2 mouse initialization");
            mouse_set_bounds(g_screen_w, g_screen_h);
            init_mouse();
            mouse_initialized = 1;
            KLOG_DEBUG("mouse", "PS/2 mouse initialized bounds=%ux%u", (unsigned)g_screen_w, (unsigned)g_screen_h);
        }

        draw_mac_progress_bar(percent);
        timer_wait_ms(25);
    }

    if (boot_to_cmd) {
        if (!mouse_initialized) {
            KLOG_INFO("mouse", "PS/2 mouse initialization for cmd mode");
            mouse_set_bounds(g_screen_w, g_screen_h);
            init_mouse();
            mouse_initialized = 1;
        }
        KLOG_INFO("boot", "F9 key detected during splash! Launching Kernel CMD Mode...");
        kernel_cmd_mode();
        /* If user typed 'desktop' or 'exit', boot continues into Desktop Shell */
    }

    if (!mouse_initialized) {
        KLOG_INFO("mouse", "PS/2 mouse initialization");
        mouse_set_bounds(g_screen_w, g_screen_h);
        init_mouse();
        mouse_initialized = 1;
    }

    draw_mac_progress_bar(100);
    timer_wait_ms(250);

    KLOG_INFO("desktop", "desktop compositor initialization");
    desktop_init(g_fb_vram, g_screen_w, g_screen_h, g_screen_pitch, g_screen_bpp);
    KLOG_INFO("desktop", "entering desktop_run main event loop");
    desktop_run();

    halt();
}

__attribute__((noreturn))
void _start(void) {
    __asm__ volatile ("cli");
    disable_pic();
    serial_init();
    klog_init();
    KLOG_NOTICE("boot", "serial COM1 initialized; build_id=%s; runtime logger active", keshos_build_id);

    kernel_main();

    KLOG_ERROR("boot", "kernel_main returned unexpectedly; build_id=%s", keshos_build_id);
    halt();
}
