#include <stdint.h>
#include <stddef.h>
#include <arch/uaccess.h>
#include <mm/vmm.h>

extern int copy_from_user_asm(void *destination, const void *source, size_t length);
extern int copy_to_user_asm(void *destination, const void *source, size_t length);

int copy_from_user(void *kernel_destination, const void *user_source,
                   size_t length)
{
    if (kernel_destination == 0 || user_source == 0 ||
        !vmm_user_range_valid((uintptr_t)user_source, length, 0))
        return -1;
    return copy_from_user_asm(kernel_destination, user_source, length);
}

int copy_to_user(void *user_destination, const void *kernel_source,
                 size_t length)
{
    if (user_destination == 0 || kernel_source == 0 ||
        !vmm_user_range_valid((uintptr_t)user_destination, length, 1))
        return -1;
    return copy_to_user_asm(user_destination, kernel_source, length);
}

uintptr_t fixup_exception(uintptr_t instruction_pointer)
{
    extern const uintptr_t __start___ex_table[];
    extern const uintptr_t __stop___ex_table[];
    const uintptr_t *entry;
    for (entry = __start___ex_table; entry < __stop___ex_table; entry += 2)
        if (entry[0] == instruction_pointer)
            return entry[1];
    return 0;
}