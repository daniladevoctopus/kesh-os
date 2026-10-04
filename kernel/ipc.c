#include "ipc.h"
#include "process.h"

#define IPC_MAX_PROCESSES 16
#define IPC_QUEUE_DEPTH 16

typedef struct { int sender; uint16_t type, size; uint8_t data[IPC_MAX_MESSAGE_BYTES]; } ipc_message_t;
typedef struct { ipc_message_t messages[IPC_QUEUE_DEPTH]; uint8_t count; } ipc_queue_t;
static ipc_queue_t g_queues[IPC_MAX_PROCESSES];
static int g_last_sender[IPC_MAX_PROCESSES];
static volatile uint8_t g_ipc_lock;

static uint64_t ipc_lock(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    while (__atomic_test_and_set(&g_ipc_lock, __ATOMIC_ACQUIRE)) __asm__ volatile("pause");
    return flags;
}

static void ipc_unlock(uint64_t flags) {
    __atomic_clear(&g_ipc_lock, __ATOMIC_RELEASE);
    __asm__ volatile("pushq %0; popfq" :: "r"(flags) : "memory", "cc");
}

static int valid_pid(int pid) { return pid >= 0 && pid < IPC_MAX_PROCESSES; }
void ipc_init(void) {
    g_ipc_lock = 0;
    for (int i = 0; i < IPC_MAX_PROCESSES; ++i) {
        g_queues[i].count = 0;
        g_last_sender[i] = -1;
    }
}
int ipc_send(int sender_pid, int target_pid, const void *data, uint32_t size) {
    return ipc_send_typed(sender_pid, target_pid, 0, data, size);
}
int ipc_send_typed(int sender_pid, int target_pid, uint16_t type, const void *data, uint32_t size) {
    if (!valid_pid(sender_pid) || !valid_pid(target_pid) || !process_exists(sender_pid) || !process_exists(target_pid) ||
        !data || !size || size > IPC_MAX_MESSAGE_BYTES) return -1;
    uint64_t flags = ipc_lock();
    ipc_queue_t *queue = &g_queues[target_pid];
    if (queue->count == IPC_QUEUE_DEPTH) { ipc_unlock(flags); return -1; }
    ipc_message_t *message = &queue->messages[queue->count++];
    message->sender = sender_pid; message->type = type; message->size = (uint16_t)size;
    for (uint32_t i = 0; i < size; ++i) message->data[i] = ((const uint8_t *)data)[i];
    ipc_unlock(flags);
    return (int)size;
}
int ipc_receive(int receiver_pid, int sender_filter, void *data, uint32_t max_size, int *out_sender) {
    return ipc_receive_typed(receiver_pid, sender_filter, -1, data, max_size, out_sender, 0);
}
int ipc_receive_typed(int receiver_pid, int sender_filter, int type_filter, void *data,
                      uint32_t max_size, int *out_sender, uint16_t *out_type) {
    if (!valid_pid(receiver_pid) || !data || !max_size) return -1;
    uint64_t flags = ipc_lock();
    ipc_queue_t *queue = &g_queues[receiver_pid];
    int index = -1;
    for (uint8_t i = 0; i < queue->count; ++i) {
        if ((sender_filter < 0 || queue->messages[i].sender == sender_filter) &&
            (type_filter < 0 || queue->messages[i].type == (uint16_t)type_filter)) { index = i; break; }
    }
    if (index < 0) { ipc_unlock(flags); return 0; }
    ipc_message_t message = queue->messages[index];
    for (uint8_t i = (uint8_t)index; i + 1 < queue->count; ++i) queue->messages[i] = queue->messages[i + 1];
    queue->count--;
    g_last_sender[receiver_pid] = message.sender;
    ipc_unlock(flags);
    uint32_t copied = message.size < max_size ? message.size : max_size;
    for (uint32_t i = 0; i < copied; ++i) ((uint8_t *)data)[i] = message.data[i];
    if (out_sender) *out_sender = message.sender;
    if (out_type) *out_type = message.type;
    return (int)copied;
}
int ipc_last_sender(int receiver_pid) {
    if (!valid_pid(receiver_pid)) return -1;
    uint64_t flags = ipc_lock();
    int sender = g_last_sender[receiver_pid];
    ipc_unlock(flags);
    return sender;
}
void ipc_close_owner(int pid) {
    if (!valid_pid(pid)) return;
    uint64_t flags = ipc_lock();
    g_queues[pid].count = 0;
    g_last_sender[pid] = -1;
    for (int target = 0; target < IPC_MAX_PROCESSES; ++target) {
        ipc_queue_t *queue = &g_queues[target];
        uint8_t out = 0;
        for (uint8_t i = 0; i < queue->count; ++i) if (queue->messages[i].sender != pid) queue->messages[out++] = queue->messages[i];
        queue->count = out;
    }
    ipc_unlock(flags);
}
