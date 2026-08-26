#include <stdint.h>
#include <kernel/mpmc.h>
#include <kernel/handles.h>
#include <drivers/gpu_fence.h>
#include <kernel/window_system.h>

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

static int test_window_primitives(void)
{
    uint32_t pixels[9];

    asm_fast_clear_buffer(pixels, 9, 0xAABBCCDDu);
    for (unsigned index = 0; index < 9; ++index)
        if (pixels[index] != 0xAABBCCDDu)
            return -1;

    if (!asm_window_hit_test(10, 20, 10, 20, 5, 5))
        return -1;
    if (asm_window_hit_test(15, 20, 10, 20, 5, 5))
        return -1;
    if (asm_window_hit_test(10, 20, 10, 20, 0, 5))
        return -1;
    return 0;
}

static int test_window_lifecycle(void)
{
    WindowManager manager;
    Window *window;

    window_manager_init(&manager, 800, 600, (WindowAllocator){0});
    window = window_create(
        &manager, "host-test", 20, 30, 240, 140, 0x11223344u, NULL, NULL
    );
    if (!window)
        return -1;
    if (window->backbuffer.pixels[0] != 0x11223344u)
        return -1;
    if (window_manager_pick_window(&manager, 25, 40) != window)
        return -1;
    if (!window_minimize(&manager, window) ||
        window->state != WINDOW_STATE_MINIMIZED ||
        (window->flags & WINDOW_FLAG_VISIBLE))
        return -1;
    if (!window_restore(&manager, window) ||
        window->state != WINDOW_STATE_NORMAL ||
        !(window->flags & WINDOW_FLAG_VISIBLE))
        return -1;

    window_destroy(&manager, window);
    return manager.head == NULL && manager.tail == NULL ? 0 : -1;
}

static uint8_t standby_event;

static void test_standby_callback(uint8_t active, void *ctx)
{
    (void)ctx;
    standby_event = active;
}

static int test_window_standby(void)
{
    WindowManager manager;

    window_manager_init(&manager, 800, 600, (WindowAllocator){0});
    window_manager_set_standby_callback(
        &manager, test_standby_callback, NULL
    );
    window_manager_enter_standby(&manager);
    if (!window_manager_is_standby(&manager) || standby_event != 1)
        return -1;

    window_dispatch_key_down(&manager, 0x1c, 0);
    if (window_manager_is_standby(&manager) || standby_event != 0)
        return -1;
    return 0;
}

static int test_master_console_commands(void)
{
    WindowManager manager;

    window_manager_init(&manager, 1024, 768, (WindowAllocator){0});
    if (window_master_console_execute(&manager, "scstat") != 0 ||
        window_master_console_execute(&manager, "files") != 0 ||
        window_master_console_execute(&manager, "hour") != 0 ||
        window_master_console_execute(&manager, "unknown") == 0)
        return -1;
    if (!manager.head || !manager.head->next || !manager.tail)
        return -1;
    return 0;
}

int main(void)
{
    if (test_mpmc() != 0 || test_handles() != 0 || test_fence() != 0 ||
        test_window_primitives() != 0 || test_window_lifecycle() != 0 ||
        test_window_standby() != 0 || test_master_console_commands() != 0)
        return 1;
    return 0;
}
