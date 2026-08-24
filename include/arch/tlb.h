#ifndef ARCH_TLB_H
#define ARCH_TLB_H

#include <stdint.h>

int x86_pcid_enable(void);
uint64_t x86_cr3_make(uint64_t physical_root, uint16_t pcid, int no_flush);
void x86_switch_address_space(uint64_t physical_root, uint16_t pcid, int no_flush);
void x86_invalidate_page(uintptr_t address);

#endif