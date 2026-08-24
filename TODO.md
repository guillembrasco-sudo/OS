Tareas y Componentes Faltantes (Roadmap de desarrollo)
[ Bootloader ] ──> [ GDT / IDT / PIC ] ──> [ PMM / VMM ] ──> [ Heap Allocator ]
                                                                   │
[ Shell / ELF ] <── [ Syscalls / Ring 3 ] <── [ Scheduler / TSS ] <┘
1. Planificación de Procesos y Multitarea (Scheduler)
Process Control Block (PCB): Estructura task_struct para almacenar PID, estado del proceso (Ready, Running, Blocked, Zombie), contexto de registros, pila del kernel y espacio de direcciones.

Algoritmo de Planificación: Implementación de un Round-Robin preentivo activado desde la IRQ0 (PIT).

Conmutación de Contexto (Context Switch): Guardado de registros de la tarea actual y restauración de los registros de la nueva tarea en ensamblador.

2. Transición a Ring 3 (User Space) & TSS
Task State Segment (TSS): Configuración e instalación de un segmento TSS en la GDT para especificar el puntero esp0 que la CPU utilizará al cambiar de Ring 3 a Ring 0 durante una interrupción.

Salto a Ring 3: Ejecución de una rutina de retorno simétrica (iret / iretq) que fuerce a la CPU a cambiar de nivel de privilegio cargando los segmentos 0x1B (User Code) y 0x23 (User Data).

3. Interfaz de Llamadas al Sistema (Syscalls)
Mecanismo de Disparo: Configuración de la puerta de interrupción int 0x80 o implementación de SYSENTER/SYSEXIT (SYSCALL/SYSRET en x86_64).

Dispatcher de Syscalls: Tabla de saltos indexada por número de syscall (ej. sys_read, sys_write, sys_yield, sys_exit, sys_malloc).

4. Asignador de Memoria Dinámica en Kernel (Kernel Heap)
Heap Manager (kmalloc / kfree): Sustitución de asignadores estáticos por un algoritmo Free-List con cabeceras de bloque (Boundary Tags), Slab Allocator o Buddy Allocation System para prevenir fragmentación externa e interna.

5. Sistema de Archivos Virtual (VFS) y Almacenamiento
Capa VFS: Abstracción de nodos de archivo (vfs_node_t) con punteros a funciones para read, write, open, close y readdir.

Ramdisk (Initrd): Implementación de un sistema de archivos en memoria (vía archivo TAR cargado como módulo Multiboot por GRUB).

Driver de Disco ATA PIO: Controlador para lectura/escritura de sectores en discos IDE/SATA.

Oportunidades de Mejora y Refactorización
1. Robustez en el Manejo de Excepciones y Registros
En la implementación actual de interrupciones, el desbordamiento o la captura de fallos de página (Page Fault) no extrae la totalidad de la información diagnóstica de la CPU.

Propuesta de Mejora (C / Assembly)
Es necesario capturar el registro CR2 (que contiene la dirección lineal que causó la falla) y estructurar el marco de registros de forma estricta.

C
// interrupt.h - Estructura estricta para el estado de la CPU
typedef struct {
    uint32_t ds;                                     // Selector de datos
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Salvados por pusha
    uint32_t int_no, err_code;                       // Número de interrupción y código de error
    uint32_t eip, cs, eflags, useresp, ss;           // Salvados automáticamente por la CPU
} __attribute__((packed)) registers_t;

void page_fault_handler(registers_t *regs) {
    uint32_t faulting_address;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(faulting_address));

    int present   = !(regs->err_code & 0x1); // Página no presente
    int rw        = regs->err_code & 0x2;    // Escritura (1) o Lectura (0)
    int user      = regs->err_code & 0x4;    // Ejecutado en Ring 3
    int reserved  = regs->err_code & 0x8;    // Sobrescritura de bits reservados

    kprintf("[KERNEL PANIC] Page Fault at 0x%x (flags: P=%d, R/W=%d, User=%d)\n",
            faulting_address, present, rw, user);
    
    __asm__ __volatile__("cli; hlt");
}
2. Higiene de Compilación y Arquitectura del Makefile
El Makefile requiere flags de compilación más estrictas para evitar comportamientos indefinidos en código a bajo nivel y asegurar la generación automática de dependencias.

