// kernel/syscall.c
#include "kernel/syscall.h"
#include "kernel/panic.h"
#include <hal/cpu.h>
#include <arch/uaccess.h>
#include <arch/x86_64/msr.h>
#include <kernel/window_system.h>

extern void syscall_entry(void);

static syscall_fn_t syscall_table[SYSCALL_MAX] = {0};

// --- Implementaciones mínimas ----------------------------------------------
// IMPORTANTE: cualquier puntero que venga de espacio de usuario (a2 en
// sys_read/sys_write) DEBE validarse contra el rango de direcciones del
// proceso actual antes de desreferenciarlo. Sin VMM con per-process address
// space todavía, dejo el TODO explícito en el propio código para que no se
// olvide al integrar el scheduler.
static int64_t sys_write_impl(uint64_t fd, uint64_t buf_user, uint64_t len, uint64_t a4, uint64_t a5) {
    (void)a4; (void)a5;
    if (fd != 1 && fd != 2) return -1; // solo stdout/stderr por ahora
    if (len > 4096 || len == 0)
        return -1;

    char buffer[4096];
    if (copy_from_user(buffer, (const void *)buf_user, (size_t)len) != 0)
        return -1;
    for (uint64_t i = 0; i < len; i++) {
        arch_console_putc(buffer[i]);
    }
    return (int64_t)len;
}

static int64_t sys_read_impl(uint64_t fd, uint64_t buf_user, uint64_t len, uint64_t a4, uint64_t a5) {
    (void)fd; (void)buf_user; (void)len; (void)a4; (void)a5;
    return -1; // sin VFS todavía: placeholder consciente, no silencioso
}

static int64_t sys_yield_impl(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    // scheduler_yield(); // enganchar cuando exista el scheduler (TODO §1)
    return 0;
}

static int64_t sys_exit_impl(uint64_t code, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    // scheduler_terminate_current((int)code); // idem
    (void)code;
    for (;;) { }
}

static int64_t sys_malloc_impl(uint64_t size, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    (void)size;
    return -38; // user heaps require a per-process address space
}

static int64_t sys_console_command_impl(uint64_t command_user, uint64_t a2,
                                        uint64_t a3, uint64_t a4, uint64_t a5) {
    char command[64];
    (void)a2; (void)a3; (void)a4; (void)a5;
    if (copy_from_user(command, (const void *)command_user,
                       sizeof(command) - 1) != 0)
        return -1;
    command[sizeof(command) - 1] = '\0';
    return window_manager_kernel_execute_command(command);
}

void syscall_init(void) {
    syscall_table[SYS_READ]   = sys_read_impl;
    syscall_table[SYS_WRITE]  = sys_write_impl;
    syscall_table[SYS_YIELD]  = sys_yield_impl;
    syscall_table[SYS_EXIT]   = sys_exit_impl;
    syscall_table[SYS_MALLOC] = sys_malloc_impl;
    syscall_table[SYS_CONSOLE_COMMAND] = sys_console_command_impl;
    x86_syscall_enable((uintptr_t)syscall_entry);
}

int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    if (num >= SYSCALL_MAX || !syscall_table[num]) {
        return -38; // -ENOSYS
    }
    return syscall_table[num](a1, a2, a3, a4, a5);
}