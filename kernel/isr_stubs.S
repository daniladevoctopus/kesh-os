.text
.extern isr_common_handler
.macro ISR_NOERR num
a0 = \num
.globl isr\num
isr\num:
    pushq $0
    pushq $a0
    jmp isr_common_stub
.endm
.macro ISR_ERR num
a1 = \num
.globl isr\num
isr\num:
    pushq $a1
    jmp isr_common_stub
.endm
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR 29
ISR_ERR 30
ISR_NOERR 31
isr_common_stub:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %rdi
    call isr_common_handler
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq
.extern irq_common_handler
.macro IRQ num vector
.globl irq\num
irq\num:
    pushq $0
    pushq $\vector
    jmp irq_common_stub
.endm
IRQ 0,32
IRQ 1,33
IRQ 2,34
IRQ 3,35
IRQ 4,36
IRQ 5,37
IRQ 6,38
IRQ 7,39
IRQ 8,40
IRQ 9,41
IRQ 10,42
IRQ 11,43
IRQ 12,44
IRQ 13,45
IRQ 14,46
IRQ 15,47
irq_common_stub:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %rdi
    call irq_common_handler
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq
.extern syscall_dispatcher
.extern g_syscall_kernel_stack_top
.globl syscall_entry_stub
.globl process_switch_to_user
.globl g_active_user_context
.globl g_syscall_should_yield
.globl g_syscall_user_rip
.section .data
.align 16
g_syscall_user_rsp: .quad 0
g_syscall_user_rip: .quad 0
g_active_user_context: .quad 0
g_syscall_should_yield: .byte 0
.align 8
g_kernel_compositor_rsp: .quad 0
g_kernel_compositor_cr3: .quad 0
.text
syscall_entry_stub:
    movq %rsp, g_syscall_user_rsp(%rip)
    movq %rcx, g_syscall_user_rip(%rip)
    movq g_syscall_kernel_stack_top(%rip), %rsp
    pushq %rcx
    pushq %r11
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    movq %rax, %rdi
    movq 40(%rsp), %rsi
    movq 32(%rsp), %rdx
    movq 24(%rsp), %rcx
    movq 0(%rsp), %r8
    movq 16(%rsp), %r9
    subq $16, %rsp
    movq 24(%rsp), %rax
    movq %rax, 0(%rsp)
    call syscall_dispatcher
    addq $16, %rsp
    cmpb $0, g_syscall_should_yield(%rip)
    jne .handle_syscall_yield
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rsi
    popq %rdi
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    popq %r11
    popq %rcx
    movq g_syscall_user_rsp(%rip), %rsp
    sysretq
.handle_syscall_yield:
    movzbl g_syscall_should_yield(%rip), %edx
    movb $0, g_syscall_should_yield(%rip)
    movq g_active_user_context(%rip), %r8
    testq %r8, %r8
    jz .fallback_sysret
    movq %rax, 88(%r8)
    movq 0(%rsp), %rax
    movq %rax, 136(%r8)
    movq 8(%rsp), %rax
    movq %rax, 128(%r8)
    movq 16(%rsp), %rax
    movq %rax, 120(%r8)
    movq 24(%rsp), %rax
    movq %rax, 112(%r8)
    movq 32(%rsp), %rax
    movq %rax, 104(%r8)
    movq 40(%rsp), %rax
    movq %rax, 96(%r8)
    movq 48(%rsp), %rax
    movq %rax, 64(%r8)
    movq 56(%rsp), %rax
    movq %rax, 56(%r8)
    movq 64(%rsp), %rax
    movq %rax, 48(%r8)
    movq 72(%rsp), %rax
    movq %rax, 40(%r8)
    movq 80(%rsp), %rax
    movq %rax, 32(%r8)
    movq 88(%rsp), %rax
    movq %rax, 24(%r8)
    movq 96(%rsp), %rax
    movq %rax, 16(%r8)
    movq 104(%rsp), %rax
    movq %rax, 0(%r8)
    movq g_syscall_user_rsp(%rip), %rax
    movq %rax, 8(%r8)
    addq $112, %rsp
    movq g_kernel_compositor_cr3(%rip), %rax
    movq %rax, %cr3
    movq g_kernel_compositor_rsp(%rip), %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    movl %edx, %eax
    sti
    ret
.fallback_sysret:
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rsi
    popq %rdi
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    popq %r11
    popq %rcx
    movq g_syscall_user_rsp(%rip), %rsp
    sysretq
.globl process_crash_return_to_compositor
process_crash_return_to_compositor:
    movq g_kernel_compositor_cr3(%rip), %rax
    movq %rax, %cr3
    movq g_kernel_compositor_rsp(%rip), %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    movl $2, %eax
    sti
    ret
