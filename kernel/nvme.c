#include "nvme.h"
#include "block.h"
#include "memory.h"
#include "timer.h"
#include "log.h"

#define NVME_QUEUE_ENTRIES 32U
#define NVME_REG_CAP 0x00U
#define NVME_REG_CC 0x14U
#define NVME_REG_CSTS 0x1CU
#define NVME_REG_AQA 0x24U
#define NVME_REG_ASQ 0x28U
#define NVME_REG_ACQ 0x30U
#define NVME_REG_DOORBELL 0x1000U

typedef struct {
    uint8_t opcode, flags;
    uint16_t command_id;
    uint32_t namespace_id;
    uint64_t reserved2, metadata, prp1, prp2;
    uint32_t cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
} __attribute__((packed)) nvme_command_t;

typedef struct {
    uint32_t result, reserved;
    uint16_t sq_head, sq_id, command_id, status;
} __attribute__((packed)) nvme_completion_t;

typedef struct {
    volatile nvme_command_t *sq;
    volatile nvme_completion_t *cq;
    uint64_t sq_phys, cq_phys;
    uint16_t tail, head, next_id;
    uint8_t phase, qid;
} nvme_queue_t;

typedef struct {
    nvme_namespace_info_t info;
    uint32_t namespace_id;
} nvme_namespace_t;

static volatile uint8_t *g_regs;
static uint32_t g_doorbell_stride;
static nvme_queue_t g_admin, g_io;
static block_dma_buffer_t g_admin_sq, g_admin_cq, g_io_sq, g_io_cq, g_identify, g_data;
static nvme_namespace_t g_namespaces[NVME_MAX_NAMESPACES];
static int g_namespace_count;
static volatile uint8_t g_nvme_lock;

_Static_assert(sizeof(nvme_command_t) == 64, "NVMe command size");
_Static_assert(sizeof(nvme_completion_t) == 16, "NVMe completion size");

static uint32_t reg32(uint32_t offset) { return *(volatile uint32_t *)(g_regs + offset); }
static uint64_t reg64(uint32_t offset) { return *(volatile uint64_t *)(g_regs + offset); }
static void write32(uint32_t offset, uint32_t value) { *(volatile uint32_t *)(g_regs + offset) = value; }
static void write64(uint32_t offset, uint64_t value) { *(volatile uint64_t *)(g_regs + offset) = value; }

static int wait_ready(int ready, uint32_t timeout_ms) {
    uint64_t deadline = timer_millis() + timeout_ms;
    while (((reg32(NVME_REG_CSTS) & 1U) != 0) != ready) {
        if (reg32(NVME_REG_CSTS) & 2U) return -1;
        if (timer_millis() >= deadline) return -1;
        __asm__ volatile("pause");
    }
    return 0;
}

static void ring_doorbell(uint8_t qid, int completion, uint16_t value) {
    uint32_t index = (uint32_t)qid * 2U + (completion ? 1U : 0U);
    *(volatile uint32_t *)(g_regs + NVME_REG_DOORBELL + index * g_doorbell_stride) = value;
}

static int submit(nvme_queue_t *queue, nvme_command_t *command, uint32_t *result) {
    uint16_t id = ++queue->next_id;
    if (!id) id = ++queue->next_id;
    command->command_id = id;
    queue->sq[queue->tail] = *command;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    queue->tail = (uint16_t)((queue->tail + 1U) % NVME_QUEUE_ENTRIES);
    ring_doorbell(queue->qid, 0, queue->tail);
    uint64_t deadline = timer_millis() + 5000U;
    for (;;) {
        nvme_completion_t completion = queue->cq[queue->head];
        if ((completion.status & 1U) == queue->phase) {
            __atomic_thread_fence(__ATOMIC_ACQUIRE);
            if (completion.command_id != id) return -1;
            if (result) *result = completion.result;
            uint16_t status = completion.status;
            queue->head = (uint16_t)((queue->head + 1U) % NVME_QUEUE_ENTRIES);
            if (!queue->head) queue->phase ^= 1U;
            ring_doorbell(queue->qid, 1, queue->head);
            return (status >> 1) & 0x7FFU ? -1 : 0;
        }
        if (reg32(NVME_REG_CSTS) & 2U) return -1;
        if (timer_millis() >= deadline) return -1;
        __asm__ volatile("pause");
    }
}

