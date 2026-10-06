#include "linux_signal.h"
#include "process.h"
#include "memory.h"

#include <stddef.h>
#include <stdint.h>

/* Linux errno values returned as negative syscall results. */
#define LINUX_EPERM   1
#define LINUX_ESRCH   3
#define LINUX_EFAULT 14
#define LINUX_EINVAL 22

#define LINUX_SIG_DFL 0ULL
#define LINUX_SIG_IGN 1ULL
#define LINUX_SS_ONSTACK 1
#define LINUX_SS_DISABLE 2

/*
 * Signal delivery is deliberately split in two layers:
 *   1. this file owns Linux ABI-visible disposition/mask/pending state;
 *   2. the return-to-userspace path will build the rt signal frame and
 *      implement rt_sigreturn.
 *
 * This makes rt_sigaction/rt_sigprocmask/kill real stateful syscalls now,
 * without pretending that user handlers can already execute safely.
 */
typedef struct linux_thread_signal_state {
    uint64_t blocked;
    uint64_t pending;
    linux_stack_t altstack;
} linux_thread_signal_state_t;

typedef struct linux_process_signal_state {
    linux_kernel_sigaction_t actions[LINUX_NSIG + 1];
    uint64_t pending;
    linux_thread_signal_state_t threads[MAX_THREADS_PER_PROCESS];
} linux_process_signal_state_t;

static linux_process_signal_state_t g_linux_signals[MAX_PROCESSES];

static void zero_bytes(void *ptr, size_t size) {
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < size; ++i) p[i] = 0;
}

static int valid_process(int pid) {
    return pid >= 0 && pid < MAX_PROCESSES && process_exists(pid);
}

static int valid_thread_index(int tid) {
    return tid >= 0 && tid < MAX_THREADS_PER_PROCESS;
}

static uint64_t signal_bit(int signal) {
    if (signal <= 0 || signal > LINUX_NSIG) return 0;
    return 1ULL << (signal - 1);
}

static uint64_t unblockable_mask(void) {
    return signal_bit(LINUX_SIGKILL) | signal_bit(LINUX_SIGSTOP);
}

static int credentials_allow(int caller_pid, int target_pid) {
    if (caller_pid < 0) return 1;
    if (!valid_process(caller_pid) || !valid_process(target_pid)) return 0;
    credentials_t caller;
    credentials_t target;
    if (process_get_credentials(caller_pid, &caller) != 0 ||
        process_get_credentials(target_pid, &target) != 0)
        return 0;
    return caller.uid == KESH_UID_ROOT || caller.uid == target.uid;
}

static int default_ignored(int signal) {
    return signal == LINUX_SIGCHLD || signal == LINUX_SIGURG ||
           signal == LINUX_SIGWINCH;
}

static int default_stops(int signal) {
    return signal == LINUX_SIGSTOP || signal == LINUX_SIGTSTP ||
           signal == LINUX_SIGTTIN || signal == LINUX_SIGTTOU;
}

static int decode_linux_tid(int linux_tid, int *pid_out, int *tid_out) {
    if (!pid_out || !tid_out || linux_tid <= 0) return -1;

    if (linux_tid <= MAX_PROCESSES) {
        int pid = linux_tid - 1;
        if (!valid_process(pid)) return -1;
        *pid_out = pid;
        *tid_out = 0;
        return 0;
    }

    if (linux_tid < 1024) return -1;
    int encoded = linux_tid - 1024;
    int pid = encoded / MAX_THREADS_PER_PROCESS;
    int tid = encoded % MAX_THREADS_PER_PROCESS;
    if (!valid_process(pid) || tid <= 0 || !valid_thread_index(tid)) return -1;
    *pid_out = pid;
    *tid_out = tid;
    return 0;
}

static void clear_pending_signal(int pid, int signal) {
    uint64_t bit = signal_bit(signal);
    if (!bit || pid < 0 || pid >= MAX_PROCESSES) return;
    g_linux_signals[pid].pending &= ~bit;
    for (int tid = 0; tid < MAX_THREADS_PER_PROCESS; ++tid)
        g_linux_signals[pid].threads[tid].pending &= ~bit;
}

