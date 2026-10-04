// драйвер сидюка iso9660
#include "iso9660.h"
#include "block.h"

static iso9660_info_t s_primary_iso;
static uint8_t s_sector_buf[ISO9660_SECTOR_SIZE];

static inline uint32_t rd_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int iso9660_probe(int block_device_id, iso9660_info_t *out_info) {
    if (!out_info) return 0;
    out_info->present = 0;
    out_info->drive_idx = block_device_id;
    out_info->block_device_id = block_device_id;
    out_info->volume_label[0] = '\0';
    out_info->root_lba = 0;
    out_info->root_size = 0;
    out_info->total_bytes = 0;

    const block_device_t *device = block_get_device(block_device_id);
    if (!device || device->block_size != ISO9660_SECTOR_SIZE || block_read(block_device_id, 16, 1, s_sector_buf) != 0) {
        return 0;
    }

    if (s_sector_buf[0] != 1 ||
        s_sector_buf[1] != 'C' || s_sector_buf[2] != 'D' ||
        s_sector_buf[3] != '0' || s_sector_buf[4] != '0' || s_sector_buf[5] != '1') {
        return 0;
    }

    int pos = 0;
    for (int i = 0; i < 32; i++) {
        char c = (char)s_sector_buf[40 + i];
        out_info->volume_label[pos++] = c;
    }
    out_info->volume_label[pos] = '\0';

    while (pos > 0 && (out_info->volume_label[pos - 1] == ' ' || out_info->volume_label[pos - 1] == '\0')) {
        out_info->volume_label[--pos] = '\0';
    }

    uint32_t total_sec = rd_le32(s_sector_buf + 80);
    out_info->total_bytes = (uint64_t)total_sec * ISO9660_SECTOR_SIZE;

    const uint8_t *root_rec = s_sector_buf + 156;
    out_info->root_lba = rd_le32(root_rec + 2);
    out_info->root_size = rd_le32(root_rec + 10);
    out_info->present = 1;

    return 1;
}

int iso9660_init(void) {
    s_primary_iso.present = 0;
    for (int i = 0; i < block_device_count(); ++i) {
        const block_device_t *device = block_get_device(i);
        if (device && !device->writable && device->block_size == ISO9660_SECTOR_SIZE && iso9660_probe(i, &s_primary_iso)) return 1;
    }

    return 0;
}

const iso9660_info_t* iso9660_get_primary_info(void) {
    return &s_primary_iso;
}

int iso9660_list_dir(uint32_t lba, uint32_t dir_size, iso9660_entry_t *out_entries, int max_entries) {
    if (!s_primary_iso.present || !out_entries || max_entries <= 0) return 0;

    int block_device_id = s_primary_iso.block_device_id;
    uint32_t sectors = (dir_size + ISO9660_SECTOR_SIZE - 1) / ISO9660_SECTOR_SIZE;
    if (sectors == 0) sectors = 1;
    if (sectors > 16) sectors = 16;

    int count = 0;

    for (uint32_t s = 0; s < sectors; s++) {
        if (block_read(block_device_id, lba + s, 1, s_sector_buf) != 0) {
            break;
        }

        uint32_t offset = 0;
        while (offset < ISO9660_SECTOR_SIZE) {
            uint8_t rec_len = s_sector_buf[offset];
            if (rec_len < 33) {

                break;
            }
            if (offset + rec_len > ISO9660_SECTOR_SIZE) break;

            const uint8_t *rec = s_sector_buf + offset;
            uint32_t ent_lba = rd_le32(rec + 2);
            uint32_t ent_size = rd_le32(rec + 10);
            uint8_t flags = rec[25];
            uint8_t name_len = rec[32];
            const char *name_ptr = (const char*)(rec + 33);

            if (!(name_len == 1 && (name_ptr[0] == 0 || name_ptr[0] == 1))) {
                if (count < max_entries) {
                    iso9660_entry_t *e = &out_entries[count++];
                    e->lba = ent_lba;
                    e->size = ent_size;
                    e->is_dir = (flags & 2) ? 1 : 0;

                    int n = 0;
                    while (n < name_len && n < 63) {
                        char c = name_ptr[n];
                        if (c == ';') break; 
                        e->name[n] = c;
                        n++;
                    }
                    e->name[n] = '\0';
                }
            }

            offset += rec_len;
        }
    }

    return count;
}

int iso9660_read_file_offset(uint32_t lba, uint32_t size, uint64_t offset, void *buf, int max_bytes) {
    if (!s_primary_iso.present || !buf || max_bytes <= 0 || offset >= size) return 0;
    int block_device_id = s_primary_iso.block_device_id;

    uint32_t available = size - (uint32_t)offset;
    uint32_t to_read = (available < (uint32_t)max_bytes) ? available : (uint32_t)max_bytes;

    uint32_t start_sec = (uint32_t)(offset / ISO9660_SECTOR_SIZE);
    uint32_t sec_offset = (uint32_t)(offset % ISO9660_SECTOR_SIZE);

    uint8_t *dst = (uint8_t*)buf;
    uint32_t bytes_left = to_read;
    uint32_t cur_sec = start_sec;

    /* 1. Unaligned head sector */
    if (sec_offset != 0) {
        if (block_read(block_device_id, lba + cur_sec, 1, s_sector_buf) != 0) {
            return 0;
        }
        uint32_t avail_in_sec = ISO9660_SECTOR_SIZE - sec_offset;
        uint32_t chunk = bytes_left < avail_in_sec ? bytes_left : avail_in_sec;
        for (uint32_t i = 0; i < chunk; i++) {
            *dst++ = s_sector_buf[sec_offset + i];
        }
        bytes_left -= chunk;
        cur_sec++;
    }

    /* 2. Full aligned sectors directly into dst in multi-sector batches (up to 32 sectors = 64KB) */
    while (bytes_left >= ISO9660_SECTOR_SIZE) {
        uint32_t full_sectors = bytes_left / ISO9660_SECTOR_SIZE;
        uint32_t batch = full_sectors > 32 ? 32 : full_sectors;
        if (block_read(block_device_id, lba + cur_sec, batch, dst) != 0) {
            return (int)(to_read - bytes_left);
        }
        uint32_t read_bytes = batch * ISO9660_SECTOR_SIZE;
        dst += read_bytes;
        bytes_left -= read_bytes;
        cur_sec += batch;
    }

    /* 3. Partial tail sector */
    if (bytes_left > 0) {
        if (block_read(block_device_id, lba + cur_sec, 1, s_sector_buf) != 0) {
            return (int)(to_read - bytes_left);
        }
        for (uint32_t i = 0; i < bytes_left; i++) {
            *dst++ = s_sector_buf[i];
        }
        bytes_left = 0;
    }

    return (int)to_read;
}

int iso9660_read_file(uint32_t lba, uint32_t size, void *buf, int max_bytes) {
    return iso9660_read_file_offset(lba, size, 0, buf, max_bytes);
}

