& "D:\qemu\qemu-system-x86_64.exe" `
    -cdrom build/keshos.iso `
    -m 2048 `
    -vga std `
    -audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0 `
    -device AC97,audiodev=snd0 `
    -serial stdio `
    -no-shutdown `
    -no-reboot