static int64_t queue_process_signal(int caller_pid, int target_pid, int signal) {
    if (!valid_process(target_pid)) return -LINUX_ESRCH;
    if (signal < 0 || signal > LINUX_NSIG) return -LINUX_EINVAL;
    if (!credentials_allow(caller_pid, target_pid)) return -LINUX_EPERM;
    if (signal == 0) return 0;

    linux_process_signal_state_t *state = &g_linux_signals[target_pid];
    linux_kernel_sigaction_t *action = &state->actions[signal];
    uint64_t bit = signal_bit(signal);

    /* SIGKILL and SIGSTOP are never maskable/catchable. */
    if (signal == LINUX_SIGKILL) return process_kill(target_pid) == 0 ? 0 : -LINUX_ESRCH;
    if (signal == LINUX_SIGSTOP)
        return process_send_signal(caller_pid, target_pid, KESH_SIGSTOP) == 0 ? 0 : -LINUX_ESRCH;

    /* SIGCONT resumes immediately; a caught handler can still be queued. */
    if (signal == LINUX_SIGCONT) {
        (void)process_send_signal(caller_pid, target_pid, KESH_SIGCONT);
        if (action->handler == LINUX_SIG_IGN || action->handler == LINUX_SIG_DFL) return 0;
        state->pending |= bit;
        return 0;
    }

    if (action->handler == LINUX_SIG_IGN) return 0;

    /* A blocked signal becomes pending even when its current disposition is default. */
    uint64_t blocked_any = 0;
    for (int tid = 0; tid < MAX_THREADS_PER_PROCESS; ++tid)
        blocked_any |= state->threads[tid].blocked;
    if (blocked_any & bit) {
        state->pending |= bit;
        return 0;
    }

    /* Caught signals wait for the future rt signal-frame delivery hook. */
    if (action->handler != LINUX_SIG_DFL) {
        state->pending |= bit;
        return 0;
    }

    if (default_ignored(signal)) return 0;
    if (default_stops(signal))
        return process_send_signal(caller_pid, target_pid, KESH_SIGSTOP) == 0 ? 0 : -LINUX_ESRCH;

    /* Remaining default actions are terminating in the current compatibility model. */
    return process_kill(target_pid) == 0 ? 0 : -LINUX_ESRCH;
}

static int64_t queue_thread_signal(int caller_pid, int target_pid, int target_tid, int signal) {
    if (!valid_process(target_pid) || !valid_thread_index(target_tid)) return -LINUX_ESRCH;
    if (signal < 0 || signal > LINUX_NSIG) return -LINUX_EINVAL;
    if (!credentials_allow(caller_pid, target_pid)) return -LINUX_EPERM;
    if (signal == 0) return 0;

    if (signal == LINUX_SIGKILL || signal == LINUX_SIGSTOP || signal == LINUX_SIGCONT)
        return queue_process_signal(caller_pid, target_pid, signal);

    linux_process_signal_state_t *state = &g_linux_signals[target_pid];
    linux_kernel_sigaction_t *action = &state->actions[signal];
    uint64_t bit = signal_bit(signal);

    if (action->handler == LINUX_SIG_IGN) return 0;
    if (state->threads[target_tid].blocked & bit || action->handler != LINUX_SIG_DFL) {
        state->threads[target_tid].pending |= bit;
        return 0;
    }
    if (default_ignored(signal)) return 0;
    if (default_stops(signal))
        return process_send_signal(caller_pid, target_pid, KESH_SIGSTOP) == 0 ? 0 : -LINUX_ESRCH;
    return process_kill(target_pid) == 0 ? 0 : -LINUX_ESRCH;
}

void linux_signal_reset_process(int internal_pid) {
    if (internal_pid < 0 || internal_pid >= MAX_PROCESSES) return;
    zero_bytes(&g_linux_signals[internal_pid], sizeof(g_linux_signals[internal_pid]));
    for (int tid = 0; tid < MAX_THREADS_PER_PROCESS; ++tid)
        g_linux_signals[internal_pid].threads[tid].altstack.flags = LINUX_SS_DISABLE;
}

int64_t linux_signal_rt_sigaction(int internal_pid, int signal,
                                  uint64_t user_action, uint64_t user_old_action,
                                  uint64_t sigset_size) {
    if (!valid_process(internal_pid)) return -LINUX_ESRCH;
    if (signal <= 0 || signal > LINUX_NSIG) return -LINUX_EINVAL;
    if (sigset_size != LINUX_SIGSET_BYTES) return -LINUX_EINVAL;

    linux_kernel_sigaction_t *slot = &g_linux_signals[internal_pid].actions[signal];
    if (user_old_action && copy_to_user((void *)user_old_action, slot, sizeof(*slot)) != 0)
        return -LINUX_EFAULT;

    if (user_action) {
        if (signal == LINUX_SIGKILL || signal == LINUX_SIGSTOP) return -LINUX_EINVAL;
        linux_kernel_sigaction_t next;
        if (copy_from_user(&next, (const void *)user_action, sizeof(next)) != 0)
            return -LINUX_EFAULT;
        next.mask &= ~unblockable_mask();
        *slot = next;
        if (next.handler == LINUX_SIG_IGN) clear_pending_signal(internal_pid, signal);
    }
    return 0;
}

int64_t linux_signal_rt_sigprocmask(int internal_pid, int internal_tid, int how,
                                    uint64_t user_set, uint64_t user_old_set,
                                    uint64_t sigset_size) {
    if (!valid_process(internal_pid) || !valid_thread_index(internal_tid)) return -LINUX_ESRCH;
    if (sigset_size != LINUX_SIGSET_BYTES) return -LINUX_EINVAL;

    linux_thread_signal_state_t *thread = &g_linux_signals[internal_pid].threads[internal_tid];
    if (user_old_set && copy_to_user((void *)user_old_set, &thread->blocked, sizeof(thread->blocked)) != 0)
        return -LINUX_EFAULT;
    if (!user_set) return 0;

    uint64_t requested = 0;
    if (copy_from_user(&requested, (const void *)user_set, sizeof(requested)) != 0)
        return -LINUX_EFAULT;
    requested &= ~unblockable_mask();

    switch (how) {
        case LINUX_SIG_BLOCK:
            thread->blocked |= requested;
            break;
        case LINUX_SIG_UNBLOCK:
            thread->blocked &= ~requested;
            break;
        case LINUX_SIG_SETMASK:
            thread->blocked = requested;
            break;
        default:
            return -LINUX_EINVAL;
    }
    thread->blocked &= ~unblockable_mask();
    return 0;
}

int64_t linux_signal_rt_sigpending(int internal_pid, int internal_tid,
                                   uint64_t user_set, uint64_t sigset_size) {
    if (!valid_process(internal_pid) || !valid_thread_index(internal_tid)) return -LINUX_ESRCH;
    if (!user_set) return -LINUX_EFAULT;
    if (sigset_size != LINUX_SIGSET_BYTES) return -LINUX_EINVAL;

    linux_process_signal_state_t *state = &g_linux_signals[internal_pid];
    uint64_t pending = (state->pending | state->threads[internal_tid].pending) &
                       state->threads[internal_tid].blocked;
    return copy_to_user((void *)user_set, &pending, sizeof(pending)) == 0 ? 0 : -LINUX_EFAULT;
}

int64_t linux_signal_sigaltstack(int internal_pid, int internal_tid,
                                 uint64_t user_stack, uint64_t user_old_stack) {
    if (!valid_process(internal_pid) || !valid_thread_index(internal_tid)) return -LINUX_ESRCH;
    linux_stack_t *slot = &g_linux_signals[internal_pid].threads[internal_tid].altstack;

    if (user_old_stack && copy_to_user((void *)user_old_stack, slot, sizeof(*slot)) != 0)
        return -LINUX_EFAULT;
    if (!user_stack) return 0;

    linux_stack_t next;
    if (copy_from_user(&next, (const void *)user_stack, sizeof(next)) != 0)
        return -LINUX_EFAULT;
    if (next.flags & ~(LINUX_SS_DISABLE)) return -LINUX_EINVAL;
    if (next.flags & LINUX_SS_DISABLE) {
        zero_bytes(slot, sizeof(*slot));
        slot->flags = LINUX_SS_DISABLE;
        return 0;
    }
    if (!next.sp || !next.size) return -LINUX_EINVAL;
    next.flags = 0;
    *slot = next;
    return 0;
}

int64_t linux_signal_kill(int caller_internal_pid, int linux_pid, int signal) {
    if (signal < 0 || signal > LINUX_NSIG) return -LINUX_EINVAL;

    if (linux_pid > 0)
        return queue_process_signal(caller_internal_pid, linux_pid - 1, signal);

    int target_group;
    if (linux_pid == 0) {
        if (!valid_process(caller_internal_pid)) return -LINUX_ESRCH;
        target_group = process_get_group(caller_internal_pid);
        if (target_group < 0) return -LINUX_ESRCH;
    } else if (linux_pid < -1) {
        target_group = (-linux_pid) - 1;
    } else {
        target_group = -1;
    }

    int delivered = 0;
    int permitted = 0;
    for (int pid = 0; pid < MAX_PROCESSES; ++pid) {
        if (!valid_process(pid)) continue;
        if (target_group >= 0 && process_get_group(pid) != target_group) continue;
        if (target_group < 0 && pid == 0) continue; /* keep kernel/init-style slot out of kill(-1) */
        if (!credentials_allow(caller_internal_pid, pid)) continue;
        permitted = 1;
        int64_t result = queue_process_signal(caller_internal_pid, pid, signal);
        if (result == 0) delivered = 1;
    }
    if (delivered) return 0;
    return permitted ? -LINUX_ESRCH : -LINUX_EPERM;
}

int64_t linux_signal_tkill(int caller_internal_pid, int linux_tid, int signal) {
    int pid = -1;
    int tid = -1;
    if (decode_linux_tid(linux_tid, &pid, &tid) != 0) return -LINUX_ESRCH;
    return queue_thread_signal(caller_internal_pid, pid, tid, signal);
}

int64_t linux_signal_tgkill(int caller_internal_pid, int linux_tgid,
                            int linux_tid, int signal) {
    if (linux_tgid <= 0) return -LINUX_EINVAL;
    int pid = -1;
    int tid = -1;
    if (decode_linux_tid(linux_tid, &pid, &tid) != 0) return -LINUX_ESRCH;
    if (linux_tgid != pid + 1) return -LINUX_ESRCH;
    return queue_thread_signal(caller_internal_pid, pid, tid, signal);
}

uint64_t linux_signal_pending_mask(int internal_pid, int internal_tid) {
    if (!valid_process(internal_pid) || !valid_thread_index(internal_tid)) return 0;
    linux_process_signal_state_t *state = &g_linux_signals[internal_pid];
    uint64_t pending = state->pending | state->threads[internal_tid].pending;
    return pending & ~state->threads[internal_tid].blocked;
}

int linux_signal_take_pending(int internal_pid, int internal_tid,
                              int *signal, linux_kernel_sigaction_t *action) {
    if (!signal || !action || !valid_process(internal_pid) || !valid_thread_index(internal_tid)) return -1;
    linux_process_signal_state_t *state = &g_linux_signals[internal_pid];
    uint64_t available = linux_signal_pending_mask(internal_pid, internal_tid);
    if (!available) return 0;

    for (int sig = 1; sig <= LINUX_NSIG; ++sig) {
        uint64_t bit = signal_bit(sig);
        if (!(available & bit)) continue;
        if (state->threads[internal_tid].pending & bit)
            state->threads[internal_tid].pending &= ~bit;
        else
            state->pending &= ~bit;
        *signal = sig;
        *action = state->actions[sig];
        return 1;
    }
    return 0;
}
