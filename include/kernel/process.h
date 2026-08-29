#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/context.h>

// Definiciones de la disposición del espacio virtual de usuario (x86_64 canonical user space)
#define USER_HEAP_START_DEFAULT  0x0000700000000000ULL
#define USER_HEAP_MAX_SIZE       0x0000000080000000ULL // 2 GiB
#define USER_MMAP_START_DEFAULT  0x00007A0000000000ULL // Región dedicada a mapeos VMA

struct process {
    uint32_t pid;
    uint64_t pml4_physical;
    uintptr_t user_entry;
    uintptr_t user_stack;
    
    /* Control del Heap de Usuario (brk) */
    uintptr_t heap_start;   // Límite inferior fijo del heap
    uintptr_t heap_end;     // Posición actual del brk
    uintptr_t heap_limit;   // Límite superior máximo permitido

	uintptr_t mmap_next;
    
    struct cpu_context context;
};

int process_create_user(struct process *process,
                        uintptr_t entry,
                        uintptr_t user_stack);
int process_activate(const struct process *process);
void process_enter_user(const struct process *process);
void process_switch(struct process *current, struct process *next);
uint32_t current_process_id(void);

#endif