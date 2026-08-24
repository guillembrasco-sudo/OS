#ifndef KERNEL_HANDLES_H
#define KERNEL_HANDLES_H

#include <stdint.h>

typedef uint32_t handle_t;

enum handle_rights {
    HANDLE_READ = 1u << 0,
    HANDLE_WRITE = 1u << 1,
    HANDLE_IOCTL = 1u << 2,
    HANDLE_MAP = 1u << 3,
    HANDLE_DMA = 1u << 4
};

int handle_install(uint32_t process_id, void *object, uint32_t rights,
                   handle_t *handle);
void *handle_lookup(uint32_t process_id, handle_t handle, uint32_t rights);
int handle_close(uint32_t process_id, handle_t handle);

#endif