// kernel/tss.c
#include "kernel/tss.h"
#include "arch/x86_64/gdt.h" // asumo gdt_set_tss_descriptor(base, limit) existente o a añadir
#include <lib/string.h>

static tss_t tss __attribute__((aligned(16)));

// Stack de emergencia para Double Fault (#DF): si el fallo ocurre porque el
// propio stack del kernel está corrupto/agotado, seguir usando rsp0 para
// manejarlo es circular. IST1 apunta a una región separada solo para esto.
static uint8_t df_stack[4096] __attribute__((aligned(16)));

void tss_init(uint64_t kernel_stack_top) {
    memset(&tss, 0, sizeof(tss));
    tss.rsp0 = kernel_stack_top;
    tss.ist1 = (uint64_t)(df_stack + sizeof(df_stack));
    tss.iomap_base = sizeof(tss_t); // sin bitmap de puertos: Ring 3 no tiene acceso directo a I/O

    // Instala el descriptor de TSS en la GDT (entrada de 16 bytes en long
    // mode) y carga LTR. gdt_set_tss_descriptor es responsabilidad del
    // módulo gdt.c existente; si no existe todavía, es el otro archivo
    // pendiente real aquí — la firma que necesita es:
    //   void gdt_set_tss_descriptor(uint64_t base, uint32_t limit);
    gdt_set_tss_descriptor((uint64_t)&tss, sizeof(tss_t) - 1);

    __asm__ volatile("ltr %0" :: "r"((uint16_t)0x28) : "memory");
    // 0x28 asume que el descriptor de TSS queda en el índice 5 de la GDT
    // (5 * 8 = 0x28): [null, kcode, kdata, ucode, udata, tss]. Ajusta el
    // selector si tu layout de GDT difiere.
}

void tss_set_kernel_stack(uint64_t stack_top) {
    tss.rsp0 = stack_top;
}

// Construye manualmente el frame que IRETQ espera y salta a Ring 3.
// Selectores: 0x1B = user code (índice 3, RPL 3) | 0x23 = user data (índice 4, RPL 3)
void enter_usermode(uint64_t entry_point, uint64_t user_stack_top) {
    __asm__ volatile (
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"     // SS se carga vía el frame de iretq, no aquí

        "pushq $0x23\n"        // SS (user data, RPL 3)
        "pushq %0\n"            // RSP de usuario
        "pushfq\n"
        "popq %%rax\n"
        "orq $0x200, %%rax\n"  // fuerza IF=1: interrupciones habilitadas en Ring 3
        "pushq %%rax\n"        // RFLAGS
        "pushq $0x1B\n"        // CS (user code, RPL 3)
        "pushq %1\n"            // RIP de entrada
        "iretq\n"
        :
        : "r"(user_stack_top), "r"(entry_point)
        : "rax", "memory"
    );
    __builtin_unreachable();
}