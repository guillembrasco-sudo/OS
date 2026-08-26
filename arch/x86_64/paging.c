// arch/x86_64/paging.c
//
// Gestion de tablas de paginas x86_64 (4 niveles: PML4 -> PDPT -> PD -> PT).
//
// boot/boot.asm ya deja, al entrar en kmain(), un PML4 minimo construido a
// mano con dos mapeos de 1GiB (paginas huge de 2MiB) que cubren fisicos
// [0, 1GiB):
//   - PML4[0]   -> identidad:    virtual [0, 1GiB)                    == fisica [0,1GiB)
//   - PML4[511] -> higher-half:  virtual [KERNEL_VMA, KERNEL_VMA+1GiB) == fisica [0,1GiB)
// (KERNEL_VMA = 0xFFFFFFFF80000000, coincide con linker.ld y -mcmodel=kernel)
//
// Este fichero NO toca esas dos entradas. Lo que hace:
//
//  1. paging_init(): anade una TERCERA region, el "mapa directo" (HHDM) en
//     PML4[256] (== PAGING_PHYS_MAP_BASE = 0xFFFF800000000000), que cubre
//     TODA la RAM fisica detectada por el PMM, no solo el primer GiB. A
//     partir de aqui, cualquier pagina fisica es accesible como
//     paging_phys_to_virt(phys) sin necesidad de mapearla explicitamente.
//
//  2. paging_map_page/_region/_unmap/_translate: API general para mapear
//     cualquier virtual a cualquier fisica con permisos arbitrarios, usada
//     por vmm.c (y, en el futuro, por el codigo de espacios de direcciones
//     de usuario). Las tablas nuevas que hagan falta se piden con
//     pmm_alloc_page() y se acceden a traves del mapa directo del punto 1.
//
// Problema del huevo y la gallina al construir el mapa directo: para poder
// escribir en una tabla nueva necesitamos poder direccionarla, pero el
// mecanismo normal para direccionar paginas fisicas nuevas ES el mapa
// directo que todavia no existe. Se resuelve con un pool estatico en el
// .bss del kernel (ver bootstrap_pool mas abajo): esas paginas ya estan
// mapeadas de fabrica por boot.asm (identidad + higher-half del primer
// GiB), asi que sirven para construir el HHDM sin depender de el.
// Una vez el HHDM esta instalado, paging_map_page/_region ya pueden usar
// pmm_alloc_page() con normalidad.

#include <stdint.h>
#include <stddef.h>
#include <lib/string.h>
#include <arch/paging.h>
#include <mm/pmm.h>
#include <kernel/panic.h>

#define PAGE_SIZE_4K   0x1000ULL
#define PAGE_SIZE_2M   0x200000ULL
#define PAGE_SIZE_1G   0x40000000ULL
#define ENTRIES        512

#define PTE_HUGE       (1ULL << 7)
#define PTE_ADDR_MASK  0x000FFFFFFFFFF000ULL

// Indice de PML4 para PAGING_PHYS_MAP_BASE (0xFFFF800000000000):
// bits 47:39 de esa direccion == 256.
#define PHYS_MAP_PML4_INDEX 256

static inline uint64_t pml4_index(uint64_t v) { return (v >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t v) { return (v >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t v)   { return (v >> 21) & 0x1FF; }
static inline uint64_t pt_index(uint64_t v)   { return (v >> 12) & 0x1FF; }

static inline uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(value));
    return value;
}

static inline void write_cr3(uint64_t value) {
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(value) : "memory");
}

static inline void invlpg(uint64_t virtual_address) {
    __asm__ __volatile__("invlpg (%0)" : : "r"(virtual_address) : "memory");
}

// Recarga completa de TLB. Se usa tras instalar mapeos huge (2MiB/1GiB) para
// no tener que calcular cuantas invlpg de 4KiB harian falta para cubrirlos.
static inline void flush_tlb_full(void) {
    write_cr3(read_cr3());
}

static int direct_map_ready = 0;

