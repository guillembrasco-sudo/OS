#ifndef DRIVERS_GPU_FENCE_H
#define DRIVERS_GPU_FENCE_H

#include <stdint.h>

struct gpu_fence {
    volatile uint64_t submitted;
    volatile uint64_t completed;
};

void gpu_fence_init(struct gpu_fence *fence);
uint64_t gpu_fence_submit(struct gpu_fence *fence);
void gpu_fence_signal(struct gpu_fence *fence, uint64_t sequence);
int gpu_fence_is_complete(const struct gpu_fence *fence, uint64_t sequence);
int gpu_fence_wait(const struct gpu_fence *fence, uint64_t sequence);

#endif