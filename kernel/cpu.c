#include "cpu.h"
#include "gdt.h"
#include "include/idt.h"
#include "include/limine.h"
#include "apic.h"
#include "log.h"
#include "serial.h"
#include <stddef.h>

__attribute__((used, section(".requests")))
static volatile struct limine_smp_request limine_smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
    .response = NULL,
    .flags = 0
};

static cpu_local_t g_cpus[KESHOS_MAX_CPUS];
static uint32_t g_cpu_count = 1;
static volatile uint32_t g_online_count = 1;
static uint32_t g_bsp_lapic_id = 0;

static uint32_t cpu_read_apic_id(void) {
    uint32_t eax, ebx, ecx, edx;
    eax = 1;
    ecx = 0;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    return (ebx >> 24) & 0xFFU;
}

void cpu_init_bsp(void) {
    KLOG_ENTER("cpu");
    uint32_t lapic_id = cpu_read_apic_id();
    if (limine_smp_request.response) lapic_id = limine_smp_request.response->bsp_lapic_id;
    g_bsp_lapic_id = lapic_id;
    g_cpu_count = 1;
    g_online_count = 1;

    for (uint32_t i = 0; i < KESHOS_MAX_CPUS; ++i) {
        g_cpus[i].index = i;
        g_cpus[i].processor_id = 0xFFFFFFFFU;
        g_cpus[i].lapic_id = 0xFFFFFFFFU;
        g_cpus[i].online = 0;
        g_cpus[i].ticks = 0;
        g_cpus[i].scheduler_epoch = 0;
        g_cpus[i].current_thread = NULL;
    }

    g_cpus[0].processor_id = 0;
    g_cpus[0].lapic_id = lapic_id;
    g_cpus[0].online = 1;
    serial_print("[CPU] BSP online, LAPIC ID=");
    serial_print_dec(lapic_id);
    serial_print(".\n");
    KLOG_LEAVE("cpu", 0);
}

static int cpu_find_by_lapic(uint32_t lapic_id) {
    for (uint32_t i = 0; i < g_cpu_count && i < KESHOS_MAX_CPUS; ++i) {
        if (g_cpus[i].lapic_id == lapic_id) return (int)i;
    }
    return -1;
}

void cpu_ap_entry(void *opaque) {
    struct limine_smp_info *info = (struct limine_smp_info *)opaque;
    if (!info) {
        for (;;) __asm__ volatile("hlt");
    }

    uint32_t idx = info->processor_id;
    if (idx >= KESHOS_MAX_CPUS) idx = 0;
    if (idx == 0 && info->lapic_id != g_bsp_lapic_id) {
        int found = cpu_find_by_lapic(info->lapic_id);
        if (found >= 0) idx = (uint32_t)found;
    }

    /* Each AP gets its own architectural bookkeeping. The current kernel
       scheduler still dispatches user work from the BSP until per-CPU user
       stacks/TSS are fully separated. */
    g_cpus[idx].index = idx;
    g_cpus[idx].processor_id = info->processor_id;
    g_cpus[idx].lapic_id = info->lapic_id;
    g_cpus[idx].online = 1;
    g_cpus[idx].ticks = 0;
    g_cpus[idx].scheduler_epoch = 0;
    __atomic_fetch_add(&g_online_count, 1, __ATOMIC_SEQ_CST);

    /* The IDT is read-only shared data, so each AP can load the same table.
       GDT/TSS is deliberately not rebuilt here because its per-CPU split is a
       separate hardening step and rebuilding one global TSS would race. */
    idt_load_cpu();
    apic_ap_init();
    __asm__ volatile("sti");

    serial_print("[CPU] AP online, processor=");
    serial_print_dec(info->processor_id);
    serial_print(" LAPIC=");
    serial_print_dec(info->lapic_id);
    serial_print(".\n");

    for (;;) {
        ++g_cpus[idx].ticks;
        __asm__ volatile("hlt");
    }
}

void cpu_smp_init(void) {
    KLOG_ENTER("cpu");
    if (!limine_smp_request.response) {
        serial_print("[CPU] Limine SMP response unavailable; staying BSP-only.\n");
        KLOG_LEAVE("cpu", -1);
        return;
    }

    uint64_t count = limine_smp_request.response->cpu_count;
    if (count == 0) count = 1;
    if (count > KESHOS_MAX_CPUS) count = KESHOS_MAX_CPUS;
    g_cpu_count = (uint32_t)count;

    for (uint32_t i = 0; i < g_cpu_count; ++i) {
        struct limine_smp_info *info = limine_smp_request.response->cpus[i];
        g_cpus[i].index = i;
        g_cpus[i].processor_id = info->processor_id;
        g_cpus[i].lapic_id = info->lapic_id;
        g_cpus[i].online = (info->lapic_id == g_bsp_lapic_id) ? 1U : 0U;
    }

    if (!apic_is_available()) {
        serial_print("[CPU] SMP topology recorded only; AP startup deferred until APIC is ready.\n");
        KLOG_LEAVE("cpu", 0);
        return;
    }

    /* This stage intentionally prepares the AP entries only after APIC is
       known-good. Full AP startup remains deferred until per-CPU GDT/TSS and
       scheduler state are ready. */
    for (uint32_t i = 0; i < g_cpu_count; ++i) {
        struct limine_smp_info *info = limine_smp_request.response->cpus[i];
        if (info->lapic_id == g_bsp_lapic_id) continue;
        info->goto_address = NULL;
    }

    serial_print("[CPU] SMP topology discovered: ");
    serial_print_dec(g_cpu_count);
    serial_print(" logical CPUs.\n");
    KLOG_LEAVE("cpu", 0);
}

uint32_t cpu_count(void) { return g_cpu_count; }
uint32_t cpu_online_count(void) { return g_online_count; }

uint32_t cpu_current_index(void) {
    uint32_t id = apic_is_available() ? apic_current_lapic_id() : cpu_read_apic_id();
    for (uint32_t i = 0; i < g_cpu_count; ++i) {
        if (g_cpus[i].lapic_id == id) return i;
    }
    return 0;
}

cpu_local_t *cpu_current(void) {
    return &g_cpus[cpu_current_index()];
}

cpu_local_t *cpu_get(uint32_t index) {
    return index < g_cpu_count ? &g_cpus[index] : NULL;
}

void cpu_note_tick(void) {
    cpu_local_t *cpu = cpu_current();
    ++cpu->ticks;
    ++cpu->scheduler_epoch;
}

int cpu_x2apic_enabled(void) {
    return limine_smp_request.response &&
           ((limine_smp_request.response->flags & LIMINE_SMP_X2APIC) != 0);
}
