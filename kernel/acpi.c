#include "acpi.h"
#include "memory.h"
#include "log.h"
#include "include/limine.h"

typedef struct {
    char signature[8]; uint8_t checksum; char oem_id[6]; uint8_t revision; uint32_t rsdt_address;
    uint32_t length; uint64_t xsdt_address; uint8_t extended_checksum; uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed)) acpi_gas_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t firmware_ctrl, dsdt;
    uint8_t reserved0, preferred_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable, acpi_disable, s4bios_request, pstate_control;
    uint32_t pm1a_event_block, pm1b_event_block, pm1a_control_block, pm1b_control_block;
    uint32_t pm2_control_block, pm_timer_block, gpe0_block, gpe1_block;
    uint8_t pm1_event_length, pm1_control_length, pm2_control_length, pm_timer_length;
    uint8_t gpe0_length, gpe1_length, gpe1_base, cstate_control;
    uint16_t worst_c2_latency, worst_c3_latency;
    uint16_t flush_size, flush_stride;
    uint8_t duty_offset, duty_width, day_alarm, month_alarm, century;
    uint16_t boot_architecture_flags;
    uint8_t reserved1;
    uint32_t flags;
    acpi_gas_t reset_register;
    uint8_t reset_value;
    uint8_t reserved2[3];
    uint64_t x_firmware_ctrl, x_dsdt;
    acpi_gas_t x_pm1a_event_block, x_pm1b_event_block;
    acpi_gas_t x_pm1a_control_block, x_pm1b_control_block;
    acpi_gas_t x_pm2_control_block, x_pm_timer_block;
    acpi_gas_t x_gpe0_block, x_gpe1_block;
} __attribute__((packed)) acpi_fadt_t;

__attribute__((used, section(".requests")))
static volatile struct limine_rsdp_request g_rsdp_request = {
    .id = LIMINE_RSDP_REQUEST, .revision = 0, .response = 0
};

static uint64_t g_root_phys;
static uint32_t g_entry_size;

static int checksum_ok(const uint8_t *bytes, uint32_t size) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < size; ++i) sum = (uint8_t)(sum + bytes[i]);
    return sum == 0;
}
static int sig_eq(const char *a, const char *b) { for (int i = 0; i < 4; ++i) if (a[i] != b[i]) return 0; return 1; }

static inline void acpi_out8(uint16_t port, uint8_t value) { __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port)); }
static inline void acpi_out16(uint16_t port, uint16_t value) { __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port)); }
static inline void acpi_out32(uint16_t port, uint32_t value) { __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port)); }
static inline uint16_t acpi_in16(uint16_t port) { uint16_t value; __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static inline uint32_t acpi_in32(uint16_t port) { uint32_t value; __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port)); return value; }

static int acpi_gas_read32(const acpi_gas_t *gas, uint32_t *value) {
    if (!gas || !value || !gas->address || gas->bit_offset || gas->bit_width > 32) return -1;
    if (gas->address_space == 1 && gas->address <= 0xFFFFU) {
        *value = acpi_in32((uint16_t)gas->address);
        return 0;
    }
    if (gas->address_space == 0 && g_hhdm_offset) {
        *value = *(volatile uint32_t *)(gas->address + g_hhdm_offset);
        return 0;
    }
    return -1;
}

static int acpi_gas_write(const acpi_gas_t *gas, uint64_t value) {
    if (!gas || !gas->address || gas->bit_offset) return -1;
    uint8_t width = gas->bit_width ? gas->bit_width : (uint8_t)(gas->access_size * 8U);
    if (gas->address_space == 1 && gas->address <= 0xFFFFU) {
        if (width <= 8) acpi_out8((uint16_t)gas->address, (uint8_t)value);
        else if (width <= 16) acpi_out16((uint16_t)gas->address, (uint16_t)value);
        else if (width <= 32) acpi_out32((uint16_t)gas->address, (uint32_t)value);
        else return -1;
        return 0;
    }
    if (gas->address_space == 0 && g_hhdm_offset) {
        volatile void *address = (volatile void *)(gas->address + g_hhdm_offset);
        if (width <= 8) *(volatile uint8_t *)address = (uint8_t)value;
        else if (width <= 16) *(volatile uint16_t *)address = (uint16_t)value;
        else if (width <= 32) *(volatile uint32_t *)address = (uint32_t)value;
        else return -1;
        return 0;
    }
    return -1;
}

