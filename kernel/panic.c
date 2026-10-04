// кернел паника keshos
#include <stdint.h>
#include "memory.h"

extern uint8_t* g_fb_vram;
extern uint32_t g_screen_w;
extern uint32_t g_screen_h;
extern uint32_t g_screen_pitch;

extern void draw_string(const char* str, int x, int y, uint32_t color, uint32_t* buf, uint32_t buf_width);
extern int font_text_width(const char* str);

static void panic_put_pixel(int x, int y, uint32_t color) {
    if (!g_fb_vram) return;
    if (x < 0 || y < 0 || (uint32_t)x >= g_screen_w || (uint32_t)y >= g_screen_h) return;
    uint32_t* row = (uint32_t*)(g_fb_vram + (uint64_t)y * g_screen_pitch);
    row[x] = color;
}

static void panic_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            panic_put_pixel(x + i, y + j, color);
        }
    }
}

static void panic_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    int r2 = r * r;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int cx = -1, cy = -1;
            if (dx < r && dy < r) { cx = r; cy = r; }
            else if (dx >= w - r && dy < r) { cx = w - r - 1; cy = r; }
            else if (dx < r && dy >= h - r) { cx = r; cy = h - r - 1; }
            else if (dx >= w - r && dy >= h - r) { cx = w - r - 1; cy = h - r - 1; }

            if (cx != -1) {
                int dist2 = (dx - cx) * (dx - cx) + (dy - cy) * (dy - cy);
                if (dist2 > r2) continue;
            }
            panic_put_pixel(x + dx, y + dy, color);
        }
    }
}

static void panic_draw_rounded_outline(int x, int y, int w, int h, int r, uint32_t color) {
    int r2 = r * r;
    int in_r = r - 1;
    int in_r2 = in_r * in_r;

    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int is_border = 0;
            int cx = -1, cy = -1;
            if (dx < r && dy < r) { cx = r; cy = r; }
            else if (dx >= w - r && dy < r) { cx = w - r - 1; cy = r; }
            else if (dx < r && dy >= h - r) { cx = r; cy = h - r - 1; }
            else if (dx >= w - r && dy >= h - r) { cx = w - r - 1; cy = h - r - 1; }

            if (cx != -1) {
                int dist2 = (dx - cx) * (dx - cx) + (dy - cy) * (dy - cy);
                if (dist2 <= r2 && dist2 >= in_r2) is_border = 1;
            } else {
                if (dx == 0 || dx == w - 1 || dy == 0 || dy == h - 1) is_border = 1;
            }

            if (is_border) panic_put_pixel(x + dx, y + dy, color);
        }
    }
}

static void panic_draw_power_icon(int cx, int cy, int radius, uint32_t color) {
    int r2 = radius * radius;
    int in_r = radius - 3;
    int in_r2 = in_r * in_r;

    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            int d2 = x * x + y * y;
            if (d2 <= r2 && d2 >= in_r2) {
                if (y < 0 && x > -4 && x < 4) continue;
                panic_put_pixel(cx + x, cy + y, color);
            }
        }
    }
    for (int y = -radius - 2; y <= 2; y++) {
        for (int x = -1; x <= 1; x++) {
            panic_put_pixel(cx + x, cy + y, color);
        }
    }
}

static void panic_draw_text_centered(const char* str, int cy, uint32_t color) {
    if (!g_fb_vram || !str) return;
    int w = font_text_width(str);
    int dst_x = (int)(g_screen_w / 2) - w / 2;
    if (dst_x < 10) dst_x = 10;
    uint32_t stride = g_screen_pitch / 4;
    draw_string(str, dst_x, cy, color, (uint32_t*)g_fb_vram, stride);
}

static void panic_draw_text_left(const char* str, int x, int y, uint32_t color) {
    if (!g_fb_vram || !str) return;
    uint32_t stride = g_screen_pitch / 4;
    draw_string(str, x, y, color, (uint32_t*)g_fb_vram, stride);
}

static const char *exception_name[32] = {
    "Division by zero",
    "Debug exception",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 floating-point exception",
    "Alignment check",
    "Machine check",
    "SIMD floating-point exception",
    "Virtualization exception",
    "Control protection exception",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor injection exception",
    "VMM communication exception",
    "Security exception",
    "Reserved",
};

static const char *panic_reason_for(uint64_t vector) {
    if (vector < 32) return exception_name[vector];
    if (vector == 0xFF) return "Manual Panic Trigger";
    return "Kernel Critical Fault";
}

