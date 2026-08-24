#include <drivers/drm.h>

int drm_kms_prepare_scanout(struct drm_device *device)
{
    if (device == 0)
        return -1;
    device->scanout = gpu_buffer_alloc(device->mode.width,
                                       device->mode.height,
                                       device->mode.format, 1);
    return device->scanout == 0 ? -1 : 0;
}