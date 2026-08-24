#ifndef KERNEL_SPINLOCK_H
#define KERNEL_SPINLOCK_H

#include <stdint.h>

struct qspinlock {
    volatile uint32_t state;
};

#define QSPINLOCK_INITIALIZER { 0 }

void qspin_lock(struct qspinlock *lock);
void qspin_unlock(struct qspinlock *lock);
int qspin_trylock(struct qspinlock *lock);

#endif