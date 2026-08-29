#ifndef FS_NAMEI_H
#define FS_NAMEI_H

#include <stddef.h>

struct vfs_node;

// Resuelve `path` (absoluta si empieza por '/', relativa a `cwd` si
// no) recorriendo el arbol generico de vfs.h. Devuelve 0 si algun
// componente no existe. No entiende "." ni ".." todavia, ni sigue
// symlinks.
struct vfs_node *namei(struct vfs_node *cwd, const char *path);

// Resuelve el directorio PADRE de `path` y deja el ULTIMO componente
// (el nombre a crear/borrar dentro de ese directorio) en
// *out_name/*out_name_length, SIN resolverlo ni terminarlo en NUL
// (es un fragmento dentro de `path`). Lo usan open(O_CREAT), mkdir()
// y unlink()/mkfifo().
struct vfs_node *namei_parent(struct vfs_node *cwd, const char *path,
                                const char **out_name, size_t *out_name_length);

#endif