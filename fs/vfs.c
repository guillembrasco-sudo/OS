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

int vfs_readdir(struct vfs_node *node, uint32_t index, struct vfs_node *entry)
{
	if (node == 0 || entry == 0 || node->operations == 0 ||
	    node->operations->readdir == 0)
		return -1;
	return node->operations->readdir(node, index, entry);
}
