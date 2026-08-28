.section .text
.global _start

_start:
    # 1. Deshabilitar interrupciones globales en el registro de estado (sstatus)
    csrw    sstatus, zero

    # 2. Configurar el puntero de pila (sp)
    la      sp, _stack_top

    # 3. Limpiar la sección .bss usando el registro zero por hardware
    la      t0, _bss_start
    la      t1, _bss_end
.loop_bss:
    bgeu    t0, t1, .ir_c
    sd      zero, 0(t0)         # Escribe 64 bits (8 bytes) de ceros directamente
    addi    t0, t0, 8
    j       .loop_bss

.ir_c:
    # 4. Saltar a C
    tail    kernel_main         # Salto directo optimizado (Tail call)

_halt:
    wfi                         # Wait For Interrupt (Bajo consumo)
    j       _halt
