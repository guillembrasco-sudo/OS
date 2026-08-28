.section .text
.global _start
.ent _start

_start:
    .set noreorder
    mtc0    $zero, $12          # Deshabilitar interrupciones en el registro Status (CP0 Reg 12)

    la      $sp, _stack_top     # Cargar el puntero de pila ($sp es el registro $29)

    # Limpiar .bss
    la      $t0, _bss_start
    la      $t1, _bss_end
.loop_bss:
    beq     $t0, $t1, .ir_c
    nop                         # Delay slot obligatorio
    sb      $zero, 0($t0)
    addiu   $t0, $t0, 1
    b       .loop_bss
    nop                         # Delay slot

.ir_c:
    jal     kernel_main
    nop                         # Delay slot para el salto a C

_halt:
    b       _halt
    nop
.end _start