Refactorización del Makefile
Makefile
CC = i686-elf-gcc
AS = nasm
LD = i686-elf-ld

CFLAGS = -std=c11 -m32 -nostdlib -ffreestanding -fno-builtin \
         -fno-stack-protector -fno-pie -Wall -Wextra -Werror \
         -g -Iinclude -MMD -MP

ASFLAGS = -f elf32 -g
LDFLAGS = -m elf_i386 -T linker.ld

C_SOURCES = $(shell find src -name '*.c')
ASM_SOURCES = $(shell find src -name '*.s')
OBJ = $(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o)
DEP = $(OBJ:.o=.d)

all: myos.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

myos.bin: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

-include $(DEP)

clean:
	rm -f $(OBJ) $(DEP) myos.bin
3. Sincronización e Inmutabilidad de Operaciones I/O
Sin abstracciones de sincronización primitiva, el acceso concurrente al buffer VGA o a estructuras del kernel desde controladores de interrupción (ej. Teclado y PIT) genera race conditions.

Implementación de Spinlocks con Atómicos C11/Assembly
C
// spinlock.h
typedef struct {
    volatile uint32_t locked;
} spinlock_t;

static inline void spinlock_acquire(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        __asm__ __volatile__("pause"); // Optimización de pipeline x86
    }
}

static inline void spinlock_release(spinlock_t *lock) {
    __sync_lock_release(&lock->locked);
}
Plan de Acción Recomendado
Fase 1: Estabilización e Infraestructura (Corto Plazo: Weeks 1–2)
Refactorización del Build System: Actualizar el Makefile incorporando -Wall -Wextra -Werror -MMD y aislar la generación de ejecutables mediante scripts de enlace (linker.ld) con alineación de secciones a 4 KB (ALIGN(4096)).

Sistema de Logs y Panic Screen: Implementar una función panic() formal que deshabilite interrupciones (cli), imprima el volcado completo de registros (eax, ebx, eip, eflags, cr0, cr2, cr3) y detenga la CPU (hlt).

PMM y Allocator Dinámico: Completar el administrador de memoria física mediante bitmap y construir un Kernel Heap elemental (kmalloc/kfree) basado en bloques con cabeceras de tamaño.

Fase 2: Multitarea y Modo Usuario (Mediano Plazo: Months 1–2)
Paginación Virtual Completa (VMM): Implementar la arquitectura Higher-Half Kernel (mapeando el código del núcleo a 0xC0000000 / 0xFFFFFFFF80000000) liberando el espacio inferior para procesos de usuario.

Planificador de Procesos (Scheduler): Construir la estructura task_struct, inicializar la pila de contexto para nuevas tareas y conectar el conmutador de contexto a la IRQ0.

Instalación de TSS y Ring 3: Configurar el segmento TSS en la GDT y construir la primera rutina de cambio de contexto a Ring 3 utilizando el marco de interrupción iret.

Fase 3: Subsistemas I/O, Syscalls y VFS (Largo Plazo: Months 3–6)
Capa Syscall (int 0x80): Definir el mapa de llamadas al sistema y la validación de punteros pasados por el usuario para evitar vulnerabilidades de memoria.

Abstracción VFS e Initrd: Diseñar la interfaz de VFS y habilitar un disco de memoria RAM montado desde los módulos de GRUB para poder leer archivos binarios ejecutables.

Cargador ELF (Executable and Linkable Format): Implementar un parser de cabeceras ELF32/ELF64 que lea segmentos LOAD, asigne páginas de memoria de usuario y transfiera el control de ejecución al punto de entrada del binario.


Multicore / multiCPU
MultiGPU / Scalar GPU (implement a system wich uses the grafic card or at least part of it to compress and descompress the data and send it like if it was a cluster or a web. the other part of the card, is focused in the task. This can be a program called "HACC" (Hardware-Accelerated Cluster Codec)).