// читаем и пишем сектора на диск
#include "ata.h"

#define ATA_SR_ERR  0x01
#define ATA_SR_DRQ  0x08
#define ATA_SR_DF   0x20
#define ATA_SR_DRDY 0x40
#define ATA_SR_BSY  0x80

#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_WRITE_SECTORS   0x30
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_PACKET          0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1

#define ATA_TIMEOUT 2000000u

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static void serial_str(const char *s) {
    while (*s) {
        while (!(inb(0x3F8 + 5) & 0x20));
        outb(0x3F8, (uint8_t)*s++);
    }
}

static void serial_dec(uint64_t val) {
    char buf[32];
    int idx = 0;
    if (val == 0) buf[idx++] = '0';
    else {
        char rev[32]; int r = 0;
        while (val > 0) { rev[r++] = '0' + (val % 10); val /= 10; }
        while (r > 0) buf[idx++] = rev[--r];
    }
    buf[idx] = '\0';
    serial_str(buf);
}

static void ata_delay_400ns(uint16_t ctrl_base) {
    (void)inb(ctrl_base);
    (void)inb(ctrl_base);
    (void)inb(ctrl_base);
    (void)inb(ctrl_base);
}

static int ata_wait_not_busy(uint16_t io_base) {
    for (uint32_t i = 0; i < ATA_TIMEOUT; ++i) {
        uint8_t st = inb(io_base + 7);
        if (!(st & ATA_SR_BSY)) return 1;
        __asm__ volatile("pause");
    }
    return 0;
}

static int ata_wait_drq(uint16_t io_base) {
    for (uint32_t i = 0; i < ATA_TIMEOUT; ++i) {
        uint8_t st = inb(io_base + 7);
        if (st & ATA_SR_BSY) {
            __asm__ volatile("pause");
            continue;
        }
        if (st & ATA_SR_ERR) {
            (void)inb(io_base + 1); 
            return 0;
        }
        if (st & ATA_SR_DRQ) return 1;
        __asm__ volatile("pause");
    }
    return 0;
}

static ata_device_t s_drives[ATA_MAX_DRIVES];
static int s_primary_hdd = -1;
static int s_primary_cdrom = -1;

static void clean_string(char *dst, const uint16_t *src, int word_count, int max_dst) {
    int pos = 0;
    for (int i = 0; i < word_count && pos + 2 < max_dst; i++) {
        uint16_t w = src[i];
        char c1 = (char)((w >> 8) & 0xFF);
        char c2 = (char)(w & 0xFF);
        dst[pos++] = c1;
        dst[pos++] = c2;
    }
    dst[pos] = '\0';
    while (pos > 0 && (dst[pos - 1] == ' ' || dst[pos - 1] == '\0')) {
        dst[--pos] = '\0';
    }
}

