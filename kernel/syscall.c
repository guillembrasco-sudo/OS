// kernel/syscall.c
#include "kernel/syscall.h"
#include "kernel/kheap.h"
#include "kernel/panic.h"

static syscall_fn_t syscall_table[SYSCALL_MAX] = {0};

// --- Implementaciones mínimas ----------------------------------------------
// IMPORTANTE: cualquier puntero que venga de espacio de usuario (a2 en
// sys_read/sys_write) DEBE validarse contra el rango de direcciones del
// proceso actual antes de desreferenciarlo. Sin VMM con per-process address
// space todavía, dejo el TODO explícito en el propio código para que no se
// olvide al integrar el scheduler.
static int64_t sys_write_impl(uint64_t fd, uint64_t buf_user, uint64_t len, uint64_t a4, uint64_t a5) {
    (void)a4; (void)a5;
    // TODO(seguridad): validar que [buf_user, buf_user+len) está dentro del
    // espacio de direcciones del proceso llamante antes de leer de ahí.
    // Sin esa validación, un proceso de usuario puede pedirle al kernel que
    // lea memoria arbitraria del kernel pasando un puntero fabricado.
    if (fd != 1 && fd != 2) return -1; // solo stdout/stderr por ahora
    const char *buf = (const char *)buf_user;
    for (uint64_t i = 0; i < len; i++) {
        kputchar(buf[i]); // asumo kputchar existente en tu driver de tty
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
    for (;;) { __asm__ volatile("hlt"); } // hasta que exista el scheduler, no hay a dónde volver
}

static int64_t sys_malloc_impl(uint64_t size, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5;
    // Exponer kmalloc directo a Ring 3 es temporal y peligroso (un proceso
    // podría agotar el heap del KERNEL, no el suyo). El diseño correcto es
    // que cada proceso tenga su propio heap de usuario gestionado vía
    // sys_brk/sys_mmap sobre su VMM. Lo dejo así de momento porque el TODO
    // lo pide explícitamente como placeholder, pero es el primer candidato
    // a reemplazar en cuanto exista VMM por proceso.
    return (int64_t)(uintptr_t)kmalloc((size_t)size);
}

void syscall_init(void) {
    syscall_table[SYS_READ]   = sys_read_impl;
    syscall_table[SYS_WRITE]  = sys_write_impl;
    syscall_table[SYS_YIELD]  = sys_yield_impl;
    syscall_table[SYS_EXIT]   = sys_exit_impl;
    syscall_table[SYS_MALLOC] = sys_malloc_impl;
}

int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    if (num >= SYSCALL_MAX || !syscall_table[num]) {
        return -38; // -ENOSYS
    }
    return syscall_table[num](a1, a2, a3, a4, a5);
}