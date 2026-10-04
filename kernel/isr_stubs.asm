; стабы прерываний, держим стек ровным
section .text
bits 64

extern isr_common_handler

%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0        
    push qword %1        
    jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1        
    jmp isr_common_stub
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

isr_common_stub:

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp        
    call isr_common_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16          
    iretq

extern irq_common_handler

%macro IRQ 2
global irq%1
irq%1:
    push qword 0
    push qword %2
    jmp irq_common_stub
%endmacro

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

irq_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call irq_common_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

extern syscall_dispatcher
extern g_syscall_kernel_stack_top
global syscall_entry_stub
global process_switch_to_user
global g_active_user_context
global g_syscall_should_yield
global g_syscall_user_rip

section .data
align 16
g_syscall_user_rsp: dq 0
g_syscall_user_rip: dq 0
g_active_user_context: dq 0
g_syscall_should_yield: db 0
g_kernel_compositor_rsp: dq 0
g_kernel_compositor_cr3: dq 0

section .text
syscall_entry_stub:

    mov [rel g_syscall_user_rsp], rsp
    mov [rel g_syscall_user_rip], rcx
    mov rsp, [rel g_syscall_kernel_stack_top]

    push rcx             
    push r11             
    push rbp             
    push rbx             
    push r12             
    push r13             
    push r14             
    push r15             
    push rdi             
    push rsi             
    push rdx             
    push r8              
    push r9              
    push r10             

    mov r11, rax
    sub rsp, 16
    mov rax, [rsp + 16 + 8]
    mov [rsp], rax
    mov r9, [rsp + 16 + 16]
    mov r8, [rsp + 16 + 0]
    mov rcx, [rsp + 16 + 24]
    mov rdx, [rsp + 16 + 32]
    mov rsi, [rsp + 16 + 40]
    mov rdi, r11
    call syscall_dispatcher
    add rsp, 16

    cmp byte [rel g_syscall_should_yield], 0
    jne .handle_syscall_yield

    pop r10
    pop r9
    pop r8
    pop rdx
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11              
    pop rcx              

    mov rsp, [rel g_syscall_user_rsp]
    o64 sysret

.handle_syscall_yield:
    movzx edx, byte [rel g_syscall_should_yield]
    mov byte [rel g_syscall_should_yield], 0

    mov r8, [rel g_active_user_context]
    test r8, r8
    jz .fallback_sysret

    mov [r8 + 88], rax   

    mov rax, [rsp + 0]
    mov [r8 + 136], rax  
    mov rax, [rsp + 8]
    mov [r8 + 128], rax  
    mov rax, [rsp + 16]
    mov [r8 + 120], rax  
    mov rax, [rsp + 24]
    mov [r8 + 112], rax  
    mov rax, [rsp + 32]
    mov [r8 + 104], rax  
    mov rax, [rsp + 40]
    mov [r8 + 96], rax   
    mov rax, [rsp + 48]
    mov [r8 + 64], rax   
    mov rax, [rsp + 56]
    mov [r8 + 56], rax   
    mov rax, [rsp + 64]
    mov [r8 + 48], rax   
    mov rax, [rsp + 72]
    mov [r8 + 40], rax   
    mov rax, [rsp + 80]
    mov [r8 + 32], rax   
    mov rax, [rsp + 88]
    mov [r8 + 24], rax   
    mov rax, [rsp + 96]
    mov [r8 + 16], rax   
    mov rax, [rsp + 104]
    mov [r8 + 0], rax    

    mov rax, [rel g_syscall_user_rsp]
    mov [r8 + 8], rax    

    add rsp, 112

    mov rax, [rel g_kernel_compositor_cr3]
    mov cr3, rax

    mov rsp, [rel g_kernel_compositor_rsp]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    mov eax, edx
    sti                          
    ret

.fallback_sysret:
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11
    pop rcx
    mov rsp, [rel g_syscall_user_rsp]
    o64 sysret

global process_crash_return_to_compositor
process_crash_return_to_compositor:
    mov rax, [rel g_kernel_compositor_cr3]
    mov cr3, rax

    mov rsp, [rel g_kernel_compositor_rsp]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    mov eax, 2                   
    sti
    ret

