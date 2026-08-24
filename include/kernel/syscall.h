#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <stdint.h>

enum syscall_number {
    SYS_HANDLE_CLOSE = 0,
    SYS_GPU_IOCTL = 1,
    SYS_YIELD = 2
};

uint64_t syscall_dispatch(uint64_t number, uint64_t arg0, uint64_t arg1,
                          uint64_t arg2, uint64_t arg3, uint64_t arg4);

#endif