// --- Pool estatico de tablas para el arranque del mapa directo -------------
// Dimensionado para poder cubrir PAGING_DIRECT_MAP_MAX_BYTES (64 GiB) con
// paginas de 2MiB: 1 PDPT + hasta 64 PD (cada PD cubre 1 GiB) = 65 tablas.
// Vive en el .bss del kernel, por lo que ya esta mapeado (identidad +
// higher-half) por boot.asm antes de que exista el mapa directo.
#define BOOTSTRAP_POOL_TABLES 68
static uint64_t bootstrap_pool[BOOTSTRAP_POOL_TABLES][ENTRIES] __attribute__((aligned(4096)));
static size_t   bootstrap_used = 0;

static uint64_t *bootstrap_alloc_table(void) {
    if (bootstrap_used >= BOOTSTRAP_POOL_TABLES)
        panic("paging: pool de arranque agotado (%d tablas) construyendo el mapa directo",
              BOOTSTRAP_POOL_TABLES);
    uint64_t *table = bootstrap_pool[bootstrap_used++];
    memset(table, 0, PAGE_SIZE_4K);
    return table;
}

// Physical de una tabla que vive en el pool de arranque: el pool esta en
// .bss dentro de la imagen del kernel, mapeada 1:1 con KERNEL_VMA, asi que
// physical = virtual - KERNEL_VMA. Definimos KERNEL_VMA aqui para no
// depender de que boot.asm exporte simbolos nuevos.
#define KERNEL_VMA 0xFFFFFFFF80000000ULL
static inline uint64_t bootstrap_table_phys(uint64_t *table) {
    return (uint64_t)(uintptr_t)table - KERNEL_VMA;
}

// --- Construccion del mapa directo (HHDM) -----------------------------------

void paging_init(uint64_t highest_physical_address) {
    if (highest_physical_address > PAGING_DIRECT_MAP_MAX_BYTES)
        highest_physical_address = PAGING_DIRECT_MAP_MAX_BYTES;

    uint64_t gib_needed = (highest_physical_address + PAGE_SIZE_1G - 1) / PAGE_SIZE_1G;
    if (gib_needed == 0)
        gib_needed = 1; // cubrir al menos el primer GiB aunque el mapa reporte poca RAM
    if (gib_needed > BOOTSTRAP_POOL_TABLES - 1)
        gib_needed = BOOTSTRAP_POOL_TABLES - 1;

    // PML4 activo: boot.asm lo deja en CR3, fisico y < 1GiB (vive en el
    // .bss de boot.asm, dentro de la imagen del kernel), asi que su
    // direccion fisica es directamente accesible como puntero (identidad
    // mapeada por boot.asm).
    uint64_t *pml4 = (uint64_t *)(uintptr_t)(read_cr3() & PTE_ADDR_MASK);

    uint64_t *pdpt = bootstrap_alloc_table();
    for (uint64_t g = 0; g < gib_needed; g++) {
        uint64_t *pd = bootstrap_alloc_table();
        for (uint64_t e = 0; e < ENTRIES; e++) {
            uint64_t phys = g * PAGE_SIZE_1G + e * PAGE_SIZE_2M;
            pd[e] = phys | PAGING_PRESENT | PAGING_WRITE | PTE_HUGE;
        }
        pdpt[g] = bootstrap_table_phys(pd) | PAGING_PRESENT | PAGING_WRITE;
    }

    pml4[PHYS_MAP_PML4_INDEX] = bootstrap_table_phys(pdpt) | PAGING_PRESENT | PAGING_WRITE;

    flush_tlb_full();
    direct_map_ready = 1;
}

// --- Mapeador generico (post mapa-directo) ----------------------------------

// Devuelve el PML4 activo, accedido via el mapa directo si ya esta listo, o
// via su direccion fisica directa (< 1GiB, identidad) si paging_init aun no
// ha corrido (permite llamar a paging_map_page antes de tiempo sin crashear,
// aunque el uso normal es siempre despues de paging_init).
static uint64_t *current_pml4(void) {
    uint64_t phys = read_cr3() & PTE_ADDR_MASK;
    if (direct_map_ready)
        return (uint64_t *)(uintptr_t)paging_phys_to_virt(phys);
    return (uint64_t *)(uintptr_t)phys;
}

