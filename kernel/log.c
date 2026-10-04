#include "log.h"
#include "timer.h"
#include "cpu.h"
#include "serial.h"
#include "vfs.h"
#include <stdarg.h>
#include <stddef.h>

static volatile int g_klog_level = KLOG_L5_INFO;
static volatile uint64_t g_klog_seq = 0;

#define KLOG_BUF_SIZE 2048
#define KLOG_RING_RECORDS 32
static char g_log_buf[KLOG_BUF_SIZE];
static size_t g_log_pos;
static char g_log_ring[KLOG_RING_RECORDS][KLOG_BUF_SIZE];
static uint8_t g_log_ring_levels[KLOG_RING_RECORDS];
static char g_log_ring_modules[KLOG_RING_RECORDS][24];
static uint32_t g_log_ring_next;
static uint32_t g_log_ring_count;
static char g_persist_buffer[KLOG_RING_RECORDS * KLOG_BUF_SIZE];
static char g_previous_log[KLOG_RING_RECORDS * KLOG_BUF_SIZE];
static int g_previous_log_size;

static const char *level_name(int level) {
    switch (level) {
        case KLOG_L1_FATAL: return "FATAL";
        case KLOG_L2_ERROR: return "ERROR";
        case KLOG_L3_WARN: return "WARN";
        case KLOG_L4_NOTICE: return "NOTICE";
        case KLOG_L5_INFO: return "INFO";
        case KLOG_L6_DEBUG: return "DEBUG";
        case KLOG_L7_TRACE: return "TRACE";
        default: return "UNKNOWN";
    }
}

static uint64_t read_rflags(void) {
    uint64_t value;
    __asm__ volatile("pushfq; popq %0" : "=r"(value));
    return value;
}

static uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

static uint64_t read_rsp(void) {
    uint64_t value;
    __asm__ volatile("mov %%rsp, %0" : "=r"(value));
    return value;
}

static uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static void reset_buffer(void) { g_log_pos = 0; }

static void put_char(char c) {
    if (g_log_pos + 1 < KLOG_BUF_SIZE) g_log_buf[g_log_pos++] = c;
}

static void put_str(const char *s) {
    if (!s) s = "(null)";
    while (*s) put_char(*s++);
}

static void put_dec(uint64_t value) {
    char buf[32];
    size_t n = 0;
    if (value == 0) { put_char('0'); return; }
    while (value && n < sizeof(buf)) { buf[n++] = (char)('0' + value % 10U); value /= 10U; }
    while (n) put_char(buf[--n]);
}

static void put_signed(int64_t value) {
    if (value < 0) { put_char('-'); put_dec((uint64_t)(-(value + 1)) + 1ULL); }
    else put_dec((uint64_t)value);
}

static void put_hex(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    put_str("0x");
    for (int shift = 60; shift >= 0; shift -= 4) put_char(digits[(value >> shift) & 0xFULL]);
}

static void put_format(const char *fmt, va_list ap) {
    if (!fmt) return;
    for (size_t i = 0; fmt[i] && g_log_pos + 1 < KLOG_BUF_SIZE; ++i) {
        if (fmt[i] != '%') { put_char(fmt[i]); continue; }
        ++i;
        int longs = 0;
        while (fmt[i] == 'l') { ++longs; ++i; }
        switch (fmt[i]) {
            case 's': put_str(va_arg(ap, const char *)); break;
            case 'c': put_char((char)va_arg(ap, int)); break;
            case 'd': case 'i': put_signed(longs ? va_arg(ap, long long) : (int64_t)va_arg(ap, int)); break;
            case 'u': put_dec(longs ? va_arg(ap, unsigned long long) : (uint64_t)va_arg(ap, unsigned int)); break;
            case 'x': case 'X': put_hex(longs ? va_arg(ap, unsigned long long) : (uint64_t)va_arg(ap, unsigned int)); break;
            case 'p': put_hex((uint64_t)va_arg(ap, void *)); break;
            case '%': put_char('%'); break;
            default: put_char('%'); if (longs) while (longs--) put_char('l'); put_char(fmt[i]); break;
        }
    }
}

void klog_init(void) {
    g_klog_level = KLOG_L5_INFO;
    g_klog_seq = 0;
    g_log_ring_next = 0;
    g_log_ring_count = 0;
    g_previous_log_size = 0;
    serial_print("<KLOG> online level=5 format=source-location cpu/tick/tsc/cr3/rflags\n");
}

void klog_set_level(int level) {
    if (level < KLOG_L1_FATAL) level = KLOG_L1_FATAL;
    if (level > KLOG_L7_TRACE) level = KLOG_L7_TRACE;
    g_klog_level = level;
}

int klog_get_level(void) { return g_klog_level; }

int klog_snapshot(char *out, int max_bytes) {
    return klog_snapshot_filtered(out, max_bytes, KLOG_L1_FATAL, KLOG_L7_TRACE, 0);
}

static int module_equal(const char *a, const char *b) {
    if (!b || !b[0]) return 1;
    int i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == 0 && b[i] == 0;
}

