#include <mm/heap.h>
#include <mm/pmm.h>
#include <arch/paging.h>
#include <stddef.h>
#include <kernel/process.h>

// NOTA: pmm_free_page() y paging_unmap_page_in_space() se asumen con
// estas firmas por simetría con pmm_alloc_page()/paging_map_page_in_space()
// (usadas ya en process.c). Si tu pmm.h/paging.h las nombra distinto,
// ajusta solo estas dos llamadas; el resto del fichero no depende de ello.

#define HEAP_PAGE_SIZE 0x1000ULL

static inline uintptr_t align_up(uintptr_t addr, uintptr_t align)
{
	return (addr + align - 1) & ~(align - 1);
}

// Mapea las páginas físicas que cubren [old_end, new_end) en el
// espacio de usuario de `proc`. Si falla a mitad de camino, desmapea
// lo que esta misma llamada haya mapeado, para no dejar el heap en un
// estado parcialmente mapeado.
static int heap_grow_pages(struct process *proc, uintptr_t old_end,
                             uintptr_t new_end)
{
	uintptr_t start = align_up(old_end, HEAP_PAGE_SIZE);
	uintptr_t end = align_up(new_end, HEAP_PAGE_SIZE);
	uintptr_t va = start;

	for (; va < end; va += HEAP_PAGE_SIZE) {
		uint64_t phys = pmm_alloc_page();
		if (!phys)
			goto rollback;
		if (paging_map_page_in_space(proc->pml4_physical, va, phys,
				PAGING_WRITE) != 0) {
			pmm_free_page(phys);
			goto rollback;
		}
	}
	return 0;

rollback:
	for (uintptr_t undo = start; undo < va; undo += HEAP_PAGE_SIZE)
		paging_unmap_page_in_space(proc->pml4_physical, undo);
	return -1;
}

// Desmapea y libera las páginas físicas que cubrían [new_end, old_end).
static void heap_shrink_pages(struct process *proc, uintptr_t new_end,
                                uintptr_t old_end)
{
	uintptr_t start = align_up(new_end, HEAP_PAGE_SIZE);
	uintptr_t end = align_up(old_end, HEAP_PAGE_SIZE);

	for (uintptr_t va = start; va < end; va += HEAP_PAGE_SIZE)
		paging_unmap_page_in_space(proc->pml4_physical, va);
}

int heap_brk(struct process *proc, uintptr_t new_end)
{
	if (!proc)
		return -1;
	if (new_end < proc->heap_start || new_end > proc->heap_limit)
		return -1;

	if (new_end > proc->heap_end) {
		if (heap_grow_pages(proc, proc->heap_end, new_end) != 0)
			return -1;
	} else if (new_end < proc->heap_end) {
		heap_shrink_pages(proc, new_end, proc->heap_end);
	}

	proc->heap_end = new_end;
	return 0;
}

uintptr_t heap_sbrk(struct process *proc, intptr_t increment)
{
	if (!proc)
		return (uintptr_t)-1;

	uintptr_t old_end = proc->heap_end;
	uintptr_t new_end = (increment >= 0)
		? old_end + (uintptr_t)increment
		: old_end - (uintptr_t)(-increment);

	// Comprueba overflow/underflow antes de tocar el mapeo.
	if (increment >= 0 && new_end < old_end)
		return (uintptr_t)-1;
	if (increment < 0 && new_end > old_end)
		return (uintptr_t)-1;

	if (heap_brk(proc, new_end) != 0)
		return (uintptr_t)-1;

	return old_end;
}