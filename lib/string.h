// lib/string.h
#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t length);

void *memset(void *destination, int value, size_t length);

size_t strlen(const char *string);

#endif // STRING_H
