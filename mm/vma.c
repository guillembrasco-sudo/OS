#include <mm/vma.h>
#include <mm/pmm.h>
#include <arch/paging.h>
#include <stddef.h>

// Ver nota de mm/heap.c sobre pmm_free_page()/paging_unmap_page_in_space().

#define VMA_PAGE_SIZE 0x1000ULL

static inline uintptr_t align_up(uintptr_t addr, uintptr_t align)
{
	return (addr + align - 1) & ~(align - 1);
}

static inline int paging_flags_from_prot(uint32_t prot)
{
	return (prot & VMA_PROT_WRITE) ? PAGING_WRITE : 0;
}

uintptr_t vma_map_anonymous(struct process *proc, size_t length, uint32_t prot)
{
    if (!proc || length == 0)
        return 0;

    size_t page_count = align_up(length, VMA_PAGE_SIZE) / VMA_PAGE_SIZE;
    uintptr_t base = proc->mmap_next;
    int flags = paging_flags_from_prot(prot);
    uintptr_t va = base;
    size_t allocated = 0;

    for (size_t i = 0; i < page_count; i++, va += VMA_PAGE_SIZE) {
        uint64_t phys = pmm_alloc_page();
        if (!phys)
            goto rollback;

        if (paging_map_page_in_space(proc->pml4_physical, va, phys, flags) != 0) {
            pmm_free_page(phys);
            goto rollback;
        }
        allocated++;
    }

    proc->mmap_next = base + page_count * VMA_PAGE_SIZE;
    return base;

rollback:
    for (size_t j = 0; j < allocated; j++) {
        uintptr_t undo = base + j * VMA_PAGE_SIZE;
        uint64_t phys = paging_translate_in_space(proc->pml4_physical, undo);
        paging_unmap_page_in_space(proc->pml4_physical, undo);
        if (phys != (uint64_t)-1)
            pmm_free_page(phys);
    }
    return 0;
}

uintptr_t vma_map_shared(struct process *owner, struct process *other,
                          size_t length, uint32_t prot, uintptr_t *mapped_at)
{
    if (!owner || !other || length == 0)
        return 0;

    size_t page_count = align_up(length, VMA_PAGE_SIZE) / VMA_PAGE_SIZE;
    uintptr_t owner_base = owner->mmap_next;
    uintptr_t other_base = other->mmap_next;
    int flags = paging_flags_from_prot(prot);

    uintptr_t va_owner = owner_base;
    uintptr_t va_other = other_base;
    size_t allocated = 0;

    for (size_t i = 0; i < page_count; i++,
         va_owner += VMA_PAGE_SIZE, va_other += VMA_PAGE_SIZE) {
        uint64_t phys = pmm_alloc_page();
        if (!phys)
            goto rollback;

        if (paging_map_page_in_space(owner->pml4_physical, va_owner, phys, flags) != 0) {
            pmm_free_page(phys);
            goto rollback;
        }

        if (paging_map_page_in_space(other->pml4_physical, va_other, phys, flags) != 0) {
            paging_unmap_page_in_space(owner->pml4_physical, va_owner);
            pmm_free_page(phys);
            goto rollback;
        }
        allocated++;
    }

    owner->mmap_next = owner_base + page_count * VMA_PAGE_SIZE;
    other->mmap_next = other_base + page_count * VMA_PAGE_SIZE;
    if (mapped_at)
        *mapped_at = other_base;
    return owner_base;

rollback:
    for (size_t j = 0; j < allocated; j++) {
        uintptr_t u_owner = owner_base + j * VMA_PAGE_SIZE;
        uintptr_t u_other = other_base + j * VMA_PAGE_SIZE;

        uint64_t phys = paging_translate_in_space(owner->pml4_physical, u_owner);
        paging_unmap_page_in_space(owner->pml4_physical, u_owner);
        paging_unmap_page_in_space(other->pml4_physical, u_other);

        if (phys != (uint64_t)-1)
            pmm_free_page(phys);
    }
    return 0;
}

int vma_unmap(struct process *proc, uintptr_t addr, size_t length)
{
    if (!proc || length == 0)
        return -1;

    uintptr_t start = addr & ~(VMA_PAGE_SIZE - 1);
    uintptr_t end = align_up(addr + length, VMA_PAGE_SIZE);

    for (uintptr_t va = start; va < end; va += VMA_PAGE_SIZE) {
        uint64_t phys = paging_translate_in_space(proc->pml4_physical, va);
        if (phys != (uint64_t)-1) {
            paging_unmap_page_in_space(proc->pml4_physical, va);
            pmm_free_page(phys);
        }
    }

    return 0;
}