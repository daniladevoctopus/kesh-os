// парсер фат32 чтоб файлы открывать
#include "fat32.h"
#include "ata.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define SECTOR_SIZE 512u
#define MAX_CLUSTER_SECTORS 64u
#define FAT32_EOC 0x0FFFFFF8u
#define FAT32_BAD_CLUSTER 0x0FFFFFF7u

static uint32_t s_partition_lba = 0;
static uint32_t s_partition_sectors = 0;
static uint32_t s_fat_start_lba = 0;
static uint32_t s_data_start_lba = 0;
static uint32_t s_sectors_per_cluster = 0;
static uint32_t s_root_cluster = 0;
static uint32_t s_fat_size_sectors = 0;
static uint32_t s_max_cluster = 0;
static uint32_t s_num_fats = 0;
static int s_ready = 0;
static uint64_t s_total_bytes = 0;
static uint64_t s_free_bytes = 0;

static uint8_t sector_buf[SECTOR_SIZE];
static uint8_t cluster_buf[MAX_CLUSTER_SECTORS * SECTOR_SIZE];
static uint8_t fat_sector_buf[SECTOR_SIZE];

static uint16_t rd_u16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32(const uint8_t* p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static int valid_boot_signature(const uint8_t* sector) {
    return sector[510] == 0x55 && sector[511] == 0xAA;
}

uint32_t fat32_root_cluster(void) {
    return s_root_cluster;
}

int fat32_get_stats(uint64_t *total_bytes, uint64_t *free_bytes) {
    if (!s_ready) return 0;
    if (total_bytes) *total_bytes = s_total_bytes;
    if (free_bytes) *free_bytes = s_free_bytes;
    return 1;
}

int fat32_init(void) {
    s_ready = 0;

    if (!ata_init()) return 0;
    if (!ata_read_sectors(0, 1, sector_buf)) return 0;

    if (!valid_boot_signature(sector_buf)) return 0;

    uint32_t part_lba = 0;
    uint32_t part_sectors = 0;

    for (int i = 0; i < 4; ++i) {
        const uint8_t* entry = sector_buf + 0x1BEu + (uint32_t)i * 16u;
        uint8_t type = entry[4];

        if (type == 0x0B || type == 0x0C || type == 0x1B || type == 0x1C || type == 0x07 || type == 0x0E) {
            part_lba = rd_u32(entry + 8);
            part_sectors = rd_u32(entry + 12);
            break;
        }
    }

    if (part_lba == 0) {
        uint16_t bps = rd_u16(sector_buf + 11);
        uint32_t fs32 = rd_u32(sector_buf + 36);
        if (bps == SECTOR_SIZE && fs32 != 0) {
            part_lba = 0;
            part_sectors = rd_u32(sector_buf + 32);
            if (part_sectors == 0) part_sectors = 0x0FFFFFFFu;
        }
    }

    if (part_lba > 0x0FFFFFFFu) return 0;
    if (part_sectors < 1) part_sectors = 0x0FFFFFFFu;

    if (part_sectors > 0x10000000u - part_lba) {
        part_sectors = 0x10000000u - part_lba;
    }

    s_partition_lba = part_lba;
    s_partition_sectors = part_sectors;

    if (!ata_read_sectors(part_lba, 1, sector_buf)) return 0;
    if (!valid_boot_signature(sector_buf)) return 0;

    uint16_t bytes_per_sector = rd_u16(sector_buf + 11);
    uint8_t sectors_per_cluster = sector_buf[13];
    uint16_t reserved_sectors = rd_u16(sector_buf + 14);
    uint8_t num_fats = sector_buf[16];
    uint16_t root_entry_count = rd_u16(sector_buf + 17);
    uint16_t total_sectors_16 = rd_u16(sector_buf + 19);
    uint16_t sectors_per_track = rd_u16(sector_buf + 24);
    uint16_t heads = rd_u16(sector_buf + 26);
    uint32_t hidden_sectors = rd_u32(sector_buf + 28);
    uint32_t total_sectors_32 = rd_u32(sector_buf + 32);
    uint32_t fat_size_32 = rd_u32(sector_buf + 36);
    uint32_t root_cluster = rd_u32(sector_buf + 44);
    uint16_t fs_info_sector = rd_u16(sector_buf + 48);
    uint16_t backup_boot_sector = rd_u16(sector_buf + 50);

    (void)sectors_per_track;
    (void)heads;
    (void)hidden_sectors;
    (void)fs_info_sector;
    (void)backup_boot_sector;

    if (bytes_per_sector != SECTOR_SIZE) return 0;
    if (sectors_per_cluster == 0 || sectors_per_cluster > MAX_CLUSTER_SECTORS) return 0;
    if ((sectors_per_cluster & (sectors_per_cluster - 1u)) != 0) return 0;
    if (reserved_sectors == 0) return 0;
    if (num_fats == 0 || num_fats > 2) return 0;
    if (root_entry_count != 0) return 0;
    if (total_sectors_16 != 0) return 0;
    if (fat_size_32 == 0) return 0;
    if (root_cluster < 2) return 0;

    uint32_t total_sectors = total_sectors_32;
    if (total_sectors == 0) total_sectors = part_sectors;
    if (part_sectors > 0 && total_sectors > part_sectors) {
        total_sectors = part_sectors;
    }

    uint32_t fat_area_sectors = (uint32_t)num_fats * fat_size_32;
    uint32_t first_data_relative = (uint32_t)reserved_sectors + fat_area_sectors;
    if (first_data_relative >= total_sectors) return 0;

    uint32_t data_sectors = total_sectors - first_data_relative;
    uint32_t cluster_count = data_sectors / sectors_per_cluster;

    if (cluster_count < 8u) return 0;

    uint32_t max_cluster = cluster_count + 1u;
    if (max_cluster < 2 || max_cluster > 0x0FFFFFEFu) return 0;

    uint64_t needed_fat_bytes = ((uint64_t)(max_cluster + 1u)) * 4u;
    uint64_t available_fat_bytes = (uint64_t)fat_size_32 * SECTOR_SIZE;
    if (needed_fat_bytes > available_fat_bytes) return 0;

    uint32_t fat_start = part_lba + reserved_sectors;
    uint32_t data_start = fat_start + fat_area_sectors;
    if (fat_start < part_lba || data_start < fat_start) return 0;

    if (data_start - part_lba >= part_sectors) return 0;
    if (data_sectors > part_sectors - (data_start - part_lba)) {
        data_sectors = part_sectors - (data_start - part_lba);
        cluster_count = data_sectors / sectors_per_cluster;
        max_cluster = cluster_count + 1u;
    }

    if (root_cluster > max_cluster) return 0;

    s_fat_start_lba = fat_start;
    s_data_start_lba = data_start;
    s_sectors_per_cluster = sectors_per_cluster;
    s_fat_size_sectors = fat_size_32;
    s_root_cluster = root_cluster;
    s_max_cluster = max_cluster;
    s_num_fats = num_fats;
    s_ready = 1;

    s_total_bytes = (uint64_t)cluster_count * sectors_per_cluster * SECTOR_SIZE;
    s_free_bytes = (s_total_bytes > 10ULL * 1024 * 1024) ? (s_total_bytes - 10ULL * 1024 * 1024) : s_total_bytes;

    if (fs_info_sector > 0 && fs_info_sector < reserved_sectors) {
        if (ata_read_sectors(part_lba + fs_info_sector, 1, sector_buf)) {
            uint32_t lead_sig = rd_u32(sector_buf);
            uint32_t str_sig = rd_u32(sector_buf + 0x1E4);
            if (lead_sig == 0x41615252 && str_sig == 0x61417272) {
                uint32_t free_clusters = rd_u32(sector_buf + 0x1E8);
                if (free_clusters != 0xFFFFFFFF && free_clusters <= cluster_count) {
                    s_free_bytes = (uint64_t)free_clusters * sectors_per_cluster * SECTOR_SIZE;
                }
            }
        }
    }

    static void (*s_dummy)(void) = 0; (void)s_dummy;
    const char *msg = "[FAT32] Storage mounted! Partition capacity: ";
    while (*msg) {
        while (!(inb(0x3F8 + 5) & 0x20));
        outb(0x3F8, (uint8_t)*msg++);
    }
    extern void serial_print_dec(uint64_t val);
    serial_print_dec(s_total_bytes / (1024 * 1024));
    msg = " MB\n";
    while (*msg) {
        while (!(inb(0x3F8 + 5) & 0x20));
        outb(0x3F8, (uint8_t)*msg++);
    }

    return 1;
}

static int valid_cluster(uint32_t cluster) {
    return cluster >= 2 && cluster <= s_max_cluster;
}

static int cluster_to_lba(uint32_t cluster, uint32_t* out_lba) {
    if (!valid_cluster(cluster) || !out_lba) return 0;

    uint32_t relative = (cluster - 2u) * s_sectors_per_cluster;
    if (relative > s_partition_sectors) return 0;
    if (s_data_start_lba > 0x0FFFFFFFu - relative) return 0;

    *out_lba = s_data_start_lba + relative;
    return 1;
}

static uint32_t fat_next_cluster(uint32_t cluster) {
    if (!s_ready || !valid_cluster(cluster)) return 0;

    uint32_t fat_offset = cluster * 4u;
    uint32_t fat_sector_index = fat_offset / SECTOR_SIZE;
    uint32_t offset_in_sector = fat_offset % SECTOR_SIZE;

    if (fat_sector_index >= s_fat_size_sectors) return 0;

    uint32_t fat_sector_lba = s_fat_start_lba + fat_sector_index;
    if (!ata_read_sectors(fat_sector_lba, 1, fat_sector_buf)) return 0;

    uint32_t value = rd_u32(fat_sector_buf + offset_in_sector) & 0x0FFFFFFFu;

    if (value >= FAT32_EOC) return 0;
    if (value == FAT32_BAD_CLUSTER) return 0;
    if (value < 2 || value > s_max_cluster) return 0;

    return value;
}

static int read_cluster(uint32_t cluster, uint8_t* out) {
    uint32_t lba;
    if (!out || !cluster_to_lba(cluster, &lba)) return 0;
    return ata_read_sectors(lba, (uint8_t)s_sectors_per_cluster, out);
}

static int write_cluster(uint32_t cluster, const uint8_t* data) {
    uint32_t lba;
    if (!data || !cluster_to_lba(cluster, &lba)) return 0;
    return ata_write_sectors(lba, (uint8_t)s_sectors_per_cluster, data);
}

static int fat_set_entry(uint32_t cluster, uint32_t value) {
    if (!s_ready || cluster < 2) return 0;

    uint32_t fat_offset = cluster * 4u;
    uint32_t fat_sector_index = fat_offset / SECTOR_SIZE;
    uint32_t offset_in_sector = fat_offset % SECTOR_SIZE;
    if (fat_sector_index >= s_fat_size_sectors) return 0;

    for (uint32_t copy = 0; copy < s_num_fats; copy++) {
        uint32_t fat_sector_lba = s_fat_start_lba + copy * s_fat_size_sectors + fat_sector_index;

        if (!ata_read_sectors(fat_sector_lba, 1, fat_sector_buf)) return 0;

        fat_sector_buf[offset_in_sector + 0] = (uint8_t)(value & 0xFF);
        fat_sector_buf[offset_in_sector + 1] = (uint8_t)((value >> 8) & 0xFF);
        fat_sector_buf[offset_in_sector + 2] = (uint8_t)((value >> 16) & 0xFF);

        fat_sector_buf[offset_in_sector + 3] =
            (uint8_t)((fat_sector_buf[offset_in_sector + 3] & 0xF0) | ((value >> 24) & 0x0F));

        if (!ata_write_sectors(fat_sector_lba, 1, fat_sector_buf)) return 0;
    }

    return 1;
}

static uint32_t find_free_cluster(void) {
    if (!s_ready) return 0;

    for (uint32_t sector_idx = 0; sector_idx < s_fat_size_sectors; sector_idx++) {
        if (!ata_read_sectors(s_fat_start_lba + sector_idx, 1, fat_sector_buf)) return 0;

        uint32_t entries_here = SECTOR_SIZE / 4u;
        for (uint32_t e = 0; e < entries_here; e++) {
            uint32_t cluster = sector_idx * (SECTOR_SIZE / 4u) + e;
            if (cluster < 2 || cluster > s_max_cluster) continue;

            uint32_t value = rd_u32(fat_sector_buf + e * 4u) & 0x0FFFFFFFu;
            if (value == 0) return cluster;
        }
    }

    return 0; 
}

static void format_short_name(const uint8_t* raw, char* out) {
    char name[9];
    char ext[4];
    int ni = 0;
    int ei = 0;

    for (int i = 0; i < 8 && raw[i] != ' '; ++i) {
        name[ni++] = (char)raw[i];
    }
    name[ni] = 0;

    for (int i = 0; i < 3 && raw[8 + i] != ' '; ++i) {
        ext[ei++] = (char)raw[8 + i];
    }
    ext[ei] = 0;

    int oi = 0;
    for (int i = 0; name[i] && oi < FAT32_MAX_NAME - 1; ++i) {
        out[oi++] = name[i];
    }

    if (ei > 0 && oi < FAT32_MAX_NAME - 1) {
        out[oi++] = '.';
        for (int i = 0; ext[i] && oi < FAT32_MAX_NAME - 1; ++i) {
            out[oi++] = ext[i];
        }
    }

    out[oi] = 0;
}

int fat32_list_dir(uint32_t dir_cluster, fat32_entry_t* out, int max_entries) {
    if (!s_ready || !out || max_entries <= 0 || !valid_cluster(dir_cluster)) return 0;

    int count = 0;
    uint32_t cluster = dir_cluster;

    uint32_t safety = 0;

    while (cluster != 0 && count < max_entries && safety++ <= s_max_cluster) {
        if (!read_cluster(cluster, cluster_buf)) break;

        uint32_t entries_in_cluster =
            (s_sectors_per_cluster * SECTOR_SIZE) / 32u;

        for (uint32_t i = 0; i < entries_in_cluster && count < max_entries; ++i) {
            const uint8_t* raw = cluster_buf + i * 32u;

            if (raw[0] == 0x00) return count;
            if (raw[0] == 0xE5) continue;

            uint8_t attr = raw[11];
            if (attr == 0x0F) continue; 
            if (attr & 0x08) continue;  
            if (raw[0] == '.') continue; 

            fat32_entry_t* e = &out[count];
            format_short_name(raw, e->name);
            e->is_dir = (attr & 0x10) ? 1 : 0;
            e->size = rd_u32(raw + 28);
            e->first_cluster =
                ((uint32_t)rd_u16(raw + 20) << 16) | rd_u16(raw + 26);

            if (e->is_dir) {
                if (!valid_cluster(e->first_cluster)) continue;
            } else if (e->first_cluster != 0 && !valid_cluster(e->first_cluster)) {
                continue;
            }

            count++;
        }

        cluster = fat_next_cluster(cluster);
    }

    return count;
}

uint32_t fat32_read_file(uint32_t first_cluster,
                         uint32_t size,
                         uint8_t* out_buf,
                         uint32_t max_len) {
    if (!s_ready || !out_buf || max_len == 0 || size == 0) return 0;
    if (first_cluster < 2) return 0; 
    if (!valid_cluster(first_cluster)) return 0;

    uint32_t total_read = 0;
    uint32_t cluster = first_cluster;
    uint32_t cluster_bytes = s_sectors_per_cluster * SECTOR_SIZE;
    uint32_t safety = 0;

    while (cluster != 0 && total_read < size && total_read < max_len &&
           safety++ <= s_max_cluster) {
        if (!read_cluster(cluster, cluster_buf)) break;

        uint32_t remaining = size - total_read;
        uint32_t space_left = max_len - total_read;
        uint32_t to_copy = cluster_bytes;

        if (to_copy > remaining) to_copy = remaining;
        if (to_copy > space_left) to_copy = space_left;

        for (uint32_t i = 0; i < to_copy; ++i) {
            out_buf[total_read + i] = cluster_buf[i];
        }

        total_read += to_copy;
        if (total_read >= size || total_read >= max_len) break;

        cluster = fat_next_cluster(cluster);
    }

    return total_read;
}

static void to_short_name(const char* input, uint8_t out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';

    int i = 0, ni = 0;
    while (input[i] && input[i] != '.' && ni < 8) {
        char c = input[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[ni++] = (uint8_t)c;
        i++;
    }
    while (input[i] && input[i] != '.') i++; 
    if (input[i] == '.') {
        i++;
        int ei = 0;
        while (input[i] && ei < 3) {
            char c = input[i];
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            out[8 + ei++] = (uint8_t)c;
            i++;
        }
    }
}

static int find_free_dir_slot(uint32_t dir_cluster, uint32_t* out_cluster, uint32_t* out_index) {
    if (!s_ready || !valid_cluster(dir_cluster)) return 0;

    uint32_t cluster = dir_cluster;
    uint32_t last_cluster = dir_cluster;
    uint32_t entries_in_cluster = (s_sectors_per_cluster * SECTOR_SIZE) / 32u;
    uint32_t safety = 0;

    while (cluster != 0 && safety++ <= s_max_cluster) {
        if (!read_cluster(cluster, cluster_buf)) return 0;

        for (uint32_t i = 0; i < entries_in_cluster; i++) {
            uint8_t first_byte = cluster_buf[i * 32u];
            if (first_byte == 0x00 || first_byte == 0xE5) {
                *out_cluster = cluster;
                *out_index = i;
                return 1;
            }
        }

        last_cluster = cluster;
        cluster = fat_next_cluster(cluster);
    }

    uint32_t new_cluster = find_free_cluster();
    if (new_cluster == 0) return 0; 

    for (uint32_t i = 0; i < sizeof(cluster_buf); i++) cluster_buf[i] = 0;
    if (!write_cluster(new_cluster, cluster_buf)) return 0;
    if (!fat_set_entry(new_cluster, FAT32_EOC)) return 0;
    if (!fat_set_entry(last_cluster, new_cluster)) return 0;

    *out_cluster = new_cluster;
    *out_index = 0;
    return 1;
}

static int names_equal_83(const uint8_t raw[11], const uint8_t candidate[11]) {
    for (int i = 0; i < 11; i++) if (raw[i] != candidate[i]) return 0;
    return 1;
}

static int fat_free_chain(uint32_t first_cluster, uint32_t *out_clusters) {
    if (!valid_cluster(first_cluster)) return first_cluster == 0;
    uint32_t cluster = first_cluster;
    uint32_t count = 0;
    uint32_t safety = 0;
    while (cluster && safety++ <= s_max_cluster) {
        uint32_t next = fat_next_cluster(cluster);
        if (!fat_set_entry(cluster, 0)) return 0;
        count++;
        if (!next) {
            if (out_clusters) *out_clusters = count;
            return 1;
        }
        cluster = next;
    }
    return 0;
}

static int locate_dir_entry(uint32_t dir_cluster, const uint8_t short_name[11],
                            uint32_t* out_cluster, uint32_t* out_index) {
    if (!s_ready || !valid_cluster(dir_cluster) || !out_cluster || !out_index) return 0;
    uint32_t cluster = dir_cluster;
    uint32_t safety = 0;
    uint32_t entries_in_cluster = (s_sectors_per_cluster * SECTOR_SIZE) / 32u;
    while (cluster && safety++ <= s_max_cluster) {
        if (!read_cluster(cluster, cluster_buf)) return 0;
        for (uint32_t i = 0; i < entries_in_cluster; i++) {
            const uint8_t* raw = cluster_buf + i * 32u;
            if (raw[0] == 0x00) return 0;
            if (raw[0] == 0xE5 || raw[11] == 0x0F || (raw[11] & 0x08)) continue;
            if (names_equal_83(raw, short_name)) {
                *out_cluster = cluster;
                *out_index = i;
                return 1;
            }
        }
        cluster = fat_next_cluster(cluster);
    }
    return 0;
}

static int dir_is_empty(uint32_t dir_cluster) {
    fat32_entry_t entries[2];
    if (!valid_cluster(dir_cluster)) return 0;
    int count = fat32_list_dir(dir_cluster, entries, 2);
    return count == 0;
}

static int allocate_file_chain(const uint8_t* data, uint32_t size,
                               uint32_t* out_first, uint32_t* out_count) {
    uint32_t cluster_bytes = s_sectors_per_cluster * SECTOR_SIZE;
    uint32_t clusters_needed = (size + cluster_bytes - 1u) / cluster_bytes;
    if (clusters_needed == 0) clusters_needed = 1;
    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;
    uint32_t written = 0;
    uint32_t allocated[256];
    uint32_t allocated_count = 0;
    if (clusters_needed > 256) return 0;

    for (uint32_t c = 0; c < clusters_needed; c++) {
        uint32_t cluster = find_free_cluster();
        if (cluster == 0 || allocated_count >= 256) {
            for (uint32_t i = 0; i < allocated_count; i++) fat_set_entry(allocated[i], 0);
            return 0;
        }
        allocated[allocated_count++] = cluster;
        if (!fat_set_entry(cluster, FAT32_EOC)) {
            for (uint32_t i = 0; i < allocated_count; i++) fat_set_entry(allocated[i], 0);
            return 0;
        }
        if (prev_cluster != 0 && !fat_set_entry(prev_cluster, cluster)) {
            for (uint32_t i = 0; i < allocated_count; i++) fat_set_entry(allocated[i], 0);
            return 0;
        }
        if (first_cluster == 0) first_cluster = cluster;

        uint32_t remaining = size - written;
        uint32_t to_write = remaining < cluster_bytes ? remaining : cluster_bytes;
        for (uint32_t i = 0; i < cluster_bytes; i++) cluster_buf[i] = 0;
        for (uint32_t i = 0; i < to_write; i++) cluster_buf[i] = data ? data[written + i] : 0;
        if (!write_cluster(cluster, cluster_buf)) {
            for (uint32_t i = 0; i < allocated_count; i++) fat_set_entry(allocated[i], 0);
            return 0;
        }
        written += to_write;
        prev_cluster = cluster;
    }

    if (out_first) *out_first = first_cluster;
    if (out_count) *out_count = allocated_count;
    return 1;
}

static void fill_dir_entry(uint8_t* raw, const uint8_t short_name[11], uint8_t attr,
                           uint32_t first_cluster, uint32_t size) {
    for (int i = 0; i < 11; i++) raw[i] = short_name[i];
    raw[11] = attr;
    for (int i = 12; i < 20; i++) raw[i] = 0;
    raw[20] = (uint8_t)(first_cluster >> 16);
    raw[21] = (uint8_t)(first_cluster >> 24);
    for (int i = 22; i < 26; i++) raw[i] = 0;
    raw[26] = (uint8_t)first_cluster;
    raw[27] = (uint8_t)(first_cluster >> 8);
    raw[28] = (uint8_t)size;
    raw[29] = (uint8_t)(size >> 8);
    raw[30] = (uint8_t)(size >> 16);
    raw[31] = (uint8_t)(size >> 24);
}

int fat32_delete_file(uint32_t dir_cluster, const char* name) {
    if (!s_ready || !name || !valid_cluster(dir_cluster)) return 0;
    uint8_t short_name[11];
    to_short_name(name, short_name);
    uint32_t entry_cluster = 0, entry_index = 0;
    if (!locate_dir_entry(dir_cluster, short_name, &entry_cluster, &entry_index)) return 0;
    if (!read_cluster(entry_cluster, cluster_buf)) return 0;
    uint8_t* raw = cluster_buf + entry_index * 32u;
    uint8_t attr = raw[11];
    uint32_t first_cluster = ((uint32_t)rd_u16(raw + 20) << 16) | rd_u16(raw + 26);
    if ((attr & 0x10) && first_cluster >= 2 && !dir_is_empty(first_cluster)) return 0;
    uint32_t freed = 0;
    if (first_cluster >= 2 && !fat_free_chain(first_cluster, &freed)) return 0;
    raw[0] = 0xE5;
    if (!write_cluster(entry_cluster, cluster_buf)) return 0;
    if (freed) s_free_bytes += (uint64_t)freed * s_sectors_per_cluster * SECTOR_SIZE;
    if (s_free_bytes > s_total_bytes) s_free_bytes = s_total_bytes;
    return 1;
}

int fat32_write_file(uint32_t dir_cluster, const char* name, const uint8_t* data, uint32_t size) {
    if (!s_ready || !name || (!data && size > 0) || !valid_cluster(dir_cluster)) return 0;
    uint8_t short_name[11];
    to_short_name(name, short_name);

    uint32_t entry_cluster = 0, entry_index = 0;
    int existing = locate_dir_entry(dir_cluster, short_name, &entry_cluster, &entry_index);
    uint32_t old_first = 0;
    uint32_t old_size = 0;
    uint8_t old_attr = 0;
    if (existing) {
        if (!read_cluster(entry_cluster, cluster_buf)) return 0;
        uint8_t* raw = cluster_buf + entry_index * 32u;
        old_attr = raw[11];
        old_first = ((uint32_t)rd_u16(raw + 20) << 16) | rd_u16(raw + 26);
        old_size = rd_u32(raw + 28);
        if (old_attr & 0x10) return 0;
    } else {
        if (!find_free_dir_slot(dir_cluster, &entry_cluster, &entry_index)) return 0;
    }

    uint32_t new_first = 0, new_clusters = 0;
    if (!allocate_file_chain(data, size, &new_first, &new_clusters)) return 0;
    uint32_t old_clusters = 0;
    if (existing && old_first >= 2) {
        uint32_t cluster_bytes = s_sectors_per_cluster * SECTOR_SIZE;
        old_clusters = (old_size + cluster_bytes - 1u) / cluster_bytes;
        if (old_clusters == 0) old_clusters = 1;
    }

    if (!read_cluster(entry_cluster, cluster_buf)) {
        fat_free_chain(new_first, NULL);
        return 0;
    }
    uint8_t* raw = cluster_buf + entry_index * 32u;
    fill_dir_entry(raw, short_name, 0x20, new_first, size);
    if (!write_cluster(entry_cluster, cluster_buf)) {
        fat_free_chain(new_first, NULL);
        return 0;
    }

    if (old_first >= 2 && !fat_free_chain(old_first, NULL)) return 0;
    uint64_t cluster_bytes64 = (uint64_t)s_sectors_per_cluster * SECTOR_SIZE;
    s_free_bytes = s_free_bytes + (uint64_t)old_clusters * cluster_bytes64;
    s_free_bytes = (s_free_bytes > (uint64_t)new_clusters * cluster_bytes64) ?
                   s_free_bytes - (uint64_t)new_clusters * cluster_bytes64 : 0;
    if (s_free_bytes > s_total_bytes) s_free_bytes = s_total_bytes;
    return 1;
}

int fat32_make_dir(uint32_t dir_cluster, const char* name) {
    if (!s_ready || !name || !valid_cluster(dir_cluster)) return 0;
    uint8_t short_name[11];
    to_short_name(name, short_name);
    uint32_t existing_cluster = 0, existing_index = 0;
    if (locate_dir_entry(dir_cluster, short_name, &existing_cluster, &existing_index)) return 0;

    uint32_t new_cluster = find_free_cluster();
    if (!new_cluster || !fat_set_entry(new_cluster, FAT32_EOC)) return 0;
    for (uint32_t i = 0; i < s_sectors_per_cluster * SECTOR_SIZE; i++) cluster_buf[i] = 0;
    uint8_t dot[11], dotdot[11];
    to_short_name(".", dot);
    to_short_name("..", dotdot);
    fill_dir_entry(cluster_buf + 0, dot, 0x10, new_cluster, 0);
    fill_dir_entry(cluster_buf + 32, dotdot, 0x10, dir_cluster, 0);
    if (!write_cluster(new_cluster, cluster_buf)) {
        fat_set_entry(new_cluster, 0);
        return 0;
    }

    uint32_t slot_cluster = 0, slot_index = 0;
    if (!find_free_dir_slot(dir_cluster, &slot_cluster, &slot_index) || !read_cluster(slot_cluster, cluster_buf)) {
        fat_set_entry(new_cluster, 0);
        return 0;
    }
    fill_dir_entry(cluster_buf + slot_index * 32u, short_name, 0x10, new_cluster, 0);
    if (!write_cluster(slot_cluster, cluster_buf)) {
        fat_set_entry(new_cluster, 0);
        return 0;
    }
    uint64_t cluster_bytes = (uint64_t)s_sectors_per_cluster * SECTOR_SIZE;
    if (s_free_bytes >= cluster_bytes) s_free_bytes -= cluster_bytes;
    else s_free_bytes = 0;
    return 1;
}
