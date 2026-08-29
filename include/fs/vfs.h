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
// Nuevas: creacion/borrado de entradas dentro de un directorio.
// `parent` es el nodo directorio sobre el que se llama; create/mkdir
// devuelven el nodo nuevo via `out_node`. Un filesystem de solo
// lectura (como devfs) simplemente deja estos punteros a 0.
typedef int (*vfs_create_fn)(struct vfs_node *parent, const char *name,
                              uint32_t mode, struct vfs_node **out_node);
typedef int (*vfs_mkdir_fn)(struct vfs_node *parent, const char *name,
                             uint32_t mode, struct vfs_node **out_node);
typedef int (*vfs_unlink_fn)(struct vfs_node *parent, const char *name);

struct vfs_operations {
	vfs_read_fn read;
	vfs_write_fn write;
	vfs_readdir_fn readdir;
	vfs_open_fn open;
	vfs_close_fn close;
	vfs_create_fn create;
	vfs_mkdir_fn mkdir;
	vfs_unlink_fn unlink;
};

enum vfs_node_type {
	VFS_TYPE_FILE = 0,
	VFS_TYPE_DIRECTORY,
	VFS_TYPE_DEVICE,
	VFS_TYPE_FIFO,
	VFS_TYPE_SYMLINK,
};

#define VFS_NAME_MAX 60

struct vfs_node {
	const char *name;
	uint32_t mode;
	uint64_t size;
	void *private_data;
	const struct vfs_operations *operations;

	// Arbol generico. Un filesystem que gestiona sus propios hijos "a
	// mano" (como hacia devfs antes de esta revision) puede dejarlos a
	// 0 y usar su propio readdir; los que se integran en el arbol
	// (ramfs, y ahora devfs tambien) los mantienen enlazados aqui para
	// que namei() pueda resolver rutas atravesandolos.
	enum vfs_node_type type;
	struct vfs_node *parent;
	struct vfs_node *first_child;
	struct vfs_node *next_sibling;
	int32_t refcount;
};

int vfs_init(void);
long vfs_read(struct vfs_node *node, void *buffer, size_t length, uint64_t offset);
long vfs_write(struct vfs_node *node, const void *buffer, size_t length, uint64_t offset);
int vfs_readdir(struct vfs_node *node, uint32_t index, struct vfs_node *entry);

// Raiz del arbol de VFS. La fija ramfs_init() durante el arranque;
// namei.c parte de aqui para resolver rutas absolutas.
struct vfs_node *vfs_root(void);
void vfs_mount_root(struct vfs_node *root);

// Enlaza `child` como hijo de `parent` en el arbol generico. No
// reserva memoria: `child` debe venir ya inicializado desde el pool
// estatico de quien lo posea (ramfs, devfs, ...).
void vfs_attach_child(struct vfs_node *parent, struct vfs_node *child);

// Busca un hijo directo de `parent` por nombre exacto. Devuelve 0 si
// no existe. Lo usan namei.c y los create/mkdir de cada filesystem
// (para rechazar nombres duplicados).
struct vfs_node *vfs_find_child(struct vfs_node *parent, const char *name);

// readdir generico para filesystems que usan el arbol de vfs_node en
// vez de una tabla propia (ramfs lo usa directamente como su
// vfs_operations.readdir).
int vfs_generic_readdir(struct vfs_node *node, uint32_t index, struct vfs_node *entry);

#endif