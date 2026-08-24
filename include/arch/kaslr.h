#ifndef ARCH_KASLR_H
#define ARCH_KASLR_H

#include <stdint.h>

void kaslr_arch_init(void);
uint64_t kaslr_arch_slide(void);

#endif