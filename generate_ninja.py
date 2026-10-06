import os
import shutil
import sys

DOOM_SRCS = [
    "am_map.c", "d_event.c", "d_items.c", "d_iwad.c", "d_loop.c", "d_main.c", "d_mode.c", "d_net.c",
    "doomdef.c", "doomgeneric.c", "doomstat.c", "dstrings.c", "dummy.c",
    "f_finale.c", "f_wipe.c", "g_game.c", "hu_lib.c", "hu_stuff.c",
    "i_cdmus.c", "i_endoom.c", "i_input.c", "i_joystick.c", "i_scale.c", "i_sound.c",
    "i_system_fs.c", "i_timer.c", "i_video.c", "info.c",
    "m_argv.c", "m_bbox.c", "m_cheat.c", "m_config.c", "m_controls.c", "m_fixed.c",
    "m_menu.c", "m_misc.c", "m_random.c", "memio.c",
    "p_ceilng.c", "p_doors.c", "p_enemy.c", "p_floor.c", "p_inter.c", "p_lights.c",
    "p_map.c", "p_maputl.c", "p_mobj.c", "p_plats.c", "p_pspr.c", "p_saveg.c", "p_setup.c",
    "p_sight.c", "p_spec.c", "p_switch.c", "p_telept.c", "p_tick.c", "p_user.c",
    "r_bsp.c", "r_data.c", "r_draw.c", "r_main.c", "r_plane.c", "r_segs.c", "r_sky.c", "r_things.c",
    "s_sound.c", "sha1.c", "sounds.c", "st_lib.c", "st_stuff.c", "statdump.c", "tables.c",
    "v_video.c", "w_checksum.c", "w_file.c", "w_file_stdc.c", "w_main.c", "w_wad.c",
    "wi_stuff.c", "z_zone.c"
]

