#ifndef FS_PIPE_H
#define FS_PIPE_H

#include <fs/vfs.h>

#define PIPE_BUFFER_SIZE 4096

// pipe(): crea un par de vfs_node en memoria (sin pasar por ningun
// filesystem) conectados por un buffer circular compartido.
// *read_node solo permite read(), *write_node solo permite write().
// Devuelve 0 en exito.
int pipe_create(struct vfs_node **read_node, struct vfs_node **write_node);

// mkfifo(): crea, dentro del directorio resuelto para `path`, un nodo
// de tipo VFS_TYPE_FIFO. El buffer circular NO se reserva aqui, sino
// la primera vez que alguien lo abre (para que dos procesos que abran
// la misma ruta despues queden conectados al mismo buffer).
int pipe_mkfifo(struct vfs_node *cwd, const char *path, uint32_t mode);

#endif