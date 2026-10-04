#ifndef KPM_SYSTEM_UPDATE_H
#define KPM_SYSTEM_UPDATE_H

#include <stddef.h>

enum {
    KPM_UPDATE_OK = 0,
    KPM_UPDATE_FORMAT = -1,
    KPM_UPDATE_UNTRUSTED = -2,
    KPM_UPDATE_DIGEST = -3,
    KPM_UPDATE_SIGNATURE = -4,
    KPM_UPDATE_STORAGE = -5
};

int kpm_system_update_stage(const void *bundle, size_t size);
int kpm_system_update_confirm(void);
int kpm_system_update_rollback(void);
const char *kpm_system_update_error(int status);

#endif