KERNEL_C_SRCS = [
    ("kernel/kernel.c", "build/kernel/kernel.o"),
    ("kernel/cmd_mode.c", "build/kernel/cmd_mode.o"),
    ("kernel/linux_syscall.c", "build/kernel/linux_syscall.o"),
    ("kernel/linux_signal.c", "build/kernel/linux_signal.o"),
    ("kernel/unix_ipc.c", "build/kernel/unix_ipc.o"),
    ("kernel/evdev.c", "build/kernel/evdev.o"),
    ("kernel/drm_fb.c", "build/kernel/drm_fb.o"),
    ("kernel/gdt.c", "build/kernel/gdt.o"),
    ("kernel/syscall.c", "build/kernel/syscall.o"),
    ("kernel/memory.c", "build/kernel/memory.o"),
    ("kernel/process.c", "build/kernel/process.o"),
    ("kernel/cpu.c", "build/kernel/cpu.o"),
    ("kernel/apic.c", "build/kernel/apic.o"),
    ("kernel/elf.c", "build/kernel/elf.o"),
    ("kernel/uwindow.c", "build/kernel/uwindow.o"),
    ("kernel/vfs.c", "build/kernel/vfs.o"),
    ("kernel/include/graphics.c", "build/kernel/graphics.o"),
    ("kernel/include/idt.c", "build/kernel/idt.o"),
    ("kernel/include/pic.c", "build/kernel/pic.o"),
    ("kernel/panic.c", "build/kernel/panic.o"),
    ("kernel/timer.c", "build/kernel/timer.o"),
    ("kernel/random.c", "build/kernel/random.o"),
    ("kernel/service.c", "build/kernel/service.o"),
    ("kernel/tty.c", "build/kernel/tty.o"),
    ("kernel/input.c", "build/kernel/input.o"),
    ("kernel/socket.c", "build/kernel/socket.o"),
    ("kernel/fd.c", "build/kernel/fd.o"),
    ("kernel/ipc.c", "build/kernel/ipc.o"),
    ("kernel/display.c", "build/kernel/display.o"),
    ("kernel/block.c", "build/kernel/block.o"),
    ("kernel/nvme.c", "build/kernel/nvme.o"),
    ("kernel/gpt.c", "build/kernel/gpt.o"),
    ("kernel/acpi.c", "build/kernel/acpi.o"),
    ("kernel/log.c", "build/kernel/log.o"),
    ("kernel/serial.c", "build/kernel/serial.o"),
    ("boot/loading/load_logo.c", "build/boot/loading/load_logo.o"),
    ("src/desktop.c", "build/src/desktop.o"),
    ("src/drivers/system/keyboard.c", "build/src/drivers/system/keyboard.o"),
    ("src/drivers/system/mouse.c", "build/src/drivers/system/mouse.o"),
    ("src/drivers/system/ac97.c", "build/src/drivers/system/ac97.o"),
    ("src/drivers/system/hda.c", "build/src/drivers/system/hda.o"),
    ("src/drivers/system/sound_manager.c", "build/src/drivers/system/sound_manager.o"),
    ("src/drivers/system/ata.c", "build/src/drivers/system/ata.o"),
    ("src/drivers/system/fat32.c", "build/src/drivers/system/fat32.o"),
    ("src/drivers/system/iso9660.c", "build/src/drivers/system/iso9660.o"),
    ("src/drivers/pci/pci.c", "build/src/drivers/pci/pci.o"),
    ("src/drivers/usb/usb.c", "build/src/drivers/usb/usb.o"),
    ("src/drivers/net/netdev.c", "build/src/drivers/net/netdev.o"),
    ("src/drivers/net/e1000.c", "build/src/drivers/net/e1000.o"),
    ("src/drivers/net/rtl8139.c", "build/src/drivers/net/rtl8139.o"),
    ("src/drivers/net/virtio_net.c", "build/src/drivers/net/virtio_net.o"),
    ("src/drivers/net/net_stack.c", "build/src/drivers/net/net_stack.o"),
    ("src/drivers/net/tls/kesh_tls.c", "build/src/drivers/net/tls/kesh_tls.o"),
    ("kpm/kea_verify.c", "build/kpm/kea_verify.o"),
    ("kpm/trusted_keys.c", "build/kpm/trusted_keys.o"),
    ("kpm/system_update.c", "build/kpm/system_update.o"),
    ("kpm/kpm_os.c", "build/kpm/kpm_os.o"),
    ("src/gui/apps/file/file_manager.c", "build/src/gui/apps/file/file_manager.o"),
    ("src/gui/apps/music/music_app.c", "build/src/gui/apps/music/music_app.o"),
    ("src/gui/apps/about/about_app.c", "build/src/gui/apps/about/about_app.o"),
    ("src/gui/apps/calc/calc_logic.c", "build/src/gui/apps/calc/calc_logic.o"),
    ("src/gui/apps/calc/calc_app.c", "build/src/gui/apps/calc/calc_app.o"),
    ("src/gui/anim/genie_anim.c", "build/src/gui/anim/genie_anim.o"),
    ("src/gui/anim/win_chrome.c", "build/src/gui/anim/win_chrome.o"),
    ("src/gui/apps/terminal/terminal_app.c", "build/src/gui/apps/terminal/terminal_app.o"),
    ("src/gui/apps/settings/settings_app.c", "build/src/gui/apps/settings/settings_app.o"),
    ("src/gui/apps/doom/doom_app.c", "build/src/gui/apps/doom/doom_app.o"),
    ("src/gui/apps/doom/doomgeneric_igoros.c", "build/src/gui/apps/doom/doomgeneric_igoros.o"),
    ("src/gui/apps/doom/doom_fs_libc.c", "build/src/gui/apps/doom/doom_fs_libc.o"),
    ("src/gui/bmp_loader.c", "build/src/gui/bmp_loader.o"),
    ("src/gui/cursor/cursor.c", "build/src/gui/cursor/cursor.o"),
    ("src/gui/font.c", "build/src/gui/font.o"),
    ("src/graphics.c", "build/src/graphics.o"),
    ("src/gfx/gpu.c", "build/src/gfx/gpu.o"),
    ("src/gfx/kesh3d.c", "build/src/gfx/kesh3d.o"),
    ("src/web/web.c", "build/src/web/web.o"),
    ("src/gui/wm_config.c", "build/src/gui/wm_config.o"),
]

