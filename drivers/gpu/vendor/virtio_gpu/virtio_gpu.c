#include <drivers/virtio_gpu.h>

static int virtio_queue_init(struct virtio_gpu_queue *queue, uint16_t size)
{
    if (queue == 0 || size == 0)
        return -1;
    queue->size = size;
    queue->available = 0;
    queue->used = 0;
    queue->descriptors = 0;
    return 0;
}

int virtio_gpu_pci_probe(uint16_t vendor, uint16_t device)
{
    return vendor == VIRTIO_GPU_PCI_VENDOR && device == VIRTIO_GPU_PCI_DEVICE;
}

int virtio_gpu_get_display_info(struct virtio_gpu *gpu)
{
    if (gpu == 0)
        return -1;
    gpu->last_command = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    return 0;
}

int virtio_gpu_resource_create_2d(struct virtio_gpu *gpu, uint32_t width,
                                  uint32_t height, uint32_t format)
{
    if (gpu == 0 || width == 0 || height == 0 || format == 0)
        return -1;
    gpu->resource_id = 1;
    gpu->last_command = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    return 0;
}

int virtio_gpu_set_scanout(struct virtio_gpu *gpu, uint32_t resource_id,
                           uint32_t width, uint32_t height)
{
    uint64_t sequence;
    if (gpu == 0 || resource_id == 0 || width == 0 || height == 0)
        return -1;
    gpu->last_command = VIRTIO_GPU_CMD_SET_SCANOUT;
    sequence = gpu_fence_submit(&gpu->fence);
    gpu_fence_signal(&gpu->fence, sequence);
    return 0;
}

int virtio_gpu_init(struct virtio_gpu *gpu, uintptr_t common_config)
{
    struct display_mode mode = { 1024, 768, 60, DISPLAY_XRGB8888 };

    if (gpu == 0 || !virtio_gpu_pci_probe(VIRTIO_GPU_PCI_VENDOR,
                                          VIRTIO_GPU_PCI_DEVICE))
        return -1;
    gpu->pci_vendor = VIRTIO_GPU_PCI_VENDOR;
    gpu->pci_device = VIRTIO_GPU_PCI_DEVICE;
    gpu->common_config = common_config;
    if (virtio_queue_init(&gpu->controlq, 64) != 0 ||
        virtio_queue_init(&gpu->cursorq, 16) != 0)
        return -1;
    gpu_fence_init(&gpu->fence);
    gpu->drm.name = "virtio-gpu";
    gpu->drm.pci_vendor = gpu->pci_vendor;
    gpu->drm.pci_device = gpu->pci_device;
    gpu->drm.mode = mode;
    if (drm_register_device(&gpu->drm) != 0 ||
        virtio_gpu_get_display_info(gpu) != 0 ||
        virtio_gpu_resource_create_2d(gpu, mode.width, mode.height,
                                       mode.format) != 0)
        return -1;
    if (drm_kms_prepare_scanout(&gpu->drm) != 0 ||
        virtio_gpu_set_scanout(gpu, gpu->resource_id, mode.width,
                               mode.height) != 0)
        return -1;
    return 0;
}