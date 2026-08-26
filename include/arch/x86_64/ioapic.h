#ifndef ARCH_X86_64_IOAPIC_H
#define ARCH_X86_64_IOAPIC_H

#include <stdint.h>

int ioapic_init(uint64_t physical_base, uint32_t gsi_base);
int ioapic_route_irq(uint8_t irq, uint8_t vector, uint8_t destination_apic_id);
int ioapic_is_ready(void);

#endif