.globl process_switch_to_user
process_switch_to_user:
    movb $0, g_syscall_should_yield(%rip)
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, g_kernel_compositor_rsp(%rip)
    movq %cr3, %rax
    movq %rax, g_kernel_compositor_cr3(%rip)
    movq %rdi, g_active_user_context(%rip)
    cmpq $0, 80(%rdi)
    jne .resume_existing_user
    movq $1, 80(%rdi)
    movq 72(%rdi), %rax
    movq %rax, %cr3
    pushq $0x3B
    pushq 8(%rdi)
    pushq $0x202
    pushq $0x43
    pushq 0(%rdi)
    movw $0x3B, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    xorl %eax, %eax
    xorl %ebx, %ebx
    xorl %ecx, %ecx
    xorl %edx, %edx
    xorl %esi, %esi
    xorl %edi, %edi
    xorl %ebp, %ebp
    xorl %r8d, %r8d
    xorl %r9d, %r9d
    xorl %r10d, %r10d
    xorl %r11d, %r11d
    xorl %r12d, %r12d
    xorl %r13d, %r13d
    xorl %r14d, %r14d
    xorl %r15d, %r15d
    iretq
.resume_existing_user:
    movq 72(%rdi), %rax
    movq %rax, %cr3
    pushq 8(%rdi)
    pushq 16(%rdi)
    pushq 24(%rdi)
    pushq 32(%rdi)
    movq 24(%rdi), %rbp
    movq 32(%rdi), %rbx
    movq 40(%rdi), %r12
    movq 48(%rdi), %r13
    movq 56(%rdi), %r14
    movq 64(%rdi), %r15
    movq 112(%rdi), %rdx
    movq 104(%rdi), %rsi
    movq 120(%rdi), %r8
    movq 128(%rdi), %r9
    movq 136(%rdi), %r10
    movq 16(%rdi), %r11
    movq 0(%rdi), %rcx
    movq 8(%rdi), %rsp
    movq 88(%rdi), %rax
    movq 96(%rdi), %rdi
    sysretq

.globl enter_user_mode
enter_user_mode:
    movq g_syscall_kernel_stack_top(%rip), %rsp
    pushq $0x3B
    pushq %rsi
    pushq $0x202
    pushq $0x43
    pushq %rdi
    movw $0x3B, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    iretq
.globl process_save_kernel_state
.globl process_return_to_kernel
.globl g_kernel_saved_pml4
.section .data
g_saved_kernel_rsp: .quad 0
g_saved_kernel_rbp: .quad 0
g_saved_kernel_rbx: .quad 0
g_saved_kernel_r12: .quad 0
g_saved_kernel_r13: .quad 0
g_saved_kernel_r14: .quad 0
g_saved_kernel_r15: .quad 0
g_saved_kernel_rip: .quad 0
g_kernel_saved_pml4: .quad 0
.text
process_save_kernel_state:
    movq (%rsp), %rax
    movq %rax, g_saved_kernel_rip(%rip)
    leaq 8(%rsp), %rax
    movq %rax, g_saved_kernel_rsp(%rip)
    movq %rbp, g_saved_kernel_rbp(%rip)
    movq %rbx, g_saved_kernel_rbx(%rip)
    movq %r12, g_saved_kernel_r12(%rip)
    movq %r13, g_saved_kernel_r13(%rip)
    movq %r14, g_saved_kernel_r14(%rip)
    movq %r15, g_saved_kernel_r15(%rip)
    movq %cr3, %rax
    movq %rax, g_kernel_saved_pml4(%rip)
    xorl %eax, %eax
    ret
process_return_to_kernel:
    movw $0x30, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movq g_kernel_saved_pml4(%rip), %rax
    movq %rax, %cr3
    movq g_saved_kernel_rsp(%rip), %rsp
    movq g_saved_kernel_rbp(%rip), %rbp
    movq g_saved_kernel_rbx(%rip), %rbx
    movq g_saved_kernel_r12(%rip), %r12
    movq g_saved_kernel_r13(%rip), %r13
    movq g_saved_kernel_r14(%rip), %r14
    movq g_saved_kernel_r15(%rip), %r15
    movl $1, %eax
    sti
    jmp *g_saved_kernel_rip(%rip)
.globl user_mode_test_payload
.globl user_mode_test_payload_end
user_mode_test_payload:
    movq $1, %rax
    leaq user_msg(%rip), %rdi
    syscall
    movq $4, %rax
    movq $999, %rdi
    syscall
    xorl %eax, %eax
    xorl %edi, %edi
    syscall
.Luser_hang:
    hlt
    jmp .Luser_hang
user_msg:
    .asciz "[RING 3 USER PROCESS] Hello from independent Ring 3 User Space!\n"
.align 16
user_mode_test_payload_end:
