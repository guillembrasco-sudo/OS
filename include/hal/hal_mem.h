// include/hal/hal_mem.h
#ifndef HAL_MEM_H
#define HAL_MEM_H
#include <stddef.h>

void  hal_mem_init(void); // llamar una vez, después de cpu_features_detect
void *(*hal_memcpy)(void *dst, const void *src, size_t n);
void *(*hal_memset)(void *dst, int val, size_t n);

#endif // HAL_MEM_H