static const char *stop_code_for(uint64_t vector) {
    switch (vector) {
        case 0:  return "DIVIDE_BY_ZERO_ERROR";
        case 1:  return "DEBUG_EXCEPTION";
        case 3:  return "BREAKPOINT_TRAP";
        case 6:  return "INVALID_OPCODE_FAULT";
        case 8:  return "DOUBLE_FAULT_ABORT";
        case 13: return "GENERAL_PROTECTION_FAULT";
        case 14: return "PAGE_FAULT_IN_NONPAGED_AREA";
        case 18: return "MACHINE_CHECK_EXCEPTION";
        case 19: return "SIMD_FLOATING_POINT_FAULT";
        case 0xFF: return "MANUAL_KERNEL_INITIATED_PANIC";
        default: return "KERNEL_SECURITY_CHECK_FAILURE";
    }
}

static void itoa_simple(uint64_t v, char *out) {
    char tmp[24];
    int i = 0;
    if (v == 0) { out[0] = '0'; out[1] = 0; return; }
    while (v > 0) { tmp[i++] = (char)('0' + (v % 10)); v /= 10; }
    int j = 0;
    while (i > 0) out[j++] = tmp[--i];
    out[j] = 0;
}

static void hex_to_str(uint64_t val, char *out, int digits) {
    const char hex[] = "0123456789ABCDEF";
    int p = 0;
    out[p++] = '0';
    out[p++] = 'x';
    for (int i = (digits - 1) * 4; i >= 0; i -= 4) {
        out[p++] = hex[(val >> i) & 0xF];
    }
    out[p] = '\0';
}

static void panic_serial_char(char c) {
    uint8_t status;
    do {
        __asm__ volatile ("inb %1, %0" : "=a"(status) : "Nd"((uint16_t)0x3FD));
    } while ((status & 0x20U) == 0);
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)c), "Nd"((uint16_t)0x3F8));
}

static void panic_serial_str(const char *s) {
    if (!s) return;
    while (*s) panic_serial_char(*s++);
}

static void panic_serial_hex(uint64_t val) {
    const char hex[] = "0123456789ABCDEF";
    panic_serial_str("0x");
    for (int i = 60; i >= 0; i -= 4) {
        panic_serial_char(hex[(val >> i) & 0xF]);
    }
}

static void panic_serial_dec(uint64_t val) {
    if (val == 0) { panic_serial_char('0'); return; }
    char buf[32];
    int i = 0;
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    while (i > 0) panic_serial_char(buf[--i]);
}