static int aml_integer(const uint8_t *aml, uint32_t size, uint32_t *offset, uint8_t *value) {
    if (*offset >= size) return -1;
    uint8_t op = aml[(*offset)++];
    if (op == 0x00 || op == 0x01) { *value = op; return 0; }
    if (op == 0x0A && *offset < size) { *value = aml[(*offset)++]; return 0; }
    return -1;
}

static int acpi_sleep_types(const acpi_fadt_t *fadt, uint8_t *a, uint8_t *b) {
    if (!fadt || !a || !b) return -1;
    uint64_t dsdt_phys = fadt->header.length >= 148 && fadt->x_dsdt ? fadt->x_dsdt : fadt->dsdt;
    if (!dsdt_phys) return -1;
    const acpi_sdt_header_t *dsdt = (const acpi_sdt_header_t *)(dsdt_phys + g_hhdm_offset);
    if (dsdt->length < sizeof(*dsdt) || dsdt->length > 4U * 1024U * 1024U || !checksum_ok((const uint8_t *)dsdt, dsdt->length)) return -1;
    const uint8_t *aml = (const uint8_t *)dsdt + sizeof(*dsdt);
    uint32_t size = dsdt->length - (uint32_t)sizeof(*dsdt);
    for (uint32_t i = 0; i + 6 < size; ++i) {
        if (aml[i] != '_' || aml[i + 1] != 'S' || aml[i + 2] != '5' || aml[i + 3] != '_') continue;
        uint32_t p = i + 4;
        if (p < size && aml[p] == 0x08) p++;
        if (p >= size || aml[p++] != 0x12 || p >= size) continue;
        uint8_t lead = aml[p++];
        uint8_t follow = lead >> 6;
        if (p + follow >= size) continue;
        p += follow;
        if (p >= size) continue;
        p++;
        if (aml_integer(aml, size, &p, a) == 0 && aml_integer(aml, size, &p, b) == 0) return 0;
    }
    return -1;
}

int acpi_init(void) {
    g_root_phys = 0; g_entry_size = 0;
    if (!g_hhdm_offset || !g_rsdp_request.response || !g_rsdp_request.response->address) return -1;
    acpi_rsdp_t *rsdp = (acpi_rsdp_t *)(uint64_t)g_rsdp_request.response->address;
    static const char rsdp_signature[8] = {'R','S','D',' ','P','T','R',' '};
    for (int i = 0; i < 8; ++i) if (rsdp->signature[i] != rsdp_signature[i]) return -1;
    if (!checksum_ok((const uint8_t *)rsdp, 20)) return -1;
    if (rsdp->revision >= 2 && (rsdp->length < sizeof(*rsdp) || !checksum_ok((const uint8_t *)rsdp, rsdp->length))) return -1;
    g_root_phys = rsdp->revision >= 2 && rsdp->xsdt_address ? rsdp->xsdt_address : rsdp->rsdt_address;
    if (!g_root_phys) return -1;
    acpi_sdt_header_t *root = (acpi_sdt_header_t *)(g_root_phys + g_hhdm_offset);
    if (root->length < sizeof(*root) || root->length > 1024 * 1024 || !checksum_ok((const uint8_t *)root, root->length)) { g_root_phys = 0; return -1; }
    if (sig_eq(root->signature, "XSDT")) g_entry_size = 8;
    else if (sig_eq(root->signature, "RSDT")) g_entry_size = 4;
    else { g_root_phys = 0; return -1; }
    KLOG_INFO("acpi", "validated %c%c%c%c entries=%u", root->signature[0], root->signature[1], root->signature[2], root->signature[3],
              (root->length - (uint32_t)sizeof(*root)) / g_entry_size);
    return 0;
}

