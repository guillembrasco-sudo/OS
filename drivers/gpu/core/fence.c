#include <drivers/gpu_fence.h>
#include <hal/cpu.h>

void gpu_fence_init(struct gpu_fence *fence)
{
    fence->submitted = 0;
    fence->completed = 0;
}

uint64_t gpu_fence_submit(struct gpu_fence *fence)
{
    return __atomic_add_fetch(&fence->submitted, 1, __ATOMIC_RELAXED);
}

void gpu_fence_signal(struct gpu_fence *fence, uint64_t sequence)
{
    uint64_t completed = __atomic_load_n(&fence->completed, __ATOMIC_RELAXED);
    while (sequence > completed &&
           !__atomic_compare_exchange_n(&fence->completed, &completed,
                                        sequence, 0, __ATOMIC_RELEASE,
                                        __ATOMIC_RELAXED))
        ;
}

int gpu_fence_is_complete(const struct gpu_fence *fence, uint64_t sequence)
{
    return __atomic_load_n(&fence->completed, __ATOMIC_ACQUIRE) >= sequence;
}

int gpu_fence_wait(const struct gpu_fence *fence, uint64_t sequence)
{
    while (!gpu_fence_is_complete(fence, sequence))
        arch_cpu_relax();
    return 0;
}