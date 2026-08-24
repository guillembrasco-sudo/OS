// mm/pmm.c
#include "kernel/pmm.h"
#include "kernel/spinlock.h"
#include "kernel/panic.h"
#include <string.h>

// Bitmap: 1 bit por página física. 1 = usada/reservada, 0 = libre.
// Tamaño máximo soportado en este build: 64 GiB físicos (ajustable).
#define PMM_MAX_MEMORY   (64ULL * 1024 * 1024 * 1024)
#define PMM_BITMAP_BITS  (PMM_MAX_MEMORY / PMM_PAGE_SIZE)
#define PMM_BITMAP_WORDS (PMM_BITMAP_BITS / 64)

static uint64_t   bitmap[PMM_BITMAP_WORDS];
static uint64_t   highest_page       = 0;
static uint64_t   total_pages        = 0;
static uint64_t   used_pages         = 0;
static uint64_t   last_alloc_index   = 0; // next-fit: evita reescanear desde 0 cada vez
static spinlock_t pmm_lock           = SPINLOCK_INIT;

static inline void bitmap_set(uint64_t bit) {
    bitmap[bit / 64] |= (1ULL << (bit % 64));
}
static inline void bitmap_clear(uint64_t bit) {
    bitmap[bit / 64] &= ~(1ULL << (bit % 64));
}
static inline int bitmap_test(uint64_t bit) {
    return (bitmap[bit / 64] >> (bit % 64)) & 1ULL;
}

// --- Parseo de memory map de Multiboot2 -----------------------------------
// ASUNCIÓN: mmap_addr apunta al primer tag MULTIBOOT_TAG_TYPE_MMAP (tipo 6)
// ya localizado por el bootloader/entry.asm y pasado por %rdi/%rbx según tu
// convención de llamada actual. Si usas Multiboot1, sustituye este parseo
// por la lectura directa de multiboot_info_t->mmap_addr/mmap_length con
// entradas multiboot_memory_map_t (formato distinto, sin campo `size` por
// entrada al principio de cada registro).
struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;   // 1 = disponible, otros = reservado/ACPI/etc.
    uint32_t reserved;
} __attribute__((packed));

void pmm_init(uint64_t mmap_addr, uint32_t mmap_len, uint64_t kernel_start, uint64_t kernel_end) {
    // 1. Todo el bitmap arranca "usado". Vamos liberando solo lo que el
    //    firmware reporta como disponible. Esto es más seguro que el
    //    enfoque inverso: si el parseo del mmap falla o hay un hueco no
    //    reportado, el fallo es "quedarse sin memoria", no "corromper
    //    memoria de hardware reservada".
    memset(bitmap, 0xFF, sizeof(bitmap));

    struct mb2_mmap_entry *entry = (struct mb2_mmap_entry *)(mmap_addr + 16); // salta el header del tag
    uint8_t *end = (uint8_t *)mmap_addr + mmap_len;

    while ((uint8_t *)entry < end) {
        if (entry->type == 1) { // AVAILABLE
            uint64_t start_page = entry->addr / PMM_PAGE_SIZE;
            uint64_t page_count  = entry->len  / PMM_PAGE_SIZE;

            for (uint64_t i = 0; i < page_count; i++) {
                uint64_t page = start_page + i;
                if (page < PMM_BITMAP_BITS) {
                    bitmap_clear(page);
                    total_pages++;
                    if (page > highest_page) highest_page = page;
                }
            }
        }
        entry = (struct mb2_mmap_entry *)((uint8_t *)entry + 24); // tamaño fijo de este layout
    }

    // 2. Reserva explícita del propio kernel (.text/.data/.bss ya cargados)
    //    y de la primera página física (null-page guard: nunca se debe
    //    poder mapear la dirección física 0, protege contra
    //    desreferencias de puntero nulo que "casualmente" funcionan).
    uint64_t k_start_page = kernel_start / PMM_PAGE_SIZE;
    uint64_t k_end_page   = (kernel_end + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    for (uint64_t p = k_start_page; p <= k_end_page; p++) {
        if (!bitmap_test(p)) { bitmap_set(p); used_pages++; }
    }
    if (!bitmap_test(0)) { bitmap_set(0); used_pages++; }

    used_pages += 0; // total_pages ya cuenta solo memoria disponible reportada
}

uint64_t pmm_alloc_page(void) {
    spinlock_acquire(&pmm_lock);

    for (uint64_t i = 0; i < PMM_BITMAP_WORDS; i++) {
        uint64_t idx = (last_alloc_index + i) % PMM_BITMAP_WORDS;
        if (bitmap[idx] != ~0ULL) {
            for (int b = 0; b < 64; b++) {
                if (!((bitmap[idx] >> b) & 1ULL)) {
                    uint64_t page = idx * 64 + b;
                    bitmap_set(page);
                    used_pages++;
                    last_alloc_index = idx;
                    spinlock_release(&pmm_lock);
                    return page * PMM_PAGE_SIZE;
                }
            }
        }
    }

    spinlock_release(&pmm_lock);
    panic("PMM: sin memoria física disponible (OOM en pmm_alloc_page)");
    return 0; // no alcanzable, panic no retorna
}

uint64_t pmm_alloc_pages(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();

    spinlock_acquire(&pmm_lock);

    uint64_t run_start = 0;
    uint64_t run_len    = 0;

    for (uint64_t page = 0; page <= highest_page; page++) {
        if (!bitmap_test(page)) {
            if (run_len == 0) run_start = page;
            run_len++;
            if (run_len == count) {
                for (uint64_t p = run_start; p < run_start + count; p++) {
                    bitmap_set(p);
                }
                used_pages += count;
                spinlock_release(&pmm_lock);
                return run_start * PMM_PAGE_SIZE;
            }
        } else {
            run_len = 0;
        }
    }

    spinlock_release(&pmm_lock);
    panic("PMM: no se encontró un bloque físico contiguo suficientemente grande");
    return 0;
}

void pmm_free_page(uint64_t phys_addr) {
    uint64_t page = phys_addr / PMM_PAGE_SIZE;
    spinlock_acquire(&pmm_lock);
    if (bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
    }
    // Doble-free silencioso -> no-op. Si quieres detectarlo en debug,
    // compílalo con PMM_DEBUG y añade un contador de refcount por página
    // en vez de solo un bit; el bitmap por sí solo no distingue
    // "libre" de "liberada dos veces".
    spinlock_release(&pmm_lock);
}

void pmm_free_pages(uint64_t phys_addr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pmm_free_page(phys_addr + i * PMM_PAGE_SIZE);
    }
}

uint64_t pmm_total_memory(void) { return total_pages * PMM_PAGE_SIZE; }
uint64_t pmm_free_memory(void)  { return (total_pages - used_pages) * PMM_PAGE_SIZE; }