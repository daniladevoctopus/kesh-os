#ifndef KESHOS_LOG_H
#define KESHOS_LOG_H

#include <stdint.h>

/* ReactOS-like verbosity ordering: L1 is most severe, L7 is most verbose. */
enum klog_level {
    KLOG_L1_FATAL = 1,
    KLOG_L2_ERROR = 2,
    KLOG_L3_WARN  = 3,
    KLOG_L4_NOTICE = 4,
    KLOG_L5_INFO  = 5,
    KLOG_L6_DEBUG = 6,
    KLOG_L7_TRACE = 7,
};

void klog_init(void);
void klog_set_level(int level);
int klog_get_level(void);
int klog_snapshot(char *out, int max_bytes);
int klog_snapshot_filtered(char *out, int max_bytes, int min_level, int max_level, const char *module);
int klog_persist(void);
int klog_load_persisted(void);
int klog_previous_snapshot(char *out, int max_bytes);
void klog_emit(int level, const char *module, const char *file, int line,
               const char *function, const char *fmt, ...);
void klog_enter(const char *module, const char *file, int line, const char *function);
void klog_leave(const char *module, const char *file, int line, const char *function, int64_t result);

#define KLOG(level, module, ...) \
    klog_emit((level), (module), __FILE__, __LINE__, __func__, __VA_ARGS__)
#define KLOG_FATAL(module, ...) KLOG(KLOG_L1_FATAL, module, __VA_ARGS__)
#define KLOG_ERROR(module, ...) KLOG(KLOG_L2_ERROR, module, __VA_ARGS__)
#define KLOG_WARN(module, ...)  KLOG(KLOG_L3_WARN, module, __VA_ARGS__)
#define KLOG_NOTICE(module, ...) KLOG(KLOG_L4_NOTICE, module, __VA_ARGS__)
#define KLOG_INFO(module, ...)  KLOG(KLOG_L5_INFO, module, __VA_ARGS__)
#define KLOG_DEBUG(module, ...) KLOG(KLOG_L6_DEBUG, module, __VA_ARGS__)
#define KLOG_TRACE(module, ...) KLOG(KLOG_L7_TRACE, module, __VA_ARGS__)
#define KLOG_ENTER(module) klog_enter(module, __FILE__, __LINE__, __func__)
#define KLOG_LEAVE(module, result) klog_leave(module, __FILE__, __LINE__, __func__, (result))

#endif
