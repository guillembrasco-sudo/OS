#ifndef DRIVERS_GPU_IOCTL_H
#define DRIVERS_GPU_IOCTL_H

#include <stdint.h>

#define GPU_IOCTL_CONTEXT_CREATE 0x4700
#define GPU_IOCTL_CONTEXT_DESTROY 0x4701
#define GPU_IOCTL_SUBMIT_3D      0x4702
#define GPU_IOCTL_WAIT_FENCE     0x4703
#define GPU_MAX_COMMANDS         256

enum gpu_engine {
    GPU_ENGINE_GRAPHICS = 0,
    GPU_ENGINE_COMPUTE = 1,
    GPU_ENGINE_TRANSFER = 2,
    GPU_ENGINE_COUNT = 3
};

struct gpu_ioctl_context {
    uint32_t context_id;
    uint32_t flags;
};

struct gpu_command {
    uint32_t opcode;
    uint32_t buffer_handle;
    uint32_t offset;
    uint32_t length;
    uint32_t argument[4];
};

enum gpu_command_opcode {
    GPU_CMD_BIND_PIPELINE = 1,
    GPU_CMD_BIND_VERTEX_BUFFER,
    GPU_CMD_BIND_INDEX_BUFFER,
    GPU_CMD_BIND_UNIFORM_BUFFER,
    GPU_CMD_DRAW,
    GPU_CMD_DRAW_INDEXED
};

struct gpu_ioctl_submit {
    uint32_t context_id;
    uint32_t command_count;
    uint32_t engine;
    uint32_t reserved;
    uintptr_t commands;
    uint64_t signal_sequence;
};

struct gpu_ioctl_wait {
    uint32_t context_id;
    uint32_t flags;
    uint64_t sequence;
};

int gpu_ioctl_dispatch(uint32_t process_id, uint32_t request, void *argument);

#endif