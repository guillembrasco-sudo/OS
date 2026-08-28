.section .text
.global _start

_start:
    // 1. Asegurar que estamos ejecutando en la CPU principal (Core 0)
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF        // Filtrar ID del procesador
    cbz     x0, .cpu_principal   // Si es 0, continúa el arranque
.loop_espera:
    wfe                          // Pone en espera a los núcleos secundarios
    b       .loop_espera

.cpu_principal:
    // 2. Configurar el puntero de pila (Stack Pointer)
    ldr     x0, =_stack_top
    mov     sp, x0

    // 3. Limpiar la sección .bss (Poner a cero variables globales no inicializadas)
    ldr     x0, =_bss_start
    ldr     x1, =_bss_end
.loop_bss:
    cmp     x0, x1
    b.hs    .ir_a_kernel
    str     xzr, [x0], #8        // Escribe cero (xzr) y avanza 8 bytes
    b       .loop_bss

.ir_a_kernel:
    // 4. Saltar al Kernel escrito en C
    bl      kernel_main

.global _inf_loop
_inf_loop:
    wfi                          // Detener CPU de forma segura si C retorna
    b       _inf_loop
