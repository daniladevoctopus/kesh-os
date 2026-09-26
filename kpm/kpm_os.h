// kpm_os
#ifndef KPM_OS_H
#define KPM_OS_H

#include <stdint.h>
#include <stddef.h>

void kpm_cmd_exec(const char *args, void (*print_fn)(const char *line));

#endif
