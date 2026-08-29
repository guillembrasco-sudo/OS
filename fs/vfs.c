#include <fs/vfs.h>

static struct vfs_node *root_node;

int vfs_init(void)
{
	root_node = 0;
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

struct vfs_node *vfs_root(void)
{
	return root_node;
}

void vfs_mount_root(struct vfs_node *root)
{
	root_node = root;
}

void vfs_attach_child(struct vfs_node *parent, struct vfs_node *child)
{
	if (parent == 0 || child == 0)
		return;
	child->parent = parent;
	child->next_sibling = parent->first_child;
	parent->first_child = child;
}

struct vfs_node *vfs_find_child(struct vfs_node *parent, const char *name)
{
	struct vfs_node *node;

	if (parent == 0 || name == 0)
		return 0;
	for (node = parent->first_child; node != 0; node = node->next_sibling) {
		const char *a = node->name;
		const char *b = name;
		while (*a != 0 && *a == *b) {
			++a;
			++b;
		}
		if (*a == 0 && *b == 0)
			return node;
	}
	return 0;
}

int vfs_generic_readdir(struct vfs_node *node, uint32_t index, struct vfs_node *entry)
{
	struct vfs_node *child;
	uint32_t i;

	if (node == 0 || entry == 0)
		return -1;
	child = node->first_child;
	for (i = 0; child != 0 && i < index; ++i)
		child = child->next_sibling;
	if (child == 0)
		return -1;
	*entry = *child;
	return 0;
}