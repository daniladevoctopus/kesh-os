#ifndef KESHOS_NVME_H
#define KESHOS_NVME_H

#include <stdint.h>

#define NVME_MAX_NAMESPACES 4

typedef struct {
    uint32_t block_size;
    uint64_t block_count;
    char model[24];
} nvme_namespace_info_t;

int nvme_init(uint64_t mmio_phys);
int nvme_namespace_count(void);
const nvme_namespace_info_t *nvme_namespace_info(int index);
int nvme_read(int index, uint64_t lba, uint32_t count, void *buffer);
int nvme_write(int index, uint64_t lba, uint32_t count, const void *buffer);
int nvme_flush(int index);

#endif
