#include "syscall.h"
#include "linux_syscall.h"
#include "timer.h"
#include "vfs.h"
#include "process.h"
#include "memory.h"
#include <stddef.h>
#include "../src/drivers/net/net_stack.h"
#include "../src/drivers/net/netdev.h"
#include "../src/drivers/net/tls/kesh_tls.h"
#include "uwindow.h"
#include "serial.h"
#include "kesh/kea.h"
#include "random.h"
#include "socket.h"
#include "fd.h"
#include "ipc.h"
#include "log.h"
#include "tty.h"
#include "service.h"
#include "../src/drivers/system/sound_manager.h"

#define MSR_EFER  0xC0000080
#define MSR_STAR  0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084
#define SYSCALL_MAX_STRING 4096
#define SYSCALL_MAX_IO (8U * 1024U * 1024U)
#define SYSCALL_MAX_LIST 64
#define SYSCALL_MAX_RANDOM 65536U
#define SYSCALL_MAX_DIAGNOSTICS 65536U

extern void syscall_entry_stub(void);

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static uint8_t g_syscall_stack[16384] __attribute__((aligned(16)));
uint64_t g_syscall_kernel_stack_top = 0;

void syscall_init(void) {
    g_syscall_kernel_stack_top = (uint64_t)&g_syscall_stack[sizeof(g_syscall_stack) - 16];

    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1ULL | (1ULL << 11);
    wrmsr(MSR_EFER, efer);

    uint64_t star = ((uint64_t)0x30 << 48) | ((uint64_t)0x28 << 32);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry_stub);
    wrmsr(MSR_FMASK, (1ULL << 9) | (1ULL << 8));
    serial_print("[SYSCALL] Fast system calls initialized; NXE enabled.\n");
}

extern void process_return_to_kernel(int exit_code) __attribute__((weak));
extern uint8_t g_syscall_should_yield;
extern uint64_t g_active_user_context;

static int copy_user_string(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0 || !src) return -1;
    int64_t len = user_strnlen(src, cap);
    if (len < 0 || (uint64_t)len >= cap) return -1;
    return copy_from_user(dst, src, (size_t)len + 1);
}

static int syscall_copy_in(void *dst, uint64_t user_ptr, size_t size) {
    return copy_from_user(dst, (const void *)user_ptr, size);
}

static int syscall_copy_out(uint64_t user_ptr, const void *src, size_t size) {
    return copy_to_user((void *)user_ptr, src, size);
}

static int syscall_has_permission(uint32_t permission) {
    return process_has_permission(permission);
}

static void syscall_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile ("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf), "c"(0));
}

static void syscall_cpu_info(kesh_cpu_info_t *info) {
    uint32_t eax, ebx, ecx, edx;
    for (size_t i = 0; i < sizeof(*info); ++i) ((uint8_t *)info)[i] = 0;

    syscall_cpuid(0, &eax, &ebx, &ecx, &edx);
    ((uint32_t *)info->vendor)[0] = ebx;
    ((uint32_t *)info->vendor)[1] = edx;
    ((uint32_t *)info->vendor)[2] = ecx;

    syscall_cpuid(1, &eax, &ebx, &ecx, &edx);
    uint32_t base_family = (eax >> 8) & 0x0FU;
    uint32_t base_model = (eax >> 4) & 0x0FU;
    info->family = base_family == 0x0FU ? base_family + ((eax >> 20) & 0xFFU) : base_family;
    info->model = (base_family == 0x06U || base_family == 0x0FU)
                    ? base_model | (((eax >> 16) & 0x0FU) << 4) : base_model;
    info->stepping = eax & 0x0FU;

    syscall_cpuid(0x80000000U, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004U) {
        uint32_t words[12];
        for (uint32_t leaf = 0; leaf < 3; ++leaf) {
            syscall_cpuid(0x80000002U + leaf, &words[leaf * 4], &words[leaf * 4 + 1],
                          &words[leaf * 4 + 2], &words[leaf * 4 + 3]);
        }
        const char *raw = (const char *)words;
        int start = 0;
        int end = 48;
        for (int i = 0; i < end; ++i) {
            if (raw[i] == '\0') {
                end = i;
                break;
            }
        }
        while (start < end && raw[start] == ' ') ++start;
        while (end > start && raw[end - 1] == ' ') --end;
        int out = 0;
        for (int i = start; i < end && out < (int)sizeof(info->brand) - 1; ++i) {
            unsigned char ch = (unsigned char)raw[i];
            info->brand[out++] = ch >= 32 && ch <= 126 ? (char)ch : '?';
        }
        info->brand[out] = '\0';
    }

    if (!info->brand[0]) {
        const char *fallback = info->vendor[0] ? info->vendor : "x86_64 Processor";
        int i = 0;
        while (fallback[i] && i < (int)sizeof(info->brand) - 1) {
            info->brand[i] = fallback[i];
            ++i;
        }
        info->brand[i] = '\0';
    }
}

