#include <user/libgpu/gpu.h>

struct triangle_vertex {
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
};

int graphics_triangle(void)
{
    struct gpu_client client;
    struct gpu_command commands[4];

    if (gpu_client_open(&client, 1) != 0 ||
        gpu_client_create_buffer(&client, 1024, 768) != 0)
        return -1;

    commands[0] = (struct gpu_command){ GPU_CMD_BIND_PIPELINE, 0, 0, 0,
                                       { 1, 0, 0, 0 } };
    commands[1] = (struct gpu_command){ GPU_CMD_BIND_VERTEX_BUFFER, 0, 0,
                                       sizeof(struct triangle_vertex) * 3,
                                       { 0, 0, 0, 0 } };
    commands[2] = (struct gpu_command){ GPU_CMD_BIND_UNIFORM_BUFFER, 0, 0,
                                       64, { 0, 0, 0, 0 } };
    commands[3] = (struct gpu_command){ GPU_CMD_DRAW, 0, 0, 0,
                                       { 3, 1, 0, 0 } };
    return gpu_client_submit(&client, commands, 4);
}