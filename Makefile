# ==========================================
# KeshOS Makefile (Unified Build Automation)
# ==========================================

PYTHON ?= python
NINJA ?= ninja
QEMU ?= qemu-system-x86_64

.PHONY: all ninja iso run qemu clean

all: ninja

ninja:
	@$(PYTHON) generate_ninja.py
	@$(NINJA)

iso: ninja
	@$(PYTHON) tools/make_iso.py

qemu: iso
	$(QEMU) \
		-cdrom build/keshos.iso \
		-m 2048 \
		-vga std \
		-netdev user,id=net0 -device e1000,netdev=net0 \
		-serial stdio \
		-no-shutdown \
		-no-reboot

run: qemu

clean:
	@$(NINJA) -t clean || true
	@powershell -Command "Remove-Item -Recurse -Force build, ready -ErrorAction SilentlyContinue"