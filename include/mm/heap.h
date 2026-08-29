#ifndef MM_HEAP_H
#define MM_HEAP_H

#include <stdint.h>
#include <kernel/process.h>

// Implementa el syscall brk(): fija heap_end a la dirección absoluta
// `new_end`. Mapea páginas físicas nuevas si el heap crece, y las
// libera si decrece. Devuelve 0 en éxito, -1 si `new_end` cae por
// debajo de heap_start o por encima de heap_limit (el mmap_next del
// proceso, ver process.h), o si falla la reserva de páginas físicas.
int heap_brk(struct process *proc, uintptr_t new_end);

// Implementa el syscall sbrk(): crece/decrece el heap en `increment`
// bytes (puede ser negativo). Devuelve la heap_end ANTERIOR a la
// operación (semántica POSIX de sbrk), o (uintptr_t)-1 si falla.
uintptr_t heap_sbrk(struct process *proc, intptr_t increment);

#endif