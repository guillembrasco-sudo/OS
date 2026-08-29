#ifndef FS_OPEN_H
#define FS_OPEN_H

#include <stddef.h>
#include <stdint.h>
#include <fs/vfs.h>

#define VFS_O_RDONLY 0x0
#define VFS_O_WRONLY 0x1
#define VFS_O_RDWR   0x2
#define VFS_O_CREAT  0x4

// Equivalentes de open()/close()/read()/write() a nivel de VFS (todavia
// no son syscalls; kernel/syscall.c llamara a estas cuando se
// implemente el modulo de syscalls). `cwd` es el directorio desde el
// que resolver rutas relativas; pasa vfs_root() mientras no haya cwd
// por proceso.
int vfs_open_path(struct vfs_node *cwd, const char *path, uint32_t flags, uint32_t mode);
int vfs_close_fd(int fd);
long vfs_read_fd(int fd, void *buffer, size_t length);
long vfs_write_fd(int fd, const void *buffer, size_t length);

#endif