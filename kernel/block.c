#include "block.h"
#include "drivers/system/ata.h"
#include "drivers/pci/pci.h"
#include "log.h"
#include "memory.h"
#include "ahci.h"
#include "nvme.h"

typedef struct {
    block_device_t device;
    int ata_index;
    ata_device_type_t type;
    volatile ahci_port_t *ahci_port;
    int ahci_port_index;
    int nvme_index;
} block_binding_t;
static block_binding_t g_devices[BLOCK_MAX_DEVICES];
static int g_count;
static block_dma_buffer_t g_ahci_dma[AHCI_PORTS];
static volatile uint8_t g_ahci_lock;
static volatile uint8_t g_device_locks[BLOCK_MAX_DEVICES];
static void name_copy(char *dst, const char *src);

static uint64_t block_irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static void block_irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) __asm__ volatile("sti" ::: "memory");
}

static void ahci_lock(uint64_t *flags) {
    *flags = block_irq_save();
    while (__atomic_test_and_set(&g_ahci_lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
}

static void ahci_unlock(uint64_t flags) {
    __atomic_clear(&g_ahci_lock, __ATOMIC_RELEASE);
    block_irq_restore(flags);
}

static uint64_t device_lock(int id) {
    while (__atomic_test_and_set(&g_device_locks[id], __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    return 0;
}

static void device_unlock(int id, uint64_t flags) {
    (void)flags;
    __atomic_clear(&g_device_locks[id], __ATOMIC_RELEASE);
}

static void ahci_stop(volatile ahci_port_t *port) {
    port->cmd &= ~(AHCI_PXCMD_ST | AHCI_PXCMD_FRE);
    for (uint32_t i = 0; i < 1000000 && (port->cmd & (AHCI_PXCMD_CR | AHCI_PXCMD_FR)); ++i) { }
}

static void ahci_start(volatile ahci_port_t *port) {
    port->cmd |= AHCI_PXCMD_FRE;
    port->cmd |= AHCI_PXCMD_ST;
}

static int ahci_wait_ready(volatile ahci_port_t *port) {
    for (uint32_t i = 0; i < 1000000; ++i) {
        if (!(port->tfd & (0x80U | 0x08U))) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

static int ahci_wait_command(volatile ahci_port_t *port) {
    for (uint32_t i = 0; i < 5000000; ++i) {
        if (port->is & AHCI_PXIS_TFES) return -1;
        if (!(port->ci & 1U)) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

static int ahci_prepare_port(volatile ahci_port_t *port, int index, int supports_64bit) {
    block_dma_buffer_t *dma = &g_ahci_dma[index];
    if (!dma->phys && block_dma_alloc(dma, 3) != 0) return -1;
    if (!supports_64bit && (dma->phys >> 32)) {
        block_dma_free(dma);
        return -1;
    }
    uint8_t *base = dma->virt;
    ahci_stop(port);
    for (uint32_t i = 0; i < PAGE_SIZE * 3; ++i) base[i] = 0;
    port->clb = (uint32_t)dma->phys;
    port->clbu = (uint32_t)(dma->phys >> 32);
    port->fb = (uint32_t)(dma->phys + 0x400);
    port->fbu = (uint32_t)((dma->phys + 0x400) >> 32);
    port->serr = 0xFFFFFFFFU;
    port->is = 0xFFFFFFFFU;
    ahci_start(port);
    return 0;
}

static int ahci_identify(volatile ahci_port_t *port, int index, uint16_t identify[256]) {
    block_dma_buffer_t *dma = &g_ahci_dma[index];
    uint8_t *base = dma->virt;
    ahci_command_header_t *headers = (ahci_command_header_t *)base;
    ahci_command_table_t *table = (ahci_command_table_t *)(base + PAGE_SIZE);
    uint8_t *data = base + 2 * PAGE_SIZE;
    for (uint32_t i = 0; i < PAGE_SIZE; ++i) { ((uint8_t *)headers)[i] = 0; ((uint8_t *)table)[i] = 0; }
    headers[0].cfl = 5;
    headers[0].prdtl = 1;
    headers[0].ctba = (uint32_t)(dma->phys + PAGE_SIZE);
    headers[0].ctbau = (uint32_t)((dma->phys + PAGE_SIZE) >> 32);
    table->cfis[0] = AHCI_FIS_REG_H2D; table->cfis[1] = 0x80; table->cfis[2] = AHCI_ATA_IDENTIFY;
    table->prdt[0].dba = (uint32_t)(dma->phys + 2 * PAGE_SIZE);
    table->prdt[0].dbau = (uint32_t)((dma->phys + 2 * PAGE_SIZE) >> 32);
    table->prdt[0].dbc_i = 511;
    port->is = 0xFFFFFFFFU;
    if (ahci_wait_ready(port) != 0) return -1;
    port->ci = 1;
    if (ahci_wait_command(port) != 0) return -1;
    for (int w = 0; w < 256; ++w) identify[w] = ((uint16_t *)data)[w];
    return 0;
}

static int ahci_transfer(volatile ahci_port_t *port, int index, uint64_t lba, uint16_t count, void *buffer, int write) {
    if (!count || count > 8 || lba > 0x0000FFFFFFFFFFFFULL) return -1;
    block_dma_buffer_t *dma = &g_ahci_dma[index];
    if (!dma->phys) return -1;
    uint8_t *base = dma->virt;
    ahci_command_header_t *headers = (ahci_command_header_t *)base;
    ahci_command_table_t *table = (ahci_command_table_t *)(base + PAGE_SIZE);
    uint8_t *data = base + 2 * PAGE_SIZE;
    for (uint32_t i = 0; i < PAGE_SIZE; ++i) { ((uint8_t *)headers)[i] = 0; ((uint8_t *)table)[i] = 0; }
    headers[0].cfl = 5; headers[0].prdtl = 1;
    if (write) headers[0].flags = AHCI_CMD_WRITE;
    headers[0].ctba = (uint32_t)(dma->phys + PAGE_SIZE); headers[0].ctbau = (uint32_t)((dma->phys + PAGE_SIZE) >> 32);
    table->cfis[0] = AHCI_FIS_REG_H2D; table->cfis[1] = 0x80;
    table->cfis[2] = write ? AHCI_ATA_WRITE_DMA_EXT : AHCI_ATA_READ_DMA_EXT;
    table->cfis[4] = (uint8_t)lba; table->cfis[5] = (uint8_t)(lba >> 8); table->cfis[6] = (uint8_t)(lba >> 16);
    table->cfis[7] = 0x40; table->cfis[8] = (uint8_t)(lba >> 24); table->cfis[9] = (uint8_t)(lba >> 32); table->cfis[10] = (uint8_t)(lba >> 40);
    table->cfis[12] = (uint8_t)count; table->cfis[13] = (uint8_t)(count >> 8);
    table->prdt[0].dba = (uint32_t)(dma->phys + 2 * PAGE_SIZE); table->prdt[0].dbau = (uint32_t)((dma->phys + 2 * PAGE_SIZE) >> 32); table->prdt[0].dbc_i = (uint32_t)count * 512U - 1U;
    if (write) for (uint32_t b = 0; b < (uint32_t)count * 512U; ++b) data[b] = ((const uint8_t *)buffer)[b];
    if (ahci_wait_ready(port) != 0) return -1;
    port->is = 0xFFFFFFFFU; port->ci = 1;
    if (ahci_wait_command(port) != 0) return -1;
    if (!write) for (uint32_t b = 0; b < (uint32_t)count * 512U; ++b) ((uint8_t *)buffer)[b] = data[b];
    return 0;
}

static int ahci_flush(volatile ahci_port_t *port, int index) {
    block_dma_buffer_t *dma = &g_ahci_dma[index];
    if (!dma->phys) return -1;
    uint8_t *base = dma->virt;
    ahci_command_header_t *headers = (ahci_command_header_t *)base;
    ahci_command_table_t *table = (ahci_command_table_t *)(base + PAGE_SIZE);
    for (uint32_t i = 0; i < PAGE_SIZE; ++i) { ((uint8_t *)headers)[i] = 0; ((uint8_t *)table)[i] = 0; }
    headers[0].cfl = 5;
    headers[0].ctba = (uint32_t)(dma->phys + PAGE_SIZE);
    headers[0].ctbau = (uint32_t)((dma->phys + PAGE_SIZE) >> 32);
    table->cfis[0] = AHCI_FIS_REG_H2D;
    table->cfis[1] = 0x80;
    table->cfis[2] = 0xEA;
    if (ahci_wait_ready(port) != 0) return -1;
    port->is = 0xFFFFFFFFU;
    port->ci = 1;
    return ahci_wait_command(port);
}

static int ahci_probe(uint64_t abar) {
    if (!abar || !g_hhdm_offset) return -1;
    volatile ahci_hba_t *hba = (volatile ahci_hba_t *)(abar + g_hhdm_offset);
    hba->ghc |= AHCI_GHC_AE;
    uint32_t ports = hba->pi;
    int supports_64bit = (hba->cap & AHCI_CAP_S64A) != 0;
    int found = 0;
    for (int i = 0; i < AHCI_PORTS; ++i) {
        if (!(ports & (1U << i))) continue;
        volatile ahci_port_t *port = (volatile ahci_port_t *)((volatile uint8_t *)hba + 0x100 + i * sizeof(ahci_port_t));
        uint32_t ssts = port->ssts;
        if ((ssts & 0x0FU) != 3 || ((ssts >> 8) & 0x0FU) != 1 || port->sig != AHCI_SIG_ATA) continue;
        if (ahci_prepare_port(port, i, supports_64bit) != 0) continue;
        uint16_t identify[256];
        int identify_ok = ahci_identify(port, i, identify);
        uint64_t sectors = identify_ok == 0 && (identify[83] & (1U << 10)) ?
            (uint64_t)identify[100] | ((uint64_t)identify[101] << 16) | ((uint64_t)identify[102] << 32) | ((uint64_t)identify[103] << 48) : 0;
        if (identify_ok != 0 || !sectors || g_count >= BLOCK_MAX_DEVICES) { block_dma_free(&g_ahci_dma[i]); continue; }
        block_binding_t *binding = &g_devices[g_count];
        binding->ata_index = -1; binding->type = ATA_TYPE_HDD; binding->ahci_port = port; binding->ahci_port_index = i;
        binding->nvme_index = -1;
        binding->device.id = g_count; binding->device.block_size = 512; binding->device.block_count = sectors;
        binding->device.writable = 1; binding->device.transport = BLOCK_TRANSPORT_AHCI;
        int name_pos = 0;
        for (int w = 27; w <= 46 && name_pos < 23; ++w) {
            char hi = (char)(identify[w] >> 8), lo = (char)identify[w];
            if (hi && hi != ' ') binding->device.name[name_pos++] = hi;
            if (lo && lo != ' ' && name_pos < 23) binding->device.name[name_pos++] = lo;
        }
        binding->device.name[name_pos] = '\0';
        if (!name_pos) name_copy(binding->device.name, "AHCI SATA");
        KLOG_INFO("ahci", "registered SATA port=%d sectors=%llu device=%d", i, (unsigned long long)sectors, g_count);
        g_count++;
        found++;
    }
    return found;
}

static void name_copy(char *dst, const char *src) {
    int i = 0; while (src && src[i] && i < 23) { dst[i] = src[i]; i++; } dst[i] = '\0';
}

int block_init(void) {
    g_count = 0;
    for (int i = 0; i < BLOCK_MAX_DEVICES; ++i) g_device_locks[i] = 0;
    ata_init();
    for (int i = 0; i < ata_get_drive_count() && g_count < BLOCK_MAX_DEVICES; ++i) {
        const ata_device_t *drive = ata_get_drive(i);
        if (!drive || !drive->present || !drive->sector_size || !drive->total_sectors) continue;
        block_binding_t *binding = &g_devices[g_count];
        binding->ata_index = i;
        binding->type = drive->type;
        binding->ahci_port = 0;
        binding->ahci_port_index = -1;
        binding->nvme_index = -1;
        binding->device.id = g_count;
        binding->device.block_size = drive->sector_size;
        binding->device.block_count = drive->total_sectors;
        binding->device.writable = drive->type == ATA_TYPE_HDD;
        binding->device.transport = BLOCK_TRANSPORT_ATA;
        name_copy(binding->device.name, drive->model);
        g_count++;
    }
    pci_device_t storage;
    if (pci_find_class(PCI_CLASS_STORAGE, 0x06, 0x01, &storage) == 0) {
        int is_io = 0;
        uint64_t abar = pci_bar_address(&storage, 5, &is_io);
        int enabled = pci_enable_device(&storage, PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER);
        int ports = enabled == 0 && !is_io ? ahci_probe(abar) : -1;
        KLOG_NOTICE("block", "AHCI controller %u:%u.%u ABAR=%p mmio=%u enabled=%d SATA-ports=%d",
                    storage.bus, storage.slot, storage.function, (void*)abar, (unsigned)!is_io, enabled == 0, ports);
    }
    if (pci_find_class(PCI_CLASS_STORAGE, 0x08, 0x02, &storage) == 0) {
        int is_io = 0;
        uint64_t bar = pci_bar_address(&storage, 0, &is_io);
        int enabled = pci_enable_device(&storage, PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER);
        int namespaces = enabled == 0 && !is_io ? nvme_init(bar) : -1;
        for (int i = 0; i < namespaces && g_count < BLOCK_MAX_DEVICES; ++i) {
            const nvme_namespace_info_t *info = nvme_namespace_info(i);
            if (!info) continue;
            block_binding_t *binding = &g_devices[g_count];
            binding->ata_index = -1; binding->type = ATA_TYPE_HDD; binding->ahci_port = 0; binding->ahci_port_index = -1; binding->nvme_index = i;
            binding->device.id = g_count; binding->device.block_size = info->block_size; binding->device.block_count = info->block_count;
            binding->device.writable = 1; binding->device.transport = BLOCK_TRANSPORT_NVME;
            name_copy(binding->device.name, info->model);
            g_count++;
        }
        KLOG_NOTICE("block", "NVMe controller %u:%u.%u BAR0=%p mmio=%u enabled=%d namespaces=%d",
                    storage.bus, storage.slot, storage.function, (void*)bar, (unsigned)!is_io, enabled == 0, namespaces);
    }
    KLOG_INFO("block", "registered devices=%d", g_count);
    return g_count;
}

int block_device_count(void) { return g_count; }
const block_device_t *block_get_device(int id) { return id >= 0 && id < g_count ? &g_devices[id].device : 0; }

int block_dma_alloc(block_dma_buffer_t *buffer, uint32_t pages) {
    if (!buffer || !pages) return -1;
    uint64_t phys = pmm_alloc_pages(pages);
    if (!phys) return -1;
    buffer->phys = phys;
    buffer->virt = (void *)(phys + g_hhdm_offset);
    buffer->pages = pages;
    uint8_t *zero = buffer->virt;
    for (uint64_t i = 0; i < (uint64_t)pages * PAGE_SIZE; ++i) zero[i] = 0;
    return 0;
}

void block_dma_free(block_dma_buffer_t *buffer) {
    if (!buffer || !buffer->phys) return;
    (void)pmm_free_pages(buffer->phys, buffer->pages);
    buffer->phys = 0; buffer->virt = 0; buffer->pages = 0;
}

static int range_valid(const block_device_t *dev, uint64_t lba, uint32_t count) {
    return dev && count && lba < dev->block_count && (uint64_t)count <= dev->block_count - lba;
}

int block_read(int id, uint64_t lba, uint32_t count, void *buffer) {
    if (!buffer || id < 0 || id >= g_count || !range_valid(&g_devices[id].device, lba, count)) return -1;
    uint64_t device_flags = device_lock(id);
    block_binding_t *binding = &g_devices[id];
    if (binding->device.transport == BLOCK_TRANSPORT_NVME) {
        int result = nvme_read(binding->nvme_index, lba, count, buffer);
        device_unlock(id, device_flags);
        return result;
    }
    if (binding->device.transport == BLOCK_TRANSPORT_AHCI) {
        uint8_t *out = buffer;
        uint64_t flags;
        ahci_lock(&flags);
        while (count) {
            uint16_t chunk = count > 8 ? 8 : (uint16_t)count;
            if (ahci_transfer(binding->ahci_port, binding->ahci_port_index, lba, chunk, out, 0) != 0) { ahci_unlock(flags); device_unlock(id, device_flags); return -1; }
            lba += chunk; count -= chunk; out += (uint32_t)chunk * 512U;
        }
        ahci_unlock(flags);
        device_unlock(id, device_flags);
        return 0;
    }
    if (lba > 0xFFFFFFFFULL || (uint64_t)count - 1ULL > 0xFFFFFFFFULL - lba) { device_unlock(id, device_flags); return -1; }
    if (binding->type == ATA_TYPE_CDROM) {
        int result = atapi_read_sectors(binding->ata_index, (uint32_t)lba, count, buffer) ? 0 : -1;
        device_unlock(id, device_flags);
        return result;
    }
    uint8_t *out = buffer;
    while (count) {
        uint8_t chunk = count > 255 ? 255 : (uint8_t)count;
        if (!ata_read_sectors_drive(binding->ata_index, (uint32_t)lba, chunk, out)) { device_unlock(id, device_flags); return -1; }
        lba += chunk; count -= chunk; out += (uint64_t)chunk * binding->device.block_size;
    }
    device_unlock(id, device_flags);
    return 0;
}

int block_write(int id, uint64_t lba, uint32_t count, const void *buffer) {
    if (!buffer || id < 0 || id >= g_count || !g_devices[id].device.writable || !range_valid(&g_devices[id].device, lba, count)) return -1;
    uint64_t device_flags = device_lock(id);
    block_binding_t *binding = &g_devices[id];
    if (binding->device.transport == BLOCK_TRANSPORT_NVME) {
        int result = nvme_write(binding->nvme_index, lba, count, buffer);
        device_unlock(id, device_flags);
        return result;
    }
    if (binding->device.transport == BLOCK_TRANSPORT_AHCI) {
        const uint8_t *in = buffer;
        uint64_t flags;
        ahci_lock(&flags);
        while (count) {
            uint16_t chunk = count > 8 ? 8 : (uint16_t)count;
            if (ahci_transfer(binding->ahci_port, binding->ahci_port_index, lba, chunk, (void *)in, 1) != 0) { ahci_unlock(flags); device_unlock(id, device_flags); return -1; }
            lba += chunk; count -= chunk; in += (uint32_t)chunk * 512U;
        }
        ahci_unlock(flags);
        device_unlock(id, device_flags);
        return 0;
    }
    if (lba > 0xFFFFFFFFULL || (uint64_t)count - 1ULL > 0xFFFFFFFFULL - lba) { device_unlock(id, device_flags); return -1; }
    const uint8_t *in = buffer;
    while (count) {
        uint8_t chunk = count > 255 ? 255 : (uint8_t)count;
        if (!ata_write_sectors_drive(binding->ata_index, (uint32_t)lba, chunk, in)) { device_unlock(id, device_flags); return -1; }
        lba += chunk; count -= chunk; in += (uint64_t)chunk * binding->device.block_size;
    }
    device_unlock(id, device_flags);
    return 0;
}

int block_flush(int id) {
    if (id < 0 || id >= g_count || !g_devices[id].device.writable) return -1;
    uint64_t device_flags = device_lock(id);
    block_binding_t *binding = &g_devices[id];
    int result;
    if (binding->device.transport == BLOCK_TRANSPORT_NVME) result = nvme_flush(binding->nvme_index);
    else if (binding->device.transport == BLOCK_TRANSPORT_AHCI) {
        uint64_t flags;
        ahci_lock(&flags);
        result = ahci_flush(binding->ahci_port, binding->ahci_port_index);
        ahci_unlock(flags);
    } else result = ata_flush_drive(binding->ata_index) ? 0 : -1;
    device_unlock(id, device_flags);
    return result;
}
