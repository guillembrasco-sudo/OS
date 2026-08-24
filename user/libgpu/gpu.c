#include <user/libgpu/gpu.h>

int gpu_client_open(struct gpu_client *client, uint32_t process_id)
{
    struct gpu_ioctl_context request;
    if (client == 0 || process_id == 0)
        return -1;
    if (gpu_ioctl_dispatch(process_id, GPU_IOCTL_CONTEXT_CREATE, &request) != 0)
        return -1;
    client->process_id = process_id;
    client->context_id = request.context_id;
    client->target = 0;
    return 0;
}

int gpu_client_create_buffer(struct gpu_client *client, uint32_t width,
                             uint32_t height)
{
    if (client == 0)
        return -1;
    client->target = gpu_buffer_alloc(width, height, 2, 1);
    return client->target == 0 ? -1 : 0;
}

int gpu_client_submit(struct gpu_client *client,
                      const struct gpu_command *commands,
                      uint32_t command_count)
{
    struct gpu_ioctl_submit submit;
    if (client == 0 || commands == 0)
        return -1;
    submit.context_id = client->context_id;
    submit.command_count = command_count;
    submit.engine = GPU_ENGINE_GRAPHICS;
    submit.reserved = 0;
    submit.commands = (uintptr_t)commands;
    submit.signal_sequence = 0;
    if (gpu_ioctl_dispatch(client->process_id, GPU_IOCTL_SUBMIT_3D,
                           &submit) != 0)
        return -1;
    return (int)submit.signal_sequence;
}

int gpu_client_wait(struct gpu_client *client, uint64_t sequence)
{
    struct gpu_ioctl_wait wait;
    if (client == 0)
        return -1;
    wait.context_id = client->context_id;
    wait.flags = 0;
    wait.sequence = sequence;
    return gpu_ioctl_dispatch(client->process_id, GPU_IOCTL_WAIT_FENCE, &wait);
}