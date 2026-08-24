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
} header_t;

/**
 * @brief Inicializa la región de memoria del Heap.
 * @param start_address Dirección virtual donde inicia el heap (ej. 0xC1000000)
 * @param initial_size Tamaño inicial asignado en bytes.
 */
void kheap_init(uintptr_t start_address, size_t initial_size);

/**
 * @brief Asigna un bloque de memoria contigua en el Heap del kernel.
 * @param size Cantidad de bytes a solicitar.
 * @return Puntero a la memoria reservada o NULL si falla.
 */
void *kmalloc(size_t size);

/**
 * @brief Libera un bloque de memoria previamente asignado.
 * @param ptr Puntero devuelto por kmalloc.
 */
void kfree(void *ptr);

#endif // KHEAP_H