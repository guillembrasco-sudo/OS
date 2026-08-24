#include <drivers/gpu_pipeline.h>

#define GPU_PIPELINE_CACHE_SIZE 64

static struct gpu_pipeline pipeline_cache[GPU_PIPELINE_CACHE_SIZE];

static uint64_t hash_description(const struct gpu_pipeline_desc *description)
{
    uint64_t hash = 1469598103934665603ULL;
    hash = (hash ^ description->shader_hash) * 1099511628211ULL;
    hash = (hash ^ description->blend_state) * 1099511628211ULL;
    hash = (hash ^ description->raster_state) * 1099511628211ULL;
    hash = (hash ^ description->depth_stencil_state) * 1099511628211ULL;
    hash = (hash ^ description->vertex_layout) * 1099511628211ULL;
    return hash;
}

int gpu_shader_translate(const struct gpu_shader_binary *shader,
                         void *native_output, size_t output_size,
                         size_t *written)
{
    if (shader == 0 || native_output == 0 || written == 0 ||
        shader->spirv == 0 || shader->spirv_words == 0)
        return -1;
    /* VirGL/Venus consumes validated SPIR-V; native ISA compilation belongs to Mesa. */
    if (shader->spirv_words * sizeof(uint32_t) > output_size)
        return -1;
    for (size_t index = 0; index < shader->spirv_words; ++index)
        ((uint32_t *)native_output)[index] = shader->spirv[index];
    *written = shader->spirv_words * sizeof(uint32_t);
    return 0;
}

struct gpu_pipeline *gpu_pipeline_get(const struct gpu_pipeline_desc *description)
{
    uint64_t key;
    if (description == 0)
        return 0;
    key = hash_description(description);
    for (unsigned index = 0; index < GPU_PIPELINE_CACHE_SIZE; ++index) {
        struct gpu_pipeline *pipeline = &pipeline_cache[index];
        if (pipeline->valid && pipeline->key == key) {
            ++pipeline->reference_count;
            return pipeline;
        }
    }
    for (unsigned index = 0; index < GPU_PIPELINE_CACHE_SIZE; ++index) {
        struct gpu_pipeline *pipeline = &pipeline_cache[index];
        if (!pipeline->valid) {
            pipeline->description = *description;
            pipeline->key = key;
            pipeline->reference_count = 1;
            pipeline->valid = 1;
            return pipeline;
        }
    }
    return 0;
}

void gpu_pipeline_put(struct gpu_pipeline *pipeline)
{
    if (pipeline == 0 || pipeline->valid == 0 || pipeline->reference_count == 0)
        return;
    if (--pipeline->reference_count == 0)
        pipeline->valid = 0;
}