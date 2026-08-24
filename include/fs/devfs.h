#ifndef FS_DEVFS_H
#define FS_DEVFS_H

#include <fs/vfs.h>

int devfs_init(void);
struct vfs_node *devfs_lookup(const char *path);
int devfs_open(struct vfs_node *node, uint32_t process_id);

#endif