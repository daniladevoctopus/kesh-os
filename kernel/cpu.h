#ifndef KESHOS_CPU_H
#define KESHOS_CPU_H

#include <stdint.h>
#include "process.h"

#define KESHOS_MAX_CPUS 64

typedef struct cpu_local {
    uint32_t index;
    uint32_t processor_id;
    uint32_t lapic_id;
    uint32_t online;
    volatile uint64_t ticks;
    volatile uint32_t scheduler_epoch;
    thread_t *current_thread;
} cpu_local_t;

void cpu_init_bsp(void);
void cpu_smp_init(void);
void cpu_ap_entry(void *smp_info);
uint32_t cpu_count(void);
uint32_t cpu_online_count(void);
int cpu_x2apic_enabled(void);
uint32_t cpu_current_index(void);
cpu_local_t *cpu_current(void);
cpu_local_t *cpu_get(uint32_t index);
void cpu_note_tick(void);

#endif
