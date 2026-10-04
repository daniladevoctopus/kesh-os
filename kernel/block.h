#ifndef KESHOS_BLOCK_H
#define KESHOS_BLOCK_H

#include <stdint.h>

#define BLOCK_MAX_DEVICES 8

typedef enum {
    BLOCK_TRANSPORT_ATA = 1,
    BLOCK_TRANSPORT_AHCI = 2,
    BLOCK_TRANSPORT_NVME = 3
} block_transport_t;

typedef struct {
    int id;
    uint32_t block_size;
    uint64_t block_count;
    uint8_t writable;
    uint8_t transport;
    char name[24];
} block_device_t;

/* AHCI/NVMe command engines need physical, page-aligned DMA buffers. */
typedef struct {
    uint64_t phys;
    void *virt;
    uint32_t pages;
} block_dma_buffer_t;

int block_init(void);
int block_device_count(void);
const block_device_t *block_get_device(int id);
int block_read(int id, uint64_t lba, uint32_t count, void *buffer);
int block_write(int id, uint64_t lba, uint32_t count, const void *buffer);
int block_flush(int id);
int block_dma_alloc(block_dma_buffer_t *buffer, uint32_t pages);
void block_dma_free(block_dma_buffer_t *buffer);

#endif
