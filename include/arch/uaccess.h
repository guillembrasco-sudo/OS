#ifndef ARCH_UACCESS_H
#define ARCH_UACCESS_H

#include <stddef.h>

int copy_from_user(void *kernel_destination, const void *user_source,
                   size_t length);
int copy_to_user(void *user_destination, const void *kernel_source,
                 size_t length);
uintptr_t fixup_exception(uintptr_t instruction_pointer);

#endif