<p align="center">
  <img src="logo.png" alt="KeshOS Logo" width="160">
</p>

<h1 align="center">KeshOS 1.0 "Drop" (RC1)</h1>

<p align="center">
  <b>Independent, lightweight 64-bit desktop operating system with a composited desktop environment.</b><br>
  Built from scratch by <b>SneakDeak Tech</b>.
</p>

<p align="center">
  <a href="README.ru.md">🇷🇺 <b>Читать на русском</b></a>
</p>

<p align="center">
  <a href="#architecture"><img src="https://img.shields.io/badge/arch-x86__64-blue.svg" alt="Architecture"></a>
  <a href="https://github.com/limine-bootloader/limine"><img src="https://img.shields.io/badge/bootloader-Limine-brightgreen.svg" alt="Bootloader"></a>
  <a href="#architecture"><img src="https://img.shields.io/badge/kernel-C%20%2F%20ASM%20(Ring%200)-orange.svg" alt="Kernel"></a>
  <a href="#gui--windowing"><img src="https://img.shields.io/badge/compositor-KeshShell%20(C%2B%2B)-purple.svg" alt="Compositor"></a>
  <a href="https://discord.gg/YgMe7ekA5y"><img src="https://img.shields.io/badge/Discord-Join%20Community-5865F2?logo=discord&logoColor=white" alt="Discord"></a>
  <a href="https://www.reddit.com/r/sneakdeak/"><img src="https://img.shields.io/badge/Reddit-r%2Fsneakdeak-FF4500?logo=reddit&logoColor=white" alt="Reddit"></a>
</p>

---

## 🎯 Target Hardware & Philosophy

Modern desktop operating systems have left capable 2011–2016 x86_64 machines behind:

* **Windows 10** reached official End-of-Life in October 2025.
* **Windows 11** enforces strict hardware gates (Intel 8th Gen+ / AMD Zen+, TPM 2.0, 4 GB RAM minimum).
* Modern Linux desktop environments (GNOME/KDE) with heavy background daemons and systemd often consume excessive memory on 2–4 GB systems.

**KeshOS** aims to provide a fast, visually fluid desktop environment for these aging 64-bit rigs, targeting **~150 MB idle memory** without bloated background services or telemetry.

---

## 🛠 Architecture & Technical Stack

```text
+-------------------------------------------------------+
|  Ring 3 (Userland)                                    |
|   - KeshShell (C++ Client-Server Window Compositor)   |
|   - Native Apps: Dolphin, Notepad, TaskMgr, Paint     |
|   - Native .kea binaries linked against /system/lib   |
+-------------------------------------------------------+
|  (System Calls: Fast SYSCALL/SYSRETQ & Kernel IPC)    |
+-------------------------------------------------------+
|  Ring 0 (Monolithic Kernel)                           |
|   - 4-Level Paging (PML4) Address Space Isolation    |
|   - In-kernel Network Stack (ARP, ICMP, DNS Resolver)|
|   - Clean Unix-like VFS (/, /apps, /system, /hdd)    |
|   - Storage Drivers: IDE ATA, ATAPI CD-ROM, FAT32    |
+-------------------------------------------------------+
|  (Limine Boot Protocol)                               |
+-------------------------------------------------------+
|  Hardware & Firmware: x86_64 UEFI (GOP) & Legacy BIOS |
+-------------------------------------------------------+
```

### 1. Boot Subsystem
* Bootstrapped via the **Limine Bootloader**, supporting both modern UEFI/GPT and legacy BIOS configurations.
* High-resolution linear framebuffer initialization via UEFI GOP with fallback modes.

### 2. Kernel & Memory Management
* **Monolithic architecture** implemented in raw C and x86_64 Assembly.
* Memory management using **4-level paging (PML4)**, providing separate virtual address spaces for kernel and userland processes.
* **Hardware-enforced Ring 3 isolation:** Faults and crashes in user applications are trapped by kernel interrupt handlers (`#GP`, `#PF`, `#UD`) without compromising the compositor or the OS session.

### 3. GUI & Compositing (KeshShell)
* Custom client-server window compositor written in modern C++.
* Applications render to isolated shared offscreen surface buffers (`0x50000000`).
* The compositor performs z-ordering, window decorations ("traffic light" controls, modern composited styling), and blits dirty rectangles to a double-buffered linear framebuffer.
* Asynchronous input routing (mouse & keyboard) dispatched via kernel syscall event queue (`SYS_POLL_EVENT`).

