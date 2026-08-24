#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t length)
{
	unsigned char *destination_bytes = destination;
	const unsigned char *source_bytes = source;
	for (size_t index = 0; index < length; ++index)
		destination_bytes[index] = source_bytes[index];
	return destination;
}

void *memset(void *destination, int value, size_t length)
{
	unsigned char *destination_bytes = destination;
	for (size_t index = 0; index < length; ++index)
		destination_bytes[index] = (unsigned char)value;
	return destination;
}

size_t strlen(const char *string)
{
	size_t length = 0;
	while (string[length] != '\0')
		++length;
	return length;
}
