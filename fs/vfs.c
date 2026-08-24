#include <fs/vfs.h>

int vfs_init(void)
{
	return 0;
}

long vfs_read(struct vfs_node *node, void *buffer, size_t length, uint64_t offset)
{
	if (node == 0 || node->operations == 0 || node->operations->read == 0)
		return -1;
	return node->operations->read(node, buffer, length, offset);
}

long vfs_write(struct vfs_node *node, const void *buffer, size_t length, uint64_t offset)
{
	if (node == 0 || node->operations == 0 || node->operations->write == 0)
		return -1;
	return node->operations->write(node, buffer, length, offset);
}
