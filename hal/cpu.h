#ifndef HAL_CPU_H
#define HAL_CPU_H

#include <stdint.h>

struct cpu_info {
	uint32_t id;
	uint32_t features;
	uint64_t frequency_hz;
};

int arch_cpu_init(void);
uint32_t arch_cpu_id(void);
void arch_cpu_halt(void);
void arch_cpu_enable_interrupts(void);
void arch_cpu_disable_interrupts(void);
void arch_cpu_relax(void);
void arch_console_putc(char character);

#endif
