#ifndef USER_LIBGPU_SWAPCHAIN_H
#define USER_LIBGPU_SWAPCHAIN_H

#include <drivers/gpu.h>
#include <user/libgpu/gpu.h>

#define GPU_SWAPCHAIN_IMAGES 3

struct gpu_attachment {
    struct gpu_buffer *buffer;
    uint32_t format;
    uint32_t samples;
};

struct gpu_swapchain {
    struct gpu_client *client;
    struct gpu_attachment color[GPU_SWAPCHAIN_IMAGES];
    struct gpu_attachment depth_stencil;
    uint32_t image_count;
    uint32_t next_image;
};

int gpu_swapchain_create(struct gpu_swapchain *swapchain,
                         struct gpu_client *client,
                         uint32_t width, uint32_t height);
struct gpu_buffer *gpu_swapchain_acquire(struct gpu_swapchain *swapchain);
int gpu_swapchain_present(struct gpu_swapchain *swapchain,
                          uint32_t image_index);

#endif