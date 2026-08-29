#ifndef MM_VMA_H
#define MM_VMA_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/process.h>

#define VMA_PROT_READ  0x1
#define VMA_PROT_WRITE 0x2

// Equivalente simplificado de mmap() anónimo: reserva `length` bytes
// (redondeados a página) a partir de proc->mmap_next y los mapea a
// páginas físicas recién asignadas. Sin backing de archivo y sin
// allocator de huecos (ver nota de mmap_next en process.h — munmap no
// recicla direcciones). Devuelve la dirección virtual base, o 0 si
// falla.
uintptr_t vma_map_anonymous(struct process *proc, size_t length, uint32_t prot);

// Memoria compartida entre dos procesos: reserva páginas físicas
// nuevas y las mapea en AMBOS espacios de direcciones a la misma
// página física (contenido compartido, direcciones virtuales
// distintas). Devuelve la dirección virtual en `owner`; si
// `mapped_at` no es NULL, recibe la dirección virtual equivalente en
// `other`.
uintptr_t vma_map_shared(struct process *owner, struct process *other,
                          size_t length, uint32_t prot, uintptr_t *mapped_at);

// Equivalente de munmap(): desmapea y libera las páginas físicas de
// `length` bytes a partir de `addr` en el espacio de `proc`. No
// recicla el hueco en mmap_next (ver process.h).
int vma_unmap(struct process *proc, uintptr_t addr, size_t length);

#endif