static uint64_t *table_from_entry(uint64_t entry) {
    return (uint64_t *)(uintptr_t)paging_phys_to_virt(entry & PTE_ADDR_MASK);
}

// Devuelve la tabla del siguiente nivel apuntada por table[index], creandola
// (via pmm_alloc_page) si todavia no esta presente. extra_flags se aplica a
// la ENTRADA DE TABLA intermedia (normalmente PAGING_USER si el mapeo final
// es de usuario, para que el permiso se propague por los 4 niveles como
// exige la MMU).
static uint64_t *get_or_create_table(uint64_t *table, uint64_t index, uint64_t extra_flags) {
    if (table[index] & PAGING_PRESENT)
        return table_from_entry(table[index]);

    uint64_t phys = pmm_alloc_page();
    uint64_t *virt = (uint64_t *)(uintptr_t)paging_phys_to_virt(phys);
    memset(virt, 0, PAGE_SIZE_4K);
    table[index] = phys | PAGING_PRESENT | PAGING_WRITE | (extra_flags & PAGING_USER);
    return virt;
}

int paging_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    if ((virtual_address % PAGE_SIZE_4K) != 0 || (physical_address % PAGE_SIZE_4K) != 0)
        return -1;

    uint64_t *pml4 = current_pml4();
    uint64_t *pdpt = get_or_create_table(pml4, pml4_index(virtual_address), flags);
    uint64_t *pd   = get_or_create_table(pdpt, pdpt_index(virtual_address), flags);
    uint64_t *pt   = get_or_create_table(pd,   pd_index(virtual_address),   flags);

    pt[pt_index(virtual_address)] = physical_address | flags | PAGING_PRESENT;
    invlpg(virtual_address);
    return 0;
}

int paging_unmap_page(uint64_t virtual_address) {
    uint64_t *pml4 = current_pml4();

    uint64_t pml4e = pml4[pml4_index(virtual_address)];
    if (!(pml4e & PAGING_PRESENT)) return 0;
    uint64_t *pdpt = table_from_entry(pml4e);

    uint64_t pdpte = pdpt[pdpt_index(virtual_address)];
    if (!(pdpte & PAGING_PRESENT) || (pdpte & PTE_HUGE)) return 0; // 1GiB huge: fuera de alcance de esta funcion
    uint64_t *pd = table_from_entry(pdpte);

    uint64_t pde = pd[pd_index(virtual_address)];
    if (!(pde & PAGING_PRESENT) || (pde & PTE_HUGE)) return 0; // 2MiB huge: idem
    uint64_t *pt = table_from_entry(pde);

    pt[pt_index(virtual_address)] = 0;
    invlpg(virtual_address);
    return 0;
}

uint64_t paging_translate(uint64_t virtual_address) {
    uint64_t *pml4 = current_pml4();

    uint64_t pml4e = pml4[pml4_index(virtual_address)];
    if (!(pml4e & PAGING_PRESENT)) return (uint64_t)-1;
    uint64_t *pdpt = table_from_entry(pml4e);

    uint64_t pdpte = pdpt[pdpt_index(virtual_address)];
    if (!(pdpte & PAGING_PRESENT)) return (uint64_t)-1;
    if (pdpte & PTE_HUGE)
        return (pdpte & PTE_ADDR_MASK) | (virtual_address & (PAGE_SIZE_1G - 1));
    uint64_t *pd = table_from_entry(pdpte);

    uint64_t pde = pd[pd_index(virtual_address)];
    if (!(pde & PAGING_PRESENT)) return (uint64_t)-1;
    if (pde & PTE_HUGE)
        return (pde & PTE_ADDR_MASK) | (virtual_address & (PAGE_SIZE_2M - 1));
    uint64_t *pt = table_from_entry(pde);

    uint64_t pte = pt[pt_index(virtual_address)];
    if (!(pte & PAGING_PRESENT)) return (uint64_t)-1;
    return (pte & PTE_ADDR_MASK) | (virtual_address & (PAGE_SIZE_4K - 1));
}

