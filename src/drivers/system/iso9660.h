// структуры iso
#ifndef ISO9660_H
#define ISO9660_H

#include <stdint.h>

#define ISO9660_SECTOR_SIZE 2048

typedef struct {
    char name[64];
    uint32_t lba;
    uint32_t size;
    uint8_t is_dir;
} iso9660_entry_t;

typedef struct {
    int present;
    int drive_idx;
    char volume_label[33];
    uint32_t root_lba;
    uint32_t root_size;
    uint64_t total_bytes;
} iso9660_info_t;

int iso9660_init(void);
int iso9660_probe(int drive_idx, iso9660_info_t *out_info);

int iso9660_list_dir(uint32_t lba, uint32_t dir_size, iso9660_entry_t *out_entries, int max_entries);
int iso9660_read_file(uint32_t lba, uint32_t size, void *buf, int max_bytes);

const iso9660_info_t* iso9660_get_primary_info(void);

#endif