ASM_SRCS = [
    ("kernel/resources.S", "build/kernel/resources.o"),
    ("kernel/isr_stubs.S", "build/kernel/isr_stubs.o"),
]

for d in DOOM_SRCS:
    KERNEL_C_SRCS.append((f"src/gui/apps/doom/engine/{d}", f"build/src/gui/apps/doom/engine/{d.replace('.c', '.o')}"))

def find_tool(name, env_key):
    explicit = os.environ.get(env_key)
    if explicit:
        return explicit.replace("\\", "/")

    roots = []
    if os.environ.get("KESHOS_TOOLCHAIN"):
        root = os.environ["KESHOS_TOOLCHAIN"]
        roots.extend([root, os.path.join(root, "bin")])
    roots.extend([
        "D:/keshos-toolchain/KeshBE/bin",
        os.path.expanduser("~/.local/keshos-toolchain/bin"),
    ])

    exe_names = [name + ".exe", name]
    for root in roots:
        for exe in exe_names:
            cand = os.path.join(root, exe)
            if os.path.exists(cand):
                return cand.replace("\\", "/")

    found = shutil.which(name) or shutil.which(name + ".exe")
    if found:
        return found.replace("\\", "/")
    return name

cc_cmd = find_tool("clang", "CC")
cxx_cmd = find_tool("clang++", "CXX")
ld_cmd = find_tool("ld.lld", "LD")
ar_cmd = find_tool("llvm-ar", "AR")
python_cmd = sys.executable.replace("\\", "/")

