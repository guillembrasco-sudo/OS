#include <stdint.h>
#include <kernel/smp.h>
#include <arch/x86_64/lapic.h>
#include <mm/kheap.h>
#include <lib/string.h>

#define TRAMPOLINE_PHYS 0x8000ULL
#define TRAMPOLINE_CONFIG 0x7000ULL
#define AP_STACK_SIZE 16384

extern uint8_t trampoline_start[];
extern uint8_t trampoline_end[];

static void smp_ap_entry(uint32_t apic_id)
{
	(void)apic_id;
	smp_ap_online();
	for (;;) {
		__asm__ volatile("hlt");
	}
}

static uint32_t detected_cpus = 1;
static uint32_t online_cpus = 1;

void smp_init(void)
{
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;

	__asm__ volatile("cpuid"
		: "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
		: "a"(1), "c"(0));
	(void)eax;
	(void)ecx;
	(void)edx;
	detected_cpus = (ebx >> 16) & 0xff;
	if (detected_cpus == 0)
		detected_cpus = 1;
	/* AP startup requires a configured LAPIC and trampoline. */
	online_cpus = 1;
	if ((uintptr_t)(trampoline_end - trampoline_start) > 0x1000)
		return;
	memcpy((void *)(uintptr_t)TRAMPOLINE_PHYS, trampoline_start,
	       (size_t)(trampoline_end - trampoline_start));
}

uint32_t smp_detected_cpus(void)
{
	return detected_cpus;
}

uint32_t smp_online_cpus(void)
{
	return online_cpus;
}

int smp_start_secondary(uint8_t apic_id, uint8_t startup_vector)
{
	if (apic_id == (uint8_t)(lapic_read(0x20) >> 24) ||
	    startup_vector == 0 || !lapic_is_ready())
		return -1;
	if (lapic_send_init(apic_id) != 0 ||
	    lapic_send_startup(apic_id, startup_vector) != 0 ||
	    lapic_send_startup(apic_id, startup_vector) != 0)
		return -1;
	return 0;
}

int smp_start_all_secondary(const uint8_t *apic_ids, uint32_t count,
	                            uint32_t kernel_cr3)
{
	if (!apic_ids || count == 0 || !lapic_is_ready() || kernel_cr3 == 0)
		return -1;
	for (uint32_t index = 0; index < count; ++index) {
		uint8_t *stack = kmalloc(AP_STACK_SIZE);
		if (!stack)
			return -1;
		*(uint32_t *)(uintptr_t)(TRAMPOLINE_CONFIG) = kernel_cr3;
		*(uint64_t *)(uintptr_t)(TRAMPOLINE_CONFIG + 8) =
			(uintptr_t)(stack + AP_STACK_SIZE - 16);
		*(uint64_t *)(uintptr_t)(TRAMPOLINE_CONFIG + 16) =
			(uintptr_t)smp_ap_entry;
		*(uint32_t *)(uintptr_t)(TRAMPOLINE_CONFIG + 24) = apic_ids[index];
		if (smp_start_secondary(apic_ids[index],
		                        (uint8_t)(TRAMPOLINE_PHYS >> 12)) != 0)
			return -1;
	}
	return 0;
}

void smp_ap_online(void)
{
	if (online_cpus < detected_cpus)
		++online_cpus;
}
