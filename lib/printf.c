#include <stdarg.h>
#include <stdint.h>
#include <hal/cpu.h>

static void print_unsigned(uint64_t value, unsigned base)
{
	static const char digits[] = "0123456789abcdef";
	char buffer[20];
	unsigned length = 0;

	if (value == 0) {
		arch_console_putc('0');
		return;
	}
	while (value != 0) {
		buffer[length++] = digits[value % base];
		value /= base;
	}
	while (length != 0)
		arch_console_putc(buffer[--length]);
}

static void print_signed(int64_t value)
{
	if (value < 0) {
		arch_console_putc('-');
		print_unsigned((uint64_t)(-(value + 1)) + 1, 10);
	} else {
		print_unsigned((uint64_t)value, 10);
	}
}

int kprintf(const char *format, ...)
{
	va_list arguments;
	int written = 0;
	va_start(arguments, format);

	while (*format != '\0') {
		if (*format != '%') {
			arch_console_putc(*format++);
			++written;
			continue;
		}
		++format;
		switch (*format++) {
		case '%':
			arch_console_putc('%');
			++written;
			break;
		case 'c':
			arch_console_putc((char)va_arg(arguments, int));
			++written;
			break;
		case 's': {
			const char *string = va_arg(arguments, const char *);
			while (*string != '\0') {
				arch_console_putc(*string++);
				++written;
			}
			break;
		}
		case 'd':
			print_signed(va_arg(arguments, int));
			break;
		case 'u':
			print_unsigned(va_arg(arguments, unsigned), 10);
			break;
		case 'x':
			print_unsigned(va_arg(arguments, unsigned), 16);
			break;
		case 'p':
			arch_console_putc('0');
			arch_console_putc('x');
			print_unsigned((uintptr_t)va_arg(arguments, void *), 16);
			break;
		default:
			arch_console_putc('%');
			arch_console_putc(format[-1]);
			written += 2;
			break;
		}
	}
	va_end(arguments);
	return written;
}