static void ata_probe_single_drive(int idx, uint16_t io_base, uint16_t ctrl_base, uint8_t drive_sel) {
    ata_device_t *d = &s_drives[idx];
    d->present = 0;
    d->type = ATA_TYPE_NONE;
    d->io_base = io_base;
    d->ctrl_base = ctrl_base;
    d->drive_sel = drive_sel;
    d->model[0] = '\0';
    d->serial[0] = '\0';
    d->total_sectors = 0;
    d->total_bytes = 0;
    d->sector_size = 512;

    outb(ctrl_base, 0x02);

    outb(io_base + 6, drive_sel);
    ata_delay_400ns(ctrl_base);

    uint8_t status = inb(io_base + 7);
    if (status == 0xFF) {

        return;
    }

    outb(io_base + 2, 0x55);
    outb(io_base + 3, 0xAA);
    if (inb(io_base + 2) != 0x55 || inb(io_base + 3) != 0xAA) {

        return;
    }

    outb(io_base + 2, 0);
    outb(io_base + 3, 0);
    outb(io_base + 4, 0);
    outb(io_base + 5, 0);

    outb(io_base + 7, ATA_CMD_IDENTIFY);
    ata_delay_400ns(ctrl_base);

    status = inb(io_base + 7);
    if (status == 0) return;

    if (!ata_wait_not_busy(io_base)) return;

    uint8_t mid = inb(io_base + 4);
    uint8_t hi  = inb(io_base + 5);

    uint16_t id_buf[256];

    if ((mid == 0x14 && hi == 0xEB) || (mid == 0x69 && hi == 0x96)) {
        d->type = ATA_TYPE_CDROM;
        d->sector_size = 2048;

        outb(io_base + 7, ATA_CMD_IDENTIFY_PACKET);
        ata_delay_400ns(ctrl_base);

        if (ata_wait_drq(io_base)) {
            for (int i = 0; i < 256; i++) {
                id_buf[i] = inw(io_base);
            }
            clean_string(d->model, &id_buf[27], 20, sizeof(d->model));
            clean_string(d->serial, &id_buf[10], 10, sizeof(d->serial));
        } else {
            const char *def_cd = "ATAPI CD-ROM Drive";
            int p = 0; while (def_cd[p]) { d->model[p] = def_cd[p]; p++; } d->model[p] = '\0';
        }

        (void)inb(io_base + 7); 

        d->present = 1;
        if (s_primary_cdrom == -1) s_primary_cdrom = idx;

        uint64_t cap_bytes = 0;
        uint32_t blk_size = 0;
        if (atapi_read_capacity(idx, &cap_bytes, &blk_size)) {
            d->total_bytes = cap_bytes;
            d->sector_size = blk_size ? blk_size : 2048;
            d->total_sectors = d->total_bytes / d->sector_size;
        }

        serial_str("[IDE] Drive ");
        serial_dec((uint64_t)idx);
        serial_str(": ATAPI CD-ROM '");
        serial_str(d->model);
        serial_str("' Size: ");
        serial_dec(d->total_bytes / (1024 * 1024));
        serial_str(" MB\n");
        return;
    }

    if (mid == 0 && hi == 0) {
        if (!ata_wait_drq(io_base)) return;

        for (int i = 0; i < 256; i++) {
            id_buf[i] = inw(io_base);
        }

        d->type = ATA_TYPE_HDD;
        d->sector_size = 512;
        clean_string(d->model, &id_buf[27], 20, sizeof(d->model));
        clean_string(d->serial, &id_buf[10], 10, sizeof(d->serial));

        uint32_t sec28 = (uint32_t)id_buf[60] | ((uint32_t)id_buf[61] << 16);
        uint64_t sec48 = 0;
        if (id_buf[83] & (1 << 10)) {
            sec48 = (uint64_t)id_buf[100] | ((uint64_t)id_buf[101] << 16) |
                    ((uint64_t)id_buf[102] << 32) | ((uint64_t)id_buf[103] << 48);
        }

        d->total_sectors = (sec48 > 0) ? sec48 : sec28;
        d->total_bytes = d->total_sectors * 512ULL;
        d->present = 1;

        if (s_primary_hdd == -1) s_primary_hdd = idx;

        (void)inb(io_base + 7); 

        serial_str("[IDE] Drive ");
        serial_dec((uint64_t)idx);
        serial_str(": ATA HDD '");
        serial_str(d->model);
        serial_str("' Capacity: ");
        serial_dec(d->total_bytes / (1024 * 1024));
        serial_str(" MB\n");
    }
}

void ata_poll_drives(void) {
    s_primary_hdd = -1;
    s_primary_cdrom = -1;

    outb(0x3F6, 0x02);
    outb(0x376, 0x02);

    ata_probe_single_drive(0, 0x1F0, 0x3F6, 0xA0);

    ata_probe_single_drive(1, 0x1F0, 0x3F6, 0xB0);

    ata_probe_single_drive(2, 0x170, 0x376, 0xA0);

    ata_probe_single_drive(3, 0x170, 0x376, 0xB0);
}

static int s_ata_inited = 0;
int ata_init(void) {
    if (s_ata_inited) return (s_primary_hdd >= 0 || s_primary_cdrom >= 0);
    s_ata_inited = 1;
    ata_poll_drives();
    return (s_primary_hdd >= 0 || s_primary_cdrom >= 0);
}

int ata_get_drive_count(void) {
    return ATA_MAX_DRIVES;
}

const ata_device_t* ata_get_drive(int index) {
    if (index < 0 || index >= ATA_MAX_DRIVES) return 0;
    return &s_drives[index];
}

int ata_get_primary_hdd(void) {
    return s_primary_hdd;
}

int ata_get_primary_cdrom(void) {
    return s_primary_cdrom;
}

int ata_has_cdrom(void) {
    return (s_primary_cdrom >= 0);
}

