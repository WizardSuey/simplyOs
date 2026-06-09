#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool print(const char* data, size_t length) {
	const unsigned char* bytes = (const unsigned char*) data;
	for (size_t i = 0; i < length; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

static bool print_uint(unsigned int value, unsigned int base) {
	char buffer[32];
	char* pos = buffer + sizeof(buffer);
	bool ok = true;

	if (base < 2 || base > 16) {
		return false;
	}

	*--pos = '\0';
	if (value == 0) {
		return print("0", 1);
	}

	while (value != 0 && pos > buffer) {
		unsigned int digit = value % base;
		*--pos = (char)(digit < 10 ? ('0' + digit) : ('a' + digit - 10));
		value /= base;
	}

	ok = print(pos, strlen(pos));
	return ok;
}

static bool print_int(int value) {
	if (value < 0) {
		if (!print("-", 1)) {
			return false;
		}
		return print_uint((unsigned int)(-(value + 1)) + 1u, 10);
	}

	return print_uint((unsigned int)value, 10);
}

int printf(const char* restrict format, ...) {
	va_list parameters;
	va_start(parameters, format);

	int written = 0;

	while (*format != '\0') {
		size_t maxrem = INT_MAX - written;

		if (format[0] != '%' || format[1] == '%') {
			if (format[0] == '%')
				format++;
			size_t amount = 1;
			while (format[amount] && format[amount] != '%')
				amount++;
			if (maxrem < amount) {
				return -1;
			}
			if (!print(format, amount))
				return -1;
			format += amount;
			written += amount;
			continue;
		}

		format++;

		if (*format == 'c') {
			format++;
			char c = (char) va_arg(parameters, int);
			if (!maxrem) {
				return -1;
			}
			if (!print(&c, sizeof(c)))
				return -1;
			written++;
		} else if (*format == 's') {
			format++;
			const char* str = va_arg(parameters, const char*);
			size_t len = strlen(str);
			if (maxrem < len) {
				return -1;
			}
			if (!print(str, len))
				return -1;
			written += len;
		} else if (*format == 'u') {
			format++;
			unsigned int value = va_arg(parameters, unsigned int);
			char buffer[32];
			char* pos = buffer + sizeof(buffer);
			size_t len;

			*--pos = '\0';
			if (value == 0) {
				len = 1;
				if (!print("0", len))
					return -1;
			} else {
				while (value != 0 && pos > buffer) {
					unsigned int digit = value % 10;
					*--pos = (char)('0' + digit);
					value /= 10;
				}
				len = strlen(pos);
				if (!print(pos, len))
					return -1;
			}
			written += (int)len;
		} else if (*format == 'd') {
			format++;
			int value = va_arg(parameters, int);
			if (!print_int(value))
				return -1;
			written++;
		} else if (*format == 'x') {
			format++;
			unsigned int value = va_arg(parameters, unsigned int);
			if (!print_uint(value, 16))
				return -1;
			written++;
		} else if (*format == 'p') {
			format++;
			uintptr_t value = (uintptr_t) va_arg(parameters, void*);
			if (!print("0x", 2))
				return -1;
			if (!print_uint((unsigned int) value, 16))
				return -1;
			written++;
		} else {
			if (!maxrem) {
				return -1;
			}
			if (!print("%", 1))
				return -1;
			written++;
		}
	}

	va_end(parameters);
	return written;
}
