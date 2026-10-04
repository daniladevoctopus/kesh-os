#ifndef KESHOS_GPT_H
#define KESHOS_GPT_H

#include <stdint.h>

#define GPT_MAX_PARTITIONS 32

typedef enum {
    GPT_PARTITION_UNKNOWN = 0,
    GPT_PARTITION_EFI_SYSTEM,
    GPT_PARTITION_BASIC_DATA,
    GPT_PARTITION_LINUX_FILESYSTEM
} gpt_partition_kind_t;

typedef struct {
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint8_t type_guid[16];
    uint8_t unique_guid[16];
    gpt_partition_kind_t kind;
    char name[37];
} gpt_partition_t;

int gpt_scan(int block_device_id);
int gpt_partition_count(void);
const gpt_partition_t *gpt_get_partition(int index);

#endif
