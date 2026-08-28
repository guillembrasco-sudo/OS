.section .text
.global _start

_start:
    // 1. Deshabilitar interrupciones (IRQ y FIQ) en el registro de estado (CPSR)
    mrs     r0, cpsr
    orr     r0, r0, #0xC0
    msr     cpsr_c, r0

    // 2. Configurar el Stack Pointer (SP / r13)
    ldr     sp, =_stack_top

    // 3. Limpiar sección .bss
    ldr     r0, =_bss_start
    ldr     r1, =_bss_end
    mov     r2, #0
.loop_bss:
    cmp     r0, r1
    strlo   r2, [r0], #4
    blo     .loop_bss

    // 4. Saltar al kernel en C
    bl      kernel_main

_halt:
    wfi
    b       _halt
