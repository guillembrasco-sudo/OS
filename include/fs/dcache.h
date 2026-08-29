#ifndef FS_DCACHE_H
#define FS_DCACHE_H

struct vfs_node;

// Cache plana de traduccion path -> vfs_node, para evitar recorrer el
// arbol entero en cada open()/stat() de una ruta repetida. No es
// coherente de forma automatica: si un filesystem borra/renombra un
// nodo debe llamar a dcache_invalidate() (ramfs_unlink() ya lo hace).
void dcache_init(void);
struct vfs_node *dcache_lookup(const char *path);
void dcache_insert(const char *path, struct vfs_node *node);
// path == 0 invalida la cache entera.
void dcache_invalidate(const char *path);

#endif