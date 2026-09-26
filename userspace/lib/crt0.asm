; входная точка программ
[bits 64]
global _start
extern main
extern kesh_exit

section .text
_start:

    and rsp, -16

    call main

    mov rdi, rax
    call kesh_exit

.hang:
    hlt
    jmp .hang
