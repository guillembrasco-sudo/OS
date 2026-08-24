#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <hal/cpu.h>

static int print_unsigned(uint64_t value, unsigned base)
{
    static const char digits[] = "0123456789abcdef";
    char buffer[32];
    int length = 0;
    int written = 0;

    if (value == 0) {
        arch_console_putc('0');
        return 1;
    }

    while (value != 0) {
        buffer[length++] = digits[value % base];
        value /= base;
    }

    written = length;
    while (length > 0) {
        arch_console_putc(buffer[--length]);
    }

    return written;
}

static int print_signed(int64_t value)
{
    int written = 0;
    if (value < 0) {
        arch_console_putc('-');
        written++;
        written += print_unsigned((uint64_t)(-(value + 1)) + 1, 10);
    } else {
        written += print_unsigned((uint64_t)value, 10);
    }
    return written;
}

int kvprintf(const char *format, va_list arguments)
{
    int written = 0;

    while (*format != '\0') {
        if (*format != '%') {
            arch_console_putc(*format++);
            written++;
            continue;
        }

        format++; // Omitir '%'
        int is_long = 0;

        if (*format == 'l') {
            is_long = 1;
            format++;
        }

        switch (*format++) {
        case '%':
            arch_console_putc('%');
            written++;
            break;

        case 'c':
            arch_console_putc((char)va_arg(arguments, int));
            written++;
            break;

        case 's': {
            const char *string = va_arg(arguments, const char *);
            if (!string) {
                string = "(null)";
            }
            while (*string != '\0') {
                arch_console_putc(*string++);
                written++;
            }
            break;
        }

        case 'd':
        case 'i': {
            int64_t val = is_long ? va_arg(arguments, int64_t) : va_arg(arguments, int);
            written += print_signed(val);
            break;
        }

        case 'u': {
            uint64_t val = is_long ? va_arg(arguments, uint64_t) : va_arg(arguments, unsigned int);
            written += print_unsigned(val, 10);
            break;
        }

        case 'x':
        case 'X': {
            uint64_t val = is_long ? va_arg(arguments, uint64_t) : va_arg(arguments, unsigned int);
            written += print_unsigned(val, 16);
            break;
        }

        case 'p': {
            uintptr_t ptr = (uintptr_t)va_arg(arguments, void *);
            arch_console_putc('0');
            arch_console_putc('x');
            written += 2;
            written += print_unsigned(ptr, 16);
            break;
        }

        default:
            arch_console_putc('%');
            arch_console_putc(format[-1]);
            written += 2;
            break;
        }
    }

    return written;
}

int kprintf(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int written = kvprintf(format, arguments);
    va_end(arguments);
    return written;
}