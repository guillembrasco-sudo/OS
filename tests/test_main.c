#include <stdint.h>
#include <kernel/mpmc.h>
#include <kernel/handles.h>
#include <drivers/gpu_fence.h>

static int test_mpmc(void)
{
    struct mpmc_queue queue;
    uint8_t source[MPMC_PAYLOAD_SIZE];
    uint8_t destination[MPMC_PAYLOAD_SIZE];
    for (unsigned index = 0; index < MPMC_PAYLOAD_SIZE; ++index)
        source[index] = (uint8_t)index;
    mpmc_init(&queue);
    if (mpmc_enqueue(&queue, source, sizeof(source)) != 0)
        return -1;
    if (mpmc_dequeue(&queue, destination, sizeof(destination)) != 0)
        return -1;
    for (unsigned index = 0; index < MPMC_PAYLOAD_SIZE; ++index)
        if (source[index] != destination[index])
            return -1;
    return 0;
}

static int test_handles(void)
{
    int object;
    handle_t handle;
    if (handle_install(7, &object, HANDLE_READ, &handle) != 0)
        return -1;
    if (handle_lookup(7, handle, HANDLE_READ) != &object)
        return -1;
    if (handle_lookup(7, handle, HANDLE_WRITE) != 0)
        return -1;
    if (handle_close(7, handle) != 0 || handle_lookup(7, handle, 0) != 0)
        return -1;
    return 0;
}

static int test_fence(void)
{
    struct gpu_fence fence;
    uint64_t sequence;
    gpu_fence_init(&fence);
    sequence = gpu_fence_submit(&fence);
    if (gpu_fence_is_complete(&fence, sequence))
        return -1;
    gpu_fence_signal(&fence, sequence);
    return gpu_fence_is_complete(&fence, sequence) ? 0 : -1;
}

int main(void)
{
    if (test_mpmc() != 0 || test_handles() != 0 || test_fence() != 0)
        return 1;
    return 0;
}
