#include <stdint.h>
#include <kernel/rcu.h>
#include <hal/cpu.h>

#define RCU_MAX_CPUS 64

static volatile uint32_t rcu_readers[RCU_MAX_CPUS];
static volatile uint64_t rcu_epoch;

void rcu_init(void)
{
    for (unsigned cpu = 0; cpu < RCU_MAX_CPUS; ++cpu)
        rcu_readers[cpu] = 0;
    rcu_epoch = 0;
}

void rcu_read_lock(void)
{
    unsigned cpu = arch_cpu_id() % RCU_MAX_CPUS;
    __atomic_add_fetch(&rcu_readers[cpu], 1, __ATOMIC_ACQUIRE);
}

void rcu_read_unlock(void)
{
    unsigned cpu = arch_cpu_id() % RCU_MAX_CPUS;
    __atomic_sub_fetch(&rcu_readers[cpu], 1, __ATOMIC_RELEASE);
}

void synchronize_rcu(void)
{
    __atomic_add_fetch(&rcu_epoch, 1, __ATOMIC_SEQ_CST);
    for (;;) {
        uint32_t active = 0;
        for (unsigned cpu = 0; cpu < RCU_MAX_CPUS; ++cpu)
            active |= __atomic_load_n(&rcu_readers[cpu], __ATOMIC_ACQUIRE);
        if (active == 0)
            return;
        __asm__ volatile ("pause");
    }
}