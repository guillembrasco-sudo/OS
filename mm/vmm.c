// mm/vmm.c
//
// Gestor de memoria virtual de alto nivel. A partir de aqui ya no se toca
// una PML4/PDPT/PD "a mano" como hacia el codigo anterior: toda la
// manipulacion real de tablas de paginas vive en arch/x86_64/paging.c
// (paging_init / paging_map_page / paging_map_region / paging_translate).
// Este fichero se limita a: 1) inicializar el mapa directo al arrancar,
// 2) ofrecer la API de mas alto nivel (vmm_kernel_map, vmm_map_*) que el
// resto del kernel espera, montada encima de paging.c.

#include <stdint.h>
#include <stddef.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <arch/paging.h>
#include <hal/cpu/cpuid.h>

#define USER_DS 0x0000800000000000ULL

// Unica instancia real de cpu_features_t del kernel. hal/cpu/cpuid.h se
// incluye aqui (no como .c independiente: sigue el mismo patron ya usado
// por gdt.c/idt.c de fichero autocontenido) porque vmm_init() es, por
// ahora, el unico consumidor: decide con esto si puede usar paginas huge
// de 1GiB al mapear regiones grandes.
cpu_features_t g_cpu_features;

int vmm_init(void)
{
	cpu_features_detect(&g_cpu_features);

	// Construye el mapa directo (HHDM) que cubre toda la RAM fisica que
	// el PMM detecto en pmm_init(). A partir de este punto,
	// paging_phys_to_virt()/paging_map_page() ya funcionan con
	// normalidad para cualquier direccion fisica del sistema, no solo
	// para el primer GiB que boot.asm mapea a mano.
	paging_init(pmm_highest_address());

	return 0;
}

int vmm_kernel_map(uintptr_t physical, size_t pages, uint64_t flags,
				   uintptr_t *virtual_address)
{
	if (physical == 0 || pages == 0 || virtual_address == 0)
		return -1;
	(void)flags; // el mapa directo ya es RW; los flags quedan reservados
	             // para cuando haga falta una alias con permisos distintos
	             // (p.ej. una region no cacheable para MMIO).

	// Toda la RAM fisica esta accesible de forma permanente a traves del
	// mapa directo instalado por vmm_init()/paging_init(): no hace falta
	// crear un mapeo nuevo, basta con devolver el alias correspondiente.
	*virtual_address = paging_phys_to_virt((uint64_t)physical);
	return 0;
}

int vmm_user_range_valid(uintptr_t address, size_t length, int write)
{
	uintptr_t end;
	(void)write;
	if (length == 0 || address >= USER_DS ||
		length > (size_t)-1 - address)
		return 0;
	end = address + length;
	return end <= USER_DS && end > address;
}

#define PAGE_PRESENT 0x001ULL
#define PAGE_WRITE   0x002ULL
#define PAGE_HUGE    0x080ULL

// Mapeadores de bajo nivel para una sola entrada, dado un puntero directo
// a la tabla correspondiente (PD o PDPT). Se mantienen por compatibilidad
// con quien ya los llamaba pasando su propia tabla; el codigo nuevo deberia
// preferir paging_map_region()/paging_map_page(), que gestionan la
// creacion de tablas intermedias por si solos.
int vmm_map_2m(uint64_t *page_directory, uint64_t virtual_address,
			  uint64_t physical_address, uint64_t flags)
{
	uint64_t index = (virtual_address >> 21) & 0x1ff;
	page_directory[index] = (physical_address & ~0x1fffffULL) |
							flags | PAGE_PRESENT | PAGE_HUGE;
	return 0;
}

int vmm_map_1g(uint64_t *page_directory_pointer, uint64_t virtual_address,
			  uint64_t physical_address, uint64_t flags)
{
	uint64_t index = (virtual_address >> 30) & 0x1ff;
	page_directory_pointer[index] = (physical_address & ~0x3fffffffULL) |
									flags | PAGE_PRESENT | PAGE_HUGE;
	return 0;
}

// Mapea [phys_base, phys_base+size) en [virt_base, virt_base+size) usando
// exclusivamente paginas de 1GiB. Antes era un bucle vacio (el cuerpo real
// estaba comentado); ahora delega en paging_map_region(), que crea PML4/
// PDPT/PD segun haga falta via pmm_alloc_page().
void vmm_map_1gb_pages(uint64_t phys_base, uint64_t virt_base, uint64_t size) {
	paging_map_region(phys_base, virt_base, size,
	                   PAGING_PRESENT | PAGING_WRITE,
	                   /*allow_1gb=*/1, /*allow_2mb=*/0);
}

// Igual que la anterior pero con paginas de 2MiB.
void vmm_map_2mb_pages(uint64_t phys_base, uint64_t virt_base, uint64_t size) {
	paging_map_region(phys_base, virt_base, size,
	                   PAGING_PRESENT | PAGING_WRITE,
	                   /*allow_1gb=*/0, /*allow_2mb=*/1);
}

// Punto de entrada de alto nivel: mapea el rango eligiendo 1GiB si la CPU
// lo soporta y el tamano lo justifica, o 2MiB en caso contrario. Antes esto
// simplemente llamaba a las dos funciones de arriba, que eran no-ops.
void vmm_map_kernel_higher_half(uint64_t phys_base, uint64_t virt_base, uint64_t size) {
	if (g_cpu_features.has_1gb_pages && size >= (1ULL << 30)) {
		vmm_map_1gb_pages(phys_base, virt_base, size);
	} else {
		vmm_map_2mb_pages(phys_base, virt_base, size);
	}
}
