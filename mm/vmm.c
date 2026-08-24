#include <stdint.h>
#include <stddef.h>
#include <mm/vmm.h>

#define USER_DS 0x0000800000000000ULL

int vmm_init(void)
{
	return 0;
}

int vmm_kernel_map(uintptr_t physical, size_t pages, uint64_t flags,
				   uintptr_t *virtual_address)
{
	if (physical == 0 || pages == 0 || virtual_address == 0)
		return -1;
	*virtual_address = physical;
	(void)flags;
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
