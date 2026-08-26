#include <stdint.h>
#include <arch/x86_64/msr.h>

#define MSR_EFER 0xC0000080u
#define MSR_STAR 0xC0000081u
#define MSR_LSTAR 0xC0000082u
#define MSR_SFMASK 0xC0000084u
#define EFER_SCE (1ULL << 0)
#define RFLAGS_IF (1ULL << 9)

static uint64_t msr_read(uint32_t index)
{
	uint32_t low;
	uint32_t high;
	__asm__ __volatile__ ("rdmsr" : "=a"(low), "=d"(high) : "c"(index));
	return ((uint64_t)high << 32) | low;
}

static void msr_write(uint32_t index, uint64_t value)
{
	uint32_t low = (uint32_t)value;
	uint32_t high = (uint32_t)(value >> 32);
	__asm__ __volatile__ ("wrmsr" : : "a"(low), "d"(high), "c"(index));
}

void x86_syscall_enable(uintptr_t entry)
{
	uint64_t efer = msr_read(MSR_EFER);

	/* GDT: kernel CS=0x08, user CS=0x18, user SS=0x20. */
	msr_write(MSR_STAR, (0x08ULL << 32) | (0x08ULL << 48));
	msr_write(MSR_LSTAR, (uint64_t)entry);
	msr_write(MSR_SFMASK, RFLAGS_IF);
	msr_write(MSR_EFER, efer | EFER_SCE);
}
