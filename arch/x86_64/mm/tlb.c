#include <stdint.h>
#include <arch/tlb.h>

#define CR4_PCIDE (1ULL << 17)

static inline uint64_t read_cr4(void)
{
    uint64_t value;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(value));
    return value;
}

static inline void write_cr4(uint64_t value)
{
    __asm__ volatile ("mov %0, %%cr4" : : "r"(value) : "memory");
}

int x86_pcid_enable(void)
{
    uint64_t cr4 = read_cr4();
    if ((cr4 & CR4_PCIDE) != 0)
        return 0;
    write_cr4(cr4 | CR4_PCIDE);
    return 0;
}

uint64_t x86_cr3_make(uint64_t physical_root, uint16_t pcid, int no_flush)
{
    return (physical_root & ~0xfffULL) |
           (uint64_t)(pcid & 0xfffU) |
           (no_flush ? (1ULL << 63) : 0);
}

void x86_switch_address_space(uint64_t physical_root, uint16_t pcid, int no_flush)
{
    uint64_t cr3 = x86_cr3_make(physical_root, pcid, no_flush);
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

void x86_invalidate_page(uintptr_t address)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory");
}