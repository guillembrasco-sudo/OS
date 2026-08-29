// fs/dcache.c
//
// Cache plana de resolucion de rutas: NR_DCACHE_ENTRIES parejas
// (ruta, vfs_node) en un array estatico, sustitucion round-robin. No
// pretende ser un dcache "de verdad" (sin hashing, sin dentries
// propios) - es deliberadamente simple, igual que el resto del arbol
// generico de vfs.h: evita repetir el recorrido de namei() cuando el
// mismo path se abre muchas veces seguidas.

#include <fs/dcache.h>
#include <fs/vfs.h>
#include <stdint.h>
#include <stddef.h>

#define NR_DCACHE_ENTRIES 32
#define DCACHE_PATH_MAX 64

struct dcache_entry {
	char path[DCACHE_PATH_MAX];
	struct vfs_node *node;
	int used;
};

static struct dcache_entry entries[NR_DCACHE_ENTRIES];
static uint32_t next_victim;

static int path_equal(const char *a, const char *b)
{
	while (*a != 0 && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

static size_t path_copy(char *dst, const char *src, size_t capacity)
{
	size_t i = 0;
	while (src[i] != 0 && i + 1 < capacity) {
		dst[i] = src[i];
		++i;
	}
	dst[i] = 0;
	return i;
}

void dcache_init(void)
{
	uint32_t i;
	for (i = 0; i < NR_DCACHE_ENTRIES; ++i)
		entries[i].used = 0;
	next_victim = 0;
}

struct vfs_node *dcache_lookup(const char *path)
{
	uint32_t i;
	if (path == 0)
		return 0;
	for (i = 0; i < NR_DCACHE_ENTRIES; ++i)
		if (entries[i].used && path_equal(entries[i].path, path))
			return entries[i].node;
	return 0;
}

void dcache_insert(const char *path, struct vfs_node *node)
{
	struct dcache_entry *slot;
	if (path == 0 || node == 0)
		return;
	slot = &entries[next_victim];
	next_victim = (next_victim + 1) % NR_DCACHE_ENTRIES;
	path_copy(slot->path, path, DCACHE_PATH_MAX);
	slot->node = node;
	slot->used = 1;
}

void dcache_invalidate(const char *path)
{
	uint32_t i;
	if (path == 0) {
		for (i = 0; i < NR_DCACHE_ENTRIES; ++i)
			entries[i].used = 0;
		return;
	}
	for (i = 0; i < NR_DCACHE_ENTRIES; ++i)
		if (entries[i].used && path_equal(entries[i].path, path))
			entries[i].used = 0;
}