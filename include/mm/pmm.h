// include/mm/pmm.h
#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PMM_PAGE_SIZE 4096

typedef struct {
    uint64_t base;
    uint64_t length;
} pmm_region_t;

extern size_t pmm_total_pages;

void     pmm_init(uint64_t mmap_addr, uint32_t mmap_len, uint64_t kernel_start, uint64_t kernel_end);
uint64_t pmm_alloc_page(void);
uint64_t pmm_alloc_pages(size_t count);   // Contiguas — necesarias para DMA/estructuras de página
void     pmm_free_page(uint64_t phys_addr);
void     pmm_free_pages(uint64_t phys_addr, size_t count);
uint64_t pmm_total_memory(void);
uint64_t pmm_free_memory(void);
void pmm_reserve_range(uint64_t physical, size_t length);
uint64_t pmm_highest_address(void);

uintptr_t pmm_alloc_contiguous(size_t pages);
void      pmm_free_contiguous(uintptr_t physical, size_t pages);

#endif // PMM_H