#include <kernel/spinlock.h>

#define QSPIN_LOCKED 1u

void qspin_lock(struct qspinlock *lock)
{
	while (__atomic_exchange_n(&lock->state, QSPIN_LOCKED, __ATOMIC_ACQUIRE) != 0) {
		while (__atomic_load_n(&lock->state, __ATOMIC_RELAXED) & QSPIN_LOCKED)
			__asm__ volatile ("pause");
	}
}

int qspin_trylock(struct qspinlock *lock)
{
	uint32_t expected = 0;
	return __atomic_compare_exchange_n(&lock->state, &expected, QSPIN_LOCKED,
									   0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

void qspin_unlock(struct qspinlock *lock)
{
	__atomic_store_n(&lock->state, 0, __ATOMIC_RELEASE);
}
