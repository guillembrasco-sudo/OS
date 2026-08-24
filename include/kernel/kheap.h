// include/kernel/kheap.h
#ifndef KHEAP_H
#define KHEAP_H

#include <stddef.h>

void  kheap_init(void *start, size_t size);
void *kmalloc(size_t size);
void  kfree(void *ptr);
void *kmalloc_aligned(size_t size, size_t align); // para estructuras que exigen alineación de página (ej. tablas de página)

#endif // KHEAP_H