int acpi_is_available(void) { return g_root_phys != 0; }
const acpi_sdt_header_t *acpi_find_table(const char signature[4]) {
    if (!signature || !g_root_phys || !g_entry_size) return 0;
    acpi_sdt_header_t *root = (acpi_sdt_header_t *)(g_root_phys + g_hhdm_offset);
    uint32_t count = (root->length - (uint32_t)sizeof(*root)) / g_entry_size;
    uint8_t *entries = (uint8_t *)root + sizeof(*root);
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t phys = g_entry_size == 8 ? *(const uint64_t *)(entries + i * 8) : *(const uint32_t *)(entries + i * 4);
        if (!phys) continue;
        acpi_sdt_header_t *table = (acpi_sdt_header_t *)(phys + g_hhdm_offset);
        if (table->length < sizeof(*table) || table->length > 1024 * 1024 || !checksum_ok((const uint8_t *)table, table->length)) continue;
        if (sig_eq(table->signature, signature)) return table;
    }
    return 0;
}

int acpi_poweroff(void) {
    const acpi_fadt_t *fadt = (const acpi_fadt_t *)acpi_find_table("FACP");
    uint8_t sleep_a = 0, sleep_b = 0;
    if (!fadt || fadt->header.length < 116 || !fadt->pm1a_control_block || acpi_sleep_types(fadt, &sleep_a, &sleep_b) != 0) return -1;
    if (fadt->smi_command_port && fadt->acpi_enable && !(acpi_in16((uint16_t)fadt->pm1a_control_block) & 1U)) {
        acpi_out8((uint16_t)fadt->smi_command_port, fadt->acpi_enable);
        for (uint32_t wait = 0; wait < 1000000 && !(acpi_in16((uint16_t)fadt->pm1a_control_block) & 1U); ++wait) __asm__ volatile("pause");
    }
    uint16_t value_a = (uint16_t)(((uint16_t)sleep_a << 10) | (1U << 13));
    acpi_out16((uint16_t)fadt->pm1a_control_block, value_a);
    if (fadt->pm1b_control_block) acpi_out16((uint16_t)fadt->pm1b_control_block, (uint16_t)(((uint16_t)sleep_b << 10) | (1U << 13)));
    return 0;
}

int acpi_reboot(void) {
    const acpi_fadt_t *fadt = (const acpi_fadt_t *)acpi_find_table("FACP");
    if (fadt && fadt->header.length >= 129 && (fadt->flags & (1U << 10)) && acpi_gas_write(&fadt->reset_register, fadt->reset_value) == 0) return 0;
    acpi_out8(0xCF9, 0x06);
    for (uint32_t wait = 0; wait < 100000; ++wait) __asm__ volatile("pause");
    acpi_out8(0x64, 0xFE);
    return -1;
}

int acpi_pm_timer_available(void) {
    const acpi_fadt_t *fadt = (const acpi_fadt_t *)acpi_find_table("FACP");
    if (!fadt) return 0;
    if (fadt->header.length >= 220 && fadt->x_pm_timer_block.address &&
        (fadt->x_pm_timer_block.address_space == 0 || fadt->x_pm_timer_block.address_space == 1)) return 1;
    return fadt->pm_timer_block != 0 && fadt->pm_timer_block <= 0xFFFFU;
}

uint32_t acpi_pm_timer_read(void) {
    const acpi_fadt_t *fadt = (const acpi_fadt_t *)acpi_find_table("FACP");
    if (!fadt) return 0;
    uint32_t value = 0;
    if (fadt->header.length >= 220 && acpi_gas_read32(&fadt->x_pm_timer_block, &value) == 0) return value & acpi_pm_timer_mask();
    if (fadt->pm_timer_block && fadt->pm_timer_block <= 0xFFFFU) return acpi_in32((uint16_t)fadt->pm_timer_block) & acpi_pm_timer_mask();
    return 0;
}

uint32_t acpi_pm_timer_mask(void) {
    const acpi_fadt_t *fadt = (const acpi_fadt_t *)acpi_find_table("FACP");
    return fadt && (fadt->flags & (1U << 8)) ? 0xFFFFFFFFU : 0x00FFFFFFU;
}
