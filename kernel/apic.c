#include "apic.h"
#include "memory.h"
#include "acpi.h"
#include "include/pic.h"
#include <stddef.h>

extern int cpu_x2apic_enabled(void);

#define IA32_APIC_BASE 0x1B
#define LAPIC_ID       0x020
#define LAPIC_EOI      0x0B0
#define LAPIC_SVR      0x0F0
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_LVT_ERROR 0x370
#define LAPIC_LVT_PERF  0x340
#define LAPIC_LVT_THERM 0x330
#define APIC_ENABLE     (1U << 11)
#define LAPIC_SW_ENABLE (1U << 8)
#define APIC_LVT_MASKED (1U << 16)
#define CPUID_FEAT_APIC  (1U << 9)

#define MADT_TYPE_LAPIC 0
#define MADT_TYPE_IOAPIC 1
#define MADT_TYPE_ISO 2

static volatile uint32_t *g_lapic = NULL;
static uint32_t g_lapic_phys = 0;
static uint32_t g_ioapic_phys = 0;
static uint32_t g_ioapic_gsi_base = 0;
static uint32_t g_ioapic_max_redir = 0;
static int g_apic_available = 0;
static uint32_t g_bsp_lapic_id = 0;

static int cpu_has_apic(void) {
    uint32_t eax = 1, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    return (edx & CPUID_FEAT_APIC) != 0;
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

static inline uint32_t lapic_read(uint32_t reg) {
    return g_lapic[reg / 4];
}

static inline void lapic_write(uint32_t reg, uint32_t value) {
    g_lapic[reg / 4] = value;
    (void)g_lapic[LAPIC_ID / 4];
}

struct madt_header {
    acpi_sdt_header_t sdt;
    uint32_t lapic_address;
    uint32_t flags;
} __attribute__((packed));

struct madt_entry_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_ioapic {
    uint8_t type;
    uint8_t length;
    uint8_t id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsi_base;
} __attribute__((packed));

struct madt_iso {
    uint8_t type;
    uint8_t length;
    uint8_t bus;
    uint8_t source_irq;
    uint16_t flags;
    uint32_t gsi;
} __attribute__((packed));

static uint32_t ioapic_read(uint32_t index) {
    volatile uint32_t *io = (volatile uint32_t *)((uint64_t)g_ioapic_phys + g_hhdm_offset);
    io[0] = index;
    return io[4];
}

static void ioapic_write(uint32_t index, uint32_t value) {
    volatile uint32_t *io = (volatile uint32_t *)((uint64_t)g_ioapic_phys + g_hhdm_offset);
    io[0] = index;
    io[4] = value;
}

static void ioapic_write_redir(uint32_t gsi, uint64_t value) {
    if (!g_ioapic_phys || gsi < g_ioapic_gsi_base) return;
    uint32_t index = gsi - g_ioapic_gsi_base;
    if (index > g_ioapic_max_redir) return;
    ioapic_write(0x10U + index * 2U, (uint32_t)value);
    ioapic_write(0x11U + index * 2U, (uint32_t)(value >> 32));
}

static acpi_sdt_header_t *find_madt(void) {
    static const char signature[4] = {'A', 'P', 'I', 'C'};
    return (acpi_sdt_header_t *)acpi_find_table(signature);
}

static void madt_discover(void) {
    acpi_sdt_header_t *sdt = find_madt();
    if (!sdt) return;
    struct madt_header *madt = (struct madt_header *)sdt;
    g_lapic_phys = madt->lapic_address;

    uint8_t *cursor = (uint8_t *)madt + sizeof(*madt);
    uint8_t *end = (uint8_t *)madt + sdt->length;
    while (cursor + sizeof(struct madt_entry_header) <= end) {
        struct madt_entry_header *h = (struct madt_entry_header *)cursor;
        if (h->length < 2 || cursor + h->length > end) break;
        if (h->type == MADT_TYPE_IOAPIC && h->length >= sizeof(struct madt_ioapic)) {
            struct madt_ioapic *io = (struct madt_ioapic *)cursor;
            if (!g_ioapic_phys || io->gsi_base == 0) {
                g_ioapic_phys = io->address;
                g_ioapic_gsi_base = io->gsi_base;
            }
        }
        cursor += h->length;
    }
}

static int ioapic_init(void) {
    if (!g_ioapic_phys || g_ioapic_phys == 0xFFFFFFFFU) return -1;
    uint32_t version = ioapic_read(1);
    if ((version & 0xFFU) == 0 || ((version >> 16) & 0xFFU) > 239U) return -1;
    g_ioapic_max_redir = (version >> 16) & 0xFFU;
    for (uint32_t i = 0; i <= g_ioapic_max_redir; ++i) {
        ioapic_write_redir(g_ioapic_gsi_base + i, (1ULL << 16));
    }

    /* Route legacy ISA IRQs to the BSP using the existing 0x20..0x2F IDT
       vectors. MADT interrupt-source overrides are applied below. */
    for (uint32_t irq = 0; irq < 16; ++irq) {
        uint64_t redir = (uint64_t)(0x20U + irq) | ((uint64_t)(g_bsp_lapic_id & 0xFFU) << 56);
        if (irq != 0 && irq != 1 && irq != 12) redir |= (1ULL << 16);
        ioapic_write_redir(g_ioapic_gsi_base + irq, redir);
    }

    acpi_sdt_header_t *sdt = find_madt();
    if (!sdt) return -1;
    struct madt_header *madt = (struct madt_header *)sdt;
    uint8_t *cursor = (uint8_t *)madt + sizeof(*madt);
    uint8_t *end = (uint8_t *)madt + sdt->length;
    while (cursor + sizeof(struct madt_entry_header) <= end) {
        struct madt_entry_header *h = (struct madt_entry_header *)cursor;
        if (h->length < 2 || cursor + h->length > end) break;
        if (h->type == MADT_TYPE_ISO && h->length >= sizeof(struct madt_iso)) {
            struct madt_iso *iso = (struct madt_iso *)cursor;
            if (iso->bus == 0 && iso->source_irq < 16) {
                uint64_t redir = (uint64_t)(0x20U + iso->source_irq) | ((uint64_t)(g_bsp_lapic_id & 0xFFU) << 56);
                if (iso->source_irq != 0 && iso->source_irq != 1 && iso->source_irq != 12) redir |= (1ULL << 16);
                if (iso->flags & 0x3) {
                    uint16_t polarity = iso->flags & 0x3;
                    if (polarity == 3) redir |= (1ULL << 13);
                }
                if (iso->flags & 0xC) {
                    uint16_t trigger = iso->flags & 0xC;
                    if (trigger == 0xC) redir |= (1ULL << 15);
                }
                if (iso->gsi != g_ioapic_gsi_base + iso->source_irq)
                    ioapic_write_redir(g_ioapic_gsi_base + iso->source_irq, 1ULL << 16);
                ioapic_write_redir(iso->gsi, redir);
            }
        }
        cursor += h->length;
    }
    return 0;
}

static void apic_common_init(void) {
    if (!g_lapic) return;
    uint32_t svr = lapic_read(LAPIC_SVR);
    lapic_write(LAPIC_SVR, svr | LAPIC_SW_ENABLE | 0xFFU);
    lapic_write(LAPIC_LVT_TIMER, APIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_ERROR, APIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_PERF, APIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_THERM, APIC_LVT_MASKED);
    lapic_write(LAPIC_EOI, 0);
}

int apic_init(void) {
    g_apic_available = 0;

    if (!cpu_has_apic()) return -1;
    if (!g_hhdm_offset) return -1;
    if (!acpi_is_available()) return -1;

    uint64_t apic_base = rdmsr(IA32_APIC_BASE);
    uint64_t phys = apic_base & 0xFFFFF000ULL;
    if (phys == 0 || phys == 0xFFFFFFFFFFFFF000ULL) return -1;

    apic_base |= APIC_ENABLE;
    wrmsr(IA32_APIC_BASE, apic_base);

    g_lapic = (volatile uint32_t *)((uint64_t)phys + g_hhdm_offset);
    if (!g_lapic) return -1;

    g_bsp_lapic_id = lapic_read(LAPIC_ID) >> 24;
    madt_discover();
    if (!g_lapic_phys) return -1;
    if (ioapic_init() != 0) return -1;

    apic_common_init();
    g_apic_available = 1;

    return 0;
}

void apic_ap_init(void) {
    uint64_t apic_base = rdmsr(IA32_APIC_BASE) | APIC_ENABLE;
    wrmsr(IA32_APIC_BASE, apic_base);
    if (!g_lapic) g_lapic = (volatile uint32_t *)((apic_base & 0xFFFFF000ULL) + g_hhdm_offset);
    apic_common_init();
}

int apic_is_available(void) { return g_apic_available; }
int apic_enable_irq_routing(void) {
    if (!g_apic_available || !g_ioapic_phys) return -1;
    pic_set_apic_mode(1);
    return 0;
}
void apic_disable_irq_routing(void) {
    pic_set_apic_mode(0);
    pic_clear_mask(0);
    pic_clear_mask(1);
    pic_clear_mask(2);
    pic_clear_mask(12);
}
void apic_eoi(void) { if (g_lapic) lapic_write(LAPIC_EOI, 0); }
uint32_t apic_current_lapic_id(void) { return g_lapic ? (lapic_read(LAPIC_ID) >> 24) : 0; }
