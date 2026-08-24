#ifndef DRIVERS_VIRTIO_GPU_H
#define DRIVERS_VIRTIO_GPU_H

#include <stdint.h>
#include <drivers/drm.h>
#include <drivers/gpu_fence.h>

#define VIRTIO_GPU_PCI_VENDOR 0x1af4
#define VIRTIO_GPU_PCI_DEVICE 0x1050

enum virtio_gpu_command {
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D = 0x0101,
    VIRTIO_GPU_CMD_SET_SCANOUT = 0x0103
};

struct virtio_gpu_queue {
    uint16_t size;
    volatile uint16_t available;
    volatile uint16_t used;
    uintptr_t descriptors;
};

struct virtio_gpu {
    uint16_t pci_vendor;
    uint16_t pci_device;
    uintptr_t common_config;
    struct virtio_gpu_queue controlq;
    struct virtio_gpu_queue cursorq;
    struct drm_device drm;
    struct gpu_fence fence;
    uint32_t resource_id;
    uint32_t last_command;
};

int virtio_gpu_pci_probe(uint16_t vendor, uint16_t device);
int virtio_gpu_init(struct virtio_gpu *gpu, uintptr_t common_config);
int virtio_gpu_get_display_info(struct virtio_gpu *gpu);
int virtio_gpu_resource_create_2d(struct virtio_gpu *gpu, uint32_t width,
                                  uint32_t height, uint32_t format);
int virtio_gpu_set_scanout(struct virtio_gpu *gpu, uint32_t resource_id,
                           uint32_t width, uint32_t height);

#endif