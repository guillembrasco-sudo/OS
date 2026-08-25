// lib/printf.h
#ifndef PRINTF_H
#define PRINTF_H

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

static int print_unsigned(uint64_t value, unsigned base);

static int print_signed(int64_t value);

int kvprintf(const char *format, va_list arguments);

int kprintf(const char *format, ...);

#endif // PRINTF_H