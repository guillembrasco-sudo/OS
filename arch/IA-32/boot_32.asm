#define MULTIBOOT_MAGIC    0x1BADB002
#define MULTIBOOT_FLAGS    0x00000003

.section .multiboot
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS) # Checksum obligatorio

.section .text
.global _start

_start:
    cli                         # Deshabilitar interrupciones de hardware
    movl    $_stack_top, %esp   # Configurar puntero de pila (ESP)

    # Pasar los datos de Multiboot (Magic e Información) como argumentos a C
    pushl   %ebx                # Argumento 2: Puntero a la estructura de info
    pushl   %eax                # Argumento 1: Magic number

    call    kernel_main

_halt:
    hlt
    jmp     _halt
