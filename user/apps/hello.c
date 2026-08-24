#include <user/libdisplay-client/display.h>
#include <user/display/compositor.h>
#include <user/display/input_server.h>

static struct display_channel client_to_compositor;
static struct display_channel compositor_to_client;
static struct display_channel input_channel;

int graphics_hello(void)
{
    struct display_client client;
    struct display_rect damage = { 0, 0, 640, 480 };
    uint32_t *pixels;

    if (compositor_init(&client_to_compositor, &compositor_to_client,
                        &input_channel) != 0 ||
        input_server_init(&input_channel) != 0 ||
        display_client_open(&client, "/dev/dri/renderD128",
                            &client_to_compositor, &compositor_to_client) != 0)
        return -1;
    pixels = display_client_map(&client);
    for (uint32_t y = 0; y < 480; ++y)
        for (uint32_t x = 0; x < 640; ++x)
            pixels[y * 1024 + x] = ((x / 32 + y / 32) & 1)
                ? 0xff204060 : 0xffd0e8f0;
    if (display_client_present(&client, &damage) != 0)
        return -1;
    return compositor_run_once();
}