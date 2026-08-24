[bits 64]
section .text
extern exception_handler
global isr_stub_table

; Macro para definir ISRs de excepciones
%macro ISR_NOERRCODE 1
isr_stub_%1:
    push qword 0             ; Dummy error code
    push qword %1            ; Vector
    jmp isr_common
%endmacro

%macro ISR_ERRCODE 1
isr_stub_%1:
    push qword %1            ; Vector (el código de error ya fue empujado por la CPU)
    jmp isr_common
%endmacro

; Definición de las 32 excepciones
ISR_NOERRCODE 0  ; #DE
ISR_NOERRCODE 1  ; #DB
ISR_NOERRCODE 2  ; NMI
ISR_NOERRCODE 3  ; #BP
ISR_NOERRCODE 4  ; #OF
ISR_NOERRCODE 5  ; #BR
ISR_NOERRCODE 6  ; #UD (Opcode invalido)
ISR_NOERRCODE 7  ; #NM
ISR_ERRCODE   8  ; #DF (Double fault)
ISR_NOERRCODE 9  ; Coprocessor Segment Overrun
ISR_ERRCODE   10 ; #TS
ISR_ERRCODE   11 ; #NP
ISR_ERRCODE   12 ; #SS
ISR_ERRCODE   13 ; #GP (General Protection)
ISR_ERRCODE   14 ; #PF (Page Fault)
ISR_NOERRCODE 15 ; Reservado
ISR_NOERRCODE 16 ; #MF
ISR_ERRCODE   17 ; #AC
ISR_NOERRCODE 18 ; #MC
ISR_NOERRCODE 19 ; #XM
ISR_NOERRCODE 20 ; #VE
ISR_ERRCODE   21 ; #CP
%assign i 22
%rep 10
ISR_NOERRCODE i
%assign i i+1
%endrep

isr_common:
    ; Guardar registros de propósito general
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

    ; Pasar argumentos según ABI System V (RDI = Vector, RSI = Error Code)
    mov rdi, [rsp + 120]     ; Vector[cite: 12]
    mov rsi, [rsp + 128]     ; Error Code[cite: 12]

    ; Ajustar alineación de la pila a 16 bytes
    sub rsp, 8
    call exception_handler
    add rsp, 8

    ; Restaurar registros
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

    add rsp, 16              ; Limpiar vector y error code de la pila
    iretq                    ; Retorno de interrupción de 64 bits

section .rodata
isr_stub_table:
%assign i 0
%rep 32
    dq isr_stub_%[i]
%assign i i+1
%endrep