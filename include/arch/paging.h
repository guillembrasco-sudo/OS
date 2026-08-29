// include/arch/paging.h
#ifndef ARCH_PAGING_H
#define ARCH_PAGING_H

#include <stdint.h>
#include <stddef.h>

// Flags de página (formato x86_64 real, no una abstracción propia).
#define PAGING_PRESENT (1ULL << 0)
#define PAGING_WRITE   (1ULL << 1)
#define PAGING_USER    (1ULL << 2)
#define PAGING_NX      (1ULL << 63) // requiere EFER.NXE=1 y CPU con soporte (cpu_features.has_nx)

// Mapa directo (HHDM, "Higher-Half Direct Map"): toda la RAM física
// detectada queda accesible en todo momento en virtual = fisica + esta base,
// una vez paging_init() ha corrido. PML4 índice 256 (0xFFFF800000000000),
// deliberadamente separado del índice 511 que usa el kernel para su propia
// imagen (KERNEL_VMA, definido en boot.asm/linker.ld) para no pisarlo.
#define PAGING_PHYS_MAP_BASE 0xFFFF800000000000ULL

// Tamaño máximo de RAM física que el mapa directo puede llegar a cubrir.
// Debe ser >= PMM_MAX_MEMORY (mm/pmm.c) para poder direccionar toda la RAM
// que el PMM es capaz de rastrear.
#define PAGING_DIRECT_MAP_MAX_BYTES (64ULL * 1024 * 1024 * 1024)

// Construye el mapa directo de toda la RAM física detectada (0 ..
// highest_physical_address) usando páginas de 2MiB. Debe llamarse una única
// vez, después de pmm_init() (necesita saber cuánta RAM hay) y antes de usar
// paging_map_page/paging_map_region o paging_phys_to_virt.
void paging_init(uint64_t highest_physical_address);

// Traduce una dirección física a su alias virtual en el mapa directo.
// Sólo válido después de paging_init().
static inline uint64_t paging_phys_to_virt(uint64_t phys) {
    return phys + PAGING_PHYS_MAP_BASE;
}

// Mapea una única página de 4KiB. Crea las tablas intermedias (PDPT/PD/PT)
// que hagan falta, tirando de pmm_alloc_page(). Devuelve 0 en éxito, -1 si
// no hay memoria física para una tabla nueva.
int paging_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);

// Retira la traducción de una página de 4KiB previamente mapeada con
// paging_map_page (no toca mapeos "huge" de 2MiB/1GiB). No-op si no estaba
// mapeada.
int paging_unmap_page(uint64_t virtual_address);

// Traduce una dirección virtual ya mapeada a su física correspondiente
// (soporta páginas huge de 2MiB/1GiB). Devuelve (uint64_t)-1 si no hay
// traducción presente en ningún nivel.
uint64_t paging_translate(uint64_t virtual_address);

// Mapea un rango [phys_base, phys_base+size) a [virt_base, virt_base+size)
// usando la página más grande que quepa en cada paso (1GiB si allow_1gb y
// hay alineación/tamaño suficiente, si no 2MiB si allow_2mb, si no 4KiB).
// phys_base, virt_base y size deben ser múltiplos de 4KiB como mínimo.
int paging_map_region(uint64_t phys_base, uint64_t virt_base, uint64_t size,
                       uint64_t flags, int allow_1gb, int allow_2mb);

/* Creates a new PML4 containing the current kernel mappings. */
int paging_create_user_space(uint64_t *pml4_physical);
int paging_map_page_in_space(uint64_t pml4_physical,
                             uint64_t virtual_address,
                             uint64_t physical_address,
                             uint64_t flags);
int paging_unmap_page_in_space(uint64_t pml4_physical, uint64_t virtual_address);
int paging_activate_space(uint64_t pml4_physical);

uint64_t paging_translate_in_space(uint64_t pml4_physical, uint64_t virtual_address);

#endif // ARCH_PAGING_H
