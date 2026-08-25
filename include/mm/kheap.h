// include/kernel/kheap.h
#ifndef KHEAP_H
#define KHEAP_H

#include <stddef.h>
#include <stdint.h>

#define KHEAP_MAGIC 0x12345678

typedef struct header {
    uint32_t magic;
    size_t size;
    uint8_t is_free;
    struct header *next;
    struct header *prev;
} header_t;

void  kheap_init(uintptr_t start_address, size_t initial_size);
void *kmalloc(size_t size);
void  kfree(void *ptr);
void *kmalloc_aligned(size_t size, size_t align); // para estructuras que exigen alineación de página (ej. tablas de página)

#endif // KHEAP_H