int ata_read_sectors_drive(int drive_idx, uint32_t lba, uint8_t count, uint8_t* buf) {
    if (drive_idx < 0 || drive_idx >= ATA_MAX_DRIVES) return 0;
    ata_device_t *d = &s_drives[drive_idx];
    if (!d->present || d->type != ATA_TYPE_HDD || !buf || count == 0) return 0;

    uint16_t io = d->io_base;
    uint16_t ctrl = d->ctrl_base;

    outb(ctrl, 0x02); 

    if (!ata_wait_not_busy(io)) return 0;

    uint8_t head = (d->drive_sel == 0xB0) ? 0xF0 : 0xE0;
    outb(io + 6, (uint8_t)(head | ((lba >> 24) & 0x0F)));
    ata_delay_400ns(ctrl);

    outb(io + 2, count);
    outb(io + 3, (uint8_t)(lba & 0xFF));
    outb(io + 4, (uint8_t)((lba >> 8) & 0xFF));
    outb(io + 5, (uint8_t)((lba >> 16) & 0xFF));
    outb(io + 7, ATA_CMD_READ_SECTORS);

    for (uint8_t s = 0; s < count; ++s) {
        if (!ata_wait_not_busy(io)) return 0;
        if (!ata_wait_drq(io)) return 0;

        uint16_t* dst = (uint16_t*)(buf + (uint32_t)s * 512u);
        for (int i = 0; i < 256; ++i) {
            dst[i] = inw(io);
        }
    }

    (void)inb(io + 7);
    return 1;
}

int ata_write_sectors_drive(int drive_idx, uint32_t lba, uint8_t count, const uint8_t* buf) {
    if (drive_idx < 0 || drive_idx >= ATA_MAX_DRIVES) return 0;
    ata_device_t *d = &s_drives[drive_idx];
    if (!d->present || d->type != ATA_TYPE_HDD || !buf || count == 0) return 0;

    uint16_t io = d->io_base;
    uint16_t ctrl = d->ctrl_base;

    outb(ctrl, 0x02); 

    if (!ata_wait_not_busy(io)) return 0;

    uint8_t head = (d->drive_sel == 0xB0) ? 0xF0 : 0xE0;
    outb(io + 6, (uint8_t)(head | ((lba >> 24) & 0x0F)));
    ata_delay_400ns(ctrl);

    outb(io + 2, count);
    outb(io + 3, (uint8_t)(lba & 0xFF));
    outb(io + 4, (uint8_t)((lba >> 8) & 0xFF));
    outb(io + 5, (uint8_t)((lba >> 16) & 0xFF));
    outb(io + 7, ATA_CMD_WRITE_SECTORS);

    for (uint8_t s = 0; s < count; ++s) {
        if (!ata_wait_not_busy(io)) return 0;
        if (!ata_wait_drq(io)) return 0;

        const uint16_t* src = (const uint16_t*)(buf + (uint32_t)s * 512u);
        for (int i = 0; i < 256; ++i) {
            outw(io, src[i]);
        }
        ata_delay_400ns(ctrl);
    }

    if (!ata_wait_not_busy(io)) return 0;
    outb(io + 7, ATA_CMD_CACHE_FLUSH);
    int res = ata_wait_not_busy(io);
    (void)inb(io + 7);
    return res;
}

int ata_read_sectors(uint32_t lba, uint8_t count, uint8_t* buf) {
    if (s_primary_hdd >= 0) {
        return ata_read_sectors_drive(s_primary_hdd, lba, count, buf);
    }
    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t* buf) {
    if (s_primary_hdd >= 0) {
        return ata_write_sectors_drive(s_primary_hdd, lba, count, buf);
    }
    return 0;
}

int ata_flush_drive(int drive_idx) {
    if (drive_idx < 0 || drive_idx >= ATA_MAX_DRIVES || !s_drives[drive_idx].present || s_drives[drive_idx].type != ATA_TYPE_HDD) return 0;
    const ata_device_t *drive = &s_drives[drive_idx];
    if (!ata_wait_not_busy(drive->io_base)) return 0;
    outb(drive->io_base + 6, drive->drive_sel);
    ata_delay_400ns(drive->ctrl_base);
    outb(drive->io_base + 7, ATA_CMD_CACHE_FLUSH);
    if (!ata_wait_not_busy(drive->io_base)) return 0;
    return !(inb(drive->io_base + 7) & 1U);
}