static void zero_command(nvme_command_t *command) {
    uint8_t *bytes = (uint8_t *)command;
    for (uint32_t i = 0; i < sizeof(*command); ++i) bytes[i] = 0;
}

static int admin_identify(uint32_t namespace_id, uint32_t selector) {
    nvme_command_t command;
    zero_command(&command);
    command.opcode = 0x06;
    command.namespace_id = namespace_id;
    command.prp1 = g_identify.phys;
    command.cdw10 = selector;
    return submit(&g_admin, &command, 0);
}

static int create_io_queues(void) {
    nvme_command_t command;
    zero_command(&command);
    command.opcode = 0x05;
    command.prp1 = g_io_cq.phys;
    command.cdw10 = 1U | ((NVME_QUEUE_ENTRIES - 1U) << 16);
    command.cdw11 = 1U;
    if (submit(&g_admin, &command, 0) != 0) return -1;
    zero_command(&command);
    command.opcode = 0x01;
    command.prp1 = g_io_sq.phys;
    command.cdw10 = 1U | ((NVME_QUEUE_ENTRIES - 1U) << 16);
    command.cdw11 = 1U | (1U << 16);
    return submit(&g_admin, &command, 0);
}

static void copy_model(char out[24], const uint8_t *identify) {
    int end = 40;
    while (end > 0 && identify[24 + end - 1] == ' ') end--;
    int count = end < 23 ? end : 23;
    for (int i = 0; i < count; ++i) out[i] = (char)identify[24 + i];
    out[count] = '\0';
    if (!count) { out[0] = 'N'; out[1] = 'V'; out[2] = 'M'; out[3] = 'e'; out[4] = '\0'; }
}

static void release_buffers(void) {
    block_dma_free(&g_admin_sq); block_dma_free(&g_admin_cq);
    block_dma_free(&g_io_sq); block_dma_free(&g_io_cq);
    block_dma_free(&g_identify); block_dma_free(&g_data);
}

static void stop_controller(uint32_t timeout_ms) {
    write32(NVME_REG_CC, reg32(NVME_REG_CC) & ~1U);
    (void)wait_ready(0, timeout_ms);
}

int nvme_init(uint64_t mmio_phys) {
    g_namespace_count = 0;
    if (!mmio_phys || !g_hhdm_offset) return -1;
    g_regs = (volatile uint8_t *)(mmio_phys + g_hhdm_offset);
    uint64_t capability = reg64(NVME_REG_CAP);
    uint32_t queue_max = (uint32_t)(capability & 0xFFFFU) + 1U;
    uint32_t timeout_ms = (uint32_t)((capability >> 24) & 0xFFU) * 500U;
    if (!timeout_ms) timeout_ms = 5000U;
    if (queue_max < NVME_QUEUE_ENTRIES || !((capability >> 37) & 1U) || ((capability >> 48) & 0xFU) != 0) return -1;
    g_doorbell_stride = 4U << ((capability >> 32) & 0xFU);
    write32(NVME_REG_CC, reg32(NVME_REG_CC) & ~1U);
    if (wait_ready(0, timeout_ms) != 0) return -1;
    if (block_dma_alloc(&g_admin_sq, 1) || block_dma_alloc(&g_admin_cq, 1) ||
        block_dma_alloc(&g_io_sq, 1) || block_dma_alloc(&g_io_cq, 1) ||
        block_dma_alloc(&g_identify, 1) || block_dma_alloc(&g_data, 1)) {
        release_buffers(); return -1;
    }
    g_admin.sq = (volatile nvme_command_t *)g_admin_sq.virt; g_admin.cq = (volatile nvme_completion_t *)g_admin_cq.virt;
    g_admin.sq_phys = g_admin_sq.phys; g_admin.cq_phys = g_admin_cq.phys;
    g_admin.tail = g_admin.head = g_admin.next_id = 0; g_admin.phase = 1; g_admin.qid = 0;
    g_io.sq = (volatile nvme_command_t *)g_io_sq.virt; g_io.cq = (volatile nvme_completion_t *)g_io_cq.virt;
    g_io.sq_phys = g_io_sq.phys; g_io.cq_phys = g_io_cq.phys;
    g_io.tail = g_io.head = g_io.next_id = 0; g_io.phase = 1; g_io.qid = 1;
    write32(NVME_REG_AQA, (NVME_QUEUE_ENTRIES - 1U) | ((NVME_QUEUE_ENTRIES - 1U) << 16));
    write64(NVME_REG_ASQ, g_admin_sq.phys); write64(NVME_REG_ACQ, g_admin_cq.phys);
    write32(NVME_REG_CC, (6U << 16) | (4U << 20) | 1U);
    if (wait_ready(1, timeout_ms) != 0 || admin_identify(0, 1) != 0) { stop_controller(timeout_ms); release_buffers(); return -1; }
    uint8_t *controller = (uint8_t *)g_identify.virt;
    char model[24]; copy_model(model, controller);
    uint32_t namespace_total = *(uint32_t *)(controller + 516);
    if (create_io_queues() != 0) { stop_controller(timeout_ms); release_buffers(); return -1; }
    for (uint32_t nsid = 1; nsid <= namespace_total && g_namespace_count < NVME_MAX_NAMESPACES; ++nsid) {
        for (uint32_t i = 0; i < PAGE_SIZE; ++i) ((uint8_t *)g_identify.virt)[i] = 0;
        if (admin_identify(nsid, 0) != 0) continue;
        uint8_t *ns = (uint8_t *)g_identify.virt;
        uint64_t blocks = *(uint64_t *)ns;
        uint8_t format = ns[26] & 0x0FU;
        uint8_t shift = ns[128U + (uint32_t)format * 4U + 2U];
        if (!blocks || shift < 9 || shift > 12) continue;
        nvme_namespace_t *entry = &g_namespaces[g_namespace_count++];
        entry->namespace_id = nsid; entry->info.block_size = 1U << shift; entry->info.block_count = blocks;
        for (int i = 0; i < 24; ++i) entry->info.model[i] = model[i];
        KLOG_INFO("nvme", "namespace=%u blocks=%llu block-size=%u", nsid, (unsigned long long)blocks, entry->info.block_size);
    }
    return g_namespace_count;
}

