#include "gpt.h"
#include "block.h"
#include "log.h"

typedef struct {
    char signature[8]; uint32_t revision; uint32_t header_size; uint32_t header_crc32; uint32_t reserved;
    uint64_t current_lba; uint64_t backup_lba; uint64_t first_usable_lba; uint64_t last_usable_lba;
    uint8_t disk_guid[16]; uint64_t partition_entries_lba; uint32_t partition_count; uint32_t partition_entry_size; uint32_t entries_crc32;
} __attribute__((packed)) gpt_header_t;
typedef struct {
    uint8_t type_guid[16]; uint8_t unique_guid[16]; uint64_t first_lba; uint64_t last_lba; uint64_t attributes; uint16_t name[36];
} __attribute__((packed)) gpt_entry_t;

static gpt_partition_t g_partitions[GPT_MAX_PARTITIONS];
static int g_partition_count;
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i < size; ++i) { crc ^= data[i]; for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (-(int32_t)(crc & 1))); }
    return crc;
}
static uint32_t crc32(const uint8_t *data, uint32_t size) { return ~crc32_update(0xFFFFFFFFU, data, size); }
static int all_zero(const uint8_t *data, int size) { for (int i = 0; i < size; ++i) if (data[i]) return 0; return 1; }

static gpt_partition_kind_t partition_kind(const uint8_t guid[16]) {
    /* GUIDs use the on-disk mixed-endian encoding from the GPT specification. */
    static const uint8_t efi_system[16] = { 0x28,0x73,0x2A,0xC1,0x1F,0xF8,0xD2,0x11,0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B };
    static const uint8_t basic_data[16] = { 0xA2,0xA0,0xD0,0xEB,0xE5,0xB9,0x33,0x44,0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7 };
    static const uint8_t linux_fs[16] = { 0xAF,0x3D,0xC6,0x0F,0x83,0x84,0x72,0x47,0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4 };
    const uint8_t *known[] = { efi_system, basic_data, linux_fs };
    for (int k = 0; k < 3; ++k) {
        int match = 1;
        for (int i = 0; i < 16; ++i) if (guid[i] != known[k][i]) { match = 0; break; }
        if (match) return (gpt_partition_kind_t)(k + 1);
    }
    return GPT_PARTITION_UNKNOWN;
}

static void name_to_utf8(char out[37], const uint16_t in[36]) {
    int used = 0;
    for (; used < 36 && in[used]; ++used) out[used] = in[used] <= 0x7F ? (char)in[used] : '?';
    out[used] = '\0';
}

int gpt_scan(int block_device_id) {
    uint8_t sector[512];
    g_partition_count = 0;
    const block_device_t *dev = block_get_device(block_device_id);
    if (!dev || dev->block_size != sizeof(sector) || block_read(block_device_id, 1, 1, sector) != 0) return -1;
    gpt_header_t *header = (gpt_header_t *)sector;
    if (header->signature[0] != 'E' || header->signature[1] != 'F' || header->signature[2] != 'I' || header->signature[3] != ' ' ||
        header->signature[4] != 'P' || header->signature[5] != 'A' || header->signature[6] != 'R' || header->signature[7] != 'T' ||
        header->header_size < 92 || header->header_size > sizeof(sector) || header->current_lba != 1 ||
        header->first_usable_lba > header->last_usable_lba || header->last_usable_lba >= dev->block_count ||
        header->partition_entry_size < sizeof(gpt_entry_t) || header->partition_entry_size > sizeof(sector) ||
        header->partition_count == 0 || header->partition_count > 4096) return -1;
    uint32_t saved_crc = header->header_crc32;
    header->header_crc32 = 0;
    int valid = crc32(sector, header->header_size) == saved_crc;
    header->header_crc32 = saved_crc;
    if (!valid) return -1;
    uint64_t entries_lba = header->partition_entries_lba;
    uint64_t first_usable_lba = header->first_usable_lba;
    uint64_t last_usable_lba = header->last_usable_lba;
    uint32_t entry_size = header->partition_entry_size;
    uint64_t entries_bytes = (uint64_t)header->partition_count * entry_size;
    uint64_t entries_sectors = (entries_bytes + sizeof(sector) - 1) / sizeof(sector);
    if (entries_bytes > 4U * 1024U * 1024U || entries_lba >= dev->block_count ||
        entries_sectors > dev->block_count - entries_lba) return -1;

    /* A valid header alone is insufficient: a corrupted partition array could
     * otherwise be treated as trusted layout metadata.  Stream its CRC in
     * sector-sized chunks so the parser never needs a large allocation. */
    uint32_t entries_crc = 0xFFFFFFFFU;
    uint64_t remaining = entries_bytes;
    for (uint64_t i = 0; i < entries_sectors; ++i) {
        if (block_read(block_device_id, entries_lba + i, 1, sector) != 0) return -1;
        uint32_t bytes = remaining > sizeof(sector) ? sizeof(sector) : (uint32_t)remaining;
        entries_crc = crc32_update(entries_crc, sector, bytes);
        remaining -= bytes;
    }
    if (~entries_crc != header->entries_crc32) return -1;

    uint32_t max_entries = header->partition_count > 128 ? 128 : header->partition_count;
    for (uint32_t i = 0; i < max_entries && g_partition_count < GPT_MAX_PARTITIONS; ++i) {
        uint64_t byte_offset = (uint64_t)i * entry_size;
        uint64_t lba = entries_lba + byte_offset / sizeof(sector);
        uint32_t offset = (uint32_t)(byte_offset % sizeof(sector));
        if (offset + sizeof(gpt_entry_t) > sizeof(sector) || lba >= dev->block_count || block_read(block_device_id, lba, 1, sector) != 0) return -1;
        gpt_entry_t *entry = (gpt_entry_t *)(sector + offset);
        if (all_zero(entry->type_guid, 16)) continue;
        if (entry->first_lba > entry->last_lba || entry->first_lba < first_usable_lba || entry->last_lba > last_usable_lba) return -1;
        gpt_partition_t *out = &g_partitions[g_partition_count++];
        for (int b = 0; b < 16; ++b) { out->type_guid[b] = entry->type_guid[b]; out->unique_guid[b] = entry->unique_guid[b]; }
        out->first_lba = entry->first_lba; out->last_lba = entry->last_lba; out->attributes = entry->attributes;
        out->kind = partition_kind(entry->type_guid);
        name_to_utf8(out->name, entry->name);
    }
    KLOG_INFO("gpt", "valid GPT device=%d partitions=%d", block_device_id, g_partition_count);
    return 0;
}
int gpt_partition_count(void) { return g_partition_count; }
const gpt_partition_t *gpt_get_partition(int index) { return index >= 0 && index < g_partition_count ? &g_partitions[index] : 0; }
