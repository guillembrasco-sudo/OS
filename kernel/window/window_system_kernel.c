#include <kernel/window_system.h>
#include <mm/kheap.h>

WindowCallbacks window_master_console_callbacks(void);
static WindowManager *kernel_window_manager;

static void *window_kernel_alloc(size_t size, void *ctx)
{
    (void)ctx;
    return kmalloc(size);
}

static void window_kernel_free(void *ptr, void *ctx)
{
    (void)ctx;
    kfree(ptr);
}

void window_manager_init_kernel(
    WindowManager *wm,
    uint32_t display_width,
    uint32_t display_height
)
{
    const WindowCallbacks console_callbacks =
        window_master_console_callbacks();
    const WindowAllocator allocator = {
        window_kernel_alloc,
        window_kernel_free,
        NULL
    };

    window_manager_init(wm, display_width, display_height, allocator);
    kernel_window_manager = wm;
    wm->master_console = window_create(
        wm,
        "Master Console",
        32,
        32,
        display_width > 640 ? 576 : 320,
        display_height > 480 ? 360 : 220,
        0x101820FFu,
        &console_callbacks,
        NULL
    );
}

int window_manager_kernel_execute_command(const char *command)
{
    if (!kernel_window_manager)
        return -1;
    return window_master_console_execute(kernel_window_manager, command);
}