#ifndef MM_VMM_H
#define MM_VMM_H

#include <stddef.h>
#include <stdint.h>
#include <hal/cpu_features.h>

#define VMM_PAGE_PRESENT 0x001ULL
#define VMM_PAGE_WRITE   0x002ULL
#define VMM_PAGE_USER    0x004ULL

int vmm_init(void);
int vmm_kernel_map(uintptr_t physical, size_t pages, uint64_t flags,
                  uintptr_t *virtual_address);
int vmm_user_range_valid(uintptr_t address, size_t length, int write);
int vmm_map_2m(uint64_t *page_directory, uint64_t virtual_address,
               uint64_t physical_address, uint64_t flags);
int vmm_map_1g(uint64_t *page_directory_pointer, uint64_t virtual_address,
               uint64_t physical_address, uint64_t flags);

void vmm_map_1gb_pages(uint64_t phys_base, uint64_t virt_base, uint64_t size);
void vmm_map_2mb_pages(uint64_t phys_base, uint64_t virt_base, uint64_t size);
void vmm_map_kernel_higher_half(uint64_t phys_base, uint64_t virt_base, uint64_t size);

#endif