// fs/ramfs.c
//
// Filesystem en RAM, montado como raiz del VFS ("/"). Los directorios
// son vfs_node encadenados via parent/first_child/next_sibling (arbol
// generico de vfs.h); cada fichero guarda su contenido en un buffer
// estatico de tamano fijo (RAMFS_FILE_MAX_SIZE) tomado de un pool -
// sencillo a proposito, igual que el bitmap del PMM o el pool de
// arranque de paging.c. Crecer mas alla de una pagina por fichero, o
// devolver nodos borrados al pool para reutilizarlos de verdad, queda
// pendiente si hace falta mas adelante.

#include <fs/ramfs.h>
#include <fs/dcache.h>
#include <lib/string.h>

#define RAMFS_MAX_NODES 128
#define RAMFS_FILE_MAX_SIZE 4096

struct ramfs_node_storage {
	struct vfs_node node;
	char name_storage[VFS_NAME_MAX];
	uint8_t data[RAMFS_FILE_MAX_SIZE];
	int used;
};

static struct ramfs_node_storage pool[RAMFS_MAX_NODES];
static struct vfs_node *root;

static long ramfs_read(struct vfs_node *node, void *buffer, size_t length, uint64_t offset);
static long ramfs_write(struct vfs_node *node, const void *buffer, size_t length, uint64_t offset);
static int ramfs_create(struct vfs_node *parent, const char *name, uint32_t mode, struct vfs_node **out_node);
static int ramfs_mkdir(struct vfs_node *parent, const char *name, uint32_t mode, struct vfs_node **out_node);
static int ramfs_unlink(struct vfs_node *parent, const char *name);

static const struct vfs_operations ramfs_file_operations = {
	ramfs_read, ramfs_write, 0, 0, 0, 0, 0, 0
};

static const struct vfs_operations ramfs_dir_operations = {
	0, 0, vfs_generic_readdir, 0, 0, ramfs_create, ramfs_mkdir, ramfs_unlink
};

static struct ramfs_node_storage *alloc_node(void)
{
	uint32_t i;
	for (i = 0; i < RAMFS_MAX_NODES; ++i)
		if (!pool[i].used) {
			pool[i].used = 1;
			return &pool[i];
		}
	return 0;
}

static void copy_name(char *dst, const char *src)
{
	size_t i = 0;
	while (src[i] != 0 && i + 1 < VFS_NAME_MAX) {
		dst[i] = src[i];
		++i;
	}
	dst[i] = 0;
}

static long ramfs_read(struct vfs_node *node, void *buffer, size_t length, uint64_t offset)
{
	struct ramfs_node_storage *storage = (struct ramfs_node_storage *)node;
	uint64_t available;

	if (node == 0 || buffer == 0 || offset > node->size)
		return -1;
	available = node->size - offset;
	if (length > available)
		length = (size_t)available;
	memcpy(buffer, storage->data + offset, length);
	return (long)length;
}

static long ramfs_write(struct vfs_node *node, const void *buffer, size_t length, uint64_t offset)
{
	struct ramfs_node_storage *storage = (struct ramfs_node_storage *)node;

	if (node == 0 || buffer == 0 || offset >= RAMFS_FILE_MAX_SIZE)
		return -1;
	if (offset + length > RAMFS_FILE_MAX_SIZE)
		length = RAMFS_FILE_MAX_SIZE - offset;
	memcpy(storage->data + offset, buffer, length);
	if (offset + length > node->size)
		node->size = offset + length;
	return (long)length;
}

static int ramfs_create(struct vfs_node *parent, const char *name, uint32_t mode, struct vfs_node **out_node)
{
	struct ramfs_node_storage *storage;

	if (parent == 0 || name == 0 || vfs_find_child(parent, name) != 0)
		return -1;
	storage = alloc_node();
	if (storage == 0)
		return -1;
	copy_name(storage->name_storage, name);
	storage->node.name = storage->name_storage;
	storage->node.mode = mode;
	storage->node.size = 0;
	storage->node.type = VFS_TYPE_FILE;
	storage->node.operations = &ramfs_file_operations;
	vfs_attach_child(parent, &storage->node);
	if (out_node != 0)
		*out_node = &storage->node;
	return 0;
}

static int ramfs_mkdir(struct vfs_node *parent, const char *name, uint32_t mode, struct vfs_node **out_node)
{
	struct ramfs_node_storage *storage;

	if (parent == 0 || name == 0 || vfs_find_child(parent, name) != 0)
		return -1;
	storage = alloc_node();
	if (storage == 0)
		return -1;
	copy_name(storage->name_storage, name);
	storage->node.name = storage->name_storage;
	storage->node.mode = mode;
	storage->node.size = 0;
	storage->node.type = VFS_TYPE_DIRECTORY;
	storage->node.operations = &ramfs_dir_operations;
	vfs_attach_child(parent, &storage->node);
	if (out_node != 0)
		*out_node = &storage->node;
	return 0;
}

static int ramfs_unlink(struct vfs_node *parent, const char *name)
{
	struct vfs_node *node;
	struct vfs_node **link;

	if (parent == 0 || name == 0)
		return -1;
	node = vfs_find_child(parent, name);
	if (node == 0)
		return -1;

	for (link = &parent->first_child; *link != 0; link = &(*link)->next_sibling) {
		if (*link == node) {
			*link = node->next_sibling;
			break;
		}
	}
	dcache_invalidate(node->name);
	((struct ramfs_node_storage *)node)->used = 0;
	return 0;
}

int ramfs_init(void)
{
	struct ramfs_node_storage *storage;
	uint32_t i;

	for (i = 0; i < RAMFS_MAX_NODES; ++i)
		pool[i].used = 0;

	storage = alloc_node();
	if (storage == 0)
		return -1;
	storage->node.name = "";
	storage->node.mode = 0755;
	storage->node.size = 0;
	storage->node.type = VFS_TYPE_DIRECTORY;
	storage->node.parent = 0;
	storage->node.first_child = 0;
	storage->node.next_sibling = 0;
	storage->node.operations = &ramfs_dir_operations;
	root = &storage->node;
	vfs_mount_root(root);
	return 0;
}

struct vfs_node *ramfs_root(void)
{
	return root;
}