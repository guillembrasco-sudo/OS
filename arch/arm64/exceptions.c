// Definición del marco de registros guardados al interrumpir (Trap Frame)
struct trap_frame {
    unsigned long regs[31]; // Registros X0 a X30
    unsigned long sp;       // Stack pointer
    unsigned long elr;      // Exception Link Register (dónde ocurrió el fallo)
    unsigned long spsr;     // Saved Program Status Register
};

// Manejador central para excepciones síncronas (como fallos de memoria)
void exception_handler_synchronous(struct trap_frame *tf) {
    unsigned long esr;
    // Leer el Exception Syndrome Register (ESR) para saber qué falló
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    
    unsigned long ec = esr >> 26; // Clase de excepción
    
    if (ec == 0x24 || ec == 0x25) { // Data Abort (Equivalente al Page Fault de x86)
        unsigned long far;
        asm volatile("mrs %0, far_el1" : "=r"(far)); // Dirección virtual que falló
        // Llamar a tu administrador de memoria (mm/page_fault)
        kernel_panic("Fallo de pagina en direccion virtual: 0x%x\n", far);
    } else {
        kernel_panic("Excepcion sincrona no manejada. Codigo: 0x%x\n", ec);
    }
}

// Manejador central para interrupciones de hardware (Temporizador, UART, etc.)
void exception_handler_irq(struct trap_frame *tf) {
    // Aquí interactúas con el GIC (Generic Interrupt Controller de ARM), 
    // que reemplaza al APIC de x86.
    unsigned int interrupt_id = gic_acknowledge_interrupt();
    
    if (interrupt_id == 30) { // Típicamente el temporizador del sistema (Timer)
        timer_handler();
    }
    
    gic_end_of_interrupt(interrupt_id);
}