static int atapi_send_packet(const ata_device_t *d, const uint8_t *cdb, uint16_t byte_count, uint8_t *buf, int is_write) {
    uint16_t io = d->io_base;
    uint16_t ctrl = d->ctrl_base;

    outb(ctrl, 0x02); 

    outb(io + 6, d->drive_sel);
    ata_delay_400ns(ctrl);

    if (!ata_wait_not_busy(io)) {
        (void)inb(io + 7);
        return 0;
    }

    outb(io + 1, 0); 
    outb(io + 4, (uint8_t)(byte_count & 0xFF));
    outb(io + 5, (uint8_t)((byte_count >> 8) & 0xFF));
    outb(io + 7, ATA_CMD_PACKET);

    ata_delay_400ns(ctrl);

    if (!ata_wait_not_busy(io)) {
        (void)inb(io + 7);
        return 0;
    }
    if (!ata_wait_drq(io)) {
        (void)inb(io + 7);
        return 0;
    }

    const uint16_t *cdb_words = (const uint16_t*)cdb;
    for (int i = 0; i < 6; i++) {
        outw(io, cdb_words[i]);
    }

    if (byte_count == 0 || !buf) {
        (void)inb(io + 7);
        return 1;
    }

    if (!is_write) {
        if (!ata_wait_not_busy(io)) {
            (void)inb(io + 7);
            return 0;
        }
        if (!ata_wait_drq(io)) {
            (void)inb(io + 7);
            return 0;
        }

        uint16_t actual_bytes = (uint16_t)inb(io + 4) | ((uint16_t)inb(io + 5) << 8);
        if (actual_bytes > byte_count) actual_bytes = byte_count;

        uint16_t *dst = (uint16_t*)buf;
        int words = (actual_bytes + 1) / 2;
        for (int i = 0; i < words; i++) {
            dst[i] = inw(io);
        }

        ata_wait_not_busy(io);
        (void)inb(io + 7); 
        return 1;
    }

    (void)inb(io + 7);
    return 0;
}

int atapi_read_capacity(int drive_idx, uint64_t *out_total_bytes, uint32_t *out_block_size) {
    if (drive_idx < 0 || drive_idx >= ATA_MAX_DRIVES) return 0;
    ata_device_t *d = &s_drives[drive_idx];
    if (!d->present || d->type != ATA_TYPE_CDROM) return 0;

    uint8_t cdb[12] = {0};
    cdb[0] = 0x25; 

    uint8_t resp[8] = {0};
    if (!atapi_send_packet(d, cdb, 8, resp, 0)) {

        if (!atapi_send_packet(d, cdb, 8, resp, 0)) {
            return 0;
        }
    }

    uint32_t last_lba = ((uint32_t)resp[0] << 24) | ((uint32_t)resp[1] << 16) |
                        ((uint32_t)resp[2] << 8)  | (uint32_t)resp[3];
    uint32_t block_sz = ((uint32_t)resp[4] << 24) | ((uint32_t)resp[5] << 16) |
                        ((uint32_t)resp[6] << 8)  | (uint32_t)resp[7];

    if (block_sz == 0) block_sz = 2048;
    if (out_block_size) *out_block_size = block_sz;
    if (out_total_bytes) *out_total_bytes = (uint64_t)(last_lba + 1) * block_sz;

    return 1;
}

int atapi_read_sectors(int drive_idx, uint32_t lba, uint32_t count, uint8_t *buf) {
    if (drive_idx < 0 || drive_idx >= ATA_MAX_DRIVES) return 0;
    ata_device_t *d = &s_drives[drive_idx];
    if (!d->present || d->type != ATA_TYPE_CDROM || !buf || count == 0) return 0;

    for (uint32_t s = 0; s < count; s++) {
        uint32_t cur_lba = lba + s;
        uint8_t cdb[12] = {0};
        cdb[0] = 0x28; 
        cdb[2] = (uint8_t)((cur_lba >> 24) & 0xFF);
        cdb[3] = (uint8_t)((cur_lba >> 16) & 0xFF);
        cdb[4] = (uint8_t)((cur_lba >> 8) & 0xFF);
        cdb[5] = (uint8_t)(cur_lba & 0xFF);
        cdb[7] = 0;
        cdb[8] = 1; 

        uint8_t *sec_dst = buf + s * 2048u;
        if (!atapi_send_packet(d, cdb, 2048, sec_dst, 0)) {

            if (!atapi_send_packet(d, cdb, 2048, sec_dst, 0)) {
                return 0;
            }
        }
    }

    return 1;
}
