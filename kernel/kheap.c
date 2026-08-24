#include <kernel/kheap.h>

static uintptr_t heap_start;
static uintptr_t heap_end;
static header_t *first_header = NULL;

void kheap_init(uintptr_t start_address, size_t initial_size) {
    heap_start = start_address;
    heap_end = start_address + initial_size;

    first_header = (header_t *)heap_start;
    first_header->magic = KHEAP_MAGIC;
    first_header->size = initial_size - sizeof(header_t);
    first_header->is_free = 1;
    first_header->next = NULL;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Alineación a 4 bytes
    if (size % 4 != 0) {
        size += 4 - (size % 4);
    }

    header_t *curr = first_header;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Fragmentar el bloque si el sobrante es suficiente para un nuevo encabezado
            if (curr->size >= size + sizeof(header_t) + 16) {
                header_t *next_block = (header_t *)((uintptr_t)curr + sizeof(header_t) + size);
                next_block->magic = KHEAP_MAGIC;
                next_block->size = curr->size - size - sizeof(header_t);
                next_block->is_free = 1;
                next_block->next = curr->next;

                curr->size = size;
                curr->next = next_block;
            }
            curr->is_free = 0;
            return (void *)((uintptr_t)curr + sizeof(header_t));
        }
        curr = curr->next;
    }

    // Sin memoria libre suficiente en el heap actual
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;

    header_t *header = (header_t *)((uintptr_t)ptr - sizeof(header_t));
    if (header->magic != KHEAP_MAGIC) {
        // Corrupción de heap detectada
        return;
    }

    header->is_free = 1;

    // Coalescencia (fusión de bloques libres adyacentes)
    header_t *curr = first_header;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += curr->next->size + sizeof(header_t);
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}