int klog_snapshot_filtered(char *out, int max_bytes, int min_level, int max_level, const char *module) {
    if (!out || max_bytes <= 1) return -1;
    if (min_level < KLOG_L1_FATAL || max_level > KLOG_L7_TRACE || min_level > max_level) return -1;
    uint64_t flags = read_rflags();
    __asm__ volatile("cli" : : : "memory");
    int used = 0;
    uint32_t first = (g_log_ring_next + KLOG_RING_RECORDS - g_log_ring_count) % KLOG_RING_RECORDS;
    for (uint32_t record = 0; record < g_log_ring_count; ++record) {
        uint32_t index = (first + record) % KLOG_RING_RECORDS;
        int level = g_log_ring_levels[index];
        if (level < min_level || level > max_level || !module_equal(g_log_ring_modules[index], module)) continue;
        const char *src = g_log_ring[index];
        for (int i = 0; src[i] && used + 1 < max_bytes; ++i) out[used++] = src[i];
        if (used + 1 >= max_bytes) break;
    }
    out[used] = '\0';
    if (flags & (1ULL << 9)) __asm__ volatile("sti" : : : "memory");
    return used;
}

static void rotate_log(const char *source, const char *destination) {
    int size = vfs_read(source, g_persist_buffer, (int)sizeof(g_persist_buffer));
    if (size > 0) (void)vfs_write(destination, g_persist_buffer, size);
}

int klog_persist(void) {
    rotate_log("/hdd/KLOG2.TXT", "/hdd/KLOG3.TXT");
    rotate_log("/hdd/KLOG1.TXT", "/hdd/KLOG2.TXT");
    rotate_log("/hdd/KLOG.TXT", "/hdd/KLOG1.TXT");
    int size = klog_snapshot(g_persist_buffer, (int)sizeof(g_persist_buffer));
    if (size < 0) return size;
    int written = vfs_write("/hdd/KLOG.TXT", g_persist_buffer, size);
    return written == size ? size : -1;
}

int klog_load_persisted(void) {
    int size = vfs_read("/hdd/KLOG.TXT", g_previous_log, (int)sizeof(g_previous_log) - 1);
    if (size < 0) {
        g_previous_log_size = 0;
        return size;
    }
    g_previous_log[size] = 0;
    g_previous_log_size = size;
    return size;
}

int klog_previous_snapshot(char *out, int max_bytes) {
    if (!out || max_bytes <= 1) return -1;
    int count = g_previous_log_size;
    if (count >= max_bytes) count = max_bytes - 1;
    for (int i = 0; i < count; ++i) out[i] = g_previous_log[i];
    out[count] = 0;
    return count;
}

void klog_emit(int level, const char *module, const char *file, int line,
               const char *function, const char *fmt, ...) {
    if (level > g_klog_level) return;

    uint64_t flags = read_rflags();
    __asm__ volatile("cli" : : : "memory");
    uint64_t seq = __atomic_add_fetch(&g_klog_seq, 1, __ATOMIC_RELAXED);
    uint64_t tsc = read_tsc();
    uint64_t ms = timer_millis();
    uint64_t ticks = timer_ticks();
    uint32_t cpu = cpu_current_index();
    uint64_t cr3 = read_cr3();
    uint64_t rsp = read_rsp();
    uint64_t rflags = read_rflags();

    reset_buffer();
    put_char('['); put_str(level_name(level)); put_str(" L"); put_dec((uint64_t)level); put_char(']');
    put_str(" #"); put_dec(seq);
    put_str(" t="); put_dec(ms); put_str("ms tick="); put_dec(ticks);
    put_str(" cpu="); put_dec(cpu); put_str(" tsc="); put_hex(tsc);
    put_str(" cr3="); put_hex(cr3); put_str(" rsp="); put_hex(rsp);
    put_str(" rflags="); put_hex(rflags);
    put_str(" "); put_str(module ? module : "kernel");
    put_str(" "); put_str(file ? file : "?"); put_char(':'); put_dec((uint64_t)line);
    put_str(" "); put_str(function ? function : "?"); put_str("() ");
    va_list ap; va_start(ap, fmt); put_format(fmt, ap); va_end(ap);
    put_str("\n");
    g_log_buf[g_log_pos] = '\0';
    size_t copy = 0;
    while (g_log_buf[copy] && copy + 1 < KLOG_BUF_SIZE) { g_log_ring[g_log_ring_next][copy] = g_log_buf[copy]; copy++; }
    g_log_ring[g_log_ring_next][copy] = '\0';
    g_log_ring_levels[g_log_ring_next] = (uint8_t)level;
    const char *module_name = module ? module : "kernel";
    int module_pos = 0;
    while (module_name[module_pos] && module_pos + 1 < (int)sizeof(g_log_ring_modules[0])) {
        g_log_ring_modules[g_log_ring_next][module_pos] = module_name[module_pos];
        module_pos++;
    }
    g_log_ring_modules[g_log_ring_next][module_pos] = 0;
    g_log_ring_next = (g_log_ring_next + 1) % KLOG_RING_RECORDS;
    if (g_log_ring_count < KLOG_RING_RECORDS) g_log_ring_count++;
    serial_print(g_log_buf);
    if (flags & (1ULL << 9)) __asm__ volatile("sti" : : : "memory");
}

void klog_enter(const char *module, const char *file, int line, const char *function) {
    klog_emit(KLOG_L7_TRACE, module, file, line, function, "ENTER");
}

void klog_leave(const char *module, const char *file, int line, const char *function, int64_t result) {
    klog_emit(KLOG_L7_TRACE, module, file, line, function, "LEAVE result=%lld", (long long)result);
}
