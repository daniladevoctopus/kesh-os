#include "linux_syscall.h"
#include "linux_signal.h"
#include "unix_ipc.h"
#include "syscall.h"
#include "process.h"
#include "memory.h"
#include "timer.h"
#include "vfs.h"
#include "serial.h"
#include "log.h"
#include "fd.h"
#include "socket.h"
#include "uwindow.h"
#include "cpu.h"
#include "random.h"

#include <stddef.h>
#include <stdint.h>

/* Linux x86_64 Syscall Numbers */
#define LINUX_SYS_READ           0
#define LINUX_SYS_WRITE          1
#define LINUX_SYS_OPEN           2
#define LINUX_SYS_CLOSE          3
#define LINUX_SYS_STAT           4
#define LINUX_SYS_FSTAT          5
#define LINUX_SYS_READV          19
#define LINUX_SYS_POLL           7
#define LINUX_SYS_LSEEK          8
#define LINUX_SYS_MMAP           9
#define LINUX_SYS_MPROTECT       10
#define LINUX_SYS_MUNMAP         11
#define LINUX_SYS_BRK            12
#define LINUX_SYS_RT_SIGACTION   13
#define LINUX_SYS_RT_SIGPROCMASK 14
#define LINUX_SYS_RT_SIGRETURN   15
#define LINUX_SYS_IOCTL          16
#define LINUX_SYS_WRITEV         20
#define LINUX_SYS_ACCESS         21
#define LINUX_SYS_PIPE           22
#define LINUX_SYS_MREMAP         25
#define LINUX_SYS_SCHED_YIELD    24
#define LINUX_SYS_MADVISE        28
#define LINUX_SYS_DUP            32
#define LINUX_SYS_DUP2           33
#define LINUX_SYS_NANOSLEEP      35
#define LINUX_SYS_GETPID         39
#define LINUX_SYS_SOCKET         41
#define LINUX_SYS_CONNECT        42
#define LINUX_SYS_ACCEPT         43
#define LINUX_SYS_SENDTO         44
#define LINUX_SYS_RECVFROM       45
#define LINUX_SYS_SENDMSG        46
#define LINUX_SYS_RECVMSG        47
#define LINUX_SYS_BIND           49
#define LINUX_SYS_LISTEN         50
#define LINUX_SYS_GETSOCKOPT     55
#define LINUX_SYS_SOCKETPAIR     53
#define LINUX_SYS_CLONE          56
#define LINUX_SYS_FORK           57
#define LINUX_SYS_EXECVE         59
#define LINUX_SYS_EXIT           60
#define LINUX_SYS_WAIT4          61
#define LINUX_SYS_KILL           62
#define LINUX_SYS_UNAME          63
#define LINUX_SYS_GETRLIMIT      97
#define LINUX_SYS_FCNTL          72
#define LINUX_SYS_FTRUNCATE      77
#define LINUX_SYS_GETCWD         79
#define LINUX_SYS_CHDIR          80
#define LINUX_SYS_READLINK       89
#define LINUX_SYS_GETUID         102
#define LINUX_SYS_GETGID         104
#define LINUX_SYS_GETEUID        107
#define LINUX_SYS_GETEGID        108
#define LINUX_SYS_RT_SIGPENDING  127
#define LINUX_SYS_SIGALTSTACK    131
#define LINUX_SYS_PRCTL          157
#define LINUX_SYS_ARCH_PRCTL     158
#define LINUX_SYS_GETTID         186
#define LINUX_SYS_TKILL          200
#define LINUX_SYS_FUTEX          202
#define LINUX_SYS_SCHED_SETAFFINITY 203
#define LINUX_SYS_SCHED_GETAFFINITY 204
#define LINUX_SYS_EPOLL_CREATE   213
#define LINUX_SYS_GETDENTS64     217
#define LINUX_SYS_SET_TID_ADDR   218
#define LINUX_SYS_CLOCK_GETTIME  228
#define LINUX_SYS_CLOCK_NANOSLEEP 230
#define LINUX_SYS_EXIT_GROUP     231
#define LINUX_SYS_EPOLL_WAIT     232
#define LINUX_SYS_EPOLL_CTL      233
#define LINUX_SYS_TGKILL         234
#define LINUX_SYS_OPENAT         257
#define LINUX_SYS_NEWFSTATAT     262
#define LINUX_SYS_PSELECT6       270
#define LINUX_SYS_PPOLL          271
#define LINUX_SYS_SET_ROBUST_LIST 273
#define LINUX_SYS_GET_ROBUST_LIST 274
#define LINUX_SYS_EPOLL_PWAIT    281
#define LINUX_SYS_ACCEPT4        288
#define LINUX_SYS_EVENTFD2       290
#define LINUX_SYS_EPOLL_CREATE1  291
#define LINUX_SYS_PIPE2          293
#define LINUX_SYS_PRLIMIT64      302
#define LINUX_SYS_GETCPU         309
#define LINUX_SYS_GETRANDOM      318
#define LINUX_SYS_MEMFD_CREATE   319
#define LINUX_SYS_MEMBARRIER     324
#define LINUX_SYS_STATX          332
#define LINUX_SYS_RSEQ           334
#define LINUX_SYS_EPOLL_PWAIT2   441

/* KeshOS Linux Extension Syscalls (500-503) */
#define LINUX_SYS_KESH_CREATE_WINDOW 500
#define LINUX_SYS_KESH_UPDATE_WINDOW 501
#define LINUX_SYS_KESH_POLL_EVENT    502
#define LINUX_SYS_KESH_SCREEN_INFO   503
#define LINUX_SYS_KESH_SPAWN         504
#define LINUX_SYS_KESH_POWER         505

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004
#define MSR_FS_BASE 0xC0000100
#define MSR_GS_BASE 0xC0000101

/* prctl(2) options needed by Chromium base on Linux-like userspace. */
#define LINUX_PR_GET_DUMPABLE 3
#define LINUX_PR_SET_DUMPABLE 4
#define LINUX_PR_SET_NAME     15
#define LINUX_PR_GET_NAME     16

static inline uint64_t linux_rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void linux_wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

/* Standard Linux error codes (negative return values) */
#define L_EPERM   1
#define L_ENOENT  2
#define L_ESRCH   3
#define L_EINTR   4
#define L_EIO     5
#define L_EBADF   9
#define L_ECHILD  10
#define L_EAGAIN  11
#define L_ENOMEM  12
#define L_EACCES  13
#define L_EFAULT  14
#define L_EEXIST  17
#define L_ENODEV  19
#define L_ENOTDIR 20
#define L_EISDIR  21
#define L_EINVAL  22
#define L_ENOTTY  25
#define L_EMFILE  24
#define L_ENOSYS  38
#define L_ENOPROTOOPT 92
#define L_EAFNOSUPPORT 97
#define L_EADDRINUSE   98
#define L_ETIMEDOUT    110
#define L_ENOTCONN     107
#define L_ECONNREFUSED 111

struct linux_iovec {
    uint64_t iov_base;
    uint64_t iov_len;
};

struct linux_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct linux_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    struct linux_timespec st_atim;
    struct linux_timespec st_mtim;
    struct linux_timespec st_ctim;
    int64_t  __unused[3];
};

struct linux_statx_timestamp {
    int64_t tv_sec;
    uint32_t tv_nsec;
    uint32_t pad;
};

struct linux_statx {
    uint32_t stx_mask;
    uint32_t stx_blksize;
    uint64_t stx_attributes;
    uint32_t stx_nlink;
    uint32_t stx_uid;
    uint32_t stx_gid;
    uint16_t stx_mode;
    uint16_t pad0;
    uint64_t stx_ino;
    uint64_t stx_size;
    uint64_t stx_blocks;
    uint64_t stx_attributes_mask;
    struct linux_statx_timestamp stx_atime;
    struct linux_statx_timestamp stx_btime;
    struct linux_statx_timestamp stx_ctime;
    struct linux_statx_timestamp stx_mtime;
    uint32_t stx_rdev_major;
    uint32_t stx_rdev_minor;
    uint32_t stx_dev_major;
    uint32_t stx_dev_minor;
    uint64_t pad1[14];
};

