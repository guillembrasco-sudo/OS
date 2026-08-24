#include <drivers/drm.h>

static struct drm_device *registered_device;

int drm_register_device(struct drm_device *device)
{
    if (device == 0 || registered_device != 0)
        return -1;
    registered_device = device;
    return 0;
}

int drm_kms_open(struct drm_device *device, uint32_t process_id)
{
    if (device == 0 || (device->kms_owner != 0 &&
                        device->kms_owner != process_id))
        return -1;
    device->kms_owner = process_id;
    return 0;
}

int drm_kms_set_mode(struct drm_device *device, const struct display_mode *mode)
{
    if (device == 0 || mode == 0 || mode->width == 0 || mode->height == 0)
        return -1;
    device->mode = *mode;
    return 0;
}

struct gpu_buffer *drm_kms_scanout(struct drm_device *device)
{
    return device == 0 ? 0 : device->scanout;
}