ninja_content = f"""cc = {cc_cmd}
cxx = {cxx_cmd}
kernel_ld = {ld_cmd}
ar = {ar_cmd}
python = {python_cmd}

cflags = --target=x86_64-unknown-elf -m64 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-red-zone -mcmodel=kernel -O2 -Wall -Wextra -Wno-unused -Wno-unused-parameter -Wno-pointer-bool-conversion -DNULL=0 -Isrc/gui/apps/doom/fs_include -Iinclude -Ikernel -Isrc -Ikernel/include -Isrc/drivers/net/bearssl/inc -Isrc/drivers/net/bearssl/src -Isrc/gui/apps/doom -Isrc/gui/apps/doom/engine -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200
asm_cflags = --target=x86_64-unknown-elf -m64 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mno-red-zone
ldflags = -m elf_x86_64 -no-pie -T kernel/linker.ld

user_cflags = --target=x86_64-unknown-elf -m64 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -mcmodel=small -O2 -Wall -Wextra -Iuserspace/include -Isrc/gui
user_cxxflags = --target=x86_64-unknown-elf -m64 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -fno-exceptions -fno-rtti -fno-threadsafe-statics -mcmodel=small -O2 -Wall -Wextra -Iuserspace/include -Isrc/gui
user_ldflags = -m elf_x86_64 -no-pie -T userspace/app.ld

rule cc
  command = $cc $cflags -c $in -o $out
  description = CC $out

rule ar
  command = $ar rcs $out $in
  description = AR $out

rule kernel_asm
  command = $cc $asm_cflags -c $in -o $out
  description = ASM $out

rule link
  command = $kernel_ld $ldflags $in -o $out
  description = LINK $out

rule user_cc
  command = $cc $user_cflags -c $in -o $out
  description = USER_CC $out

rule user_cxx
  command = $cxx $user_cxxflags -c $in -o $out
  description = USER_CXX $out

rule user_asm
  command = $cc $asm_cflags -c $in -o $out
  description = USER_ASM $out

rule user_link
  command = $kernel_ld $user_ldflags $in -o $out
  description = USER_LINK $out

# Userspace Standalone Applications
build build/userspace/crt0.o: user_asm userspace/lib/crt0.S
build build/userspace/kesh.o: user_cc userspace/lib/kesh.c
build build/userspace/font.o: user_cc src/gui/font.c

build build/userspace/notepad.o: user_cc userspace/apps/notepad/main.c
build build/apps/notepad.elf: user_link build/userspace/crt0.o build/userspace/kesh.o build/userspace/font.o build/userspace/notepad.o | userspace/app.ld

build build/userspace/explorer.o: user_cxx userspace/apps/explorer/main.cpp
build build/apps/explorer.elf: user_link build/userspace/crt0.o build/userspace/kesh.o build/userspace/font.o build/userspace/explorer.o | userspace/app.ld

build build/userspace/taskmgr.o: user_cc userspace/apps/taskmgr/main.c
build build/apps/taskmgr.elf: user_link build/userspace/crt0.o build/userspace/kesh.o build/userspace/font.o build/userspace/taskmgr.o | userspace/app.ld

build build/userspace/paint.o: user_cc userspace/apps/paint/main.c
build build/apps/paint.elf: user_link build/userspace/crt0.o build/userspace/kesh.o build/userspace/font.o build/userspace/paint.o | userspace/app.ld

build build/userspace/shell.o: user_cc userspace/apps/shell/main.c
build build/apps/shell.elf: user_link build/userspace/crt0.o build/userspace/kesh.o build/userspace/font.o build/userspace/shell.o | userspace/app.ld

build build/userspace/settings.o: user_cxx userspace/apps/settings/main.cpp
build build/apps/settings.elf: user_link build/userspace/crt0.o build/userspace/kesh.o build/userspace/font.o build/userspace/settings.o | userspace/app.ld

build build/userspace/browser.o: user_cc userspace/apps/browser/main.c
build build/apps/browser.elf: user_link build/userspace/crt0.o build/userspace/kesh.o build/userspace/font.o build/userspace/browser.o | userspace/app.ld

rule kea_pack
  command = $python tools/kea-pack.py --elf $in --out $out --name $app_name --ver 1.0.0 --author KeshOS --category $app_cat --desc $app_desc --perms $app_perms
  description = KEA_PACK $out

build build/apps/notepad.kea: kea_pack build/apps/notepad.elf
  app_name = Notepad
  app_cat = Utilities
  app_desc = "Native text editor for KeshOS"
  app_perms = 0xA

build build/apps/explorer.kea: kea_pack build/apps/explorer.elf
  app_name = Explorer
  app_cat = System
  app_desc = "File manager for KeshOS"
  app_perms = 0xA

build build/apps/taskmgr.kea: kea_pack build/apps/taskmgr.elf
  app_name = TaskMgr
  app_cat = System
  app_desc = "Process manager and system monitor"
  app_perms = 0xA

build build/apps/paint.kea: kea_pack build/apps/paint.elf
  app_name = Paint
  app_cat = Graphics
  app_desc = "Native drawing and sketching app"
  app_perms = 0xA

build build/apps/shell.kea: kea_pack build/apps/shell.elf
  app_name = Shell
  app_cat = System
  app_desc = "Native modular desktop shell"
  app_perms = 0xA

build build/apps/settings.kea: kea_pack build/apps/settings.elf
  app_name = Settings
  app_cat = System
  app_desc = "System Settings for KeshOS"
  app_perms = 0xA

build build/apps/browser.kea: kea_pack build/apps/browser.elf
  app_name = Browser
  app_cat = Internet
  app_desc = "Native KeshOS web client"
  app_perms = 0xB

build build/userspace/installer.o: user_cc userspace/apps/installer/main.c
build build/apps/installer.elf: user_link build/userspace/crt0.o build/userspace/kesh.o build/userspace/font.o build/userspace/installer.o | userspace/app.ld

build build/apps/installer.kea: kea_pack build/apps/installer.elf
  app_name = KeshInstaller
  app_cat = System
  app_desc = "Live KeshOS installer"
  app_perms = 0xA

build build/userspace/about.o: user_cc userspace/apps/about/main.c
build build/apps/about.elf: user_link build/userspace/crt0.o build/userspace/kesh.o build/userspace/font.o build/userspace/about.o | userspace/app.ld

build build/apps/about.kea: kea_pack build/apps/about.elf
  app_name = About
  app_cat = System
  app_desc = "About KeshOS system information"
  app_perms = 0xA

"""

