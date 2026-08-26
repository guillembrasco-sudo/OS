#ifndef KERNEL_SMP_H
#define KERNEL_SMP_H

#include <stdint.h>

void smp_init(void);
uint32_t smp_detected_cpus(void);
uint32_t smp_online_cpus(void);
int smp_start_secondary(uint8_t apic_id, uint8_t startup_vector);
void smp_ap_online(void);
int smp_start_all_secondary(const uint8_t *apic_ids, uint32_t count,
							uint32_t kernel_cr3);

#endif
