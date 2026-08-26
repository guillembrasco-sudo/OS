#ifndef ARCH_X86_64_LAPIC_H
#define ARCH_X86_64_LAPIC_H

#include <stdint.h>

int lapic_init(uint64_t physical_base);
void lapic_write(uint32_t offset, uint32_t value);
uint32_t lapic_read(uint32_t offset);
void lapic_eoi(void);
int lapic_send_init(uint8_t destination_apic_id);
int lapic_send_startup(uint8_t destination_apic_id, uint8_t vector);
int lapic_is_ready(void);

#endif
