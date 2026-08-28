.section ".text"
.global _start

_start:
    # 1. Configurar la pila en el registro obligatorio r1
    lis     1, _stack_top@h     # Cargar la parte alta de la dirección
    ori     1, 1, _stack_top@l  # Cargar la parte baja de la dirección

    # 2. Asegurar que las interrupciones están apagadas en el Machine State Register (MSR)
    mfmsr   3
    rlwinm  3, 3, 0, 17, 15     # Apagar el bit de interrupciones externas (EE)
    mtmsr   3

    # 3. Saltar a la función en C
    bl      kernel_main
    nop

_halt:
    b       _halt               # Bucle infinito
