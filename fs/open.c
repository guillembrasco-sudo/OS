// fs/open.c
//
// Une namei (resolucion de rutas) + dcache (cache de esa resolucion)
// + file.c (tabla de descriptores) en una API estilo POSIX de
// open/close/read/write. Esto es lo que syscall.c llamara
// directamente desde el handler de cada syscall de fichero.

#include <fs/open.h>
#include <fs/namei.h>
#include <fs/dcache.h>
#include <fs/file.h>

#define OPEN_NAME_MAX 60

static int create_missing_node(struct vfs_node *cwd, const char *path,
                                 uint32_t mode, struct vfs_node **out_node)
{
	const char *name;
	size_t name_length;
	struct vfs_node *parent;
	char name_buffer[OPEN_NAME_MAX];
	size_t i;

	parent = namei_parent(cwd, path, &name, &name_length);
	if (parent == 0 || parent->operations == 0 ||
	    parent->operations->create == 0 ||
	    name_length == 0 || name_length >= OPEN_NAME_MAX)
		return -1;

	for (i = 0; i < name_length; ++i)
		name_buffer[i] = name[i];
	name_buffer[name_length] = 0;

	return parent->operations->create(parent, name_buffer, mode, out_node);
}

int vfs_open_path(struct vfs_node *cwd, const char *path, uint32_t flags, uint32_t mode)
{
	struct vfs_node *node;
	int fd;

	if (path == 0)
		return -1;

	node = dcache_lookup(path);
	if (node == 0)
		node = namei(cwd, path);

	if (node == 0) {
		if (!(flags & VFS_O_CREAT))
			return -1;
		if (create_missing_node(cwd, path, mode, &node) != 0)
			return -1;
	} else {
		dcache_insert(path, node);
	}

	if (node->operations != 0 && node->operations->open != 0)
		if (node->operations->open(node) != 0)
			return -1;

	fd = file_alloc(node, flags);
	if (fd < 0 && node->operations != 0 && node->operations->close != 0)
		node->operations->close(node);
	return fd;
}

int vfs_close_fd(int fd)
{
	struct file *entry = file_get(fd);
	if (entry == 0)
		return -1;
	if (entry->node != 0 && entry->node->operations != 0 &&
	    entry->node->operations->close != 0)
		entry->node->operations->close(entry->node);
	return file_release(fd);
}

long vfs_read_fd(int fd, void *buffer, size_t length)
{
	struct file *entry = file_get(fd);
	long result;

	if (entry == 0)
		return -1;
	result = vfs_read(entry->node, buffer, length, entry->offset);
	if (result > 0)
		entry->offset += (uint64_t)result;
	return result;
}

long vfs_write_fd(int fd, const void *buffer, size_t length)
{
	struct file *entry = file_get(fd);
	long result;

	if (entry == 0)
		return -1;
	result = vfs_write(entry->node, buffer, length, entry->offset);
	if (result > 0)
		entry->offset += (uint64_t)result;
	return result;
}