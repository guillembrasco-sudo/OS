// include/kernel/tss.h
#ifndef TSS_H
#define TSS_H

#include <stdint.h>

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;   // Stack del kernel usada al entrar desde Ring 3 vía interrupción/syscall
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;   // Interrupt Stack Table: stack dedicada para #DF, NMI, etc.
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

void tss_init(uint64_t kernel_stack_top);
void tss_set_kernel_stack(uint64_t stack_top); // se llama en cada context switch
void enter_usermode(uint64_t entry_point, uint64_t user_stack_top);

#endif // TSS_H