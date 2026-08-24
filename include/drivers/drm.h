#ifndef DRIVERS_DRM_H
#define DRIVERS_DRM_H

#include <drivers/gpu.h>
#include <hal/display.h>

struct drm_device {
    const char *name;
    uint16_t pci_vendor;
    uint16_t pci_device;
    struct display_mode mode;
    struct gpu_buffer *scanout;
    uint32_t kms_owner;
};

int drm_register_device(struct drm_device *device);
int drm_kms_open(struct drm_device *device, uint32_t process_id);
int drm_kms_set_mode(struct drm_device *device, const struct display_mode *mode);
int drm_kms_prepare_scanout(struct drm_device *device);
struct gpu_buffer *drm_kms_scanout(struct drm_device *device);

#endif