#ifndef KESH_LINUX_SIGNAL_H
#define KESH_LINUX_SIGNAL_H

#include <stdint.h>

/* Linux x86_64 uses an 8-byte kernel sigset_t for rt_* signal syscalls. */
#define LINUX_NSIG 64
#define LINUX_SIGSET_BYTES 8U

#define LINUX_SIGHUP   1
#define LINUX_SIGINT   2
#define LINUX_SIGQUIT  3
#define LINUX_SIGILL   4
#define LINUX_SIGTRAP  5
#define LINUX_SIGABRT  6
#define LINUX_SIGBUS   7
#define LINUX_SIGFPE   8
#define LINUX_SIGKILL  9
#define LINUX_SIGUSR1 10
#define LINUX_SIGSEGV 11
#define LINUX_SIGUSR2 12
#define LINUX_SIGPIPE 13
#define LINUX_SIGALRM 14
#define LINUX_SIGTERM 15
#define LINUX_SIGCHLD 17
#define LINUX_SIGCONT 18
#define LINUX_SIGSTOP 19
#define LINUX_SIGTSTP 20
#define LINUX_SIGTTIN 21
#define LINUX_SIGTTOU 22
#define LINUX_SIGURG  23
#define LINUX_SIGWINCH 28

#define LINUX_SIG_BLOCK   0
#define LINUX_SIG_UNBLOCK 1
#define LINUX_SIG_SETMASK 2

typedef struct linux_kernel_sigaction {
    uint64_t handler;
    uint64_t flags;
    uint64_t restorer;
    uint64_t mask;
} linux_kernel_sigaction_t;

typedef struct linux_stack {
    uint64_t sp;
    int32_t flags;
    uint32_t _pad;
    uint64_t size;
} linux_stack_t;

/* Reset all Linux signal state for an internal KeshOS process slot. */
void linux_signal_reset_process(int internal_pid);

/* Linux-compatible signal state syscalls. Return Linux-style negative errno. */
int64_t linux_signal_rt_sigaction(int internal_pid, int signal,
                                  uint64_t user_action, uint64_t user_old_action,
                                  uint64_t sigset_size);
int64_t linux_signal_rt_sigprocmask(int internal_pid, int internal_tid, int how,
                                    uint64_t user_set, uint64_t user_old_set,
                                    uint64_t sigset_size);
int64_t linux_signal_rt_sigpending(int internal_pid, int internal_tid,
                                   uint64_t user_set, uint64_t sigset_size);
int64_t linux_signal_sigaltstack(int internal_pid, int internal_tid,
                                 uint64_t user_stack, uint64_t user_old_stack);

/* Linux PID/TID-facing delivery helpers used by linux_syscall.c. */
int64_t linux_signal_kill(int caller_internal_pid, int linux_pid, int signal);
int64_t linux_signal_tkill(int caller_internal_pid, int linux_tid, int signal);
int64_t linux_signal_tgkill(int caller_internal_pid, int linux_tgid,
                            int linux_tid, int signal);

/* Pending state consumed by the future return-to-userspace signal-frame path. */
uint64_t linux_signal_pending_mask(int internal_pid, int internal_tid);
int linux_signal_take_pending(int internal_pid, int internal_tid,
                              int *signal, linux_kernel_sigaction_t *action);

#endif
