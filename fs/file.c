#include <fs/file.h>

static struct file table[NR_OPEN_FILES];

void file_table_init(void)
{
	int i;
	for (i = 0; i < NR_OPEN_FILES; ++i)
		table[i].used = 0;
}

int file_alloc(struct vfs_node *node, uint32_t flags)
{
	int i;
	if (node == 0)
		return -1;
	for (i = 0; i < NR_OPEN_FILES; ++i) {
		if (!table[i].used) {
			table[i].used = 1;
			table[i].node = node;
			table[i].offset = 0;
			table[i].flags = flags;
			table[i].refcount = 1;
			return i;
		}
	}
	return -1;
}

struct file *file_get(int fd)
{
	if (fd < 0 || fd >= NR_OPEN_FILES || !table[fd].used)
		return 0;
	return &table[fd];
}

int file_release(int fd)
{
	struct file *entry = file_get(fd);
	if (entry == 0)
		return -1;
	entry->refcount--;
	if (entry->refcount <= 0)
		entry->used = 0;
	return 0;
}