static user_window_t *syscall_current_window(int win_id) {
    user_window_t *win = uwindow_get(win_id);
    uint64_t current_pml4 = vmm_get_current_pml4();
    if (win && win->pml4_phys == current_pml4) return win;
    /* Existing app ABI passes 0 as the window argument.  For every slot
       except slot 0 resolve it from the caller's address space instead of
       letting another app's window silently consume the request. */
    if (win_id == 0) return uwindow_get_process_window(current_pml4);
    return NULL;
}

static uint64_t alloc_kernel_buffer(size_t size, size_t *pages_out) {
    if (!size || size > SYSCALL_MAX_IO || !pages_out) return 0;
    size_t pages = (size + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    uint64_t phys = pmm_alloc_pages(pages);
    if (!phys) return 0;
    *pages_out = pages;
    return phys;
}

int64_t syscall_dispatcher(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    if (process_current_is_linux() || num > SYS_CPU_INFO) {
        return linux_syscall_dispatcher(num, a1, a2, a3, a4, a5, a6);
    }
    /* For native KeshOS ABI: a3 was passed in r10 (a4) */
    a3 = a4;
    switch (num) {
        case SYS_ABI_INFO: {
            if (!a1) return -1;
            kesh_abi_info_t info;
            info.version = KESH_ABI_VERSION;
            info.max_syscall = SYS_CPU_INFO;
            info.features = KESH_ABI_FEATURE_USER_COPY | KESH_ABI_FEATURE_WX |
                            KESH_ABI_FEATURE_TYPED_IPC | KESH_ABI_FEATURE_FD |
                            KESH_ABI_FEATURE_CA_UPDATE | KESH_ABI_FEATURE_CPU_INFO;
            info.page_size = PAGE_SIZE;
            info.max_io_size = SYSCALL_MAX_IO;
            return syscall_copy_out(a1, &info, sizeof(info));
        }

        case SYS_CPU_INFO: {
            if (!a1) return -1;
            kesh_cpu_info_t info;
            syscall_cpu_info(&info);
            return syscall_copy_out(a1, &info, sizeof(info));
        }

        case SYS_EXIT:
            serial_print("[SYSCALL] SYS_EXIT called by user process.\n");
            g_syscall_should_yield = 2;
            if (!g_active_user_context && process_return_to_kernel) {
                g_syscall_should_yield = 0;
                process_return_to_kernel((int)a1);
            }
            return 0;

        case SYS_PRINT: {
            char text[SYSCALL_MAX_STRING];
            if (copy_user_string(text, sizeof(text), (const char *)a1) != 0) return -1;
            serial_print(text);
            return 0;
        }

        case SYS_YIELD:
            g_syscall_should_yield = 1;
            return 0;

        case SYS_THREAD_CREATE: {
            int tid = process_create_thread(a1);
            if (tid >= 0) {
                g_syscall_should_yield = 1;
                return tid;
            }
            return -1;
        }

        case SYS_THREAD_EXIT:
            g_syscall_should_yield = 2;
            return 0;

        case SYS_GETRANDOM: {
            if (!a1 || a2 == 0 || a2 > SYSCALL_MAX_RANDOM || !random_is_ready()) return -1;
            uint8_t buffer[256];
            uint64_t user_addr = a1;
            uint64_t left = a2;
            while (left) {
                size_t chunk = left > sizeof(buffer) ? sizeof(buffer) : (size_t)left;
                if (random_bytes(buffer, chunk) != 0 || syscall_copy_out(user_addr, buffer, chunk) != 0) return -1;
                user_addr += chunk;
                left -= chunk;
            }
            return (int64_t)a2;
        }

        case SYS_SOCKET:
            if (!syscall_has_permission(KEA_PERM_NETWORK)) return -1;
            return socket_open(process_current_id(), (int)a1);

        case SYS_SOCKET_CONNECT:
            if (!syscall_has_permission(KEA_PERM_NETWORK) || a2 > 0xFFFFFFFFULL || a3 == 0 || a3 > 65535) return -1;
            return socket_connect(process_current_id(), (int)a1, (uint32_t)a2, (uint16_t)a3);

        case SYS_SOCKET_BIND:
            if (!syscall_has_permission(KEA_PERM_NETWORK) || a2 > 0xFFFFFFFFULL || a3 == 0 || a3 > 65535) return -1;
            return socket_bind(process_current_id(), (int)a1, (uint32_t)a2, (uint16_t)a3);

        case SYS_SOCKET_SENDTO: {
            if (!syscall_has_permission(KEA_PERM_NETWORK) || !a2) return -1;
            socket_datagram_t request;
            if (syscall_copy_in(&request, a2, sizeof(request)) != 0 || !request.data || !request.length || request.length > 1400) return -1;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer(request.length, &pages);
            if (!phys) return -1;
            void *buffer = (void *)(phys + g_hhdm_offset);
            int result = syscall_copy_in(buffer, request.data, request.length) == 0 ?
                         socket_send_to(process_current_id(), (int)a1, request.address, request.port, buffer, request.length) : -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_SOCKET_RECVFROM: {
            if (!syscall_has_permission(KEA_PERM_NETWORK) || !a2) return -1;
            socket_datagram_t request;
            if (syscall_copy_in(&request, a2, sizeof(request)) != 0 || !request.data || !request.length || request.length > 2048) return -1;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer(request.length, &pages);
            if (!phys) return -1;
            void *buffer = (void *)(phys + g_hhdm_offset);
            int result = socket_recv_from(process_current_id(), (int)a1, &request.address, &request.port,
                                          buffer, request.length, 5000);
            if (result > 0) {
                request.length = (uint16_t)result;
                if (syscall_copy_out(request.data, buffer, (size_t)result) != 0 ||
                    syscall_copy_out(a2, &request, sizeof(request)) != 0) result = -1;
            }
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_SOCKET_POLL:
            if (!syscall_has_permission(KEA_PERM_NETWORK)) return -1;
            return socket_poll(process_current_id(), (int)a1, (uint32_t)a2);

        case SYS_AUDIO_DEVICE:
            if (!syscall_has_permission(KEA_PERM_SOUND)) return -1;
            return sound_is_ready() ? (int)sound_device() : 0;

        case SYS_AUDIO_VOLUME:
            if (!syscall_has_permission(KEA_PERM_SOUND) || a1 > 100) return -1;
            return sound_set_volume((uint8_t)a1);

        case SYS_AUDIO_PLAY: {
            if (!syscall_has_permission(KEA_PERM_SOUND) || !a1 || !a2 || a2 > 65536) return -1;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer((size_t)a2, &pages);
            if (!phys) return -1;
            void *buffer = (void *)(phys + g_hhdm_offset);
            int result = syscall_copy_in(buffer, a1, (size_t)a2) == 0 ? sound_play_pcm(buffer, (uint32_t)a2) : -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_SOCKET_SEND: {
            if (!syscall_has_permission(KEA_PERM_NETWORK) || !a2 || a3 == 0 || a3 > 65535) return -1;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer((size_t)a3, &pages);
            if (!phys) return -1;
            void *buffer = (void *)(phys + g_hhdm_offset);
            int result = syscall_copy_in(buffer, a2, (size_t)a3) == 0 ? socket_send(process_current_id(), (int)a1, buffer, (uint16_t)a3) : -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_SOCKET_RECV: {
            if (!syscall_has_permission(KEA_PERM_NETWORK) || !a2 || a3 == 0 || a3 > 65535) return -1;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer((size_t)a3, &pages);
            if (!phys) return -1;
            void *buffer = (void *)(phys + g_hhdm_offset);
            int result = socket_recv(process_current_id(), (int)a1, buffer, (uint16_t)a3, 5000);
            if (result > 0 && syscall_copy_out(a2, buffer, (size_t)result) != 0) result = -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_SOCKET_CLOSE:
            if (!syscall_has_permission(KEA_PERM_NETWORK)) return -1;
            return socket_close(process_current_id(), (int)a1);

        case SYS_FD_OPEN: {
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            char path[SYSCALL_MAX_STRING];
            const uint64_t fd_known_flags = FD_OPEN_READ | FD_OPEN_WRITE | FD_OPEN_CREATE | FD_OPEN_TRUNC | FD_OPEN_APPEND | FD_OPEN_DIRECTORY;
            if (copy_user_string(path, sizeof(path), (const char *)a1) != 0 || (a2 & ~fd_known_flags)) return -1;
            return fd_open(process_current_id(), path, (uint32_t)a2);
        }

        case SYS_FD_CLOSE:
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            return fd_close(process_current_id(), (int)a1);

        case SYS_FD_DUP:
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            return fd_dup(process_current_id(), (int)a1);

        case SYS_FD_DUP2:
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            return fd_dup2(process_current_id(), (int)a1, (int)a2);

        case SYS_FD_READ: {
            if (!syscall_has_permission(KEA_PERM_FS) || !a2 || a3 == 0 || a3 > SYSCALL_MAX_IO) return -1;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer((size_t)a3, &pages);
            if (!phys) return -1;
            void *buffer = (void *)(phys + g_hhdm_offset);
            int result = fd_read(process_current_id(), (int)a1, buffer, (uint32_t)a3);
            if (result > 0 && syscall_copy_out(a2, buffer, (size_t)result) != 0) result = -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_FD_WRITE: {
            if (!syscall_has_permission(KEA_PERM_FS) || !a2 || a3 > SYSCALL_MAX_IO) return -1;
            if (a3 == 0) return fd_write(process_current_id(), (int)a1, 0, 0);
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer((size_t)a3, &pages);
            if (!phys) return -1;
            void *buffer = (void *)(phys + g_hhdm_offset);
            int result = syscall_copy_in(buffer, a2, (size_t)a3) == 0 ? fd_write(process_current_id(), (int)a1, buffer, (uint32_t)a3) : -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_FD_SEEK:
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            return fd_seek(process_current_id(), (int)a1, (int64_t)a2, (int)a3);

        case SYS_IPC_SEND: {
            if (!a2 || a3 == 0 || a3 > IPC_MAX_MESSAGE_BYTES) return -1;
            uint8_t message[IPC_MAX_MESSAGE_BYTES];
            if (syscall_copy_in(message, a2, (size_t)a3) != 0) return -1;
            return ipc_send(process_current_id(), (int)a1, message, (uint32_t)a3);
        }

        case SYS_IPC_RECV: {
            if (!a2 || a3 == 0 || a3 > IPC_MAX_MESSAGE_BYTES) return -1;
            uint8_t message[IPC_MAX_MESSAGE_BYTES];
            int sender = -1;
            int result = ipc_receive(process_current_id(), (int)a1, message, (uint32_t)a3, &sender);
            if (result > 0 && syscall_copy_out(a2, message, (size_t)result) != 0) return -1;
            return result;
        }

        case SYS_GET_CREDENTIALS: {
            const credentials_t *credentials = process_current_credentials();
            if (!credentials || !a1) return -1;
            return syscall_copy_out(a1, credentials, sizeof(*credentials)) == 0 ? 0 : -1;
        }

        case SYS_IPC_LAST_SENDER:
            return ipc_last_sender(process_current_id());

        case SYS_IPC_SEND_TYPED: {
            if (!a2) return -1;
            ipc_user_request_t request;
            if (syscall_copy_in(&request, a2, sizeof(request)) != 0 || !request.data ||
                request.size == 0 || request.size > IPC_MAX_MESSAGE_BYTES || request.type < 0 || request.type > 65535) return -1;
            uint8_t message[IPC_MAX_MESSAGE_BYTES];
            if (syscall_copy_in(message, request.data, request.size) != 0) return -1;
            return ipc_send_typed(process_current_id(), (int)a1, (uint16_t)request.type, message, request.size);
        }

        case SYS_IPC_RECV_TYPED: {
            if (!a1) return -1;
            ipc_user_request_t request;
            if (syscall_copy_in(&request, a1, sizeof(request)) != 0 || !request.data ||
                request.size == 0 || request.size > IPC_MAX_MESSAGE_BYTES || request.type < -1 || request.type > 65535) return -1;
            uint8_t message[IPC_MAX_MESSAGE_BYTES];
            int sender = -1;
            uint16_t type = 0;
            int result = ipc_receive_typed(process_current_id(), request.sender, request.type,
                                           message, request.size, &sender, &type);
            if (result > 0 && syscall_copy_out(request.data, message, (size_t)result) != 0) return -1;
            request.size = result > 0 ? (uint32_t)result : 0;
            request.sender = sender;
            request.type = type;
            if (syscall_copy_out(a1, &request, sizeof(request)) != 0) return -1;
            return result;
        }

        case SYS_DIAGNOSTICS_READ: {
            if (!syscall_has_permission(KEA_PERM_FS) || !a1 || a2 < 2 || a2 > SYSCALL_MAX_DIAGNOSTICS) return -1;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer((size_t)a2, &pages);
            if (!phys) return -1;
            void *buffer = (void *)(phys + g_hhdm_offset);
            int result = klog_snapshot(buffer, (int)a2);
            if (result >= 0 && syscall_copy_out(a1, buffer, (size_t)result + 1U) != 0) result = -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_DIAGNOSTICS_QUERY: {
            if (!syscall_has_permission(KEA_PERM_FS) || !a1) return -1;
            kesh_diagnostics_query_t request;
            if (syscall_copy_in(&request, a1, sizeof(request)) != 0 || !request.buffer ||
                request.size < 2 || request.size > SYSCALL_MAX_DIAGNOSTICS) return -1;
            request.module[sizeof(request.module) - 1] = 0;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer(request.size, &pages);
            if (!phys) return -1;
            void *buffer = (void *)(phys + g_hhdm_offset);
            int result = klog_snapshot_filtered(buffer, (int)request.size, request.min_level,
                                                request.max_level, request.module);
            if (result >= 0 && syscall_copy_out(request.buffer, buffer, (size_t)result + 1U) != 0) result = -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_SERVICE_HEARTBEAT:
            return service_heartbeat(process_current_id());

        case SYS_PERMISSION_REVOKE:
            return process_revoke_permissions(process_current_id(), (int)a1, (uint32_t)a2);

        case SYS_TRUST_STORE_RELOAD:
            if (!syscall_has_permission(KEA_PERM_ROOT)) return -1;
            return kesh_tls_reload_trust_store();

        case SYS_TTY_CREATE:
            return tty_create_for_process(process_current_id());

        case SYS_TTY_CLOSE:
            return tty_close(process_current_id(), (int)a1);

        case SYS_TTY_READ: {
            if (!a2 || a3 == 0 || a3 > TTY_BUFFER_SIZE) return -1;
            uint8_t buffer[TTY_BUFFER_SIZE];
            int result = tty_read_process(process_current_id(), (int)a1, buffer, (size_t)a3);
            if (result > 0 && syscall_copy_out(a2, buffer, (size_t)result) != 0) return -1;
            return result;
        }

        case SYS_TTY_WRITE: {
            if (!a2 || a3 == 0 || a3 > TTY_BUFFER_SIZE) return -1;
            uint8_t buffer[TTY_BUFFER_SIZE];
            if (syscall_copy_in(buffer, a2, (size_t)a3) != 0) return -1;
            return tty_write_process(process_current_id(), (int)a1, buffer, (size_t)a3);
        }

        case SYS_TTY_AVAILABLE:
            return tty_available_process(process_current_id(), (int)a1);

        case SYS_PTY_CREATE: {
            if (!a1) return -1;
            int handles[2];
            if (pty_create(process_current_id(), handles) != 0) return -1;
            if (syscall_copy_out(a1, handles, sizeof(handles)) != 0) {
                (void)pty_close(process_current_id(), handles[0]);
                (void)pty_close(process_current_id(), handles[1]);
                return -1;
            }
            return 0;
        }

        case SYS_PTY_ATTACH:
            return pty_attach(process_current_id(), (int)a1, (int)a2);

        case SYS_PTY_READ: {
            if (!a2 || !a3 || a3 > TTY_BUFFER_SIZE) return -1;
            uint8_t buffer[TTY_BUFFER_SIZE];
            int result = pty_read(process_current_id(), (int)a1, buffer, (size_t)a3);
            if (result > 0 && syscall_copy_out(a2, buffer, (size_t)result) != 0) return -1;
            return result;
        }

        case SYS_PTY_WRITE: {
            if (!a2 || !a3 || a3 > TTY_BUFFER_SIZE) return -1;
            uint8_t buffer[TTY_BUFFER_SIZE];
            if (syscall_copy_in(buffer, a2, (size_t)a3) != 0) return -1;
            return pty_write(process_current_id(), (int)a1, buffer, (size_t)a3);
        }

        case SYS_PTY_AVAILABLE:
            return pty_available(process_current_id(), (int)a1);

        case SYS_PTY_SET_MODE:
            return pty_set_mode(process_current_id(), (int)a1, (uint32_t)a2);

        case SYS_PTY_CLOSE:
            return pty_close(process_current_id(), (int)a1);

        case SYS_PTY_CURRENT:
            return process_current_pty();

        case SYS_GET_PGID:
            return process_get_group(process_current_id());

        case SYS_SET_PGID:
            return process_set_group(process_current_id(), process_current_id(), (int)a1);

        case SYS_PTY_SET_FG:
            return pty_set_foreground_group(process_current_id(), (int)a1, (int)a2);

        case SYS_PTY_GET_FG:
            return pty_get_foreground_group(process_current_id(), (int)a1);

        case SYS_PTY_SIGNAL:
            return pty_signal_foreground(process_current_id(), (int)a1, (int)a2);

        case SYS_PTY_OPEN_FD: {
            int handle = process_current_pty();
            if (handle < 0) return -1;
            return fd_bind_pty(process_current_id(), handle, (uint32_t)a1);
        }

        case SYS_GET_TIME:
            return (int64_t)timer_millis();

        case SYS_CLOCK_GET: {
            uint64_t ns = a1 == KESH_CLOCK_MONOTONIC ? timer_monotonic_ns() :
                          a1 == KESH_CLOCK_REALTIME ? timer_realtime_ns() : 0;
            if (!ns || !a2) return -1;
            kesh_timespec_t value;
            value.seconds = ns / 1000000000ULL;
            value.nanoseconds = (uint32_t)(ns % 1000000000ULL);
            value.clock_id = (uint32_t)a1;
            return syscall_copy_out(a2, &value, sizeof(value));
        }

        case SYS_VM_MAP:
            return (int64_t)process_vm_map(a1, (uint32_t)a2);

        case SYS_VM_UNMAP:
            return process_vm_unmap(a1, a2);

        case SYS_PIPE_CREATE: {
            if (!a1) return -1;
            int descriptors[2];
            if (fd_pipe_create(process_current_id(), &descriptors[0], &descriptors[1]) != 0) return -1;
            if (syscall_copy_out(a1, descriptors, sizeof(descriptors)) != 0) {
                (void)fd_close(process_current_id(), descriptors[0]);
                (void)fd_close(process_current_id(), descriptors[1]);
                return -1;
            }
            return 0;
        }

        case SYS_FD_POLL:
            return fd_poll(process_current_id(), (int)a1, (uint32_t)a2);

        case SYS_FD_READDIR: {
            if (!syscall_has_permission(KEA_PERM_FS) || !a2) return -1;
            fd_dirent_t entry;
            int result = fd_readdir(process_current_id(), (int)a1, &entry);
            if (result == 1 && syscall_copy_out(a2, &entry, sizeof(entry)) != 0) return -1;
            return result;
        }

        case SYS_TEST:
            serial_print("[SYSCALL] SYS_TEST received from Ring 3! Argument: ");
            {
                char buf[32];
                int i = 0;
                uint64_t val = a1;
                if (val == 0) buf[i++] = '0';
                while (val > 0 && i < 31) { buf[i++] = (char)('0' + (val % 10)); val /= 10; }
                while (i > 0) serial_write_char(buf[--i]);
            }
            serial_print(" [SUCCESS]\n");
            return (int64_t)(a1 * 2);

        case SYS_CREATE_WINDOW: {
            if (!syscall_has_permission(KEA_PERM_GUI)) return -1;
            char title[64];
            if (copy_user_string(title, sizeof(title), (const char *)a3) != 0) return 0;
            uint64_t user_cr3 = vmm_get_current_pml4();
            user_window_t *win = uwindow_create((int)a1, (int)a2, title, user_cr3);
            if (!win) {
                serial_print("[SYSCALL] Failed to create user window!\n");
                return 0;
            }
            return (int64_t)win->user_fb_vaddr;
        }

        case SYS_UPDATE_WINDOW: {
            if (!syscall_has_permission(KEA_PERM_GUI)) return -1;
            user_window_t *win = syscall_current_window((int)a1);
            if (!win) return -1;
            win->dirty = 1;
            g_syscall_should_yield = 1;
            return 0;
        }

        case SYS_POLL_EVENT: {
            if (!syscall_has_permission(KEA_PERM_GUI)) return -1;
            uevent_t ev;
            user_window_t *win = syscall_current_window((int)a1);
            if (!win) return -1;
            int has_ev = uwindow_pop_event(win->win_id, &ev);
            if (!has_ev) return 0;
            if (!a2 || syscall_copy_out(a2, &ev, sizeof(ev)) != 0) return -1;
            return 1;
        }

        case SYS_SLEEP:
            process_sleep_current((uint32_t)a1);
            g_syscall_should_yield = 1;
            return 0;

        case SYS_VFS_LIST: {
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            char path[SYSCALL_MAX_STRING];
            kesh_vfs_entry_t entries[SYSCALL_MAX_LIST];
            int max_entries = (int)a3;
            if (max_entries < 1 || max_entries > SYSCALL_MAX_LIST) return -1;
            if (copy_user_string(path, sizeof(path), (const char *)a1) != 0) return -1;
            int count = vfs_list(path, entries, max_entries);
            if (count < 0) return count;
            if (syscall_copy_out(a2, entries, (size_t)count * sizeof(entries[0])) != 0) return -1;
            return count;
        }

        case SYS_VFS_READ: {
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            char path[SYSCALL_MAX_STRING];
            int max_bytes = (int)a3;
            if (a3 == 0 || a3 > SYSCALL_MAX_IO || max_bytes <= 0) return -1;
            if (copy_user_string(path, sizeof(path), (const char *)a1) != 0) return -1;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer((size_t)max_bytes, &pages);
            if (!phys) return -1;
            void *buf = (void *)(phys + g_hhdm_offset);
            int result = vfs_read(path, buf, max_bytes);
            if (result > 0 && syscall_copy_out(a2, buf, (size_t)result) != 0) result = -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_VFS_WRITE: {
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            char path[SYSCALL_MAX_STRING];
            int bytes = (int)a3;
            if (a3 > SYSCALL_MAX_IO || bytes < 0) return -1;
            if (copy_user_string(path, sizeof(path), (const char *)a1) != 0) return -1;
            if (bytes == 0) {
                uint8_t zero = 0;
                return vfs_write(path, &zero, 0);
            }
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer((size_t)bytes, &pages);
            if (!phys) return -1;
            void *buf = (void *)(phys + g_hhdm_offset);
            int result = syscall_copy_in(buf, a2, (size_t)bytes) == 0 ? vfs_write(path, buf, bytes) : -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        case SYS_VFS_STAT: {
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            char path[SYSCALL_MAX_STRING];
            kesh_vfs_stat_t st;
            if (copy_user_string(path, sizeof(path), (const char *)a1) != 0) return -1;
            if (vfs_stat(path, &st) != 0) return -1;
            return syscall_copy_out(a2, &st, sizeof(st)) == 0 ? 0 : -1;
        }

        case SYS_EXEC: {
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            char path[SYSCALL_MAX_STRING];
            if (copy_user_string(path, sizeof(path), (const char *)a1) != 0) return -1;
            if (path[0] == 't' && path[1] == 'e' && path[2] == 'r' && path[3] == 'm') {
                extern void toggle_terminal_app(void);
                extern void win_bring_to_front(int id);
                toggle_terminal_app();
                win_bring_to_front(4);
                return 1001;
            }
            if (path[0] == 'd' && path[1] == 'o' && path[2] == 'o' && path[3] == 'm') {
                extern void toggle_doom_app(void);
                extern void win_bring_to_front(int id);
                if (toggle_doom_app) toggle_doom_app();
                win_bring_to_front(6);
                return 1002;
            }
            if (path[0] == 'r' && path[1] == 'e' && path[2] == 'b' && path[3] == 'o') {
                extern int acpi_reboot(void);
                acpi_reboot();
                return 0;
            }
            if (path[0] == 's' && path[1] == 'h' && path[2] == 'u' && path[3] == 't') {
                extern int acpi_poweroff(void);
                acpi_poweroff();
                return 0;
            }
            int parent_pid = process_current_id();
            process_t *p = process_spawn_path(path);
            if (!p) return -1;
            if (fd_inherit(parent_pid, p->id) != 0) {
                (void)process_kill(p->id);
                return -1;
            }
            g_syscall_should_yield = 1;
            return p->id;
        }

        case SYS_GET_PROCS: {
            kesh_proc_info_t table[16];
            if (!a1 || a2 == 0 || a2 > 16) return -1;
            int count = process_get_table(table, (int)a2);
            if (count > 0 && syscall_copy_out(a1, table, (size_t)count * sizeof(table[0])) != 0) return -1;
            return count;
        }

        case SYS_KILL: {
            int target = (int)a1;
            if (target != process_current_id() && !process_current_is_root()) return -1;
            int result = process_kill(target);
            if (result == 0 && target == process_current_id()) g_syscall_should_yield = 2;
            return result;
        }

        case SYS_GET_SYSINFO: {
            kesh_sysinfo_t info;
            uint64_t total = 0, used = 0;
            pmm_get_stats(&total, &used);
            info.total_ram_bytes = total;
            info.free_ram_bytes = (total > used) ? (total - used) : 0;
            info.uptime_ms = timer_millis();
            extern uint32_t g_screen_w, g_screen_h;
            info.screen_w = g_screen_w;
            info.screen_h = g_screen_h;
            kesh_proc_info_t dummy[16];
            info.active_procs = (uint32_t)process_get_table(dummy, 16);
            vfs_get_disk_stats(&info.disk_total_bytes, &info.disk_free_bytes);
            return syscall_copy_out(a1, &info, sizeof(info)) == 0 ? 0 : -1;
        }

        case SYS_VFS_MKDIR:
        case SYS_VFS_DELETE: {
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            char path[SYSCALL_MAX_STRING];
            if (copy_user_string(path, sizeof(path), (const char *)a1) != 0) return -1;
            return num == SYS_VFS_MKDIR ? vfs_mkdir(path) : vfs_delete(path);
        }

        case SYS_VFS_RESCAN:
            if (!syscall_has_permission(KEA_PERM_FS)) return -1;
            vfs_rescan_drives();
            return 0;

        case SYS_GET_SETTINGS: {
            kesh_settings_t st;
            extern int g_wallpaper_choice;
            extern int g_dark_mode;
            extern int g_dock_mag_enabled;
            extern int g_dock_mag_level;
            extern int g_window_controls_align;
            extern int g_accent_color;
            extern int g_theme_id;
            extern int g_animations_enabled;
            extern int g_animation_speed;
            extern int g_interface_density;
            extern int g_show_seconds;
            extern int g_liquid_glass;
            st.wallpaper_choice = g_wallpaper_choice;
            st.dark_mode = g_dark_mode;
            st.dock_mag_enabled = g_dock_mag_enabled;
            st.dock_mag_level = g_dock_mag_level;
            st.window_controls_align = g_window_controls_align;
            st.accent_color = g_accent_color;
            st.theme_id = g_theme_id;
            st.animations_enabled = g_animations_enabled;
            st.animation_speed = g_animation_speed;
            st.interface_density = g_interface_density;
            st.show_seconds = g_show_seconds;
            st.liquid_glass = g_liquid_glass;
            return syscall_copy_out(a1, &st, sizeof(st)) == 0 ? 0 : -1;
        }

        case SYS_SET_SETTINGS: {
            kesh_settings_t st;
            if (syscall_copy_in(&st, a1, sizeof(st)) != 0) return -1;
            extern int g_wallpaper_choice;
            extern int g_dark_mode;
            extern int g_dock_mag_enabled;
            extern int g_dock_mag_level;
            extern int g_window_controls_align;
            extern int g_accent_color;
            extern int g_theme_id;
            extern int g_animations_enabled;
            extern int g_animation_speed;
            extern int g_interface_density;
            extern int g_show_seconds;
            extern int g_liquid_glass;
            if (st.wallpaper_choice >= 1 && st.wallpaper_choice <= 2) g_wallpaper_choice = st.wallpaper_choice;
            g_dark_mode = st.dark_mode ? 1 : 0;
            g_dock_mag_enabled = st.dock_mag_enabled ? 1 : 0;
            if (st.dock_mag_level >= 0 && st.dock_mag_level <= 100) g_dock_mag_level = st.dock_mag_level;
            if (st.window_controls_align == 0 || st.window_controls_align == 1) g_window_controls_align = st.window_controls_align;
            if (st.accent_color >= 0 && st.accent_color <= 2) g_accent_color = st.accent_color;
            if (st.theme_id >= 0 && st.theme_id <= 2) {
                g_theme_id = st.theme_id;
                /* Keep legacy native surfaces coherent with the selected
                   complete palette while they migrate to palette tokens. */
                if (g_theme_id == 0) { g_dark_mode = 0; g_accent_color = 1; }
                else if (g_theme_id == 1) { g_dark_mode = 1; g_accent_color = 1; }
                else { g_dark_mode = 0; g_accent_color = 0; }
            }
            g_animations_enabled = st.animations_enabled ? 1 : 0;
            if (st.animation_speed >= 20 && st.animation_speed <= 100) g_animation_speed = st.animation_speed;
            if (st.interface_density >= 0 && st.interface_density <= 2) g_interface_density = st.interface_density;
            g_show_seconds = st.show_seconds ? 1 : 0;
            g_liquid_glass = st.liquid_glass ? 1 : 0;
            return 0;
        }

        case SYS_PANIC: {
            if (!syscall_has_permission(KEA_PERM_ROOT) || !process_current_is_root()) return -1;
            char msg[SYSCALL_MAX_STRING];
            extern void kernel_panic(const char *msg);
            if (a1 && copy_user_string(msg, sizeof(msg), (const char *)a1) != 0) return -1;
            kernel_panic(a1 ? msg : "Kernel panic triggered from userspace application");
            return 0;
        }

        case SYS_NET_INFO: {
            if (!syscall_has_permission(KEA_PERM_NETWORK)) return -1;
            typedef struct {
                uint32_t ip;
                uint32_t netmask;
                uint32_t gateway;
                uint32_t dns;
                uint8_t mac[6];
                int link_up;
                char driver[16];
            } net_info_t;
            net_info_t info;
            const netdev_info_t *dev = netdev_get_info();
            extern uint32_t net_get_netmask(void);
            extern uint32_t net_get_gateway(void);
            extern uint32_t net_get_dns_server(void);
            info.ip = net_get_my_ip();
            info.netmask = net_get_netmask();
            info.gateway = net_get_gateway();
            info.dns = net_get_dns_server();
            const uint8_t *mac = net_get_my_mac();
            for (int i = 0; i < 6; i++) info.mac[i] = mac[i];
            info.link_up = dev && dev->link_up ? 1 : 0;
            int i = 0;
            if (dev && dev->name) {
                while (dev->name[i] && i < 15) { info.driver[i] = dev->name[i]; i++; }
            }
            info.driver[i] = '\0';
            return syscall_copy_out(a1, &info, sizeof(info)) == 0 ? 0 : -1;
        }

        case SYS_NET_FETCH: {
            if (!syscall_has_permission(KEA_PERM_NETWORK)) return -1;
            if (!a1 || !a2 || a3 < 2 || a3 > 65535) return -1;
            char url[SYSCALL_MAX_STRING];
            if (copy_user_string(url, sizeof(url), (const char *)a1) != 0) return -1;
            size_t max_bytes = (size_t)a3;
            size_t pages = 0;
            uint64_t phys = alloc_kernel_buffer(max_bytes, &pages);
            if (!phys) return -1;
            char *buf = (char *)(phys + g_hhdm_offset);
            int result = kesh_http_fetch(url, buf, max_bytes, NULL);
            if (result > 0 && syscall_copy_out(a2, buf, (size_t)result) != 0) result = -1;
            pmm_free_pages(phys, pages);
            return result;
        }

        default:
            serial_print("[SYSCALL] Unknown syscall number!\n");
            return -1;
    }
}
