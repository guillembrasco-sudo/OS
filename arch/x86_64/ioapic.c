#include <stdint.h>
#include <arch/paging.h>
#include <arch/x86_64/ioapic.h>

#define IOAPIC_MAP_BASE 0xFFFFFD0000000000ULL
#define IOAPIC_REGSEL 0x00
#define IOAPIC_WINDOW 0x10

static volatile uint32_t *ioapic_base;
static uint32_t ioapic_gsi_base;

static void ioapic_select(uint8_t register_index)
{
	ioapic_base[IOAPIC_REGSEL / 4] = register_index;
}

static void ioapic_write(uint8_t register_index, uint32_t value)
{
	ioapic_select(register_index);
	ioapic_base[IOAPIC_WINDOW / 4] = value;
}

int ioapic_init(uint64_t physical_base, uint32_t gsi_base)
{
	if (!physical_base || paging_map_region(
			physical_base & ~0xfffULL, IOAPIC_MAP_BASE, 0x1000,
			PAGING_WRITE, 0, 0) != 0)
		return -1;
	ioapic_base = (volatile uint32_t *)(uintptr_t)IOAPIC_MAP_BASE;
	ioapic_gsi_base = gsi_base;
	return 0;
}

int ioapic_route_irq(uint8_t irq, uint8_t vector, uint8_t destination_apic_id)
{
	uint64_t redirection;
	uint32_t index;
	if (!ioapic_base || irq < ioapic_gsi_base)
		return -1;
	index = (uint32_t)irq - ioapic_gsi_base;
	if (index >= 24)
		return -1;
	redirection = (uint64_t)vector |
	              ((uint64_t)destination_apic_id << 56);
	ioapic_write((uint8_t)(0x10 + index * 2 + 1), redirection >> 32);
	ioapic_write((uint8_t)(0x10 + index * 2), redirection & 0xffffffffu);
	return 0;
}

int ioapic_is_ready(void)
{
	return ioapic_base != 0;
}
