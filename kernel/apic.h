#ifndef KESHOS_APIC_H
#define KESHOS_APIC_H

#include <stdint.h>

/* Returns 0 when APIC/IOAPIC could not be safely enabled.
   The caller must keep the legacy PIC path in that case. */
int apic_init(void);
void apic_ap_init(void);
int apic_is_available(void);
int apic_enable_irq_routing(void);
void apic_disable_irq_routing(void);
void apic_eoi(void);
uint32_t apic_current_lapic_id(void);

#endif