process_switch_to_user:

    mov byte [rel g_syscall_should_yield], 0
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [rel g_kernel_compositor_rsp], rsp
    mov rax, cr3
    mov [rel g_kernel_compositor_cr3], rax

    mov [rel g_active_user_context], rdi

    cmp qword [rdi + 80], 0
    jne .resume_existing_user

    mov qword [rdi + 80], 1     

    mov rax, [rdi + 72]
    mov cr3, rax

    push qword 0x3B             
    push qword [rdi + 8]        
    push qword 0x202            
    push qword 0x43             
    push qword [rdi + 0]        

    mov ax, 0x3B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp
    xor r8d, r8d
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    xor r12d, r12d
    xor r13d, r13d
    xor r14d, r14d
    xor r15d, r15d

    iretq

.resume_existing_user:

    mov rax, [rdi + 72]
    mov cr3, rax

    mov rbp, [rdi + 24]
    mov rbx, [rdi + 32]
    mov r12, [rdi + 40]
    mov r13, [rdi + 48]
    mov r14, [rdi + 56]
    mov r15, [rdi + 64]
    mov rdx, [rdi + 112]
    mov rsi, [rdi + 104]
    mov r8,  [rdi + 120]
    mov r9,  [rdi + 128]
    mov r10, [rdi + 136]
    mov r11, [rdi + 16]         
    mov rcx, [rdi + 0]          
    mov rsp, [rdi + 8]          
    mov rax, [rdi + 88]         
    mov rdi, [rdi + 96]         
    o64 sysret

global enter_user_mode
enter_user_mode:

    mov rsp, [rel g_syscall_kernel_stack_top]

    push qword 0x3B
    push rsi
    push qword 0x202
    push qword 0x43
    push rdi

    mov ax, 0x3B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    iretq

global process_save_kernel_state
global process_return_to_kernel
global g_kernel_saved_pml4

section .data
g_saved_kernel_rsp: dq 0
g_saved_kernel_rbp: dq 0
g_saved_kernel_rbx: dq 0
g_saved_kernel_r12: dq 0
g_saved_kernel_r13: dq 0
g_saved_kernel_r14: dq 0
g_saved_kernel_r15: dq 0
g_saved_kernel_rip: dq 0
g_kernel_saved_pml4: dq 0

section .text
process_save_kernel_state:

    mov rax, [rsp]
    mov [rel g_saved_kernel_rip], rax

    lea rax, [rsp + 8]
    mov [rel g_saved_kernel_rsp], rax

    mov [rel g_saved_kernel_rbp], rbp
    mov [rel g_saved_kernel_rbx], rbx
    mov [rel g_saved_kernel_r12], r12
    mov [rel g_saved_kernel_r13], r13
    mov [rel g_saved_kernel_r14], r14
    mov [rel g_saved_kernel_r15], r15

    mov rax, cr3
    mov [rel g_kernel_saved_pml4], rax

    xor eax, eax
    ret

process_return_to_kernel:

    mov ax, 0x30
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov rax, [rel g_kernel_saved_pml4]
    mov cr3, rax

    mov rsp, [rel g_saved_kernel_rsp]
    mov rbp, [rel g_saved_kernel_rbp]
    mov rbx, [rel g_saved_kernel_rbx]
    mov r12, [rel g_saved_kernel_r12]
    mov r13, [rel g_saved_kernel_r13]
    mov r14, [rel g_saved_kernel_r14]
    mov r15, [rel g_saved_kernel_r15]

    mov rax, 1
    sti                          
    jmp qword [rel g_saved_kernel_rip]

global user_mode_test_payload
global user_mode_test_payload_end

user_mode_test_payload:

    mov rax, 1                  
    lea rdi, [rel user_msg]     
    syscall

    mov rax, 4                  
    mov rdi, 999
    syscall

    mov rax, 0                  
    mov rdi, 0
    syscall

.hang:
    hlt
    jmp .hang

user_msg:
    db "[RING 3 USER PROCESS] Hello from independent Ring 3 User Space!", 10, 0

align 16
user_mode_test_payload_end:
