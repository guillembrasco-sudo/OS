// include/kernel/interrupt.h
#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

// x86_64: no hay pusha/popa. El stub de ensamblador debe empujar los
// registros de propósito general uno a uno en este orden exacto para que
// el struct calce con la pila. Ver 2.5 para el stub .asm correspondiente.
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;                    // empujados por el stub común
    uint64_t rip, cs, rflags, rsp, ss;             // empujados automáticamente por la CPU
} registers_t;

typedef void (*isr_handler_t)(registers_t *regs);

void isr_register_handler(uint8_t int_no, isr_handler_t handler);
void isr_common_dispatch(registers_t *regs); // llamado desde el stub asm

#endif // INTERRUPT_H