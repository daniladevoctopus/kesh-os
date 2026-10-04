#include "service.h"
#include "log.h"
#include "process.h"
#include "timer.h"
#include "vfs.h"

static service_info_t g_services[SERVICE_MAX];
static int g_service_count;

static int service_find(const char *name) {
    for (int i = 0; name && i < g_service_count; ++i) {
        int p = 0;
        while (name[p] && g_services[i].name[p] == name[p]) p++;
        if (!name[p] && !g_services[i].name[p]) return i;
    }
    return -1;
}

static uint64_t parse_u64(const char *text, int *ok) {
    uint64_t value = 0;
    *ok = 0;
    if (!text || !*text) return 0;
    while (*text) {
        if (*text < '0' || *text > '9' || value > (~0ULL - (uint64_t)(*text - '0')) / 10ULL) return 0;
        value = value * 10ULL + (uint64_t)(*text++ - '0');
    }
    *ok = 1;
    return value;
}

static void copy_name(char *dst, const char *src) {
    int i = 0;
    while (src && src[i] && i + 1 < SERVICE_NAME_MAX) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void service_manager_init(void) {
    g_service_count = 0;
    for (int i = 0; i < SERVICE_MAX; ++i) g_services[i].state = SERVICE_STOPPED;
}

int service_register(const char *name, int dependency, service_start_fn start) {
    if (!name || !start || g_service_count >= SERVICE_MAX) return -1;
    if (dependency >= g_service_count || dependency < SERVICE_NO_DEPENDENCY) return -1;
    service_info_t *service = &g_services[g_service_count];
    service->id = g_service_count;
    copy_name(service->name, name);
    service->dependency = dependency;
    service->start = start;
    service->state = SERVICE_STOPPED;
    service->start_count = 0;
    service->restart_limit = 2;
    service->restart_count = 0;
    service->process_id = -1;
    service->userspace = 0;
    service->heartbeat_timeout_ms = 0;
    service->last_heartbeat_ms = 0;
    service->restart_at_ms = 0;
    service->path[0] = 0;
    return g_service_count++;
}

int service_register_userspace(const char *name, int dependency, const char *path, uint64_t heartbeat_timeout_ms) {
    if (!name || !path || !path[0] || g_service_count >= SERVICE_MAX ||
        dependency >= g_service_count || dependency < SERVICE_NO_DEPENDENCY || heartbeat_timeout_ms < 1000) return -1;
    service_info_t *service = &g_services[g_service_count];
    service->id = g_service_count;
    copy_name(service->name, name);
    int i = 0;
    while (path[i] && i + 1 < SERVICE_PATH_MAX) { service->path[i] = path[i]; i++; }
    if (path[i]) return -1;
    service->path[i] = 0;
    service->dependency = dependency;
    service->start = 0;
    service->state = SERVICE_STOPPED;
    service->start_count = 0;
    service->restart_limit = 4;
    service->restart_count = 0;
    service->process_id = -1;
    service->userspace = 1;
    service->heartbeat_timeout_ms = heartbeat_timeout_ms;
    service->last_heartbeat_ms = 0;
    service->restart_at_ms = 0;
    return g_service_count++;
}

int service_load_manifest(const char *path) {
    char data[4096];
    int bytes = path ? vfs_read(path, data, sizeof(data) - 1) : -1;
    if (bytes <= 0 || bytes >= (int)sizeof(data)) return -1;
    data[bytes] = 0;
    int loaded = 0;
    for (int pos = 0; pos < bytes;) {
        char *fields[5] = {0};
        int field_count = 0;
        while (pos < bytes && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\r' || data[pos] == '\n')) pos++;
        if (pos >= bytes) break;
        if (data[pos] == '#') { while (pos < bytes && data[pos] != '\n') pos++; continue; }
        while (pos < bytes && data[pos] != '\n' && field_count < 5) {
            fields[field_count++] = &data[pos];
            while (pos < bytes && data[pos] != ' ' && data[pos] != '\t' && data[pos] != '\r' && data[pos] != '\n') pos++;
            if (pos < bytes && data[pos] != '\n') data[pos++] = 0;
            while (pos < bytes && (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\r')) pos++;
        }
        if (field_count == 5 && pos < bytes && data[pos] != '\n') {
            while (pos < bytes && data[pos] != '\n') pos++;
            field_count = 0;
        }
        if (pos < bytes && data[pos] == '\n') data[pos++] = 0;
        if (field_count != 5) { KLOG_ERROR("service", "invalid manifest record"); continue; }
        int no_dependency = fields[2][0] == '-' && fields[2][1] == '1' && !fields[2][2];
        int dependency = no_dependency ? SERVICE_NO_DEPENDENCY : service_find(fields[2]);
        int timeout_ok = 0, limit_ok = 0;
        uint64_t timeout = parse_u64(fields[3], &timeout_ok);
        uint64_t limit = parse_u64(fields[4], &limit_ok);
        if ((!no_dependency && dependency < 0) || !timeout_ok || !limit_ok || limit > 16) {
            KLOG_ERROR("service", "invalid manifest service=%s", fields[0]);
            continue;
        }
        int id = service_register_userspace(fields[0], dependency, fields[1], timeout);
        if (id < 0 || service_set_restart_limit(id, (uint32_t)limit) != 0) {
            KLOG_ERROR("service", "registration failed service=%s", fields[0]);
            continue;
        }
        loaded++;
    }
    return loaded;
}

static int service_start_one(int id, uint32_t visiting) {
    if (id < 0 || id >= g_service_count) return -1;
    service_info_t *service = &g_services[id];
    if (service->state == SERVICE_READY) return 0;
    if (visiting & (1U << id)) { service->state = SERVICE_FAILED; return -1; }
    service->state = SERVICE_STARTING;
    if (service->dependency != SERVICE_NO_DEPENDENCY &&
        service_start_one(service->dependency, visiting | (1U << id)) != 0) {
        service->state = SERVICE_FAILED;
        return -1;
    }
    int result;
    if (service->userspace) {
        process_t *process = process_spawn_path(service->path);
        service->process_id = process ? process->id : -1;
        service->last_heartbeat_ms = timer_millis();
        result = process ? 0 : -1;
    } else result = service->start();
    service->start_count++;
    service->state = result == 0 ? SERVICE_READY : SERVICE_FAILED;
    if (result != 0) KLOG_ERROR("service", "startup failed service=%s id=%d result=%d", service->name, id, result);
    return result;
}

int service_start_all(void) {
    int failures = 0;
    for (int i = 0; i < g_service_count; ++i) {
        if (service_start_one(i, 0) == 0) continue;
        service_info_t *service = &g_services[i];
        while (service->state == SERVICE_FAILED && service->restart_count < service->restart_limit) {
            service->restart_count++;
            KLOG_WARN("service", "retrying service=%s attempt=%u/%u", service->name,
                      service->restart_count, service->restart_limit);
            service->state = SERVICE_STOPPED;
            if (service_start_one(i, 0) == 0) break;
        }
        if (service->state != SERVICE_READY) failures++;
    }
    return failures ? -failures : 0;
}

int service_restart(int id) {
    if (id < 0 || id >= g_service_count) return -1;
    g_services[id].state = SERVICE_STOPPED;
    g_services[id].restart_count = 0;
    return service_start_one(id, 0);
}

int service_heartbeat(int process_id) {
    for (int i = 0; i < g_service_count; ++i) {
        service_info_t *service = &g_services[i];
        if (!service->userspace || service->process_id != process_id || service->state != SERVICE_READY) continue;
        service->last_heartbeat_ms = timer_millis();
        return 0;
    }
    return -1;
}

void service_poll(void) {
    static uint64_t next_poll_ms;
    uint64_t now = timer_millis();
    if (now < next_poll_ms) return;
    next_poll_ms = now + 250;
    for (int i = 0; i < g_service_count; ++i) {
        service_info_t *service = &g_services[i];
        if (service->dependency != SERVICE_NO_DEPENDENCY && g_services[service->dependency].state != SERVICE_READY) {
            if (service->userspace && process_exists(service->process_id)) (void)process_kill(service->process_id);
            service->state = SERVICE_FAILED;
            service->restart_at_ms = now + 250;
            continue;
        }
        if (!service->userspace) continue;
        if (service->state == SERVICE_READY &&
            (!process_exists(service->process_id) || now - service->last_heartbeat_ms > service->heartbeat_timeout_ms)) {
            if (process_exists(service->process_id)) (void)process_kill(service->process_id);
            service->state = SERVICE_FAILED;
            uint64_t backoff = 250ULL << (service->restart_count < 5 ? service->restart_count : 5);
            service->restart_at_ms = now + backoff;
            KLOG_WARN("service", "userspace service failed name=%s pid=%d restart-in=%llu ms",
                      service->name, service->process_id, (unsigned long long)backoff);
        }
        if (service->state == SERVICE_FAILED && service->restart_count < service->restart_limit && now >= service->restart_at_ms) {
            service->restart_count++;
            service->state = SERVICE_STOPPED;
            (void)service_start_one(i, 0);
        }
    }
}

int service_set_restart_limit(int id, uint32_t limit) {
    if (id < 0 || id >= g_service_count || limit > 16) return -1;
    g_services[id].restart_limit = limit;
    return 0;
}

const service_info_t *service_get(int id) {
    return id >= 0 && id < g_service_count ? &g_services[id] : 0;
}

int service_ready_count(void) {
    int count = 0;
    for (int i = 0; i < g_service_count; ++i) if (g_services[i].state == SERVICE_READY) count++;
    return count;
}
