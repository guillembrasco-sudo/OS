// kernel/isr.c
#include <kernel/interrupt.h>
#include <kernel/panic.h>
#include <drivers/irqdomain.h>
#include <arch/x86_64/io.h>

#define IDT_ENTRIES 256
static isr_handler_t handlers[IDT_ENTRIES] = {0};

void isr_register_handler(uint8_t int_no, isr_handler_t handler) {
    handlers[int_no] = handler;
}

void isr_common_dispatch(registers_t *regs) {
    if (regs->int_no >= 32 && regs->int_no < 48) {
        uint8_t irq = (uint8_t)(regs->int_no - 32);
        irq_dispatch(irq);
        if (irq >= 8)
            io_out8(0xA0, 0x20);
        io_out8(0x20, 0x20);
        return;
    }
    if (handlers[regs->int_no]) {
        handlers[regs->int_no](regs);
    } else {
        panic_with_regs(regs, "Excepción/IRQ #%lu sin handler registrado", regs->int_no);
    }
}

// --- Page Fault (vector 14) ------------------------------------------------
static inline uint64_t read_cr2(void) {
    uint64_t v;
    __asm__ volatile("mov %%cr2, %0" : "=r"(v));
    return v;
}

void page_fault_handler(registers_t *regs) {
    uint64_t faulting_address = read_cr2();

    int present  = !(regs->err_code & 0x1); // 0 = página no presente
    int rw       =  (regs->err_code & 0x2); // 1 = fallo en escritura
    int user     =  (regs->err_code & 0x4); // 1 = ocurrió en Ring 3
    int reserved =  (regs->err_code & 0x8); // 1 = bit reservado de la PTE sobrescrito
    int fetch    =  (regs->err_code & 0x10);// 1 = fallo por fetch de instrucción (NX)

    // Punto de enganche para copy-on-write / demand paging futuro:
    // aquí es donde, antes de hacer panic, consultarías el VMM para ver
    // si la dirección pertenece a una región válida pero no mapeada
    // (ej. stack de usuario creciendo, o página COW) y la resolverías
    // sin matar el proceso. De momento, sin VMM, todo fault es fatal.

    panic_with_regs(regs,
        "Page Fault en 0x%lx (present=%d write=%d user=%d reserved=%d fetch=%d)",
        faulting_address, present, rw, user, reserved, fetch);
}

void isr_install_defaults(void) {
    isr_register_handler(14, page_fault_handler);
}