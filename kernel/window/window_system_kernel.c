#include <kernel/window_system.h>
#include <mm/kheap.h>
#include <hal/display.h>

WindowCallbacks window_master_console_callbacks(void);
static WindowManager *kernel_window_manager;

void window_manager_present(WindowManager *wm)
{
    if (!wm)
        return;

    display_framebuffer_clear();

    for (Window *window = wm->head; window; window = window->next) {
        if (!(window->flags & WINDOW_FLAG_VISIBLE) ||
            !window->backbuffer.pixels)
            continue;
        display_framebuffer_blit(
            window->backbuffer.pixels,
            window->backbuffer.width,
            window->backbuffer.height,
            window->backbuffer.stride_pixels,
            window->x,
            window->y
        );
        display_framebuffer_fill_rect(window->x, window->y,
                                      (uint32_t)window->width, 2,
                                      0x8CA6B8FFu);
        display_framebuffer_fill_rect(window->x, window->y,
                                      2, (uint32_t)window->height,
                                      0x8CA6B8FFu);
        display_framebuffer_fill_rect(window->x,
                                      window->y + window->height - 2,
                                      (uint32_t)window->width, 2,
                                      0x526D80FFu);
        display_framebuffer_fill_rect(window->x + window->width - 2,
                                      window->y, 2,
                                      (uint32_t)window->height,
                                      0x526D80FFu);
        display_framebuffer_fill_rect(window->x + window->titlebar.x,
                                      window->y + window->titlebar.y,
                                      (uint32_t)window->titlebar.width,
                                      (uint32_t)window->titlebar.height,
                                      0x263849FFu);
        display_framebuffer_fill_rect(window->x + window->footer.x,
                          window->y + window->footer.y,
                          (uint32_t)window->footer.width,
                          (uint32_t)window->footer.height,
                          0x17232EFFu);
        display_framebuffer_fill_rect(window->x + window->maximize_button.x,
                          window->y + window->maximize_button.y,
                          (uint32_t)window->maximize_button.width,
                          (uint32_t)window->maximize_button.height,
                          0x4B7896FFu);
        display_framebuffer_fill_rect(window->x + window->minimize_button.x,
                          window->y + window->minimize_button.y,
                          (uint32_t)window->minimize_button.width,
                          (uint32_t)window->minimize_button.height,
                          0x4B7896FFu);
        display_framebuffer_fill_rect(window->x + window->close_button.x,
                          window->y + window->close_button.y,
                          (uint32_t)window->close_button.width,
                          (uint32_t)window->close_button.height,
                          0xB83B4BFFu);
        display_framebuffer_draw_text(window->title,
                                      window->x + window->title_rect.x + 4,
                                      window->y + 10, 0xE0ECF4FFu);
        display_framebuffer_draw_text("MAX",
                                      window->x + window->maximize_button.x + 28,
                                      window->y + window->maximize_button.y + 8,
                                      0xE0ECF4FFu);
        display_framebuffer_draw_text("MIN",
                                      window->x + window->minimize_button.x + 28,
                                      window->y + window->minimize_button.y + 8,
                                      0xE0ECF4FFu);
        display_framebuffer_draw_text("X",
                                      window->x + window->close_button.x + 15,
                                      window->y + window->close_button.y + 5,
                                      0xFFFFFFFFu);
        display_framebuffer_draw_text(">",
                                      window->x + window->client.x + 20,
                                      window->y + window->client.y + window->client.height - 34,
                                      0xE0ECF4FFu);
        display_framebuffer_draw_text(window->input_text,
                                      window->x + window->client.x + 34,
                                      window->y + window->client.y + window->client.height - 34,
                                      0xE0ECF4FFu);
        display_framebuffer_draw_text(window->status_text,
                          window->x + window->client.x + 16,
                          window->y + window->client.y + 20,
                          0x9FB7C8FFu);
    }
    display_framebuffer_present_cursor();
}

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
        wm
    );
    window_master_console_render(wm->master_console);
    wm->focused = wm->master_console;
    wm->master_console->flags |= WINDOW_FLAG_FOCUSED | WINDOW_FLAG_ACTIVE;
    window_manager_present(wm);
}

int window_manager_kernel_execute_command(const char *command)
{
    if (!kernel_window_manager)
        return -1;
    return window_master_console_execute(kernel_window_manager, command);
}