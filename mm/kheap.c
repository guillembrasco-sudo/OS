// mm/kheap.c
#include "kernel/kheap.h"
#include "kernel/spinlock.h"
#include "kernel/panic.h"
#include <stdint.h>

#define ALIGN8(x) (((x) + 7) & ~((size_t)7))
#define MIN_BLOCK_SIZE 32

typedef struct block_header {
    size_t                size;   // tamaño del payload, sin contar el header
    int                   free;
    struct block_header  *next;
    struct block_header  *prev;
} block_header_t;

static block_header_t *heap_start = NULL;
static uint8_t         *heap_end_ptr = NULL; // límite superior actual del arena
static spinlock_t       heap_lock = SPINLOCK_INIT;

void kheap_init(void *start, size_t size) {
    heap_start = (block_header_t *)start;
    heap_start->size = size - sizeof(block_header_t);
    heap_start->free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_end_ptr = (uint8_t *)start + size;
}

static void split_block(block_header_t *block, size_t size) {
    size_t remaining = block->size - size;
    if (remaining <= sizeof(block_header_t) + MIN_BLOCK_SIZE) {
        return; // el resto no compensa el overhead del header: se lo queda el bloque
    }

    block_header_t *new_block = (block_header_t *)((uint8_t *)block + sizeof(block_header_t) + size);
    new_block->size = remaining - sizeof(block_header_t);
    new_block->free = 1;
    new_block->next = block->next;
    new_block->prev = block;

    if (block->next) block->next->prev = new_block;
    block->next = new_block;
    block->size = size;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN8(size);
    if (size < MIN_BLOCK_SIZE) size = MIN_BLOCK_SIZE;

    spinlock_acquire(&heap_lock);

    block_header_t *block = heap_start;
    // First-fit. Con boundary tags + coalescing agresivo en free() la
    // fragmentación externa se mantiene manejable sin necesidad de
    // best-fit (que es más caro por bloque y en la práctica no gana
    // mucho para los tamaños de allocación típicos de un kernel).
    while (block) {
        if (block->free && block->size >= size) {
            split_block(block, size);
            block->free = 0;
            spinlock_release(&heap_lock);
            return (void *)((uint8_t *)block + sizeof(block_header_t));
        }
        block = block->next;
    }

    spinlock_release(&heap_lock);
    panic("kmalloc: heap agotado solicitando %zu bytes (considera crecer el arena vía pmm_alloc_pages + kheap_extend)", size);
    return NULL;
}

static void coalesce(block_header_t *block) {
    // Fusiona con el siguiente si también está libre
    if (block->next && block->next->free) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }
    // Fusiona con el anterior si también está libre
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(block_header_t) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
    }
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));

    spinlock_acquire(&heap_lock);

    if (block->free) {
        // Double-free detectado. En debug esto debería panic(); en
        // release, no-op es más seguro que corromper la free-list.
#ifdef KHEAP_DEBUG
        spinlock_release(&heap_lock);
        panic("kfree: double-free detectado en %p", ptr);
#endif
        spinlock_release(&heap_lock);
        return;
    }

    block->free = 1;
    coalesce(block);

    spinlock_release(&heap_lock);
}

void *kmalloc_aligned(size_t size, size_t align) {
    // Sobre-reserva y ajusta manualmente. Desperdicia hasta `align` bytes
    // por asignación — aceptable porque este camino se usa poco (tablas
    // de página, buffers DMA), no en el hot path general.
    void *raw = kmalloc(size + align);
    if (!raw) return NULL;
    uintptr_t aligned = ((uintptr_t)raw + align - 1) & ~(align - 1);
    return (void *)aligned;
}