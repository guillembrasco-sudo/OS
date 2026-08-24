#include <user/libgpu/swapchain.h>

int gpu_swapchain_create(struct gpu_swapchain *swapchain,
                         struct gpu_client *client,
                         uint32_t width, uint32_t height)
{
    if (swapchain == 0 || client == 0 || width == 0 || height == 0)
        return -1;
    swapchain->client = client;
    swapchain->image_count = GPU_SWAPCHAIN_IMAGES;
    swapchain->next_image = 0;
    for (uint32_t index = 0; index < GPU_SWAPCHAIN_IMAGES; ++index) {
        swapchain->color[index].buffer = gpu_buffer_alloc(width, height, 2, 1);
        swapchain->color[index].format = 2;
        swapchain->color[index].samples = 1;
        if (swapchain->color[index].buffer == 0)
            return -1;
    }
    swapchain->depth_stencil.buffer = gpu_buffer_alloc(width, height, 3, 1);
    swapchain->depth_stencil.format = 3;
    swapchain->depth_stencil.samples = 1;
    return swapchain->depth_stencil.buffer == 0 ? -1 : 0;
}

struct gpu_buffer *gpu_swapchain_acquire(struct gpu_swapchain *swapchain)
{
    uint32_t image;
    if (swapchain == 0 || swapchain->image_count == 0)
        return 0;
    image = swapchain->next_image++ % swapchain->image_count;
    return swapchain->color[image].buffer;
}

int gpu_swapchain_present(struct gpu_swapchain *swapchain, uint32_t image_index)
{
    struct gpu_command command;
    if (swapchain == 0 || image_index >= swapchain->image_count)
        return -1;
    command = (struct gpu_command){ GPU_CMD_DRAW, 0, 0, 0, { 3, 1, 0, 0 } };
    return gpu_client_submit(swapchain->client, &command, 1);
}