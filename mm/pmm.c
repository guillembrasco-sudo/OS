#include <mm/pmm.h>

#define PMM_MAX_PAGES 16384
#define PMM_BASE 0x00100000UL

static uint8_t page_used[PMM_MAX_PAGES];

int pmm_init(void)
{
	for (unsigned index = 0; index < PMM_MAX_PAGES; ++index)
		page_used[index] = 0;
	return 0;
}

uintptr_t pmm_alloc_contiguous(size_t pages)
{
	if (pages == 0 || pages > PMM_MAX_PAGES)
		return 0;
	for (size_t start = 0; start + pages <= PMM_MAX_PAGES; ++start) {
		size_t index;
		for (index = 0; index < pages && page_used[start + index] == 0; ++index)
			;
		if (index != pages)
			continue;
		for (index = 0; index < pages; ++index)
			page_used[start + index] = 1;
		return PMM_BASE + start * PMM_PAGE_SIZE;
	}
	return 0;
}

void pmm_free_contiguous(uintptr_t address, size_t pages)
{
	size_t start;
	if (pages == 0 || address < PMM_BASE ||
		((address - PMM_BASE) % PMM_PAGE_SIZE) != 0)
		return;
	start = (address - PMM_BASE) / PMM_PAGE_SIZE;
	if (start >= PMM_MAX_PAGES || pages > PMM_MAX_PAGES - start)
		return;
	for (size_t index = 0; index < pages; ++index)
		page_used[start + index] = 0;
}
