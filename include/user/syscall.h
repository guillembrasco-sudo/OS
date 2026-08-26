#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>

#define SYS_CONSOLE_COMMAND 5

static inline int64_t user_console_command(const char *command)
{
    int64_t result;

    __asm__ volatile (
        "syscall"
        : "=a" (result)
        : "a" (SYS_CONSOLE_COMMAND), "D" (command)
        : "rcx", "r11", "memory"
    );
    return result;
}

#endif