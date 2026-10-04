#ifndef KESHOS_ACPI_H
#define KESHOS_ACPI_H

#include <stdint.h>

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

int acpi_init(void);
int acpi_is_available(void);
const acpi_sdt_header_t *acpi_find_table(const char signature[4]);
int acpi_poweroff(void);
int acpi_reboot(void);
int acpi_pm_timer_available(void);
uint32_t acpi_pm_timer_read(void);
uint32_t acpi_pm_timer_mask(void);

#endif