static int map_1gb(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pml4 = current_pml4();
    uint64_t *pdpt = get_or_create_table(pml4, pml4_index(virt), flags);
    pdpt[pdpt_index(virt)] = phys | flags | PAGING_PRESENT | PTE_HUGE;
    return 0;
}

static int map_2mb(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pml4 = current_pml4();
    uint64_t *pdpt = get_or_create_table(pml4, pml4_index(virt), flags);
    uint64_t *pd   = get_or_create_table(pdpt, pdpt_index(virt), flags);
    pd[pd_index(virt)] = phys | flags | PAGING_PRESENT | PTE_HUGE;
    return 0;
}

int paging_map_region(uint64_t phys_base, uint64_t virt_base, uint64_t size,
                       uint64_t flags, int allow_1gb, int allow_2mb) {
    if ((phys_base % PAGE_SIZE_4K) || (virt_base % PAGE_SIZE_4K) || (size % PAGE_SIZE_4K))
        return -1;

    uint64_t offset = 0;
    int used_huge = 0;
    while (offset < size) {
        uint64_t phys = phys_base + offset;
        uint64_t virt = virt_base + offset;
        uint64_t remaining = size - offset;

        if (allow_1gb && (phys % PAGE_SIZE_1G) == 0 && (virt % PAGE_SIZE_1G) == 0 &&
            remaining >= PAGE_SIZE_1G) {
            map_1gb(virt, phys, flags);
            offset += PAGE_SIZE_1G;
            used_huge = 1;
        } else if (allow_2mb && (phys % PAGE_SIZE_2M) == 0 && (virt % PAGE_SIZE_2M) == 0 &&
                   remaining >= PAGE_SIZE_2M) {
            map_2mb(virt, phys, flags);
            offset += PAGE_SIZE_2M;
            used_huge = 1;
        } else {
            paging_map_page(virt, phys, flags);
            offset += PAGE_SIZE_4K;
        }
    }

    // Los mapeos huge no pasan por invlpg individual (ver map_1gb/map_2mb);
    // una unica recarga de TLB al final es mas barata que ir calculando
    // cuantas invlpg de 4KiB harian falta para cubrir el rango.
    if (used_huge)
        flush_tlb_full();
    return 0;
}

int paging_create_user_space(uint64_t *pml4_physical)
{
    uint64_t physical;
    uint64_t *new_pml4;
    uint64_t *current;

    if (!pml4_physical || !direct_map_ready)
        return -1;
    physical = pmm_alloc_page();
    if (!physical)
        return -1;
    new_pml4 = (uint64_t *)(uintptr_t)paging_phys_to_virt(physical);
    current = current_pml4();
    memcpy(new_pml4, current, PAGE_SIZE_4K);
    /* User mappings are added explicitly by the process loader. */
    new_pml4[0] &= ~PAGING_USER;
    new_pml4[256] &= ~PAGING_USER;
    new_pml4[511] &= ~PAGING_USER;
    *pml4_physical = physical;
    return 0;
}

int paging_map_page_in_space(uint64_t pml4_physical,
                                uint64_t virtual_address,
                                uint64_t physical_address,
                                uint64_t flags)
{
    uint64_t previous_cr3;
    int result;

    if (!direct_map_ready || !pml4_physical ||
        (virtual_address % PAGE_SIZE_4K) != 0 ||
        (physical_address % PAGE_SIZE_4K) != 0)
        return -1;
    previous_cr3 = read_cr3();
    write_cr3(pml4_physical);
    result = paging_map_page(virtual_address, physical_address,
                             flags | PAGING_USER);
    write_cr3(previous_cr3);
    return result;
}

int paging_activate_space(uint64_t pml4_physical)
{
    if (!direct_map_ready || (pml4_physical & (PAGE_SIZE_4K - 1)) != 0)
        return -1;
    write_cr3(pml4_physical);
    return 0;
}
