#include <kernel/mpmc.h>
#include <hal/cpu.h>

#define MPMC_MAX_BACKOFF 64u

static inline uint64_t queue_index(uint64_t position)
{
    return position & (MPMC_CAPACITY - 1u);
}

static inline int64_t sequence_difference(uint64_t sequence, uint64_t expected)
{
    return (int64_t)(sequence - expected);
}

static inline void copy_bytes(void *destination, const void *source, size_t length)
{
    uint8_t *destination_bytes = destination;
    const uint8_t *source_bytes = source;
    for (size_t index = 0; index < length; ++index)
        destination_bytes[index] = source_bytes[index];
}

static inline void backoff(unsigned *delay)
{
    for (unsigned count = 0; count < *delay; ++count)
        arch_cpu_relax();
    if (*delay < MPMC_MAX_BACKOFF)
        *delay <<= 1;
}

void mpmc_init(struct mpmc_queue *queue)
{
    queue->enqueue_position = 0;
    queue->dequeue_position = 0;
    for (uint64_t index = 0; index < MPMC_CAPACITY; ++index)
        queue->cells[index].sequence = index;
}

int mpmc_enqueue(struct mpmc_queue *queue, const void *source, size_t length)
{
    uint64_t position;
    struct mpmc_cell *cell;
    unsigned delay = 1;

    if (queue == 0 || source == 0 || length > MPMC_PAYLOAD_SIZE)
        return -1;
    position = __atomic_load_n(&queue->enqueue_position, __ATOMIC_RELAXED);

    for (;;) {
        uint64_t sequence;
        int64_t difference;
        uint64_t candidate;

        cell = &queue->cells[queue_index(position)];
        sequence = __atomic_load_n(&cell->sequence, __ATOMIC_ACQUIRE);
        difference = sequence_difference(sequence, position);
        if (difference == 0) {
            candidate = position + 1;
            if (__atomic_compare_exchange_n(&queue->enqueue_position, &position,
                                            candidate, 0, __ATOMIC_RELAXED,
                                            __ATOMIC_RELAXED))
                break;
            backoff(&delay);
        } else if (difference < 0) {
            return -1;
        } else {
            position = __atomic_load_n(&queue->enqueue_position,
                                       __ATOMIC_RELAXED);
        }
    }

    copy_bytes(cell->payload, source, length);
    __atomic_store_n(&cell->sequence, position + 1, __ATOMIC_RELEASE);
    return 0;
}

int mpmc_dequeue(struct mpmc_queue *queue, void *destination, size_t length)
{
    uint64_t position;
    struct mpmc_cell *cell;
    unsigned delay = 1;

    if (queue == 0 || destination == 0 || length > MPMC_PAYLOAD_SIZE)
        return -1;
    position = __atomic_load_n(&queue->dequeue_position, __ATOMIC_RELAXED);

    for (;;) {
        uint64_t sequence;
        int64_t difference;
        uint64_t candidate;

        cell = &queue->cells[queue_index(position)];
        sequence = __atomic_load_n(&cell->sequence, __ATOMIC_ACQUIRE);
        difference = sequence_difference(sequence, position + 1);
        if (difference == 0) {
            candidate = position + 1;
            if (__atomic_compare_exchange_n(&queue->dequeue_position, &position,
                                            candidate, 0, __ATOMIC_RELAXED,
                                            __ATOMIC_RELAXED))
                break;
            backoff(&delay);
        } else if (difference < 0) {
            return -1;
        } else {
            position = __atomic_load_n(&queue->dequeue_position,
                                       __ATOMIC_RELAXED);
        }
    }

    copy_bytes(destination, cell->payload, length);
    __atomic_store_n(&cell->sequence, position + MPMC_CAPACITY,
                     __ATOMIC_RELEASE);
    return 0;
}