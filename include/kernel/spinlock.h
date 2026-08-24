// include/kernel/spinlock.h
#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t locked;
#ifdef SPINLOCK_DEBUG
    const char *owner_file;
    int         owner_line;
#endif
} spinlock_t;

#define SPINLOCK_INIT { .locked = 0 }

static inline void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

static inline void spinlock_acquire(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        // No hacemos busy-wait "duro": PAUSE reduce el consumo del
        // pipeline y evita el penalty de memory-order-violation en
        // arquitecturas que especulan la salida del bucle.
        while (lock->locked) {
            __asm__ __volatile__("pause" ::: "memory");
        }
    }
}

static inline int spinlock_try_acquire(spinlock_t *lock) {
    return !__sync_lock_test_and_set(&lock->locked, 1);
}

static inline void spinlock_release(spinlock_t *lock) {
    __sync_lock_release(&lock->locked);
}

// Variante IRQ-safe: para código compartido entre contexto de IRQ y
// contexto normal del kernel hay que deshabilitar interrupciones,
// no solo tomar el lock, o un IRQ en el mismo core puede
// re-entrar y deadlockear contra sí mismo.
static inline uint64_t spinlock_acquire_irqsave(spinlock_t *lock) {
    uint64_t flags;
    __asm__ __volatile__("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    spinlock_acquire(lock);
    return flags;
}

static inline void spinlock_release_irqrestore(spinlock_t *lock, uint64_t flags) {
    spinlock_release(lock);
    __asm__ __volatile__("push %0; popfq" :: "r"(flags) : "memory", "cc");
}

#endif // SPINLOCK_H