#!/usr/bin/env python3
"""Wire the Linux signal compatibility foundation into the KeshOS kernel.

This is intentionally deterministic and idempotent so the self-hosted builder can
apply the small cross-file integration without carrying a second checkout patch.
"""

from pathlib import Path


def replace_once(path: Path, old: str, new: str, marker: str) -> bool:
    text = path.read_text(encoding="utf-8")
    if marker in text:
        return False
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one integration anchor, found {count}: {old!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    return True


def main() -> None:
    changed = []

    linux = Path("kernel/linux_syscall.c")
    if replace_once(
        linux,
        '#include "linux_syscall.h"\n',
        '#include "linux_syscall.h"\n#include "linux_signal.h"\n',
        '#include "linux_signal.h"',
    ):
        changed.append(str(linux))

    if replace_once(
        linux,
        '#define LINUX_SYS_RT_SIGPROCMASK 14\n',
        '#define LINUX_SYS_RT_SIGPROCMASK 14\n#define LINUX_SYS_RT_SIGRETURN   15\n',
        '#define LINUX_SYS_RT_SIGRETURN',
    ):
        changed.append(str(linux))

    if replace_once(
        linux,
        '#define LINUX_SYS_WAIT4          61\n#define LINUX_SYS_UNAME          63\n',
        '#define LINUX_SYS_WAIT4          61\n#define LINUX_SYS_KILL           62\n#define LINUX_SYS_UNAME          63\n',
        '#define LINUX_SYS_KILL',
    ):
        changed.append(str(linux))

    if replace_once(
        linux,
        '#define LINUX_SYS_GETEGID        108\n#define LINUX_SYS_PRCTL          157\n',
        '#define LINUX_SYS_GETEGID        108\n#define LINUX_SYS_RT_SIGPENDING  127\n#define LINUX_SYS_SIGALTSTACK    131\n#define LINUX_SYS_PRCTL          157\n',
        '#define LINUX_SYS_RT_SIGPENDING',
    ):
        changed.append(str(linux))

    if replace_once(
        linux,
        '#define LINUX_SYS_EPOLL_CTL      233\n#define LINUX_SYS_OPENAT         257\n',
        '#define LINUX_SYS_EPOLL_CTL      233\n#define LINUX_SYS_TGKILL         234\n#define LINUX_SYS_OPENAT         257\n',
        '#define LINUX_SYS_TGKILL',
    ):
        changed.append(str(linux))

    old_signal_stub = '''        /* rt_sigaction / rt_sigprocmask: stub */
        case LINUX_SYS_RT_SIGACTION:
        case LINUX_SYS_RT_SIGPROCMASK: {
            return 0;
        }
'''
    new_signal_cases = '''        /* Stateful Linux signal ABI.  User-handler frame delivery and
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
'''
    if replace_once(
        linux,
        old_signal_stub,
        new_signal_cases,
        'return linux_signal_rt_sigaction(',
    ):
        changed.append(str(linux))

    old_tkill = '''        case LINUX_SYS_TKILL: {
            if ((int64_t)a1 != linux_current_tid()) return -L_ESRCH;
            if ((int)a2 == 0) return 0;
            g_syscall_should_yield = 2;
            return 0;
        }
'''
    new_kill_cases = '''        case LINUX_SYS_KILL:
            return linux_signal_kill(process_current_id(), (int)a1, (int)a2);

        case LINUX_SYS_TKILL:
            return linux_signal_tkill(process_current_id(), (int)a1, (int)a2);

        case LINUX_SYS_TGKILL:
            return linux_signal_tgkill(process_current_id(), (int)a1, (int)a2, (int)a3);
'''
    if replace_once(
        linux,
        old_tkill,
        new_kill_cases,
        'return linux_signal_tgkill(',
    ):
        changed.append(str(linux))

    process = Path("kernel/process.c")
    if replace_once(
        process,
        '#include "process.h"\n',
        '#include "process.h"\n#include "linux_signal.h"\n',
        '#include "linux_signal.h"',
    ):
        changed.append(str(process))

    if replace_once(
        process,
        '    for (int i = 0; i < MAX_PROCESSES; i++) {\n        g_processes[i].id = i;\n',
        '    for (int i = 0; i < MAX_PROCESSES; i++) {\n        linux_signal_reset_process(i);\n        g_processes[i].id = i;\n',
        '        linux_signal_reset_process(i);',
    ):
        changed.append(str(process))

    if replace_once(
        process,
        '    process_t *proc = &g_processes[slot];\n    int n = 0;\n',
        '    process_t *proc = &g_processes[slot];\n    linux_signal_reset_process(slot);\n    int n = 0;\n',
        '    linux_signal_reset_process(slot);',
    ):
        changed.append(str(process))

    ninja = Path("generate_ninja.py")
    if replace_once(
        ninja,
        '    ("kernel/linux_syscall.c", "build/kernel/linux_syscall.o"),\n',
        '    ("kernel/linux_syscall.c", "build/kernel/linux_syscall.o"),\n'
        '    ("kernel/linux_signal.c", "build/kernel/linux_signal.o"),\n',
        '("kernel/linux_signal.c", "build/kernel/linux_signal.o")',
    ):
        changed.append(str(ninja))

    unique = []
    for item in changed:
        if item not in unique:
            unique.append(item)
    if unique:
        print("KeshOS signal foundation wired into: " + ", ".join(unique))
    else:
        print("KeshOS signal foundation already wired; no changes needed.")


if __name__ == "__main__":
    main()
