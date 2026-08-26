[bits 64]
section .text
extern isr_common_dispatch
global isr_stub_table

%macro ISR_NOERRCODE 1
isr_stub_%1:
    push qword 0             ; Dummy error code
    push qword %1            ; Vector
    jmp isr_common
%endmacro

%macro ISR_ERRCODE 1
isr_stub_%1:
    push qword %1            ; Vector
    jmp isr_common
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8  ; #DF
ISR_NOERRCODE 9
ISR_ERRCODE   10 ; #TS
ISR_ERRCODE   11 ; #NP
ISR_ERRCODE   12 ; #SS
ISR_ERRCODE   13 ; #GP
ISR_ERRCODE   14 ; #PF
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17 ; #AC
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21 ; #CP

%assign i 22
%rep 8
ISR_NOERRCODE i
%assign i i+1
%endrep

ISR_ERRCODE   30 ; #SX (Security Exception en AMD64)
ISR_NOERRCODE 31

%assign i 32
%rep 16
ISR_NOERRCODE i
%assign i i+1
%endrep

isr_common:
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

    mov rdi, rsp              ; registers_t *regs
    call isr_common_dispatch

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

    add rsp, 16              ; Limpiar vector y error code
    iretq

section .rodata
isr_stub_table:
%assign i 0
%rep 48
    dq isr_stub_%[i]
%assign i i+1
%endrep