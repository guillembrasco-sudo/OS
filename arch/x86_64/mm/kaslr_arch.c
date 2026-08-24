#include <stdint.h>
#include <arch/kaslr.h>

#define KASLR_ALIGN (2ULL * 1024 * 1024)
#define KASLR_SLOTS 256

static uint64_t kernel_slide;
extern uint64_t boot_kaslr_seed;

void kaslr_arch_init(void)
{
    uint64_t entropy = boot_kaslr_seed;
    if (entropy == 0)
        __asm__ volatile ("rdtsc" : "=a"(*(uint32_t *)&entropy),
                          "=d"(*((uint32_t *)&entropy + 1)));
    kernel_slide = (entropy % KASLR_SLOTS) * KASLR_ALIGN;
}

uint64_t kaslr_arch_slide(void)
{
    return kernel_slide;
}