# KeshOS unified build automation
PYTHON ?= python3
NINJA ?= ninja
QEMU ?= qemu-system-x86_64

.PHONY: all ninja iso qemu run clean

all: ninja

ninja:
	@$(PYTHON) generate_ninja.py
	@$(NINJA)

iso:
	@./build.sh

qemu: iso
	@$(QEMU) -cdrom build/keshos.iso -m 2048 -smp 2 -vga std -display gtk -serial stdio -no-reboot -no-shutdown

run: qemu

clean:
	rm -rf build ready
