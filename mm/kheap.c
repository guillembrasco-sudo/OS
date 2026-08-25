// mm/kheap.c
#include "mm/kheap.h"
#include "kernel/spinlock.h"
#include "kernel/panic.h"
#include <stdint.h>

#define ALIGN8(x) (((x) + 7) & ~((size_t)7))
#define MIN_BLOCK_SIZE 32

static header_t *heap_start = NULL;
static uint8_t         *heap_end_ptr = NULL; // límite superior actual del arena
static spinlock_t       heap_lock = SPINLOCK_INIT;

void kheap_init(uintptr_t start_address, size_t initial_size) {
    heap_start = (header_t *)start_address;
    heap_start->size = initial_size - sizeof(header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_end_ptr = (uint8_t *)start_address + initial_size;
}

static void split_block(header_t *block, size_t size) {
    size_t remaining = block->size - size;
    if (remaining <= sizeof(header_t) + MIN_BLOCK_SIZE) {
        return; // el resto no compensa el overhead del header: se lo queda el bloque
    }

    header_t *new_block = (header_t *)((uint8_t *)block + sizeof(header_t) + size);
    new_block->size = remaining - sizeof(header_t);
    new_block->is_free = 1;
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

    header_t *block = heap_start;
    // First-fit. Con boundary tags + coalescing agresivo en free() la
    // fragmentación externa se mantiene manejable sin necesidad de
    // best-fit (que es más caro por bloque y en la práctica no gana
    // mucho para los tamaños de allocación típicos de un kernel).
    while (block) {
        if (block->is_free && block->size >= size) {
            split_block(block, size);
            block->is_free = 0;
            spinlock_release(&heap_lock);
            return (void *)((uint8_t *)block + sizeof(header_t));
        }
        block = block->next;
    }

    spinlock_release(&heap_lock);
    panic("kmalloc: heap agotado solicitando %zu bytes (considera crecer el arena vía pmm_alloc_pages + kheap_extend)", size);
    return NULL;
}

static void coalesce(header_t *block) {
    // Fusiona con el siguiente si también está libre
    if (block->next && block->next->is_free) {
        block->size += sizeof(header_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }
    // Fusiona con el anterior si también está libre
    if (block->prev && block->prev->is_free) {
        block->prev->size += sizeof(header_t) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
    }
}

void kfree(void *ptr) {
    if (!ptr) return;

    header_t *block = (header_t *)((uint8_t *)ptr - sizeof(header_t));

    spinlock_acquire(&heap_lock);

    if (block->is_free) {
        // Double-free detectado. En debug esto debería panic(); en
        // release, no-op es más seguro que corromper la free-list.
#ifdef KHEAP_DEBUG
        spinlock_release(&heap_lock);
        panic("kfree: double-free detectado en %p", ptr);
#endif
        spinlock_release(&heap_lock);
        return;
    }

    block->is_free = 1;
    coalesce(block);

    spinlock_release(&heap_lock);
}

void *kmalloc_aligned(size_t size, size_t align) {
    if (align < sizeof(void *)) align = sizeof(void *);
    if ((align & (align - 1)) != 0) return NULL; // Debe ser potencia de 2

    size = ALIGN8(size);
    if (size < MIN_BLOCK_SIZE) size = MIN_BLOCK_SIZE;

    if (align <= 8) {
        return kmalloc(size);
    }

    // Reserva espacio adicional para reajustar el descriptor manteniendo MIN_BLOCK_SIZE
    size_t total_size = size + align + sizeof(header_t) + MIN_BLOCK_SIZE;
    void *raw = kmalloc(total_size);
    if (!raw) return NULL;

    uintptr_t raw_addr = (uintptr_t)raw;

    // Si coincide que ya está alineado, ajusta el excedente final y retorna
    if ((raw_addr & (align - 1)) == 0) {
        spinlock_acquire(&heap_lock);
        header_t *block = (header_t *)(raw_addr - sizeof(header_t));
        split_block(block, size);
        spinlock_release(&heap_lock);
        return raw;
    }

    // Calcula la dirección alineada garantizando espacio para una nueva cabecera válida
    uintptr_t aligned_addr = (raw_addr + sizeof(header_t) + MIN_BLOCK_SIZE + align - 1) & ~(align - 1);

    spinlock_acquire(&heap_lock);

    header_t *orig_block = (header_t *)(raw_addr - sizeof(header_t));
    header_t *aligned_block = (header_t *)(aligned_addr - sizeof(header_t));

    size_t orig_payload_size = (uint8_t *)aligned_block - (uint8_t *)raw;

    aligned_block->magic = KHEAP_MAGIC;
    aligned_block->size = orig_block->size - orig_payload_size - sizeof(header_t);
    aligned_block->is_free = 0;
    aligned_block->next = orig_block->next;
    aligned_block->prev = orig_block;

    if (orig_block->next) {
        orig_block->next->prev = aligned_block;
    }
    orig_block->next = aligned_block;
    orig_block->size = orig_payload_size;

    // Recorta el bloque alineado al tamaño solicitado
    split_block(aligned_block, size);

    // Marca el bloque de relleno anterior como libre y lo coalesce
    orig_block->is_free = 1;
    coalesce(orig_block);

    spinlock_release(&heap_lock);

    return (void *)aligned_addr;
}