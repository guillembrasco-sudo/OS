#include <fs/devfs.h>

static struct vfs_node card0;
static struct vfs_node render128;
static struct vfs_node dri_dir;
static uint32_t card_owner;

static int card_open(struct vfs_node *node)
{
	(void)node;
	return 0;
}

static int card_close(struct vfs_node *node)
{
	(void)node;
	card_owner = 0;
	return 0;
}

static const struct vfs_operations card_operations = {
	0, 0, 0, card_open, card_close
};

static int dri_readdir(struct vfs_node *node, uint32_t index,
	                       struct vfs_node *entry)
{
	(void)node;
	if (index == 0) {
		*entry = card0;
		return 0;
	}
	if (index == 1) {
		*entry = render128;
		return 0;
	}
	return -1;
}

static const struct vfs_operations directory_operations = {
	0, 0, dri_readdir, 0, 0
};

static int path_equal(const char *left, const char *right)
{
	while (*left != 0 && *left == *right) {
		++left;
		++right;
	}
	return *left == 0 && *right == 0;
}

int devfs_init(void)
{
	card0.name = "card0";
	card0.mode = 0600;
	card0.operations = &card_operations;
	render128.name = "renderD128";
	render128.mode = 0660;
	render128.operations = &card_operations;
	dri_dir.name = "dri";
	dri_dir.mode = 0555;
	dri_dir.operations = &directory_operations;
	card_owner = 0;
	return 0;
}

struct vfs_node *devfs_lookup(const char *path)
{
	if (path == 0)
		return 0;
	if (path_equal(path, "/dev/dri/card0"))
		return &card0;
	if (path_equal(path, "/dev/dri/renderD128"))
		return &render128;
	if (path_equal(path, "/dev/dri"))
		return &dri_dir;
	return 0;
}

int devfs_open(struct vfs_node *node, uint32_t process_id)
{
	if (node == 0 || process_id == 0)
		return -1;
	if (node == &card0) {
		if (card_owner != 0 && card_owner != process_id)
			return -1;
		card_owner = process_id;
	}
	return node->operations != 0 && node->operations->open != 0
		? node->operations->open(node) : 0;
}
