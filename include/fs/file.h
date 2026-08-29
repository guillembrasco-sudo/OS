#ifndef FS_FILE_H
#define FS_FILE_H

#include <stdint.h>
#include <fs/vfs.h>

#define NR_OPEN_FILES 256

// Descriptor de fichero abierto. Vive en una tabla GLOBAL por ahora
// (un unico espacio de fds para todo el sistema); cuando exista una
// tabla por proceso de verdad, esto se movera dentro de
// kernel/process.h y el fd pasara a indexar esa tabla. Mientras
// tanto, esto deja open/read/write/close funcionando de extremo a
// extremo para que syscalls.c tenga algo real que llamar.
struct file {
	struct vfs_node *node;
	uint64_t offset;
	uint32_t flags;
	int32_t refcount;
	int used;
};

void file_table_init(void);
int file_alloc(struct vfs_node *node, uint32_t flags);
struct file *file_get(int fd);
int file_release(int fd);

#endif