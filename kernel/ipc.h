#ifndef KESHOS_IPC_H
#define KESHOS_IPC_H

#include <stdint.h>

#define IPC_MAX_MESSAGE_BYTES 256

typedef struct {
    uint64_t data;
    uint32_t size;
    int32_t sender;
    int32_t type;
    uint32_t reserved;
} ipc_user_request_t;

void ipc_init(void);
int ipc_send(int sender_pid, int target_pid, const void *data, uint32_t size);
int ipc_receive(int receiver_pid, int sender_filter, void *data, uint32_t max_size, int *out_sender);
int ipc_send_typed(int sender_pid, int target_pid, uint16_t type, const void *data, uint32_t size);
int ipc_receive_typed(int receiver_pid, int sender_filter, int type_filter, void *data,
                      uint32_t max_size, int *out_sender, uint16_t *out_type);
int ipc_last_sender(int receiver_pid);
void ipc_close_owner(int pid);

#endif
