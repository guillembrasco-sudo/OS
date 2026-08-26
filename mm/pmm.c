// mm/pmm.c
#include "mm/pmm.h"
#include "kernel/spinlock.h"
#include "kernel/panic.h"
#include <lib/string.h>

// Bitmap: 1 bit por página física. 1 = usada/reservada, 0 = libre.
// Tamaño máximo soportado en este build: 64 GiB físicos (ajustable).
#define PMM_MAX_MEMORY   (64ULL * 1024 * 1024 * 1024)
#define PMM_BITMAP_BITS  (PMM_MAX_MEMORY / PMM_PAGE_SIZE)
#define PMM_BITMAP_WORDS (PMM_BITMAP_BITS / 64)
#define BITMAP_MAX_PAGES (sizeof(bitmap) * 8)

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

// --- Parseo de memory map de Multiboot1 ------------------------------------
// boot/boot.asm arranca con la cabecera clásica Multiboot1 (magic 0x1BADB002
// + AOUT kludge), por lo que las entradas del mmap llegan en el formato
// "multiboot_memory_map_t": cada entrada empieza con un campo `size` de 32
// bits que indica cuánto mide el RESTO de la entrada (sin contarse a sí
// mismo), así que hay que avanzar `entry->size + 4` bytes en cada iteración.
struct mb1_mmap_entry {
    uint32_t size;   // tamaño del resto de la entrada (no incluye este campo)
    uint64_t addr;
    uint64_t len;
    uint32_t type;   // 1 = disponible, otros = reservado/ACPI/etc.
} __attribute__((packed));

void pmm_init(uint64_t mmap_addr, uint32_t mmap_len, uint64_t kernel_start, uint64_t kernel_end) {
    // 1. Todo el bitmap arranca "usado" (reservado/0xFF).
    memset(bitmap, 0xFF, sizeof(bitmap));

    // mmap_addr apunta directamente a la primera entrada Multiboot1
    // (mb_info->mmap_addr), sin desplazamiento de tag como en MB2.
    struct mb1_mmap_entry *entry = (struct mb1_mmap_entry *)(uintptr_t)mmap_addr;
    uint8_t *end = (uint8_t *)(uintptr_t)mmap_addr + mmap_len;

    // Pase único: Recorrer las entradas de la tabla de memoria Multiboot1
    while ((uint8_t *)entry < end && entry->size > 0) {
        if (entry->type == 1) { // 1 = Memoria RAM Disponible
            uint64_t start_page = entry->addr / PMM_PAGE_SIZE;
            uint64_t page_count = entry->len / PMM_PAGE_SIZE;

            for (uint64_t i = 0; i < page_count; i++) {
                uint64_t page = start_page + i;
                if (page < PMM_BITMAP_BITS) {
                    bitmap_clear(page);
                    total_pages++;
                    if (page > highest_page) {
                        highest_page = page;
                    }
                }
            }
        }
        // Avanzar a la siguiente entrada: el campo `size` no se cuenta a sí mismo.
        entry = (struct mb1_mmap_entry *)((uint8_t *)entry + entry->size + 4);
    }

    pmm_total_pages = total_pages;

    // 2. Proteger las páginas ocupadas por el Kernel y la página 0 (NULL pointer protection)
    uint64_t k_start_page = kernel_start / PMM_PAGE_SIZE;
    uint64_t k_end_page   = (kernel_end + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    for (uint64_t p = k_start_page; p <= k_end_page; p++) {
        if (!bitmap_test(p)) { 
            bitmap_set(p); 
            used_pages++; 
        }
    }
    if (!bitmap_test(0)) { 
        bitmap_set(0); 
        used_pages++; 
    }
}

void pmm_reserve_range(uint64_t physical, size_t length)
{
    uint64_t first;
    uint64_t last;
    if (!length || physical >= PMM_MAX_MEMORY ||
        length > PMM_MAX_MEMORY - physical)
        return;
    first = physical / PMM_PAGE_SIZE;
    last = (physical + length + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    spinlock_acquire(&pmm_lock);
    for (uint64_t page = first; page < last; ++page)
        if (!bitmap_test(page)) {
            bitmap_set(page);
            ++used_pages;
        }
    spinlock_release(&pmm_lock);
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
    return 0;
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
    return 0;
}

uintptr_t pmm_alloc_contiguous(size_t pages) {
    return (uintptr_t)pmm_alloc_pages(pages);
}

void pmm_free_page(uint64_t phys_addr) {
    uint64_t page = phys_addr / PMM_PAGE_SIZE;
    spinlock_acquire(&pmm_lock);
    if (bitmap_test(page)) {
        bitmap_clear(page);
        if (used_pages > 0) used_pages--;
    }
    spinlock_release(&pmm_lock);
}

void pmm_free_pages(uint64_t phys_addr, size_t count) {
    if (phys_addr == 0 || count == 0 || (phys_addr % PMM_PAGE_SIZE) != 0) return;

    spinlock_acquire(&pmm_lock);
    uint64_t start_page = phys_addr / PMM_PAGE_SIZE;

    for (size_t i = 0; i < count; i++) {
        uint64_t page = start_page + i;
        if (bitmap_test(page)) {
            bitmap_clear(page);
            if (used_pages > 0) used_pages--;
        }
    }
    spinlock_release(&pmm_lock);
}

uint64_t pmm_total_memory(void) { return total_pages * PMM_PAGE_SIZE; }
uint64_t pmm_free_memory(void)  { return (total_pages - used_pages) * PMM_PAGE_SIZE; }

// Byte físico más alto visto en el mapa de memoria (redondeado a página).
// Lo usa paging_init() para saber cuánta RAM cubrir con el mapa directo.
uint64_t pmm_highest_address(void) { return (highest_page + 1) * PMM_PAGE_SIZE; }


size_t pmm_total_pages = 0;
uint8_t *pmm_bitmap = NULL;

void pmm_free_contiguous(uintptr_t physical, size_t pages) {
    pmm_free_pages((uint64_t)physical, pages);
}