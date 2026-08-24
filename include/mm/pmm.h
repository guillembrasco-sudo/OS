#ifndef MM_PMM_H
#define MM_PMM_H

#include <stddef.h>
#include <stdint.h>

#define PMM_PAGE_SIZE 4096UL

int pmm_init(void);
uintptr_t pmm_alloc_contiguous(size_t pages);
void pmm_free_contiguous(uintptr_t address, size_t pages);

#endif