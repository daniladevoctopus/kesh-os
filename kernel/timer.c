// PIT tick source plus a monotonic TSC safety clock for boot-time waits.
#include "timer.h"
#include "acpi.h"
#include "include/pic.h"
#include "log.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182U

static volatile uint64_t g_ticks = 0;
static uint32_t g_frequency = 250;
static volatile uint64_t g_irq_count = 0;

static uint64_t g_tsc_hz = 0;
static uint64_t g_tsc_start = 0;
static uint8_t g_tsc_ready = 0;
static uint8_t g_tsc_invariant = 0;
static uint64_t g_monotonic_base_ns = 0;
static uint64_t g_realtime_boot_ns = 0;

static inline void pit_outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint8_t cmos_read(uint8_t reg) {
    asm volatile("outb %0, $0x70" : : "a"(reg));
    uint8_t value;
    asm volatile("inb $0x71, %0" : "=a"(value));
    return value;
}

static uint32_t rtc_days_before_year(uint32_t year) {
    uint32_t y = year - 1;
    return y * 365U + y / 4U - y / 100U + y / 400U;
}

static uint64_t rtc_epoch_seconds(void) {
    uint8_t second, minute, hour, day, month, year, century, status_b;
    for (int attempt = 0; attempt < 8; ++attempt) {
        while (cmos_read(0x0A) & 0x80U) __asm__ volatile("pause");
        second = cmos_read(0x00); minute = cmos_read(0x02); hour = cmos_read(0x04);
        day = cmos_read(0x07); month = cmos_read(0x08); year = cmos_read(0x09);
        century = cmos_read(0x32); status_b = cmos_read(0x0B);
        if (!(cmos_read(0x0A) & 0x80U)) break;
    }
    uint8_t pm = hour & 0x80U;
    if (!(status_b & 0x04U)) {
        second = (uint8_t)((second & 0x0FU) + ((second >> 4) * 10U));
        minute = (uint8_t)((minute & 0x0FU) + ((minute >> 4) * 10U));
        hour = (uint8_t)((hour & 0x0FU) + (((hour & 0x70U) >> 4) * 10U));
        day = (uint8_t)((day & 0x0FU) + ((day >> 4) * 10U));
        month = (uint8_t)((month & 0x0FU) + ((month >> 4) * 10U));
        year = (uint8_t)((year & 0x0FU) + ((year >> 4) * 10U));
        century = (uint8_t)((century & 0x0FU) + ((century >> 4) * 10U));
    }
    if (!(status_b & 0x02U)) {
        hour %= 12U;
        if (pm) hour = (uint8_t)(hour + 12U);
    }
    uint32_t full_year = century >= 19 && century <= 99 ? (uint32_t)century * 100U + year : 2000U + year;
    if (full_year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) return 0;
    static const uint16_t before_month[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
    uint32_t days = rtc_days_before_year(full_year) - rtc_days_before_year(1970) + before_month[month - 1] + day - 1;
    int leap = (full_year % 4U == 0 && (full_year % 100U != 0 || full_year % 400U == 0));
    if (leap && month > 2) days++;
    return (uint64_t)days * 86400ULL + (uint64_t)hour * 3600ULL + (uint64_t)minute * 60ULL + second;
}

static uint64_t detect_tsc_hz(void) {
    uint32_t max_leaf;
    uint32_t ebx, ecx, edx;
    uint32_t eax = 0;
    asm volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    max_leaf = eax;

    if (max_leaf >= 0x15U) {
        eax = 0x15U;
        ebx = ecx = edx = 0;
        asm volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
        uint32_t denominator = eax;
        uint32_t numerator = ebx;
        uint32_t crystal_hz = ecx;
        if (denominator != 0 && numerator != 0 && crystal_hz != 0) {
            return ((uint64_t)crystal_hz * (uint64_t)numerator) / (uint64_t)denominator;
        }
    }

    if (max_leaf >= 0x16U) {
        eax = 0x16U;
        ebx = ecx = edx = 0;
        asm volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
        if (eax != 0) return (uint64_t)eax * 1000000ULL;
    }

    return 0;
}

static int tsc_is_invariant(void) {
    uint32_t max_extended = 0;
    uint32_t ebx, ecx, edx;
    uint32_t eax = 0x80000000U;
    asm volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    max_extended = eax;
    if (max_extended < 0x80000007U) return 0;
    eax = 0x80000007U;
    ebx = ecx = edx = 0;
    asm volatile ("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    return (edx & (1U << 8)) != 0;
}

static uint64_t tsc_delta_to_ms(uint64_t delta) {
    if (!g_tsc_hz) return 0;
    uint64_t whole_seconds = delta / g_tsc_hz;
    uint64_t remainder = delta % g_tsc_hz;
    return whole_seconds * 1000ULL + (remainder * 1000ULL) / g_tsc_hz;
}

static uint64_t ms_to_tsc(uint64_t ms) {
    if (!g_tsc_hz || ms == 0) return 0;
    uint64_t hz_per_ms = g_tsc_hz / 1000ULL;
    uint64_t remainder_hz = g_tsc_hz % 1000ULL;
    return ms * hz_per_ms + (ms * remainder_hz) / 1000ULL;
}

static void timer_init_tsc(void) {
    g_tsc_invariant = (uint8_t)tsc_is_invariant();
    g_tsc_hz = g_tsc_invariant ? detect_tsc_hz() : 0;
    g_tsc_start = read_tsc();
    g_tsc_ready = g_tsc_hz != 0;
    g_monotonic_base_ns = 0;
    if (g_tsc_ready) {
        KLOG_DEBUG("timer", "TSC clock=%llu Hz source=CPUID.15/16 monotonic=on",
                   (unsigned long long)g_tsc_hz);
    } else {
        KLOG_WARN("timer", "invariant TSC frequency unavailable; PIT ticks remain the only clock source");
    }
}

int timer_calibrate_from_acpi(void) {
    if (g_tsc_ready || !g_tsc_invariant || !acpi_pm_timer_available()) return g_tsc_ready ? 0 : -1;
    const uint64_t pm_hz = 3579545ULL;
    uint32_t mask = acpi_pm_timer_mask();
    uint32_t target = (uint32_t)(pm_hz / 100ULL);
    uint32_t pm_start = acpi_pm_timer_read();
    uint64_t tsc_start = read_tsc();
    uint32_t delta = 0;
    uint32_t budget = 100000000U;
    while (delta < target && budget--) {
        delta = (acpi_pm_timer_read() - pm_start) & mask;
        __asm__ volatile("pause");
    }
    if (!budget || delta < target) return -1;
    uint64_t tsc_delta = read_tsc() - tsc_start;
    if (!tsc_delta || tsc_delta > (~0ULL / pm_hz)) return -1;
    uint64_t calibrated_hz = (tsc_delta * pm_hz) / delta;
    if (calibrated_hz < 1000000ULL || calibrated_hz > 10000000000ULL) return -1;
    g_monotonic_base_ns = (g_ticks * 1000000000ULL) / g_frequency;
    g_tsc_start = read_tsc();
    g_tsc_hz = calibrated_hz;
    g_tsc_ready = 1;
    KLOG_INFO("timer", "invariant TSC calibrated from ACPI PM timer at %llu Hz",
              (unsigned long long)g_tsc_hz);
    return 0;
}

static void timer_irq_handler(void) {
    ++g_ticks;
    ++g_irq_count;

    /* Emit only the first four timer interrupts so the serial log contains
       direct kernel evidence that IRQ0 is physically reaching the handler. */
    if (g_irq_count <= 4) {
        KLOG_TRACE("timer", "IRQ0 handler entry count=%llu ticks=%llu",
                   (unsigned long long)g_irq_count, (unsigned long long)g_ticks);
    }
}

void timer_init(uint32_t frequency_hz) {
    KLOG_ENTER("timer");
    if (frequency_hz < 20) frequency_hz = 20;
    if (frequency_hz > 1000) frequency_hz = 1000;

    uint32_t divisor = PIT_BASE_HZ / frequency_hz;
    if (divisor < 1) divisor = 1;
    if (divisor > 65535) divisor = 65535;

    g_frequency = PIT_BASE_HZ / divisor;
    if (g_frequency == 0) g_frequency = frequency_hz;

    g_ticks = 0;
    g_irq_count = 0;
    timer_init_tsc();
    g_realtime_boot_ns = rtc_epoch_seconds() * 1000000000ULL;

    pit_outb(PIT_COMMAND, 0x36);
    pit_outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    pit_outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_install_handler(0, timer_irq_handler);
    pic_clear_mask(0);
    KLOG_DEBUG("timer", "configured PIT divisor=%u actual_hz=%u irq_line=0 vector=32 mask0=%u",
               divisor, g_frequency, (unsigned)pic_get_mask(0));
    KLOG_LEAVE("timer", 0);
}

uint64_t timer_ticks(void) {
    return g_ticks;
}

uint64_t timer_millis(void) {
    uint64_t pit_ms = (g_ticks * 1000ULL) / g_frequency;
    if (!g_tsc_ready) return pit_ms;

    uint64_t delta = read_tsc() - g_tsc_start;
    uint64_t tsc_ms = g_monotonic_base_ns / 1000000ULL + tsc_delta_to_ms(delta);
    return tsc_ms > pit_ms ? tsc_ms : pit_ms;
}

uint64_t timer_monotonic_ns(void) {
    if (!g_tsc_ready) return (g_ticks * 1000000000ULL) / g_frequency;
    uint64_t delta = read_tsc() - g_tsc_start;
    uint64_t seconds = delta / g_tsc_hz;
    uint64_t remainder = delta % g_tsc_hz;
    return g_monotonic_base_ns + seconds * 1000000000ULL + (remainder * 1000000000ULL) / g_tsc_hz;
}

uint64_t timer_realtime_ns(void) {
    return g_realtime_boot_ns ? g_realtime_boot_ns + timer_monotonic_ns() : 0;
}

uint64_t timer_irq_count(void) {
    return g_irq_count;
}

void timer_wait_ticks(uint64_t ticks) {
    if (ticks == 0) return;

    uint64_t flags;
    asm volatile("pushfq; popq %0" : "=r"(flags));
    int was_enabled = (flags & (1ULL << 9)) != 0;

    uint64_t start_tick = g_ticks;
    uint64_t target = start_tick + ticks;
    if (target < start_tick) target = ~0ULL;
    uint64_t wait_ms = ((ticks * 1000ULL) + g_frequency - 1ULL) / g_frequency;
    if (wait_ms == 0) wait_ms = 1;

    uint64_t deadline = 0;
    if (g_tsc_ready) {
        uint64_t delta_tsc = ms_to_tsc(wait_ms);
        deadline = read_tsc() + delta_tsc;
    }

    asm volatile("sti" : : : "memory");

    while (g_ticks < target) {
        if (g_tsc_ready && read_tsc() >= deadline) {
            break;
        }
        asm volatile("pause");
    }

    if (!was_enabled) asm volatile("cli" : : : "memory");
}

void timer_wait_ms(uint32_t ms) {
    if (ms == 0) return;
    uint64_t ticks = ((uint64_t)ms * g_frequency + 999ULL) / 1000ULL;
    if (ticks == 0) ticks = 1;
    timer_wait_ticks(ticks);
}