# BearSSL static library compilation
bad_bearssl_names = {'sysrng.c', 'aes_x86ni.c', 'aes_x86ni_cbcdec.c', 'aes_x86ni_cbcenc.c', 'aes_x86ni_ctr.c', 'aes_x86ni_ctrcbc.c', 'chacha20_sse2.c', 'ghash_pclmul.c'}
bearssl_objs = []
if os.path.exists("src/drivers/net/bearssl/src"):
    for root, dirs, files in os.walk("src/drivers/net/bearssl/src"):
        for f in sorted(files):
            if f.endswith(".c") and f not in bad_bearssl_names:
                src_path = os.path.join(root, f).replace("\\", "/")
                rel = os.path.relpath(src_path, "src/drivers/net/bearssl/src").replace("\\", "_").replace("/", "_").replace(".c", ".o")
                obj_path = f"build/bearssl/{rel}"
                bearssl_objs.append((src_path, obj_path))

for src, obj in bearssl_objs:
    ninja_content += f"build {obj}: cc {src}\n"

b_objs_str = " ".join([obj for _, obj in bearssl_objs])
ninja_content += f"\nbuild build/libbearssl.a: ar {b_objs_str}\n\n"

all_objs = []
for src, obj in KERNEL_C_SRCS:
    ninja_content += f"build {obj}: cc {src}\n"
    all_objs.append(obj)

for src, obj in ASM_SRCS:
    if "resources.S" in src:
        ninja_content += f"build {obj}: kernel_asm {src} | build/apps/notepad.kea build/apps/explorer.kea build/apps/taskmgr.kea build/apps/paint.kea build/apps/shell.kea build/apps/settings.kea build/apps/about.kea build/apps/browser.kea build/apps/installer.kea\n"
    else:
        ninja_content += f"build {obj}: kernel_asm {src}\n"
    all_objs.append(obj)

ninja_content += f"\nbuild build/kernel.elf: link {' '.join(all_objs)} build/libbearssl.a\n"
ninja_content += f"""
rule iso
  command = {python_cmd} tools/make_iso.py
  description = ISO $out

build build/keshos.iso: iso build/kernel.elf build/apps/notepad.elf build/apps/explorer.elf build/apps/settings.elf build/apps/about.elf build/apps/installer.elf build/apps/notepad.kea build/apps/explorer.kea build/apps/taskmgr.kea build/apps/paint.kea build/apps/shell.kea build/apps/settings.kea build/apps/about.kea build/apps/browser.kea build/apps/installer.kea | boot/limine/limine-bios-cd.bin boot/limine/limine-uefi-cd.bin boot/limine/limine-bios.sys boot/limine/BOOTX64.EFI boot/limine/limine.conf boot/limine/limine.cfg boot/wm.conf

build iso: phony build/keshos.iso

default build/apps/notepad.kea build/apps/explorer.kea build/apps/taskmgr.kea build/apps/paint.kea build/apps/shell.kea build/apps/settings.kea build/apps/about.kea build/apps/browser.kea build/apps/installer.kea build/kernel.elf
"""

with open("build.ninja", "w", encoding="utf-8") as f:
    f.write(ninja_content)

print(f"Generated build.ninja with {len(all_objs)} kernel objs, {len(bearssl_objs)} BearSSL objs.")
