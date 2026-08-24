#ifndef USER_LIBGPU_H
#define USER_LIBGPU_H

#include <drivers/gpu_ioctl.h>
#include <drivers/gpu.h>

struct gpu_client {
    uint32_t process_id;
    uint32_t context_id;
    struct gpu_buffer *target;
};

int gpu_client_open(struct gpu_client *client, uint32_t process_id);
int gpu_client_create_buffer(struct gpu_client *client, uint32_t width,
                             uint32_t height);
int gpu_client_submit(struct gpu_client *client,
                      const struct gpu_command *commands,
                      uint32_t command_count);
int gpu_client_wait(struct gpu_client *client, uint64_t sequence);

#endif