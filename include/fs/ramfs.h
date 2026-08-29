#ifndef FS_RAMFS_H
#define FS_RAMFS_H

#include <fs/vfs.h>

// Inicializa ramfs y lo monta como raiz del VFS (vfs_mount_root()).
// Debe llamarse ANTES que devfs_init() (que cuelga /dev de esta
// raiz) y antes de dcache_init()/file_table_init().
int ramfs_init(void);

// Nodo raiz de ramfs ("/"). Ya montado via vfs_mount_root() por
// ramfs_init(); lo usan otros filesystems para colgar sus puntos de
// montaje con vfs_attach_child().
struct vfs_node *ramfs_root(void);

#endif