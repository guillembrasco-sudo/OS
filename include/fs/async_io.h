#ifndef FS_ASYNC_IO_H
#define FS_ASYNC_IO_H

#include <stdint.h>

#define ASYNC_IO_RING_SIZE 256

struct io_request {
    uint64_t user_data;
    uint64_t address;
    uint32_t length;
    uint32_t operation;
};

struct io_completion {
    uint64_t user_data;
    int64_t result;
    uint32_t flags;
};

struct io_ring {
    volatile uint32_t submission_head;
    volatile uint32_t submission_tail;
    volatile uint32_t completion_head;
    volatile uint32_t completion_tail;
    struct io_request submissions[ASYNC_IO_RING_SIZE];
    struct io_completion completions[ASYNC_IO_RING_SIZE];
};

int io_ring_submit(struct io_ring *ring, const struct io_request *request);
int io_ring_next_submission(struct io_ring *ring, struct io_request *request);
int io_ring_complete(struct io_ring *ring, const struct io_completion *completion);
int io_ring_wait_completion(struct io_ring *ring, struct io_completion *completion);

#endif