#ifndef DRIVERS_GPU_PIPELINE_H
#define DRIVERS_GPU_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

enum gpu_shader_stage {
    GPU_SHADER_VERTEX = 1,
    GPU_SHADER_FRAGMENT = 2,
    GPU_SHADER_COMPUTE = 3
};

struct gpu_shader_binary {
    uint32_t stage;
    const uint32_t *spirv;
    size_t spirv_words;
    const void *native_code;
    size_t native_size;
};

struct gpu_pipeline_desc {
    uint64_t shader_hash;
    uint32_t blend_state;
    uint32_t raster_state;
    uint32_t depth_stencil_state;
    uint32_t vertex_layout;
};

struct gpu_pipeline {
    struct gpu_pipeline_desc description;
    uint64_t key;
    uint32_t reference_count;
    uint8_t valid;
};

int gpu_shader_translate(const struct gpu_shader_binary *shader,
                         void *native_output, size_t output_size,
                         size_t *written);
struct gpu_pipeline *gpu_pipeline_get(const struct gpu_pipeline_desc *description);
void gpu_pipeline_put(struct gpu_pipeline *pipeline);

#endif