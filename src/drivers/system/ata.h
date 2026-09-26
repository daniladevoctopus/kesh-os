// команды ata контроллера
#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_MAX_DRIVES 4

typedef enum {
    ATA_TYPE_NONE = 0,
    ATA_TYPE_HDD,    
    ATA_TYPE_CDROM   
} ata_device_type_t;

typedef struct {
    int present;
    ata_device_type_t type;
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t drive_sel;
    char model[42];
    char serial[22];
    uint64_t total_sectors;
    uint64_t total_bytes;
    uint32_t sector_size;
} ata_device_t;

int ata_init(void);
void ata_poll_drives(void);

int ata_get_drive_count(void);
const ata_device_t* ata_get_drive(int index);
int ata_get_primary_hdd(void);
int ata_get_primary_cdrom(void);

int ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buf);
int ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buf);
int ata_read_sectors_drive(int drive_idx, uint32_t lba, uint8_t count, uint8_t* buf);
int ata_write_sectors_drive(int drive_idx, uint32_t lba, uint8_t count, const uint8_t* buf);

int atapi_read_capacity(int drive_idx, uint64_t *out_total_bytes, uint32_t *out_block_size);
int atapi_read_sectors(int drive_idx, uint32_t lba, uint32_t count, uint8_t *buf);

int ata_has_cdrom(void);

#endif
