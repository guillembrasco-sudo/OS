// kernel/panic.c
#include "kernel/panic.h"
#include <lib/printf.h>     // asumo que existe kprintf/terminal_write; ajusta al nombre real de tu driver de pantalla
#include <stdarg.h>

static inline uint64_t read_cr0(void) { uint64_t v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static inline uint64_t read_cr2(void) { uint64_t v; __asm__ volatile("mov %%cr2, %0" : "=r"(v)); return v; }
static inline uint64_t read_cr3(void) { uint64_t v; __asm__ volatile("mov %%cr3, %0" : "=r"(v)); return v; }

static void panic_halt_all_cores(void) {
    // En SMP habría que enviar un IPI (NMI) al resto de cores antes de
    // detener este; se deja como comentario porque el TODO aún no lista
    // SMP como implementado. Cuando lo esté, este es el punto de enganche.
    __asm__ volatile("cli");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void panic(const char *fmt, ...) {
    __asm__ volatile("cli");

    kprintf("\n\n*** KERNEL PANIC ***\n");
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args); // asumo una variante kvprintf(fmt, va_list) junto a tu kprintf
    va_end(args);
    kprintf("\n");

    kprintf("CR0=0x%lx CR2=0x%lx CR3=0x%lx\n", read_cr0(), read_cr2(), read_cr3());
    kprintf("System halted.\n");

    panic_halt_all_cores();
}

void panic_with_regs(registers_t *regs, const char *fmt, ...) {
    __asm__ volatile("cli");

    kprintf("\n\n*** KERNEL PANIC ***\n");
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
    kprintf("\n");

    kprintf("RAX=0x%lx RBX=0x%lx RCX=0x%lx RDX=0x%lx\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    kprintf("RSI=0x%lx RDI=0x%lx RBP=0x%lx RSP=0x%lx\n", regs->rsi, regs->rdi, regs->rbp, regs->rsp);
    kprintf("RIP=0x%lx CS=0x%lx RFLAGS=0x%lx\n", regs->rip, regs->cs, regs->rflags);
    kprintf("int_no=%lu err_code=0x%lx\n", regs->int_no, regs->err_code);
    kprintf("CR0=0x%lx CR2=0x%lx CR3=0x%lx\n", read_cr0(), read_cr2(), read_cr3());

    panic_halt_all_cores();
}