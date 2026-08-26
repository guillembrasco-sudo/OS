// include/kernel/syscall.h
#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_YIELD 2
#define SYS_EXIT  3
#define SYS_MALLOC 4
#define SYS_CONSOLE_COMMAND 5
#define SYSCALL_MAX 6

typedef int64_t (*syscall_fn_t)(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

void    syscall_init(void);
int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

#endif // SYSCALL_H