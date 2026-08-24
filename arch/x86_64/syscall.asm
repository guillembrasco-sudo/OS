bits 64
global syscall_entry
extern syscall_dispatch

section .text
syscall_entry:
    ; MSR setup and a per-CPU kernel stack are required before enabling this gate.
    push r11
    push rcx
    mov r9, r8
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    call syscall_dispatch
    pop rcx
    pop r11
    sysret