struct linux_rlimit {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

struct linux_pollfd {
    int fd;
    int16_t events;
    int16_t revents;
};

struct linux_dirent64_header {
    uint64_t ino;
    int64_t offset;
    uint16_t record_length;
    uint8_t type;
} __attribute__((packed));

struct kesh_screen_info {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
};

struct linux_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

extern volatile uint8_t g_syscall_should_yield;
extern void process_return_to_kernel(int code) __attribute__((noreturn));
extern uint32_t g_screen_w, g_screen_h, g_screen_pitch, g_screen_bpp;

/* Bounded bring-up trace: enough to diagnose Qt loader synchronization without
 * flooding the serial port if libc retries a futex in a tight loop. */
static uint32_t g_linux_sync_trace_count = 0;

static int linux_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static int64_t linux_current_pid(void) {
    int pid = process_current_id();
    return pid < 0 ? -L_ESRCH : (int64_t)pid + 1;
}

static int64_t linux_current_tid(void) {
    int pid = process_current_id();
    int tid = process_current_thread_id();
    if (pid < 0 || tid < 0) return -L_ESRCH;
    return tid == 0 ? (int64_t)pid + 1
                    : 1024 + (int64_t)pid * MAX_THREADS_PER_PROCESS + tid;
}

static int linux_copy_in(void *kernel_dst, uint64_t user_src, size_t size) {
    if (!kernel_dst || !user_src || !size) return -1;
    return copy_from_user(kernel_dst, (const void *)user_src, size);
}

static int linux_copy_out(uint64_t user_dst, const void *kernel_src, size_t size) {
    if (!user_dst || !kernel_src || !size) return -1;
    return copy_to_user((void *)user_dst, kernel_src, size);
}

static int linux_copy_string(char *dst, size_t max_len, uint64_t user_src) {
    if (!dst || !user_src || max_len == 0) return -1;
    for (size_t i = 0; i < max_len; ++i) {
        char ch;
        if (copy_from_user(&ch, (const void *)(user_src + i), 1) != 0) return -1;
        dst[i] = ch;
        if (ch == '\0') return 0;
    }
    dst[max_len - 1] = '\0';
    return 0;
}

static int linux_stat_path(const char *path, struct linux_stat *st) {
    if (!path || !st) return -1;
    for (size_t i = 0; i < sizeof(*st); ++i) ((uint8_t *)st)[i] = 0;
    if (linux_strcmp(path, "/dev/null") == 0 || linux_strcmp(path, "/dev/zero") == 0 ||
        linux_strcmp(path, "/dev/urandom") == 0 || linux_strcmp(path, "/dev/random") == 0 ||
        linux_strcmp(path, "/dev/fb0") == 0 || linux_strcmp(path, "/dev/dri/card0") == 0 ||
        linux_strcmp(path, "/dev/input/event0") == 0 || linux_strcmp(path, "/dev/input/event1") == 0) {
        st->st_mode = 0020666;
        st->st_blksize = 4096;
        return 0;
    }
    kesh_vfs_stat_t vst;
    if (vfs_stat(path, &vst) != 0) return -1;
    st->st_mode = vst.is_dir ? 0040555 : 0100444;
    st->st_size = vst.size;
    st->st_blksize = 4096;
    st->st_blocks = (vst.size + 511U) / 512U;
    return 0;
}

static int linux_stat_fd(int fd, struct linux_stat *st) {
    if (!st || fd < 0) return -1;
    for (size_t i = 0; i < sizeof(*st); ++i) ((uint8_t *)st)[i] = 0;
    int kind = -1;
    void *custom = fd_get_custom(process_current_id(), fd, &kind);
    if (custom && kind == FD_KIND_UNIX_SOCKET) {
        st->st_mode = 0140666;
    } else if (custom && kind == FD_KIND_MEMFD) {
        uint64_t size = unix_memfd_get_size(custom);
        st->st_mode = 0100666;
        st->st_size = (int64_t)size;
        st->st_blocks = (int64_t)((size + 511U) / 512U);
    } else if (custom && (kind == FD_KIND_EVDEV || kind == FD_KIND_DRM_FB || kind == FD_KIND_EVENTFD)) {
        st->st_mode = 0020666;
    } else if (fd <= 2) {
        st->st_mode = 0020666;
    } else if (kind == FD_KIND_FILE || kind == FD_KIND_DIRECTORY) {
        uint64_t size = 0;
        uint8_t is_dir = 0;
        if (fd_stat(process_current_id(), fd, &size, &is_dir) != 0) return -1;
        st->st_mode = is_dir ? 0040555 : 0100444;
        st->st_size = (int64_t)size;
        st->st_blocks = (int64_t)((size + 511U) / 512U);
    } else if (kind >= 0) {
        st->st_mode = 0100666;
    } else {
        return -1;
    }
    st->st_nlink = 1;
    st->st_blksize = 4096;
    return 0;
}

static void linux_stat_to_statx(const struct linux_stat *source, struct linux_statx *target) {
    for (size_t i = 0; i < sizeof(*target); ++i) ((uint8_t *)target)[i] = 0;
    target->stx_mask = 0x7FFU;
    target->stx_blksize = source->st_blksize ? (uint32_t)source->st_blksize : 4096U;
    target->stx_nlink = source->st_nlink ? (uint32_t)source->st_nlink : 1U;
    target->stx_uid = source->st_uid;
    target->stx_gid = source->st_gid;
    target->stx_mode = (uint16_t)source->st_mode;
    target->stx_ino = source->st_ino;
    target->stx_size = (uint64_t)source->st_size;
    target->stx_blocks = (uint64_t)source->st_blocks;
}

static int linux_get_rlimit(int resource, struct linux_rlimit *limit) {
    if (!limit || resource < 0 || resource >= 16) return -1;
    limit->rlim_cur = ~0ULL;
    limit->rlim_max = ~0ULL;
    if (resource == 3) limit->rlim_cur = 8ULL * 1024ULL * 1024ULL;
    if (resource == 7) limit->rlim_cur = limit->rlim_max = FD_MAX_PER_PROCESS;
    return 0;
}

int64_t linux_syscall_dispatcher(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    process_set_current_linux(1);

    switch (num) {
        /* write(fd, buf, count) */
        case LINUX_SYS_WRITE: {
            int fd = (int)a1;
            uint64_t u_buf = a2;
            size_t count = (size_t)a3;
            if (count == 0) return 0;
            if (!u_buf) return -L_EFAULT;

            if (fd == 1 || fd == 2) {
                /* stdout / stderr: print to serial console */
                char kbuf[512];
                size_t left = count;
                size_t offset = 0;
                while (left > 0) {
                    size_t chunk = left > (sizeof(kbuf) - 1) ? (sizeof(kbuf) - 1) : left;
                    if (linux_copy_in(kbuf, u_buf + offset, chunk) != 0) return -L_EFAULT;
                    kbuf[chunk] = '\0';
                    serial_print(kbuf);
                    offset += chunk;
                    left -= chunk;
                }
                return (int64_t)count;
            }

            /* Generic file descriptor */
            char kbuf[512];
            size_t chunk = count > sizeof(kbuf) ? sizeof(kbuf) : count;
            if (linux_copy_in(kbuf, u_buf, chunk) != 0) return -L_EFAULT;
            int r = fd_write(process_current_id(), fd, (const uint8_t *)kbuf, (uint32_t)chunk);
            return (r >= 0) ? r : -L_EBADF;
        }

        /* writev(fd, iov, iovcnt) */
        case LINUX_SYS_WRITEV: {
            int fd = (int)a1;
            uint64_t u_iov = a2;
            int iovcnt = (int)a3;
            if (iovcnt <= 0 || iovcnt > 64) return -L_EINVAL;
            if (!u_iov) return -L_EFAULT;

            int64_t total_written = 0;
            for (int i = 0; i < iovcnt; i++) {
                struct linux_iovec iov;
                if (linux_copy_in(&iov, u_iov + (uint64_t)i * sizeof(iov), sizeof(iov)) != 0) return -L_EFAULT;
                if (iov.iov_len == 0) continue;
                int64_t ret = linux_syscall_dispatcher(LINUX_SYS_WRITE, (uint64_t)fd, iov.iov_base, iov.iov_len, 0, 0, 0);
                if (ret < 0) return (total_written > 0) ? total_written : ret;
                total_written += ret;
            }
            return total_written;
        }

        case LINUX_SYS_READV: {
            int fd = (int)a1;
            int iovcnt = (int)a3;
            if (!a2) return -L_EFAULT;
            if (iovcnt < 0 || iovcnt > 64) return -L_EINVAL;
            int64_t total = 0;
            for (int i = 0; i < iovcnt; ++i) {
                struct linux_iovec iov;
                if (linux_copy_in(&iov, a2 + (uint64_t)i * sizeof(iov), sizeof(iov)) != 0)
                    return total ? total : -L_EFAULT;
                if (!iov.iov_len) continue;
                int64_t got = linux_syscall_dispatcher(LINUX_SYS_READ, (uint64_t)fd,
                                                       iov.iov_base, iov.iov_len, 0, 0, 0);
                if (got < 0) return total ? total : got;
                total += got;
                if ((uint64_t)got < iov.iov_len) break;
            }
            return total;
        }

        /* read(fd, buf, count) */
        case LINUX_SYS_READ: {
            int fd = (int)a1;
            uint64_t u_buf = a2;
            size_t count = (size_t)a3;
            if (count == 0) return 0;
            if (!u_buf) return -L_EFAULT;

            char kbuf[512];
            size_t chunk = count > sizeof(kbuf) ? sizeof(kbuf) : count;
            int r = fd_read(process_current_id(), fd, (uint8_t *)kbuf, (uint32_t)chunk);
            if (r > 0) {
                if (linux_copy_out(u_buf, kbuf, (size_t)r) != 0) return -L_EFAULT;
                return r;
            }
            if (r == 0) return 0;
            return (r < 0) ? r : -L_EBADF;
        }

        case LINUX_SYS_LSEEK: {
            int64_t result = fd_seek(process_current_id(), (int)a1, (int64_t)a2, (int)a3);
            return result >= 0 ? result : -L_EINVAL;
        }

        case LINUX_SYS_PIPE:
        case LINUX_SYS_PIPE2: {
            if (!a1) return -L_EFAULT;
            if (num == LINUX_SYS_PIPE2 && ((uint32_t)a2 & ~(04000U | 02000000U))) return -L_EINVAL;
            int descriptors[2];
            if (fd_pipe_create(process_current_id(), &descriptors[0], &descriptors[1]) != 0) return -L_EMFILE;
            if (linux_copy_out(a1, descriptors, sizeof(descriptors)) != 0) {
                (void)fd_close(process_current_id(), descriptors[0]);
                (void)fd_close(process_current_id(), descriptors[1]);
                return -L_EFAULT;
            }
            return 0;
        }

        case LINUX_SYS_DUP: {
            int descriptor = fd_dup(process_current_id(), (int)a1);
            return descriptor >= 0 ? descriptor : -L_EBADF;
        }

        case LINUX_SYS_DUP2: {
            int descriptor = fd_dup2(process_current_id(), (int)a1, (int)a2);
            return descriptor >= 0 ? descriptor : -L_EBADF;
        }

        case LINUX_SYS_NANOSLEEP: {
            struct linux_timespec requested;
            if (!a1 || linux_copy_in(&requested, a1, sizeof(requested)) != 0) return -L_EFAULT;
            if (requested.tv_sec < 0 || requested.tv_nsec < 0 || requested.tv_nsec >= 1000000000LL) return -L_EINVAL;
            uint64_t milliseconds = (uint64_t)requested.tv_sec * 1000ULL +
                                    ((uint64_t)requested.tv_nsec + 999999ULL) / 1000000ULL;
            if (milliseconds > 0xFFFFFFFFULL) milliseconds = 0xFFFFFFFFULL;
            if (milliseconds) process_sleep_current((uint32_t)milliseconds);
            if (a2) {
                struct linux_timespec remaining = {0, 0};
                if (linux_copy_out(a2, &remaining, sizeof(remaining)) != 0) return -L_EFAULT;
            }
            return 0;
        }

        case LINUX_SYS_CLOCK_NANOSLEEP: {
            struct linux_timespec requested;
            if (!a3 || linux_copy_in(&requested, a3, sizeof(requested)) != 0) return -L_EFAULT;
            if (requested.tv_sec < 0 || requested.tv_nsec < 0 || requested.tv_nsec >= 1000000000LL)
                return -L_EINVAL;
            uint64_t milliseconds;
            if (a2 & 1U) { /* TIMER_ABSTIME */
                uint64_t target = (uint64_t)requested.tv_sec * 1000ULL +
                                  (uint64_t)requested.tv_nsec / 1000000ULL;
                uint64_t now = timer_monotonic_ns() / 1000000ULL;
                milliseconds = target > now ? target - now : 0;
            } else {
                milliseconds = (uint64_t)requested.tv_sec * 1000ULL +
                               ((uint64_t)requested.tv_nsec + 999999ULL) / 1000000ULL;
            }
            if (milliseconds > 0xFFFFFFFFULL) milliseconds = 0xFFFFFFFFULL;
            if (milliseconds) process_sleep_current((uint32_t)milliseconds);
            if (a4) {
                struct linux_timespec remaining = {0, 0};
                if (linux_copy_out(a4, &remaining, sizeof(remaining)) != 0) return -L_EFAULT;
            }
            return 0;
        }

        /* close(fd) */
        case LINUX_SYS_CLOSE: {
            int fd = (int)a1;
            int r = fd_close(process_current_id(), fd);
            return (r == 0) ? 0 : -L_EBADF;
        }

        /* exit(code) */
        case LINUX_SYS_EXIT: {
            int tid = process_current_thread_id();
            if (tid > 0) {
                g_syscall_should_yield = 2;
                return 0;
            }
            /* Main thread falls through to exit_group */
        }
        case LINUX_SYS_EXIT_GROUP: {
            int code = (int)a1;
            KLOG_INFO("linux_sys", "process exit code=%d", code);
            g_syscall_should_yield = 2;
            g_syscall_should_yield = 0;
            process_return_to_kernel(code);
        }

        /* set_tid_address(tidptr) */
        case LINUX_SYS_SET_TID_ADDR: {
            if (a1) {
                process_set_current_clear_child_tid(a1);
            }
            return linux_current_tid();
        }

        case LINUX_SYS_SET_ROBUST_LIST:
            return a2 == 24 ? 0 : -L_EINVAL;

        case LINUX_SYS_GET_ROBUST_LIST: {
            if (!a2 || !a3) return -L_EFAULT;
            uint64_t head = 0;
            uint64_t length = 24;
            if (linux_copy_out(a2, &head, sizeof(head)) != 0 ||
                linux_copy_out(a3, &length, sizeof(length)) != 0) return -L_EFAULT;
            return 0;
        }

        /* prctl(option, arg2, arg3, arg4, arg5) */
        case LINUX_SYS_PRCTL: {
            switch ((int)a1) {
                case LINUX_PR_SET_NAME: {
                    if (!a2) return -L_EFAULT;
                    char name[16];
                    if (linux_copy_string(name, sizeof(name), a2) != 0) return -L_EFAULT;
                    return process_set_current_thread_name(name) == 0 ? 0 : -L_ESRCH;
                }
                case LINUX_PR_GET_NAME: {
                    if (!a2) return -L_EFAULT;
                    char name[16];
                    if (process_get_current_thread_name(name) != 0) return -L_ESRCH;
                    return linux_copy_out(a2, name, sizeof(name)) == 0 ? 0 : -L_EFAULT;
                }
                case LINUX_PR_GET_DUMPABLE: {
                    int dumpable = process_get_current_dumpable();
                    return dumpable >= 0 ? dumpable : -L_ESRCH;
                }
                case LINUX_PR_SET_DUMPABLE:
                    return process_set_current_dumpable((int)a2) == 0 ? 0 : -L_EINVAL;
                default:
                    return -L_EINVAL;
            }
        }

        case LINUX_SYS_ARCH_PRCTL: {
            int code = (int)a1;
            uint64_t addr = a2;
            if (code == ARCH_SET_FS) {
                linux_wrmsr(MSR_FS_BASE, addr);
                process_set_current_fs_base(addr);
                return 0;
            } else if (code == ARCH_GET_FS) {
                uint64_t fs_base = process_get_current_fs_base();
                if (linux_copy_out(addr, &fs_base, sizeof(fs_base)) != 0) return -L_EFAULT;
                return 0;
            }
            return -L_EINVAL;
        }

        /* ioctl(fd, req, arg) */
        case LINUX_SYS_IOCTL: {
            int fd = (int)a1;
            uint64_t req = a2;
            uint64_t arg = a3;

            int kind = -1;
            void *custom = fd_get_custom(process_current_id(), fd, &kind);
            if (custom && kind == FD_KIND_EVDEV) {
                extern int evdev_ioctl(void *custom_ptr, uint64_t req, uint64_t arg);
                int result = evdev_ioctl(custom, req, arg);
                if (result < 0) KLOG_WARN("linux_ioctl", "evdev fd=%d req=%llx result=%d", fd, (unsigned long long)req, result);
                return result;
            }
            if (custom && kind == FD_KIND_DRM_FB) {
                extern int drm_fb_ioctl(void *custom_ptr, uint64_t req, uint64_t arg);
                int result = drm_fb_ioctl(custom, req, arg);
                if (result < 0) KLOG_WARN("linux_ioctl", "drm fd=%d req=%llx result=%d", fd, (unsigned long long)req, result);
                return result;
            }

            /* For stdin/stdout/stderr, return ENOTTY (standard for non-controlling terminal) */
            return -L_ENOTTY;
        }

        /* getpid() */
        case LINUX_SYS_GETPID: {
            return linux_current_pid();
        }

        case LINUX_SYS_GETTID: {
            return linux_current_tid();
        }

        case LINUX_SYS_GETUID:
        case LINUX_SYS_GETEUID: {
            const credentials_t *credentials = process_current_credentials();
            return credentials ? credentials->uid : -L_ESRCH;
        }

        case LINUX_SYS_GETGID:
        case LINUX_SYS_GETEGID: {
            const credentials_t *credentials = process_current_credentials();
            return credentials ? credentials->gid : -L_ESRCH;
        }

        case LINUX_SYS_KILL:
            return linux_signal_kill(process_current_id(), (int)a1, (int)a2);

        case LINUX_SYS_TKILL:
            return linux_signal_tkill(process_current_id(), (int)a1, (int)a2);

        case LINUX_SYS_TGKILL:
            return linux_signal_tgkill(process_current_id(), (int)a1, (int)a2, (int)a3);

        /* The scheduler currently runs userspace on the BSP, but exposing a
         * truthful online mask lets Qt size its render/thread pools without
         * repeatedly probing ever larger cpu_set_t buffers. */
        case LINUX_SYS_SCHED_GETAFFINITY: {
            if (!a3) return -L_EFAULT;
            if (a2 < sizeof(uint64_t)) return -L_EINVAL;
            uint64_t mask = 0;
            uint32_t online = cpu_online_count();
            if (!online) online = 1;
            for (uint32_t cpu = 0; cpu < online && cpu < 64; ++cpu) mask |= 1ULL << cpu;
            if (linux_copy_out(a3, &mask, sizeof(mask)) != 0) return -L_EFAULT;
            return sizeof(mask); /* raw syscall returns bytes written */
        }

        case LINUX_SYS_SCHED_SETAFFINITY: {
            if (!a3) return -L_EFAULT;
            if (a2 < sizeof(uint64_t)) return -L_EINVAL;
            uint64_t requested = 0;
            if (linux_copy_in(&requested, a3, sizeof(requested)) != 0) return -L_EFAULT;
            uint64_t available = 0;
            uint32_t online = cpu_online_count();
            if (!online) online = 1;
            for (uint32_t cpu = 0; cpu < online && cpu < 64; ++cpu) available |= 1ULL << cpu;
            return (requested & available) ? 0 : -L_EINVAL;
        }

        /* musl x86_64 __clone arranges the raw syscall as:
         * clone(flags, child_stack, ptid, ctid, tls, fn). */
        case LINUX_SYS_CLONE: {
            uint64_t flags = a1;
            uint64_t child_stack = a2;
            uint64_t ptid_ptr = a3;
            uint64_t ctid_ptr = a4;
            uint64_t tls = a5;
            uint64_t fn = a6;

            KLOG_INFO("linux_sync", "clone flags=%llx stack=%llx ptid=%llx tls=%llx ctid=%llx fn=%llx",
                      (unsigned long long)flags, (unsigned long long)child_stack,
                      (unsigned long long)ptid_ptr, (unsigned long long)tls,
                      (unsigned long long)ctid_ptr, (unsigned long long)fn);

            if (!child_stack || !fn) return -L_EINVAL;

            extern uint64_t g_syscall_user_rip;
            int tidx = process_create_linux_thread(g_syscall_user_rip, child_stack, tls, fn);
            if (tidx < 0) return -L_EAGAIN;

            int pid = process_current_id();
            int32_t linux_tid = (int32_t)(1024 + (int64_t)pid * MAX_THREADS_PER_PROCESS + tidx);

            /* CLONE_PARENT_SETTID (0x00100000) */
            if ((flags & 0x00100000ULL) && ptid_ptr) {
                (void)linux_copy_out(ptid_ptr, &linux_tid, sizeof(linux_tid));
            }
            /* CLONE_CHILD_SETTID (0x01000000) */
            if ((flags & 0x01000000ULL) && ctid_ptr) {
                (void)linux_copy_out(ctid_ptr, &linux_tid, sizeof(linux_tid));
            }
            /* CLONE_CHILD_CLEARTID (0x00200000) */
            if (flags & 0x00200000ULL) {
                process_set_thread_clear_child_tid(tidx, ctid_ptr);
            }

            g_syscall_should_yield = 1;
            return linux_tid;
        }

        /* futex(uaddr, op, val, timeout, uaddr2, val3) */
        case LINUX_SYS_FUTEX: {
            uint64_t uaddr = a1;
            int op = (int)a2;
            uint32_t val = (uint32_t)a3;
            uint64_t u_timeout = a4;
            int cmd = op & ~(128 | 256);

            if (!uaddr) return -L_EFAULT;

            if (g_linux_sync_trace_count < 48) {
                uint32_t observed = 0;
                int readable = linux_copy_in(&observed, uaddr, sizeof(observed)) == 0;
                KLOG_INFO("linux_sync", "futex #%u tid=%lld cmd=%d op=%x addr=%llx val=%u observed=%u readable=%d timeout=%llx",
                          g_linux_sync_trace_count, (long long)linux_current_tid(), cmd,
                          (unsigned int)op, (unsigned long long)uaddr, val, observed,
                          readable, (unsigned long long)u_timeout);
                ++g_linux_sync_trace_count;
            }

            switch (cmd) {
                case 0: /* FUTEX_WAIT */
                case 9: /* FUTEX_WAIT_BITSET */ {
                    uint32_t cur = 0;
                    if (linux_copy_in(&cur, uaddr, sizeof(cur)) != 0) return -L_EFAULT;
                    if (cur != val) return -L_EAGAIN;

                    /* musl also uses the main thread's set_tid_address word as
                     * __thread_list_lock.  With no other live thread, a value
                     * naming any other owner is necessarily orphaned and no
                     * FUTEX_WAKE can ever arrive.  Recover only this narrowly
                     * identifiable startup case so pthread_create can reach
                     * clone; ordinary futex words are left untouched. */
                    if (process_current_thread_id() == 0 &&
                        !process_has_other_live_threads() &&
                        uaddr == process_get_current_clear_child_tid() &&
                        cur != 0 && cur != (uint32_t)linux_current_tid()) {
                        uint32_t unlocked = 0;
                        if (linux_copy_out(uaddr, &unlocked, sizeof(unlocked)) != 0)
                            return -L_EFAULT;
                        KLOG_WARN("linux_sync", "recovered orphaned musl thread-list lock addr=%llx owner=%u live_tid=%lld",
                                  (unsigned long long)uaddr, cur,
                                  (long long)linux_current_tid());
                        return -L_EAGAIN;
                    }

                    if (u_timeout) {
                        struct linux_timespec ts;
                        if (linux_copy_in(&ts, u_timeout, sizeof(ts)) == 0) {
                            uint64_t ms = (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
                            if (ms > 0) {
                                timer_wait_ms((uint32_t)(ms > 50 ? 50 : ms));
                            }
                            return -L_ETIMEDOUT;
                        }
                    }

                    g_syscall_should_yield = 1;
                    return 0;
                }
                case 1:  /* FUTEX_WAKE */
                case 10: /* FUTEX_WAKE_BITSET */ {
                    g_syscall_should_yield = 1;
                    return (val > 0) ? (int64_t)val : 1;
                }
                case 3:  /* FUTEX_REQUEUE */
                case 4:  /* FUTEX_CMP_REQUEUE */ {
                    g_syscall_should_yield = 1;
                    return 0;
                }
                default:
                    return -L_ENOSYS;
            }
        }

        /* sched_yield() */
        case LINUX_SYS_SCHED_YIELD: {
            g_syscall_should_yield = 1;
            return 0;
        }

        /* brk(addr) */
        case LINUX_SYS_BRK: {
            uint64_t addr = a1;
            return (int64_t)process_vm_brk(addr);
        }

        /* mmap(addr, len, prot, flags, fd, offset) */
        case LINUX_SYS_MMAP: {
            uint64_t requested_address = a1;
            size_t len = (size_t)a2;
            uint32_t prot = (uint32_t)a3;
            uint32_t flags = (uint32_t)a4;
            int fd = (int)a5;
            if (len == 0) return -L_EINVAL;

            uint32_t kprot = 0;
            if (prot & 1) kprot |= 1;
            if (prot & 2) kprot |= 2; /* WRITE */
            if (prot & 4) kprot |= 4; /* EXEC */

            if (fd >= 0) {
                int kind = -1;
                void *custom = fd_get_custom(process_current_id(), fd, &kind);
                if (custom && kind == FD_KIND_MEMFD) {
                    uint64_t base = unix_memfd_mmap(process_current_id(), fd, len, kprot, a6);
                    if (!base) return -L_ENOMEM;
                    return (int64_t)base;
                }
                if (custom && kind == FD_KIND_DRM_FB) {
                    extern uint64_t drm_fb_mmap(int pid, size_t len, uint32_t prot);
                    uint64_t base = drm_fb_mmap(process_current_id(), len, kprot);
                    if (!base) return -L_ENOMEM;
                    return (int64_t)base;
                }
                if (kind == FD_KIND_FILE) {
                    if (len > 8U * 1024U * 1024U) return -L_ENOMEM;
                    uint64_t base = process_vm_map(len, 2U);
                    if (!base) return -L_ENOMEM;
                    size_t pages = (len + PAGE_SIZE - 1ULL) / PAGE_SIZE;
                    uint64_t scratch_phys = pmm_alloc_pages(pages);
                    if (!scratch_phys) {
                        (void)process_vm_unmap(base, len);
                        return -L_ENOMEM;
                    }
                    void *scratch = (void *)(scratch_phys + g_hhdm_offset);
                    int bytes = fd_read_at(process_current_id(), fd, a6, scratch, (uint32_t)len);
                    if (bytes < 0 || (bytes > 0 && linux_copy_out(base, scratch, (size_t)bytes) != 0)) {
                        pmm_free_pages(scratch_phys, pages);
                        (void)process_vm_unmap(base, len);
                        return -L_EIO;
                    }
                    pmm_free_pages(scratch_phys, pages);
                    if (process_vm_protect(base, len, kprot) != 0) {
                        (void)process_vm_unmap(base, len);
                        return -L_EACCES;
                    }
                    return (int64_t)base;
                }
            }

            uint64_t base;
            if ((flags & 0x10U) && requested_address) /* MAP_FIXED */
                base = process_vm_map_fixed(requested_address, len, kprot);
            else
                base = process_vm_map(len, kprot);
            if (!base) return -L_ENOMEM;
            return (int64_t)base;
        }

        /* ftruncate(fd, length) */
        case LINUX_SYS_FTRUNCATE: {
            int fd = (int)a1;
            uint64_t length = a2;
            int r = unix_memfd_ftruncate(process_current_id(), fd, length);
            return (r == 0) ? 0 : -L_EINVAL;
        }

        /* memfd_create(name, flags) */
        case LINUX_SYS_MEMFD_CREATE: {
            uint64_t u_name = a1;
            uint32_t flags = (uint32_t)a2;
            char name[64] = "memfd";
            if (u_name) linux_copy_string(name, sizeof(name), u_name);
            int fd = unix_memfd_create(process_current_id(), name, flags);
            return (fd >= 0) ? (int64_t)fd : -L_EMFILE;
        }

        case LINUX_SYS_EVENTFD2: {
            int fd = unix_eventfd_create(process_current_id(), (uint32_t)a1, (int)a2);
            return fd >= 0 ? fd : -L_EMFILE;
        }

        /* socket(domain, type, protocol) */
        case LINUX_SYS_SOCKET: {
            int domain = (int)a1;
            int type = (int)a2;
            int protocol = (int)a3;
            if (domain == AF_UNIX) {
                int fd = unix_socket_create(process_current_id(), type, protocol);
                return (fd >= 0) ? fd : -L_EMFILE;
            }
            return -L_EAFNOSUPPORT;
        }

        /* bind(sockfd, addr, addrlen) */
        case LINUX_SYS_BIND: {
            int fd = (int)a1;
            uint64_t u_addr = a2;
            uint32_t addrlen = (uint32_t)a3;
            if (!u_addr || addrlen < 2) return -L_EINVAL;
            uint16_t family = 0;
            if (linux_copy_in(&family, u_addr, 2) != 0) return -L_EFAULT;
            if (family == AF_UNIX) {
                char path[108];
                if (linux_copy_string(path, sizeof(path), u_addr + 2) != 0) return -L_EFAULT;
                int r = unix_socket_bind(process_current_id(), fd, path);
                return (r == 0) ? 0 : -L_EADDRINUSE;
            }
            return -L_EAFNOSUPPORT;
        }

        /* listen(sockfd, backlog) */
        case LINUX_SYS_LISTEN: {
            int fd = (int)a1;
            int backlog = (int)a2;
            int r = unix_socket_listen(process_current_id(), fd, backlog);
            return (r == 0) ? 0 : -L_EINVAL;
        }

        /* connect(sockfd, addr, addrlen) */
        case LINUX_SYS_CONNECT: {
            int fd = (int)a1;
            uint64_t u_addr = a2;
            uint32_t addrlen = (uint32_t)a3;
            if (!u_addr || addrlen < 2) return -L_EINVAL;
            uint16_t family = 0;
            if (linux_copy_in(&family, u_addr, 2) != 0) return -L_EFAULT;
            if (family == AF_UNIX) {
                char path[108];
                if (linux_copy_string(path, sizeof(path), u_addr + 2) != 0) return -L_EFAULT;
                int r = unix_socket_connect(process_current_id(), fd, path);
                return (r == 0) ? 0 : -L_ECONNREFUSED;
            }
            return -L_EAFNOSUPPORT;
        }

        /* accept(sockfd, addr, addrlen) */
        case LINUX_SYS_ACCEPT: {
            int fd = (int)a1;
            int client_fd = unix_socket_accept(process_current_id(), fd, NULL, 0);
            return (client_fd >= 0) ? client_fd : -L_EAGAIN;
        }

        /* accept4(sockfd, addr, addrlen, flags) */
        case LINUX_SYS_ACCEPT4: {
            int fd = (int)a1;
            int flags = (int)a4;
            int nonblock = (flags & SOCK_NONBLOCK) ? 1 : 0;
            int client_fd = unix_socket_accept(process_current_id(), fd, NULL, nonblock);
            return (client_fd >= 0) ? client_fd : -L_EAGAIN;
        }

        /* socketpair(domain, type, protocol, sv) */
        case LINUX_SYS_SOCKETPAIR: {
            int domain = (int)a1;
            uint64_t u_sv = a4;
            if (domain != AF_UNIX || !u_sv) return -L_EINVAL;
            int sv[2];
            int r = unix_socket_pair(process_current_id(), sv);
            if (r != 0) return -L_EMFILE;
            if (linux_copy_out(u_sv, sv, sizeof(sv)) != 0) return -L_EFAULT;
            return 0;
        }

        /* sendmsg(sockfd, msg, flags) */
        case LINUX_SYS_SENDMSG: {
            int fd = (int)a1;
            uint64_t u_msg = a2;
            int flags = (int)a3;
            int64_t r = unix_socket_sendmsg(process_current_id(), fd, u_msg, flags);
            return (r >= 0) ? r : -L_EINVAL;
        }

        /* recvmsg(sockfd, msg, flags) */
        case LINUX_SYS_RECVMSG: {
            int fd = (int)a1;
            uint64_t u_msg = a2;
            int flags = (int)a3;
            int64_t r = unix_socket_recvmsg(process_current_id(), fd, u_msg, flags);
            if (r == -11) {
                g_syscall_should_yield = 1;
                return -L_EAGAIN;
            }
            if (r < 0) return (r == -14) ? -L_EFAULT : -L_EINVAL;
            return r;
        }

        /* getsockopt(fd, SOL_SOCKET, SO_PEERCRED, struct ucred, socklen_t *).
         * libwayland uses this immediately after accept4() to authenticate
         * every new client before creating its wl_client object. */
        case LINUX_SYS_GETSOCKOPT: {
            const int level = (int)a2;
            const int option = (int)a3;
            const uint64_t u_value = a4;
            const uint64_t u_length = a5;
            const int SOL_SOCKET_VALUE = 1;
            const int SO_PEERCRED_VALUE = 17;
            struct linux_ucred {
                int32_t pid;
                uint32_t uid;
                uint32_t gid;
            } credentials;
            uint32_t length = 0;

            if (level != SOL_SOCKET_VALUE || option != SO_PEERCRED_VALUE)
                return -L_ENOPROTOOPT;
            if (!u_value || !u_length || linux_copy_in(&length, u_length, sizeof(length)) != 0)
                return -L_EFAULT;
            if (length < sizeof(credentials)) return -L_EINVAL;
            if (unix_socket_get_peercred(process_current_id(), (int)a1,
                                         &credentials.pid, &credentials.uid,
                                         &credentials.gid) != 0)
                return -L_ENOTCONN;
            length = sizeof(credentials);
            if (linux_copy_out(u_value, &credentials, sizeof(credentials)) != 0 ||
                linux_copy_out(u_length, &length, sizeof(length)) != 0)
                return -L_EFAULT;
            KLOG_INFO("unix_ipc", "SO_PEERCRED fd=%d peer_pid=%d uid=%u gid=%u",
                      (int)a1, credentials.pid, credentials.uid, credentials.gid);
            return 0;
        }

        /* epoll_create / epoll_create1(flags) */
        case LINUX_SYS_EPOLL_CREATE:
        case LINUX_SYS_EPOLL_CREATE1: {
            int flags = (int)a1;
            int epfd = unix_epoll_create(process_current_id(), flags);
            return (epfd >= 0) ? epfd : -L_EMFILE;
        }

        /* epoll_ctl(epfd, op, fd, event) */
        case LINUX_SYS_EPOLL_CTL: {
            int epfd = (int)a1;
            int op = (int)a2;
            int target_fd = (int)a3;
            uint64_t u_event = a4;
            int r = unix_epoll_ctl(process_current_id(), epfd, op, target_fd, u_event);
            return (r == 0) ? 0 : -L_EINVAL;
        }

        /* epoll_wait(epfd, events, maxevents, timeout) */
        case LINUX_SYS_EPOLL_WAIT: {
            int epfd = (int)a1;
            uint64_t u_events = a2;
            int maxevents = (int)a3;
            int timeout = (int)a4;
            int r = unix_epoll_wait(process_current_id(), epfd, u_events, maxevents, timeout);
            if (r == -4) return -L_EINTR;
            if (r == -14) return -L_EFAULT;
            return (r >= 0) ? r : -L_EINVAL;
        }

        case LINUX_SYS_EPOLL_PWAIT: {
            int r = unix_epoll_wait(process_current_id(), (int)a1, a2, (int)a3, (int)a4);
            if (r == -4) return -L_EINTR;
            if (r == -14) return -L_EFAULT;
            return (r >= 0) ? r : -L_EINVAL;
        }

        case LINUX_SYS_EPOLL_PWAIT2: {
            int timeout = -1;
            if (a4) {
                struct linux_timespec ts;
                if (linux_copy_in(&ts, a4, sizeof(ts)) != 0) return -L_EFAULT;
                if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000LL) return -L_EINVAL;
                uint64_t ms = (uint64_t)ts.tv_sec * 1000ULL +
                              ((uint64_t)ts.tv_nsec + 999999ULL) / 1000000ULL;
                timeout = ms > 0x7FFFFFFFULL ? 0x7FFFFFFF : (int)ms;
            }
            int r = unix_epoll_wait(process_current_id(), (int)a1, a2, (int)a3, timeout);
            if (r == -4) return -L_EINTR;
            if (r == -14) return -L_EFAULT;
            return (r >= 0) ? r : -L_EINVAL;
        }

        /* munmap(addr, len) */
        case LINUX_SYS_MUNMAP: {
            uint64_t addr = a1;
            size_t len = (size_t)a2;
            if (!addr || !len) return -L_EINVAL;
            int r = process_vm_unmap(addr, len);
            return (r == 0) ? 0 : -L_EINVAL;
        }

        /* mprotect(addr, len, prot) */
        case LINUX_SYS_MPROTECT: {
            int r = process_vm_protect(a1, (size_t)a2, (uint32_t)a3);
            return r == 0 ? 0 : -L_EINVAL;
        }

        case LINUX_SYS_MREMAP: {
            uint64_t old_address = a1;
            uint64_t old_size = a2;
            uint64_t new_size = a3;
            uint32_t flags = (uint32_t)a4;
            if (!old_address || !old_size || !new_size || (flags & ~5U)) return -L_EINVAL;
            if (new_size <= old_size && !(flags & 4U)) return (int64_t)old_address;
            if (!(flags & 1U)) return -L_ENOMEM; /* MREMAP_MAYMOVE */
            uint64_t replacement = process_vm_map(new_size, 3U);
            if (!replacement) return -L_ENOMEM;
            uint8_t transfer[512];
            uint64_t copy_size = old_size < new_size ? old_size : new_size;
            for (uint64_t offset = 0; offset < copy_size; offset += sizeof(transfer)) {
                size_t chunk = copy_size - offset > sizeof(transfer) ? sizeof(transfer)
                                                                    : (size_t)(copy_size - offset);
                if (linux_copy_in(transfer, old_address + offset, chunk) != 0 ||
                    linux_copy_out(replacement + offset, transfer, chunk) != 0) {
                    (void)process_vm_unmap(replacement, new_size);
                    return -L_EFAULT;
                }
            }
            if (!(flags & 4U) && process_vm_unmap(old_address, old_size) != 0) {
                (void)process_vm_unmap(replacement, new_size);
                return -L_EINVAL;
            }
            return (int64_t)replacement;
        }

        /* madvise(addr, len, advice) */
        case LINUX_SYS_MADVISE: {
            return 0;
        }

        case LINUX_SYS_ACCESS: {
            char path[128];
            struct linux_stat st;
            if (linux_copy_string(path, sizeof(path), a1) != 0) return -L_EFAULT;
            return linux_stat_path(path, &st) == 0 ? 0 : -L_ENOENT;
        }

        case LINUX_SYS_READLINK: {
            char path[128];
            if (!a2 || a3 == 0 || linux_copy_string(path, sizeof(path), a1) != 0) return -L_EFAULT;
            if (linux_strcmp(path, "/proc/self/exe") != 0) return -L_EINVAL;
            const char *target = process_current_name();
            if (!target) return -L_ENOENT;
            size_t length = 0;
            while (target[length] && length < (size_t)a3) ++length;
            if (linux_copy_out(a2, target, length) != 0) return -L_EFAULT;
            return (int64_t)length;
        }

        case LINUX_SYS_GETCWD: {
            static const char root[] = "/";
            if (!a1 || a2 < sizeof(root)) return a1 ? -L_EINVAL : -L_EFAULT;
            if (linux_copy_out(a1, root, sizeof(root)) != 0) return -L_EFAULT;
            return (int64_t)a1;
        }

        case LINUX_SYS_CHDIR: {
            char path[256];
            kesh_vfs_stat_t stat;
            if (linux_copy_string(path, sizeof(path), a1) != 0) return -L_EFAULT;
            if (vfs_stat(path, &stat) != 0) return -L_ENOENT;
            return stat.is_dir ? 0 : -L_ENOTDIR;
        }

        case LINUX_SYS_GETDENTS64: {
            int fd = (int)a1;
            uint64_t user_buffer = a2;
            uint32_t capacity = (uint32_t)a3;
            uint32_t written = 0;
            uint64_t inode = 1;
            if (!user_buffer) return -L_EFAULT;
            while (written + 24U <= capacity) {
                fd_dirent_t entry;
                int result = fd_readdir(process_current_id(), fd, &entry);
                if (result < 0) return written ? (int64_t)written : -L_EBADF;
                if (result == 0) break;
                uint32_t name_length = 0;
                while (name_length < sizeof(entry.name) && entry.name[name_length]) name_length++;
                uint32_t record_length = (uint32_t)((sizeof(struct linux_dirent64_header) + name_length + 1U + 7U) & ~7U);
                if (record_length > capacity - written) return written ? (int64_t)written : -L_EINVAL;
                uint8_t record[96];
                for (uint32_t i = 0; i < record_length; ++i) record[i] = 0;
                struct linux_dirent64_header *header = (struct linux_dirent64_header *)record;
                header->ino = inode++;
                header->offset = (int64_t)inode;
                header->record_length = (uint16_t)record_length;
                header->type = entry.is_dir ? 4U : 8U;
                for (uint32_t i = 0; i < name_length; ++i) record[sizeof(*header) + i] = (uint8_t)entry.name[i];
                if (linux_copy_out(user_buffer + written, record, record_length) != 0) return -L_EFAULT;
                written += record_length;
            }
            return written;
        }

        /* clock_gettime(clk_id, tp) */
        case LINUX_SYS_CLOCK_GETTIME: {
            uint64_t u_tp = a2;
            if (!u_tp) return -L_EFAULT;
            uint64_t ms = timer_millis();
            struct linux_timespec ts;
            ts.tv_sec = (int64_t)(ms / 1000ULL);
            ts.tv_nsec = (int64_t)((ms % 1000ULL) * 1000000ULL);
            if (linux_copy_out(u_tp, &ts, sizeof(ts)) != 0) return -L_EFAULT;
            return 0;
        }

        case LINUX_SYS_GETCPU: {
            uint32_t cpu = 0;
            uint32_t node = 0;
            if (a1 && linux_copy_out(a1, &cpu, sizeof(cpu)) != 0) return -L_EFAULT;
            if (a2 && linux_copy_out(a2, &node, sizeof(node)) != 0) return -L_EFAULT;
            return 0;
        }

        case LINUX_SYS_GETRANDOM: {
            if (!a1 && a2) return -L_EFAULT;
            uint8_t bytes[256];
            uint64_t offset = 0;
            while (offset < a2) {
                size_t chunk = a2 - offset > sizeof(bytes) ? sizeof(bytes) : (size_t)(a2 - offset);
                if (random_bytes(bytes, chunk) != 0) return offset ? (int64_t)offset : -L_EIO;
                if (linux_copy_out(a1 + offset, bytes, chunk) != 0) return offset ? (int64_t)offset : -L_EFAULT;
                offset += chunk;
            }
            return (int64_t)offset;
        }

        case LINUX_SYS_MEMBARRIER:
            /* Query + private expedited registration/execution.  All current
             * userspace threads are coherently scheduled by one kernel. */
            if ((int)a1 == 0) return 8 | 16;
            if ((int)a1 == 8 || (int)a1 == 16) return 0;
            return -L_EINVAL;

        case LINUX_SYS_RSEQ:
            /* libc has a defined fallback when restartable sequences are not
             * provided.  Keep this explicit so it is not reported as an
             * accidental missing syscall in diagnostics. */
            return -L_ENOSYS;

        /* uname(buf) */
        case LINUX_SYS_UNAME: {
            uint64_t u_buf = a1;
            if (!u_buf) return -L_EFAULT;
            struct linux_utsname u;
            const char sys[] = "KeshOS";
            const char node[] = "keshos";
            const char rel[] = "1.0.0-musl";
            const char ver[] = "KeshOS (x86_64 SMP)";
            const char mach[] = "x86_64";
            const char dom[] = "local";

            for (int i = 0; i < 65; i++) {
                u.sysname[i] = (i < (int)sizeof(sys)) ? sys[i] : '\0';
                u.nodename[i] = (i < (int)sizeof(node)) ? node[i] : '\0';
                u.release[i] = (i < (int)sizeof(rel)) ? rel[i] : '\0';
                u.version[i] = (i < (int)sizeof(ver)) ? ver[i] : '\0';
                u.machine[i] = (i < (int)sizeof(mach)) ? mach[i] : '\0';
                u.domainname[i] = (i < (int)sizeof(dom)) ? dom[i] : '\0';
            }
            if (linux_copy_out(u_buf, &u, sizeof(u)) != 0) return -L_EFAULT;
            return 0;
        }

        case LINUX_SYS_GETRLIMIT: {
            struct linux_rlimit limit;
            if (!a2) return -L_EFAULT;
            if (linux_get_rlimit((int)a1, &limit) != 0) return -L_EINVAL;
            return linux_copy_out(a2, &limit, sizeof(limit)) == 0 ? 0 : -L_EFAULT;
        }

        case LINUX_SYS_PRLIMIT64: {
            int64_t pid = (int64_t)a1;
            if (pid != 0 && pid != linux_current_pid()) return -L_ESRCH;
            if (a3) return -L_EPERM;
            if (!a4) return 0;
            struct linux_rlimit limit;
            if (linux_get_rlimit((int)a2, &limit) != 0) return -L_EINVAL;
            return linux_copy_out(a4, &limit, sizeof(limit)) == 0 ? 0 : -L_EFAULT;
        }

        /* Stateful Linux signal ABI.  User-handler frame delivery and
         * rt_sigreturn are implemented separately in the return-to-user path. */
        case LINUX_SYS_RT_SIGACTION:
            return linux_signal_rt_sigaction(process_current_id(), (int)a1, a2, a3, a4);

        case LINUX_SYS_RT_SIGPROCMASK:
            return linux_signal_rt_sigprocmask(process_current_id(), process_current_thread_id(),
                                               (int)a1, a2, a3, a4);

        case LINUX_SYS_RT_SIGPENDING:
            return linux_signal_rt_sigpending(process_current_id(), process_current_thread_id(),
                                              a1, a2);

        case LINUX_SYS_SIGALTSTACK:
            return linux_signal_sigaltstack(process_current_id(), process_current_thread_id(),
                                            a1, a2);

        /* open / openat */
        case LINUX_SYS_OPEN:
        case LINUX_SYS_OPENAT: {
            uint64_t u_path = (num == LINUX_SYS_OPEN) ? a1 : a2;
            uint32_t flags = (num == LINUX_SYS_OPEN) ? (uint32_t)a2 : (uint32_t)a3;
            char path[128];
            if (linux_copy_string(path, sizeof(path), u_path) != 0) return -L_EFAULT;

            if (linux_strcmp(path, "/dev/null") == 0 || linux_strcmp(path, "/dev/zero") == 0 ||
                linux_strcmp(path, "/dev/urandom") == 0 || linux_strcmp(path, "/dev/random") == 0) {
                return 0; /* stdin/null fallback */
            }

            if (linux_strcmp(path, "/dev/input/event0") == 0) {
                extern int evdev_open(int dev_id, int pid);
                int fd = evdev_open(0, process_current_id());
                return (fd >= 0) ? (int64_t)fd : -L_ENODEV;
            }
            if (linux_strcmp(path, "/dev/input/event1") == 0 || linux_strcmp(path, "/dev/input/mice") == 0) {
                extern int evdev_open(int dev_id, int pid);
                int fd = evdev_open(1, process_current_id());
                return (fd >= 0) ? (int64_t)fd : -L_ENODEV;
            }
            if (linux_strcmp(path, "/dev/fb0") == 0) {
                extern int drm_fb_open_fb0(int pid);
                int fd = drm_fb_open_fb0(process_current_id());
                return (fd >= 0) ? (int64_t)fd : -L_ENODEV;
            }
            if (linux_strcmp(path, "/dev/dri/card0") == 0) {
                extern int drm_fb_open_card0(int pid);
                int fd = drm_fb_open_card0(process_current_id());
                return (fd >= 0) ? (int64_t)fd : -L_ENODEV;
            }

            uint32_t kflags = 0;
            uint32_t access = flags & 3;
            if (access == 0) kflags |= FD_OPEN_READ;
            else if (access == 1) kflags |= FD_OPEN_WRITE;
            else kflags |= (FD_OPEN_READ | FD_OPEN_WRITE);

            if (flags & 0100) kflags |= FD_OPEN_CREATE;
            if (flags & 01000) kflags |= FD_OPEN_TRUNC;
            if (flags & 02000) kflags |= FD_OPEN_APPEND;

            int fd = fd_open(process_current_id(), path, kflags);
            return (fd >= 0) ? (int64_t)fd : -L_ENOENT;
        }

        /* fstat(fd, statbuf) */
        case LINUX_SYS_FSTAT: {
            int fd = (int)a1;
            uint64_t u_stat = a2;
            if (!u_stat) return -L_EFAULT;
            struct linux_stat st;
            if (linux_stat_fd(fd, &st) != 0) return -L_EBADF;
            if (linux_copy_out(u_stat, &st, sizeof(st)) != 0) return -L_EFAULT;
            return 0;
        }

        case LINUX_SYS_STATX: {
            char path[256];
            struct linux_stat st;
            struct linux_statx stx;
            if (!a2 || !a5) return -L_EFAULT;
            if (linux_copy_string(path, sizeof(path), a2) != 0) return -L_EFAULT;
            int result;
            if (path[0] == '\0' && ((uint32_t)a3 & 0x1000U)) result = linux_stat_fd((int)a1, &st);
            else result = linux_stat_path(path, &st);
            if (result != 0) return -L_ENOENT;
            linux_stat_to_statx(&st, &stx);
            return linux_copy_out(a5, &stx, sizeof(stx)) == 0 ? 0 : -L_EFAULT;
        }

        /* stat(path, statbuf) */
        case LINUX_SYS_STAT: {
            uint64_t u_path = a1;
            uint64_t u_stat = a2;
            if (!u_path || !u_stat) return -L_EFAULT;
            char path[128];
            if (linux_copy_string(path, sizeof(path), u_path) != 0) return -L_EFAULT;

            struct linux_stat st;
            if (linux_stat_path(path, &st) != 0) return -L_ENOENT;
            if (linux_copy_out(u_stat, &st, sizeof(st)) != 0) return -L_EFAULT;
            return 0;
        }

        case LINUX_SYS_NEWFSTATAT: {
            uint64_t u_path = a2;
            uint64_t u_stat = a3;
            char path[128];
            struct linux_stat st;
            if (!u_path || !u_stat) return -L_EFAULT;
            if (linux_copy_string(path, sizeof(path), u_path) != 0) return -L_EFAULT;
            if (linux_stat_path(path, &st) != 0) return -L_ENOENT;
            if (linux_copy_out(u_stat, &st, sizeof(st)) != 0) return -L_EFAULT;
            return 0;
        }

        /* fcntl(fd, cmd, arg) */
        case LINUX_SYS_FCNTL: {
            int fd = (int)a1;
            int cmd = (int)a2;
            switch (cmd) {
                case 0:    /* F_DUPFD */
                case 1030: /* F_DUPFD_CLOEXEC */
                    {
                        int duplicated = fd_dup_min(process_current_id(), fd, (int)a3);
                        return duplicated >= 0 ? (int64_t)duplicated : -L_EINVAL;
                    }
                case 1: /* F_GETFD */
                    return 0;
                case 2: /* F_SETFD */
                    return 0;
                case 3: /* F_GETFL */
                    return 2; /* O_RDWR */
                case 4: /* F_SETFL */
                    return 0;
                case 1033: /* F_ADD_SEALS */
                    return 0;
                case 1034: /* F_GET_SEALS */
                    return 0;
                default:
                    return 0;
            }
        }

        /* poll(fds, nfds, timeout_ms) / ppoll(fds, nfds, timeout_ts, ...). */
        case LINUX_SYS_POLL:
        case LINUX_SYS_PPOLL: {
            uint64_t u_fds = a1;
            uint32_t nfds = (uint32_t)a2;
            int timeout = -1;
            if (num == LINUX_SYS_POLL) {
                timeout = (int)a3;
            } else if (a3) {
                struct linux_timespec ts;
                if (linux_copy_in(&ts, a3, sizeof(ts)) != 0) return -L_EFAULT;
                if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000LL)
                    return -L_EINVAL;
                uint64_t timeout_ms = (uint64_t)ts.tv_sec * 1000ULL +
                                      ((uint64_t)ts.tv_nsec + 999999ULL) / 1000000ULL;
                timeout = timeout_ms > 0x7FFFFFFFULL ? 0x7FFFFFFF : (int)timeout_ms;
            }
            if (nfds == 0) {
                if (timeout > 0) process_sleep_current((uint32_t)timeout);
                if (timeout != 0) g_syscall_should_yield = 1;
                return 0;
            }
            if (!u_fds || nfds > 64) return -L_EINVAL;

            struct linux_pollfd pfds[16];
            uint32_t count = nfds > 16 ? 16 : nfds;
            if (linux_copy_in(pfds, u_fds, count * sizeof(struct linux_pollfd)) != 0) return -L_EFAULT;

            int ready = 0;
            for (uint32_t i = 0; i < count; i++) {
                pfds[i].revents = 0;
                uint32_t requested = 0;
                if (pfds[i].events & 0x0003) requested |= FD_POLL_READ;
                if (pfds[i].events & 0x0004) requested |= FD_POLL_WRITE;
                int p = fd_poll(process_current_id(), pfds[i].fd, requested | FD_POLL_HANGUP);
                if (p > 0) {
                    if (p & FD_POLL_READ) pfds[i].revents |= 0x0001;
                    if (p & FD_POLL_WRITE) pfds[i].revents |= 0x0004;
                    if (p & FD_POLL_HANGUP) pfds[i].revents |= 0x0010;
                    ready++;
                }
            }
            if (ready == 0) {
                if (timeout == 0) {
                    if (linux_copy_out(u_fds, pfds, count * sizeof(struct linux_pollfd)) != 0) return -L_EFAULT;
                    return 0;
                }
                g_syscall_should_yield = 1;
                return -L_EINTR;
            }
            if (linux_copy_out(u_fds, pfds, count * sizeof(struct linux_pollfd)) != 0) return -L_EFAULT;
            return ready;
        }

        case LINUX_SYS_PSELECT6: {
            /* Full fd_set multiplexing is not yet exposed by the VFS.  The
             * timeout-only form is sufficient for libc/Qt delay paths. */
            if (a1 != 0 || a2 || a3 || a4) return -L_ENOSYS;
            if (a5) {
                struct linux_timespec ts;
                if (linux_copy_in(&ts, a5, sizeof(ts)) != 0) return -L_EFAULT;
                if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000LL) return -L_EINVAL;
                uint64_t ms = (uint64_t)ts.tv_sec * 1000ULL +
                              ((uint64_t)ts.tv_nsec + 999999ULL) / 1000000ULL;
                if (ms > 0xFFFFFFFFULL) ms = 0xFFFFFFFFULL;
                if (ms) process_sleep_current((uint32_t)ms);
            }
            return 0;
        }

        /* KeshOS GUI Syscalls for Linux Userspace */
        case LINUX_SYS_KESH_CREATE_WINDOW: {
            int w = (int)a1;
            int h = (int)a2;
            uint64_t u_title = a3;
            char title[64] = "Linux Window";
            if (u_title) linux_copy_string(title, sizeof(title), u_title);
            uint64_t user_cr3 = vmm_get_current_pml4();
            user_window_t *win = uwindow_create(w, h, title, user_cr3);
            if (!win) return 0;
            return (int64_t)win->user_fb_vaddr;
        }

        case LINUX_SYS_KESH_UPDATE_WINDOW: {
            uint64_t user_cr3 = vmm_get_current_pml4();
            user_window_t *win = uwindow_get_process_window(user_cr3);
            if (!win) return -1;
            win->dirty = 1;
            g_syscall_should_yield = 1;
            return 0;
        }

        case LINUX_SYS_KESH_POLL_EVENT: {
            uint64_t u_ev = a1;
            if (!u_ev) return -L_EFAULT;
            uint64_t user_cr3 = vmm_get_current_pml4();
            user_window_t *win = uwindow_get_process_window(user_cr3);
            if (!win) return -1;
            uevent_t ev;
            int has_ev = uwindow_pop_event(win->win_id, &ev);
            if (!has_ev) return 0;
            if (linux_copy_out(u_ev, &ev, sizeof(ev)) != 0) return -L_EFAULT;
            return 1;
        }

        case LINUX_SYS_KESH_SCREEN_INFO: {
            uint64_t u_info = a1;
            if (!u_info) return -L_EFAULT;
            struct kesh_screen_info si;
            si.width = g_screen_w;
            si.height = g_screen_h;
            si.pitch = g_screen_pitch;
            si.bpp = g_screen_bpp;
            if (linux_copy_out(u_info, &si, sizeof(si)) != 0) return -L_EFAULT;
            return 0;
        }

        case LINUX_SYS_KESH_SPAWN: {
            uint64_t u_path = a1;
            if (!u_path) return -L_EINVAL;
            char path[128] = {0};
            if (linux_copy_string(path, sizeof(path), u_path) <= 0) return -L_EFAULT;
            process_t *p = process_spawn_path(path);
            if (!p) {
                char full[160] = "/cdrom/boot/apps/";
                int len = 17;
                for (int i = 0; path[i] && len < 155; i++) full[len++] = path[i];
                full[len] = '\0';
                p = process_spawn_path(full);
            }
            if (!p) return -L_ENOENT;
            g_syscall_should_yield = 1;
            return (int64_t)p->id;
        }

        case LINUX_SYS_KESH_POWER: {
            uint64_t action = a1;
            if (action == 1) {
                extern int acpi_reboot(void);
                acpi_reboot();
            } else if (action == 2) {
                extern int acpi_poweroff(void);
                acpi_poweroff();
            }
            return 0;
        }

        default: {
            static uint8_t s_warned_syscalls[512] = {0};
            if (num < 512) {
                if (!s_warned_syscalls[num]) {
                    s_warned_syscalls[num] = 1;
                    KLOG_WARN("linux_sys", "unimplemented syscall=%llu args=%llx,%llx,%llx,%llx,%llx,%llx",
                              (unsigned long long)num,
                              (unsigned long long)a1, (unsigned long long)a2,
                              (unsigned long long)a3, (unsigned long long)a4,
                              (unsigned long long)a5, (unsigned long long)a6);
                }
            } else {
                KLOG_WARN("linux_sys", "unimplemented syscall=%llu args=%llx,%llx,%llx,%llx,%llx,%llx",
                          (unsigned long long)num,
                          (unsigned long long)a1, (unsigned long long)a2,
                          (unsigned long long)a3, (unsigned long long)a4,
                          (unsigned long long)a5, (unsigned long long)a6);
            }
            return -L_ENOSYS;
        }
    }
}
