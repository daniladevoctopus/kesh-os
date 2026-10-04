// апишка таймера
#ifndef KESHOS_TIMER_H
#define KESHOS_TIMER_H

#include <stdint.h>

void timer_init(uint32_t frequency_hz);
int timer_calibrate_from_acpi(void);
uint64_t timer_ticks(void);
uint64_t timer_millis(void);
uint64_t timer_monotonic_ns(void);
uint64_t timer_realtime_ns(void);
uint64_t timer_irq_count(void);
void timer_wait_ticks(uint64_t ticks);
void timer_wait_ms(uint32_t ms);

#endif
