// структуры fat32
#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#define FAT32_MAX_NAME 13 

typedef struct {
    char name[FAT32_MAX_NAME];
    int is_dir;
    uint32_t size;
    uint32_t first_cluster;
} fat32_entry_t;

typedef struct {
    uint32_t files;
    uint32_t directories;
    uint32_t repaired_chains;
    uint32_t reclaimed_clusters;
    uint32_t free_clusters;
} fat32_repair_report_t;

int fat32_init(void);

int fat32_list_dir(uint32_t dir_cluster, fat32_entry_t* out, int max_entries);
uint32_t fat32_root_cluster(void);

uint32_t fat32_read_file(uint32_t first_cluster,
                         uint32_t size,
                         uint8_t* out_buf,
                         uint32_t max_len);

int fat32_write_file(uint32_t dir_cluster, const char* name, const uint8_t* data, uint32_t size);
int fat32_delete_file(uint32_t dir_cluster, const char* name);
int fat32_rename_file(uint32_t dir_cluster, const char* old_name, const char* new_name);
int fat32_make_dir(uint32_t dir_cluster, const char* name);

int fat32_get_stats(uint64_t *total_bytes, uint64_t *free_bytes);
int fat32_check_repair(fat32_repair_report_t *report);
int fat32_shutdown_clean(void);

#endif
