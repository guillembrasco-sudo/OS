#ifndef DRIVERS_GPU_H
#define DRIVERS_GPU_H

#include <stddef.h>
#include <stdint.h>

struct gpu_buffer {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t pitch;
    size_t size;
    uint32_t handle;
    uintptr_t physical_address;
    uintptr_t vram_address;
    uint32_t flags;
    uint32_t owner;
    uint32_t dma_refs;
    uint8_t released;
};

struct dma_buf {
    struct gpu_buffer *buffer;
    uint32_t export_handle;
    uint32_t importer;
    uint32_t rights;
};

struct gpu_buffer *gpu_buffer_alloc(uint32_t width, uint32_t height,
                                    uint32_t format, uint32_t flags);
void gpu_buffer_release(struct gpu_buffer *buffer);
struct dma_buf *dma_buf_export(struct gpu_buffer *buffer, uint32_t rights);
struct gpu_buffer *dma_buf_import(struct dma_buf *dma, uint32_t owner);
void dma_buf_release(struct dma_buf *dma);

#endif