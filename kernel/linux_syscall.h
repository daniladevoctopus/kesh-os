#ifndef KERNEL_LINUX_SYSCALL_H
#define KERNEL_LINUX_SYSCALL_H

#include <stdint.h>

/**
 * @brief Dispatcher for Linux x86_64 POSIX system calls (used by musl libc, Wayland, Qt, and KDE).
 */
int64_t linux_syscall_dispatcher(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

#endif /* KERNEL_LINUX_SYSCALL_H */
