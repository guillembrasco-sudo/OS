// include/kernel/panic.h
#ifndef PANIC_H
#define PANIC_H

#include "kernel/interrupt.h" // registers_t, ver 2.4

void panic(const char *fmt, ...);
void panic_with_regs(registers_t *regs, const char *fmt, ...);

#endif // PANIC_H