static void panic_render_screen(
    uint64_t vector,
    uint64_t error_code,
    uint64_t rip,
    uint64_t cs,
    uint64_t cr2,
    uint64_t cr3,
    uint64_t rsp,
    const char *custom_msg
) {
    if (!g_fb_vram || g_screen_w == 0 || g_screen_h == 0) {
        for (;;) { __asm__ volatile ("hlt"); }
    }

    uint32_t stride = g_screen_pitch / 4;
    uint32_t *fb = (uint32_t*)g_fb_vram;

    // затемненная шторка macos поверх экрана
    for (uint32_t y = 0; y < g_screen_h; y++) {
        uint32_t *row = &fb[y * stride];
        for (uint32_t x = 0; x < g_screen_w; x++) {
            uint32_t p = row[x];
            uint32_t r = ((p >> 16) & 0xFF) / 5;
            uint32_t g = ((p >> 8) & 0xFF) / 5;
            uint32_t b = (p & 0xFF) / 4 + 8;
            row[x] = (r << 16) | (g << 8) | b;
        }
    }

    int card_w = 640;
    int card_h = 380;
    int card_x = ((int)g_screen_w - card_w) / 2;
    int card_y = ((int)g_screen_h - card_h) / 2;

    // мягкая тень модального окна
    panic_fill_rounded_rect(card_x - 6, card_y - 4, card_w + 12, card_h + 12, 18, 0x0006080E);
    panic_fill_rounded_rect(card_x, card_y, card_w, card_h, 14, 0x00161822);
    panic_draw_rounded_outline(card_x, card_y, card_w, card_h, 14, 0x0032384C);

    // иконка перезагрузки в стиле macos
    panic_draw_power_icon(card_x + card_w / 2, card_y + 34, 14, 0x00FF453A);

    // заголовок рестарта
    panic_draw_text_centered("You need to restart your computer.", card_y + 64, 0x00FFFFFF);
    panic_draw_text_centered("Hold down the Power button until your PC turns off, then press it again.", card_y + 86, 0x008E95A5);

    // диагностический блок windows bsod
    int diag_x = card_x + 22;
    int diag_y = card_y + 116;
    int diag_w = card_w - 44;
    int diag_h = 224;

    panic_fill_rounded_rect(diag_x, diag_y, diag_w, diag_h, 8, 0x000E1016);
    panic_draw_rounded_outline(diag_x, diag_y, diag_w, diag_h, 8, 0x00242838);

    // смайлик и системное сообщение
    panic_draw_text_left(":(", diag_x + 18, diag_y + 16, 0x000A84FF);
    panic_draw_text_left("Your PC ran into a problem and KeshOS had to stop.", diag_x + 46, diag_y + 16, 0x00FFFFFF);

    char stop_buf[128];
    const char *stop_name = stop_code_for(vector);
    int sp = 0;
    const char *st_pfx = "STOP CODE: ";
    while (st_pfx[sp]) { stop_buf[sp] = st_pfx[sp]; sp++; }
    int sn = 0;
    while (stop_name[sn]) { stop_buf[sp++] = stop_name[sn++]; }
    stop_buf[sp] = '\0';
    panic_draw_text_left(stop_buf, diag_x + 18, diag_y + 44, 0x00FF453A);

    char reason_buf[128];
    char vec_str[16];
    itoa_simple(vector, vec_str);
    const char *reason_desc = custom_msg ? custom_msg : panic_reason_for(vector);
    int rp = 0;
    const char *ex_pfx = "EXCEPTION: Vector ";
    while (ex_pfx[rp]) { reason_buf[rp] = ex_pfx[rp]; rp++; }
    int vp = 0;
    while (vec_str[vp]) { reason_buf[rp++] = vec_str[vp++]; }
    reason_buf[rp++] = ' ';
    reason_buf[rp++] = '(';
    int dp = 0;
    while (reason_desc[dp] && rp < 120) { reason_buf[rp++] = reason_desc[dp++]; }
    reason_buf[rp++] = ')';
    reason_buf[rp] = '\0';
    panic_draw_text_left(reason_buf, diag_x + 18, diag_y + 68, 0x00E5E5EA);

    char rip_buf[64], err_buf[64];
    hex_to_str(rip, rip_buf, 16);
    hex_to_str(error_code, err_buf, 8);
    char fault_line[128];
    int fl = 0;
    const char *fl_pfx = "FAULT RIP: ";
    while (fl_pfx[fl]) { fault_line[fl] = fl_pfx[fl]; fl++; }
    int lp = 0;
    while (rip_buf[lp]) { fault_line[fl++] = rip_buf[lp++]; }
    const char *fl_mid = "   ERR: ";
    int mp = 0;
    while (fl_mid[mp]) { fault_line[fl++] = fl_mid[mp++]; }
    int ep = 0;
    while (err_buf[ep]) { fault_line[fl++] = err_buf[ep++]; }
    fault_line[fl] = '\0';
    panic_draw_text_left(fault_line, diag_x + 18, diag_y + 92, 0x009BA3B5);

    char cr2_buf[64], cr3_buf[64];
    hex_to_str(cr2, cr2_buf, 16);
    hex_to_str(cr3, cr3_buf, 16);
    char mem_line[128];
    int ml = 0;
    const char *ml_pfx = "MEMORY   : CR2=";
    while (ml_pfx[ml]) { mem_line[ml] = ml_pfx[ml]; ml++; }
    int c2 = 0;
    while (cr2_buf[c2]) { mem_line[ml++] = cr2_buf[c2++]; }
    const char *ml_mid = " CR3=";
    int m2 = 0;
    while (ml_mid[m2]) { mem_line[ml++] = ml_mid[m2++]; }
    int c3 = 0;
    while (cr3_buf[c3]) { mem_line[ml++] = cr3_buf[c3++]; }
    mem_line[ml] = '\0';
    panic_draw_text_left(mem_line, diag_x + 18, diag_y + 114, 0x009BA3B5);

    char rsp_buf[64], cs_buf[32];
    hex_to_str(rsp, rsp_buf, 16);
    hex_to_str(cs, cs_buf, 4);
    char reg_line[128];
    int rl = 0;
    const char *rl_pfx = "REGISTERS: RSP=";
    while (rl_pfx[rl]) { reg_line[rl] = rl_pfx[rl]; rl++; }
    int sp_idx = 0;
    while (rsp_buf[sp_idx]) { reg_line[rl++] = rsp_buf[sp_idx++]; }
    const char *rl_mid = " CS=";
    int m3 = 0;
    while (rl_mid[m3]) { reg_line[rl++] = rl_mid[m3++]; }
    int cs_idx = 0;
    while (cs_buf[cs_idx]) { reg_line[rl++] = cs_buf[cs_idx++]; }
    reg_line[rl] = '\0';
    panic_draw_text_left(reg_line, diag_x + 18, diag_y + 136, 0x009BA3B5);

    panic_draw_text_left("BUILD    : KeshOS 1.0 Drop (Kernel 1.0.0-rc1-ksh x86_64)", diag_x + 18, diag_y + 162, 0x006E7688);
    panic_draw_text_left("STATUS   : CPU halted • Serial COM1 dump dispatched", diag_x + 18, diag_y + 184, 0x0030D158);

    panic_draw_text_centered("SneakDeak Technologies • KeshOS Kernel Diagnostic Subsystem", card_y + card_h - 22, 0x005E6578);

    for (;;) { __asm__ volatile ("hlt"); }
}

