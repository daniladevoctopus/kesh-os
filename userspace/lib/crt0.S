.text
.globl _start
.extern main
.extern kesh_exit
_start:
    andq $-16, %rsp
    call main
    movq %rax, %rdi
    call kesh_exit
.Lhang:
    hlt
    jmp .Lhang
