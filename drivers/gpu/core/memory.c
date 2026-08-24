#include <drivers/gpu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define GPU_BUFFER_LIMIT 128
#define GPU_PAGE_SIZE 4096UL

static struct gpu_buffer buffers[GPU_BUFFER_LIMIT];
static struct dma_buf dma_buffers[GPU_BUFFER_LIMIT];
static uint8_t buffer_used[GPU_BUFFER_LIMIT];
static uint8_t dma_used[GPU_BUFFER_LIMIT];
static uint32_t next_handle = 1;

static uint32_t bytes_per_pixel(uint32_t format)
{
    return format == 0 ? 4 : 4;
}

struct gpu_buffer *gpu_buffer_alloc(uint32_t width, uint32_t height,
                                    uint32_t format, uint32_t flags)
{
    size_t pixel_size = bytes_per_pixel(format);
    size_t pitch;
    size_t size;

    if (width == 0 || height == 0 || width > (size_t)-1 / pixel_size)
        return 0;
    pitch = (size_t)width * pixel_size;
    if (height > (size_t)-1 / pitch)
        return 0;
    size = pitch * height;
    size_t pages = (size + GPU_PAGE_SIZE - 1) / GPU_PAGE_SIZE;
    for (unsigned index = 0; index < GPU_BUFFER_LIMIT; ++index) {
        if (buffer_used[index] == 0) {
            struct gpu_buffer *buffer = &buffers[index];
            uintptr_t physical = pmm_alloc_contiguous(pages);
            uintptr_t mapped = 0;
            if (physical == 0 || vmm_kernel_map(physical, pages,
                                                 VMM_PAGE_WRITE, &mapped) != 0) {
                if (physical != 0)
                    pmm_free_contiguous(physical, pages);
                return 0;
            }
            buffer_used[index] = 1;
            buffer->width = width;
            buffer->height = height;
            buffer->format = format;
            buffer->pitch = (uint32_t)pitch;
            buffer->size = size;
            buffer->handle = next_handle++;
            buffer->physical_address = physical;
            buffer->vram_address = mapped;
            buffer->flags = flags;
            buffer->owner = 0;
            buffer->dma_refs = 0;
            buffer->released = 0;
            return buffer;
        }
    }
    return 0;
}

void gpu_buffer_release(struct gpu_buffer *buffer)
{
    if (buffer == 0)
        return;
    for (unsigned index = 0; index < GPU_BUFFER_LIMIT; ++index)
        if (&buffers[index] == buffer) {
            buffer->released = 1;
            if (buffer->dma_refs == 0) {
                pmm_free_contiguous(buffer->physical_address,
                                    (buffer->size + GPU_PAGE_SIZE - 1) /
                                    GPU_PAGE_SIZE);
                buffer_used[index] = 0;
            }
        }
}

struct dma_buf *dma_buf_export(struct gpu_buffer *buffer, uint32_t rights)
{
    if (buffer == 0 || buffer->released)
        return 0;
    for (unsigned index = 0; index < GPU_BUFFER_LIMIT; ++index) {
        if (dma_used[index] == 0) {
            dma_used[index] = 1;
            dma_buffers[index].buffer = buffer;
            dma_buffers[index].export_handle = buffer->handle;
            dma_buffers[index].rights = rights;
            dma_buffers[index].importer = 0;
            ++buffer->dma_refs;
            return &dma_buffers[index];
        }
    }
    return 0;
}

struct gpu_buffer *dma_buf_import(struct dma_buf *dma, uint32_t owner)
{
    if (dma == 0 || dma->buffer == 0 || dma->buffer->released ||
        dma->rights == 0)
        return 0;
    dma->importer = owner;
    dma->buffer->owner = owner;
    return dma->buffer;
}

void dma_buf_release(struct dma_buf *dma)
{
    unsigned dma_index;
    if (dma == 0)
        return;
    for (dma_index = 0; dma_index < GPU_BUFFER_LIMIT; ++dma_index)
        if (&dma_buffers[dma_index] == dma)
            break;
    if (dma_index == GPU_BUFFER_LIMIT || dma_used[dma_index] == 0 ||
        dma->buffer == 0)
        return;
    if (dma->buffer->dma_refs != 0)
        --dma->buffer->dma_refs;
    if (dma->buffer->released && dma->buffer->dma_refs == 0)
        for (unsigned index = 0; index < GPU_BUFFER_LIMIT; ++index)
            if (&buffers[index] == dma->buffer) {
                pmm_free_contiguous(dma->buffer->physical_address,
                                    (dma->buffer->size + GPU_PAGE_SIZE - 1) /
                                    GPU_PAGE_SIZE);
                buffer_used[index] = 0;
            }
    dma->buffer = 0;
    dma_used[dma_index] = 0;
}