void kernel_panic(const char *msg) {
    __asm__ volatile ("cli");
    panic_serial_str("\n\n================ [KERNEL PANIC: MANUAL] ================\n");
    panic_serial_str(msg ? msg : "Manual kernel panic");
    panic_serial_str("\n========================================================\n\n");

    uint64_t rip = 0, rsp = 0, cr2 = 0, cr3 = 0;
    __asm__ volatile ("lea (%%rip), %0" : "=r"(rip));
    __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp));
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    panic_render_screen(0xFF, 0, rip, 0x08, cr2, cr3, rsp, msg ? msg : "MANUAL_KERNEL_PANIC");
}

void isr_common_handler(uint64_t *stack_ptr) {
    __asm__ volatile ("cli");

    uint64_t vector = stack_ptr[15];
    uint64_t error_code = stack_ptr[16];
    uint64_t rip = stack_ptr[17];
    uint64_t cs = stack_ptr[18];
    uint64_t rflags = stack_ptr[19];
    uint64_t rsp = stack_ptr[20];
    uint64_t ss = stack_ptr[21];
    uint64_t cr2 = 0, cr3 = 0;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    panic_serial_str("\n\n================ [KERNEL PANIC] ================\n");
    panic_serial_str("EXCEPTION VECTOR: ");
    panic_serial_dec(vector);
    panic_serial_str(" (");
    panic_serial_str(panic_reason_for(vector));
    panic_serial_str(")\n");
    panic_serial_str("ERROR CODE    : ");
    panic_serial_hex(error_code);
    panic_serial_str("\n");
    panic_serial_str("FAULTING RIP  : ");
    panic_serial_hex(rip);
    panic_serial_str("  CS: ");
    panic_serial_hex(cs);
    panic_serial_str("\n");
    panic_serial_str("FAULTING CR2  : ");
    panic_serial_hex(cr2);
    panic_serial_str("\n");
    panic_serial_str("CURRENT  CR3  : ");
    panic_serial_hex(cr3);
    panic_serial_str("\n");
    panic_serial_str("SAVED    RSP  : ");
    panic_serial_hex(rsp);
    panic_serial_str("  SS: ");
    panic_serial_hex(ss);
    panic_serial_str("\n");
    panic_serial_str("RFLAGS        : ");
    panic_serial_hex(rflags);
    panic_serial_str("\n================================================\n\n");

    if ((cs & 3) == 3) {
        panic_serial_str("[PROCESS] User Mode Registers:\n");
        panic_serial_str("  RAX: "); panic_serial_hex(stack_ptr[14]);
        panic_serial_str("  RBX: "); panic_serial_hex(stack_ptr[13]);
        panic_serial_str("  RCX: "); panic_serial_hex(stack_ptr[12]);
        panic_serial_str("  RDX: "); panic_serial_hex(stack_ptr[11]); panic_serial_str("\n");
        panic_serial_str("  RSI: "); panic_serial_hex(stack_ptr[10]);
        panic_serial_str("  RDI: "); panic_serial_hex(stack_ptr[9]);
        panic_serial_str("  RBP: "); panic_serial_hex(stack_ptr[8]); panic_serial_str("\n");
        panic_serial_str("  R8 : "); panic_serial_hex(stack_ptr[7]);
        panic_serial_str("  R9 : "); panic_serial_hex(stack_ptr[6]);
        panic_serial_str("  R10: "); panic_serial_hex(stack_ptr[5]); panic_serial_str("\n");

        panic_serial_str("[PROCESS] User Stack (RSP = "); panic_serial_hex(rsp); panic_serial_str("):\n");
        uint64_t *ustack = (uint64_t *)rsp;
        for (int i = 0; i < 16; i++) {
            uint64_t val = 0;
            if (copy_from_user(&val, &ustack[i], sizeof(val)) == 0) {
                panic_serial_str("  [+0x");
                panic_serial_hex((uint64_t)(i * 8));
                panic_serial_str("] = ");
                panic_serial_hex(val);
                panic_serial_str("\n");
            } else {
                break;
            }
        }

        panic_serial_str("[PROCESS] User Mode fault caught! Terminating crashed Ring 3 process.\n");
        extern int g_active_proc_idx;
        extern int process_kill(int pid);
        if (g_active_proc_idx >= 0) {
            process_kill(g_active_proc_idx);
        }
        extern void process_crash_return_to_compositor(void);
        process_crash_return_to_compositor();
    }

    panic_render_screen(vector, error_code, rip, cs, cr2, cr3, rsp, NULL);
}
