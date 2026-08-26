#ifndef FS_VFS_H
#define FS_VFS_H

#include <stddef.h>
#include <stdint.h>

struct vfs_node;

typedef long (*vfs_read_fn)(struct vfs_node *node, void *buffer, size_t length, uint64_t offset);
typedef long (*vfs_write_fn)(struct vfs_node *node, const void *buffer, size_t length, uint64_t offset);
typedef int (*vfs_readdir_fn)(struct vfs_node *node, uint32_t index,
                              struct vfs_node *entry);
typedef int (*vfs_open_fn)(struct vfs_node *node);
typedef int (*vfs_close_fn)(struct vfs_node *node);

struct vfs_operations {
    vfs_read_fn read;
    vfs_write_fn write;
    vfs_readdir_fn readdir;
    vfs_open_fn open;
    vfs_close_fn close;
};

struct vfs_node {
    const char *name;
    uint32_t mode;
    uint64_t size;
    void *private_data;
    const struct vfs_operations *operations;
};

int vfs_init(void);
long vfs_read(struct vfs_node *node, void *buffer, size_t length, uint64_t offset);
long vfs_write(struct vfs_node *node, const void *buffer, size_t length, uint64_t offset);
int vfs_readdir(struct vfs_node *node, uint32_t index, struct vfs_node *entry);

#endif