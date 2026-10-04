#ifndef KESHOS_SERVICE_H
#define KESHOS_SERVICE_H

#include <stdint.h>

#define SERVICE_MAX 16
#define SERVICE_NAME_MAX 24
#define SERVICE_PATH_MAX 128
#define SERVICE_NO_DEPENDENCY (-1)

typedef enum {
    SERVICE_STOPPED = 0,
    SERVICE_STARTING,
    SERVICE_READY,
    SERVICE_FAILED
} service_state_t;

typedef int (*service_start_fn)(void);

typedef struct {
    int id;
    char name[SERVICE_NAME_MAX];
    int dependency;
    service_start_fn start;
    service_state_t state;
    uint32_t start_count;
    uint32_t restart_limit;
    uint32_t restart_count;
    int process_id;
    uint8_t userspace;
    uint64_t heartbeat_timeout_ms;
    uint64_t last_heartbeat_ms;
    uint64_t restart_at_ms;
    char path[SERVICE_PATH_MAX];
} service_info_t;

void service_manager_init(void);
int service_register(const char *name, int dependency, service_start_fn start);
int service_register_userspace(const char *name, int dependency, const char *path, uint64_t heartbeat_timeout_ms);
int service_load_manifest(const char *path);
int service_start_all(void);
int service_restart(int id);
int service_heartbeat(int process_id);
void service_poll(void);
int service_set_restart_limit(int id, uint32_t limit);
const service_info_t *service_get(int id);
int service_ready_count(void);

#endif
