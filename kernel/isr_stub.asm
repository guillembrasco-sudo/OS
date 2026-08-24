; kernel/isr_stub.asm — stub común x86_64 (NASM), referenciado por 2.4
; Cada ISR con código de error empuja el vector y salta aquí; las que no
; tienen código de error (la mayoría de IRQs) deben empujar un 0 dummy
; ANTES de saltar, para que el layout de registers_t sea uniforme.

extern isr_common_dispatch

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

    mov rdi, rsp        ; System V ABI: 1er argumento entero en RDI
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

    add rsp, 16          ; descarta int_no + err_code empujados antes del salto
    iretq

; Ejemplo para el vector 14 (Page Fault, SÍ empuja err_code la CPU):
global isr14
isr14:
    push qword 14        ; int_no
    jmp isr_common_stub  ; err_code ya fue empujado por la CPU antes de esto