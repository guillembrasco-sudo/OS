#ifndef DRIVERS_DRIVER_H
#define DRIVERS_DRIVER_H

#include <stdint.h>

struct device;
struct driver;

typedef int (*device_probe_fn)(struct device *device);
typedef int (*device_bind_fn)(struct device *device);
typedef void (*device_release_fn)(struct device *device);

struct device {
    const char *name;
    uint16_t vendor_id;
    uint16_t device_id;
    void *driver_data;
    struct driver *driver;
};

struct driver {
    const char *name;
    device_probe_fn probe;
    device_bind_fn bind;
    device_release_fn release;
};

int driver_register(struct driver *driver);
int driver_bind(struct device *device);
void driver_unregister(struct driver *driver);

#endif