# KeshOS Quality Build Status

This build is a real source tree plus compiled artifacts and a generated bootable ISO.

## Verified in the sandbox

- `python3 generate_ninja.py && ninja -j4` completes and links `build/kernel.elf`.
- The ISO builder creates `build/keshos.iso` as ISO 9660 media with Limine BIOS and EFI entries.
- ISO structure validation confirms `KERNEL.ELF`, `LIMINE.CONF`, `NOTEPAD.KEA`, BIOS Limine media, and EFI boot media are present.
- Host-side web parser tests pass.
- Host-side software 3D rasterizer tests pass.
- FAT32 write, overwrite, mkdir, and delete paths are implemented in the filesystem layer.
- VFS write has a user-space syscall path and Notepad uses persistent save support.
- Process sleep now tracks wake deadlines instead of being a plain yield.
- PCI enumeration and USB-controller discovery layers are present.
- Network abstraction supports e1000, RTL8139, and legacy/transitional virtio-net backends.
- DHCP configuration, ARP caching, gateway routing, IPv4 DNS, and the existing TCP path are integrated with the network abstraction.
- CPU-side SSE2/AVX/AVX2 acceleration is available for framebuffer copy/fill/blend operations.
- A software 3D rasterizer and transformed cube demo are included.
- A web parsing foundation covers URL parsing, HTTP response parsing, HTML tokenization, and entity sanitization.
- A user-space Kesh Browser is packaged and wired to the network fetch syscall for basic HTTP/HTTPS page retrieval and text-oriented HTML rendering.

## Explicit limitations

- A universal Wi-Fi driver is not complete. Real 802.11 support requires chipset-specific drivers, firmware handling, and hardware testing.
- Real GPU hardware acceleration is not complete. The current graphics acceleration is CPU SIMD plus a software 3D rasterizer.
- Full USB host/device stacks are not complete. The current USB layer discovers USB controllers through PCI.
- IPv6 is not implemented in the current network stack.
- The web layer is not yet a modern browser engine. Full CSS layout, JavaScript, WebAssembly, WebGL/WebGPU, browser sandboxing, storage, and broad site compatibility remain future work.
- HTTPS currently reaches the existing BearSSL-based TLS path, but certificate trust validation is not yet a complete system trust-store implementation.
- AHCI/NVMe/GPT storage support is not complete; the current persistent path is centered on the existing ATA/FAT32 architecture.
- The scheduler remains a simple cooperative scheduler. It is not a full preemptive SMP scheduler.
- A full production recovery/update system is not yet implemented.
- The KeshOS SDK was intentionally excluded from this build per project direction.

## Build environment limits

The sandbox does not provide QEMU, xorriso, mkisofs, or an equivalent ISO inspection tool. Runtime boot and real network/Wi-Fi hardware compatibility therefore still need to be tested on the target machine or QEMU environment by the project owner.
