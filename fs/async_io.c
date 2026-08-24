#include <fs/async_io.h>

static uint32_t ring_next(uint32_t index)
{
    return (index + 1u) % ASYNC_IO_RING_SIZE;
}

int io_ring_submit(struct io_ring *ring, const struct io_request *request)
{
    uint32_t head;
    uint32_t next;
    if (ring == 0 || request == 0)
        return -1;
    head = ring->submission_head;
    next = ring_next(head);
    if (next == ring->submission_tail)
        return -1;
    ring->submissions[head] = *request;
    __asm__ volatile ("" ::: "memory");
    ring->submission_head = next;
    return 0;
}

int io_ring_next_submission(struct io_ring *ring, struct io_request *request)
{
    uint32_t tail;
    if (ring == 0 || request == 0)
        return -1;
    tail = ring->submission_tail;
    if (tail == ring->submission_head)
        return -1;
    __asm__ volatile ("" ::: "memory");
    *request = ring->submissions[tail];
    ring->submission_tail = ring_next(tail);
    return 0;
}

int io_ring_complete(struct io_ring *ring, const struct io_completion *completion)
{
    uint32_t head;
    uint32_t next;
    if (ring == 0 || completion == 0)
        return -1;
    head = ring->completion_head;
    next = ring_next(head);
    if (next == ring->completion_tail)
        return -1;
    ring->completions[head] = *completion;
    __asm__ volatile ("" ::: "memory");
    ring->completion_head = next;
    return 0;
}

int io_ring_wait_completion(struct io_ring *ring, struct io_completion *completion)
{
    uint32_t tail;
    if (ring == 0 || completion == 0)
        return -1;
    tail = ring->completion_tail;
    if (tail == ring->completion_head)
        return -1;
    __asm__ volatile ("" ::: "memory");
    *completion = ring->completions[tail];
    ring->completion_tail = ring_next(tail);
    return 0;
}