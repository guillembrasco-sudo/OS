// lib/string.h
#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t length);

void *memset(void *destination, int value, size_t length);

size_t strlen(const char *string);

int memcmp(const void *left, const void *right, size_t length);

#endif // STRING_H
