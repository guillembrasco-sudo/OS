#ifndef KERNEL_ELF_LOADER_H
#define KERNEL_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>
#include <kernel/process.h>

int elf64_load_user(struct process *process, const void *image, size_t image_size);

#endif
