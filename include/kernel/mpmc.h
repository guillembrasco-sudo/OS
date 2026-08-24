#ifndef KERNEL_MPMC_H
#define KERNEL_MPMC_H

#include <stddef.h>
#include <stdint.h>

#define MPMC_CAPACITY 1024u
#define MPMC_PAYLOAD_SIZE 56u
#define MPMC_CACHE_LINE_SIZE 64u

#if defined(__GNUC__)
#define MPMC_ALIGNED __attribute__((aligned(MPMC_CACHE_LINE_SIZE)))
#else
#define MPMC_ALIGNED
#endif

_Static_assert((MPMC_CAPACITY & (MPMC_CAPACITY - 1u)) == 0,
               "MPMC_CAPACITY must be a power of two");

struct MPMC_ALIGNED mpmc_cell {
    volatile uint64_t sequence;
    uint8_t payload[MPMC_PAYLOAD_SIZE];
};

_Static_assert(sizeof(struct mpmc_cell) == MPMC_CACHE_LINE_SIZE,
               "MPMC cell must occupy one cache line");

struct mpmc_queue {
    volatile uint64_t enqueue_position;
    uint8_t enqueue_padding[MPMC_CACHE_LINE_SIZE - sizeof(uint64_t)];
    volatile uint64_t dequeue_position;
    uint8_t dequeue_padding[MPMC_CACHE_LINE_SIZE - sizeof(uint64_t)];
    struct mpmc_cell cells[MPMC_CAPACITY];
};

void mpmc_init(struct mpmc_queue *queue);
int mpmc_enqueue(struct mpmc_queue *queue, const void *source, size_t length);
int mpmc_dequeue(struct mpmc_queue *queue, void *destination, size_t length);

#endif