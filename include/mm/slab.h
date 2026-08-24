#ifndef MM_SLAB_H
#define MM_SLAB_H

#include <stddef.h>

void slab_init(void);
void *kmalloc(size_t size);
void kfree(void *address);

#endif