### 4. Networking Stack
* In-kernel bare-metal networking stack with Intel **e1000** Gigabit Ethernet support:
  * Ethernet frame processing and dynamic ARP cache resolution.
  * ICMP engine supporting network diagnostics (`ping`).
  * Direct DNS resolver for domain lookups over UDP sockets.
  * Lightweight TLS capabilities via embedded BearSSL.

### 5. Application Packaging & File System
* **`.kea` Packages:** Native userland package format built on top of the ELF64 standard with structured metadata headers, dynamically linked with system runtimes.
* **Unix-like VFS:** Clean tree structure (`/`, `/apps`, `/system`, `/hdd`, `/cdrom`) with support for in-memory nodes, FAT32, and ISO 9660 filesystems.
* **Dolphin File Manager:** Modern C++ file manager with breadcrumb navigation, grid/list view modes, and direct storage inspection.

---

## 🔍 Development Transparency & Methodology

* **The Core:** Memory management (PMM/VMM), PML4 setup, Ring 3 context switching, IPC, the C++ framebuffer compositor, storage drivers, and packet handlers were manually written, debugged, and validated.
* **Privacy:** 100% offline-first. Zero telemetry. Network interfaces only initiate outbound traffic upon explicit user commands.

---

## 🚀 Building & Running from Source

### Prerequisites

Ensure you have the following host tools installed:

* **LLVM / Clang** (`clang`, `clang++`, `ld.lld`) or `x86_64-elf-gcc`
* **NASM** (x86_64 assembler)
* **Ninja** build system & **Python 3**
* **xorriso** (for bootable ISO creation)
* **QEMU** or **VirtualBox** (for virtualization)

### 1. Generating Build Script & Compiling Kernel

KeshOS utilizes a high-speed Ninja build configuration generated by Python:

```bash
# Generate Ninja build graph
python generate_ninja.py

# Compile kernel, drivers, compositor, and userspace apps
ninja
```

*(Alternatively, run `make` or `make all` to trigger the build automated pipeline).*

### 2. Building the Bootable ISO Image

To pack Limine bootloader binaries, kernel, and `.kea` packages into an ISO:

```powershell
# On Windows (PowerShell):
powershell -ExecutionPolicy Bypass -File .\build-iso.ps1

# Or via Makefile:
make iso
```

This outputs `build/keshos.iso`.

### 3. Running in Virtual Machine

#### Running via QEMU:
```powershell
powershell -ExecutionPolicy Bypass -File .\run-qemu.ps1
# or
make qemu
```

#### Running in VirtualBox:
1. Create a VM with type **Other / Unknown (64-bit)**.
2. Set Memory to **1024 MB – 2048 MB**, Video Memory to **32 MB – 64 MB**.
3. In **System -> Motherboard -> Boot Order**, set **Optical Drive** as the first boot device.
4. Attach `build/keshos.iso` to the Optical Drive.
5. Enable **Intel PRO/1000 MT Desktop (82540EM)** for network emulation.
6. Start the VM and enjoy KeshOS!

---

## 🗺 Roadmap

- [x] UEFI/GPT boot via Limine protocol
- [x] 4-level paging (PML4) & Ring 3 hardware isolation
- [x] Linear framebuffer compositor (KeshShell) with double-buffering & composited UI
- [x] In-kernel ARP, ICMP ping, and DNS resolver
- [x] Native Ring 3 applications (Dolphin Explorer, Notepad, Task Manager, Paint, Terminal, Settings)
- [x] Native `.kea` packaging pipeline and runtime execution
- [x] **Release Candidate 1 (RC1) Public ISO Build**
- [ ] Read/Write Ext4 filesystem persistence with extent tree parsing
- [ ] Graphical LiveCD installer for bare-metal targets
- [ ] OTA updates via web-hosted manifest & A/B slot fallback
- [ ] *Long-term:* POSIX CLI binary compatibility layer

---

## 💬 Community & Contributing

KeshOS is developed by **SneakDeak Tech**. We welcome OS developers, low-level enthusiasts, and testers!

- **Discord Server:** [discord.gg/YgMe7ekA5y](https://discord.gg/YgMe7ekA5y)
- **Subreddit:** [r/sneakdeak](https://www.reddit.com/r/sneakdeak/)