int nvme_namespace_count(void) { return g_namespace_count; }
const nvme_namespace_info_t *nvme_namespace_info(int index) { return index >= 0 && index < g_namespace_count ? &g_namespaces[index].info : 0; }

static int transfer(int index, uint64_t lba, uint32_t count, void *buffer, int write) {
    if (index < 0 || index >= g_namespace_count || !buffer || !count) return -1;
    nvme_namespace_t *ns = &g_namespaces[index];
    uint32_t max_blocks = PAGE_SIZE / ns->info.block_size;
    uint8_t *bytes = (uint8_t *)buffer;
    while (count) {
        uint32_t chunk = count < max_blocks ? count : max_blocks;
        uint32_t byte_count = chunk * ns->info.block_size;
        if (write) for (uint32_t i = 0; i < byte_count; ++i) ((uint8_t *)g_data.virt)[i] = bytes[i];
        nvme_command_t command;
        zero_command(&command);
        command.opcode = write ? 0x01 : 0x02;
        command.namespace_id = ns->namespace_id;
        command.prp1 = g_data.phys;
        command.cdw10 = (uint32_t)lba; command.cdw11 = (uint32_t)(lba >> 32);
        command.cdw12 = chunk - 1U;
        if (submit(&g_io, &command, 0) != 0) return -1;
        if (!write) for (uint32_t i = 0; i < byte_count; ++i) bytes[i] = ((uint8_t *)g_data.virt)[i];
        lba += chunk; count -= chunk; bytes += byte_count;
    }
    return 0;
}

static uint64_t lock_nvme(void) {
    while (__atomic_test_and_set(&g_nvme_lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    return 0;
}

static void unlock_nvme(uint64_t flags) {
    (void)flags;
    __atomic_clear(&g_nvme_lock, __ATOMIC_RELEASE);
}

int nvme_read(int index, uint64_t lba, uint32_t count, void *buffer) {
    uint64_t flags = lock_nvme(); int result = transfer(index, lba, count, buffer, 0); unlock_nvme(flags); return result;
}

int nvme_write(int index, uint64_t lba, uint32_t count, const void *buffer) {
    uint64_t flags = lock_nvme(); int result = transfer(index, lba, count, (void *)buffer, 1); unlock_nvme(flags); return result;
}

int nvme_flush(int index) {
    if (index < 0 || index >= g_namespace_count) return -1;
    uint64_t flags = lock_nvme();
    nvme_command_t command;
    zero_command(&command);
    command.opcode = 0x00;
    command.namespace_id = g_namespaces[index].namespace_id;
    int result = submit(&g_io, &command, 0);
    unlock_nvme(flags);
    return result;
}
