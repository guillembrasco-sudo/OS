#include <stdint.h>
#include <arch/paging.h>
#include <arch/x86_64/lapic.h>

#define LAPIC_MAP_BASE 0xFFFFFE0000000000ULL
#define LAPIC_REG_EOI 0x0B0
#define LAPIC_REG_SVR 0x0F0
#define LAPIC_REG_ICR_LOW 0x300
#define LAPIC_REG_ICR_HIGH 0x310
#define LAPIC_ENABLE 0x100
#define LAPIC_ICR_DELIVERY_STATUS (1u << 12)
#define LAPIC_ICR_INIT (5u << 8)
#define LAPIC_ICR_STARTUP (6u << 8)
#define LAPIC_ICR_LEVEL (1u << 14)

static volatile uint32_t *lapic_base;

int lapic_init(uint64_t physical_base)
{
	if (!physical_base || paging_map_region(
			physical_base & ~0xfffULL, LAPIC_MAP_BASE, 0x1000,
			PAGING_WRITE, 0, 0) != 0)
		return -1;
	lapic_base = (volatile uint32_t *)(uintptr_t)LAPIC_MAP_BASE;
	lapic_write(LAPIC_REG_SVR, lapic_read(LAPIC_REG_SVR) | LAPIC_ENABLE | 0xff);
	return 0;
}

void lapic_write(uint32_t offset, uint32_t value)
{
	if (lapic_base && offset < 0x1000 && (offset & 3) == 0)
		lapic_base[offset / 4] = value;
}

uint32_t lapic_read(uint32_t offset)
{
	if (!lapic_base || offset >= 0x1000 || (offset & 3) != 0)
		return 0;
	return lapic_base[offset / 4];
}

void lapic_eoi(void)
{
	lapic_write(LAPIC_REG_EOI, 0);
}

static int lapic_wait_ipi(void)
{
	for (uint32_t attempt = 0; attempt < 100000; ++attempt)
		if (!(lapic_read(LAPIC_REG_ICR_LOW) & LAPIC_ICR_DELIVERY_STATUS))
			return 0;
	return -1;
}

int lapic_send_init(uint8_t destination_apic_id)
{
	if (!lapic_base || lapic_wait_ipi() != 0)
		return -1;
	lapic_write(LAPIC_REG_ICR_HIGH, (uint32_t)destination_apic_id << 24);
	lapic_write(LAPIC_REG_ICR_LOW, LAPIC_ICR_INIT | LAPIC_ICR_LEVEL);
	if (lapic_wait_ipi() != 0)
		return -1;
	lapic_write(LAPIC_REG_ICR_LOW, LAPIC_ICR_INIT);
	return lapic_wait_ipi();
}

int lapic_send_startup(uint8_t destination_apic_id, uint8_t vector)
{
	if (!lapic_base || vector == 0 || lapic_wait_ipi() != 0)
		return -1;
	lapic_write(LAPIC_REG_ICR_HIGH, (uint32_t)destination_apic_id << 24);
	lapic_write(LAPIC_REG_ICR_LOW, LAPIC_ICR_STARTUP | vector);
	return lapic_wait_ipi();
}

int lapic_is_ready(void)
{
	return lapic_base != 0;
}
