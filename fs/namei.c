// fs/namei.c
//
// Resolucion de rutas ("name to inode"): convierte una ruta como
// "/dev/dri/card0" en el struct vfs_node correspondiente, recorriendo
// el arbol generico (parent/first_child/next_sibling de vfs.h) un
// componente cada vez.
//
// No entiende "." ni ".." (no hace falta mientras no haya cwd real por
// proceso) ni sigue symlinks. Rutas relativas se resuelven pasando
// `cwd` en vez de vfs_root() como nodo de partida.

#include <fs/namei.h>
#include <fs/vfs.h>

static size_t component_length(const char *path)
{
	size_t length = 0;
	while (path[length] != 0 && path[length] != '/')
		++length;
	return length;
}

static int component_equals(const char *path, size_t length, const char *name)
{
	size_t i;
	for (i = 0; i < length; ++i)
		if (name[i] == 0 || path[i] != name[i])
			return 0;
	return name[length] == 0;
}

// Como vfs_find_child(), pero comparando contra un fragmento de
// longitud fija (el componente dentro de `path` no esta NUL-terminado)
// en vez de una cadena completa, para no tener que copiarlo antes.
static struct vfs_node *find_child_n(struct vfs_node *parent, const char *path,
                                       size_t length)
{
	struct vfs_node *node;

	if (parent == 0)
		return 0;
	for (node = parent->first_child; node != 0; node = node->next_sibling)
		if (component_equals(path, length, node->name))
			return node;
	return 0;
}

struct vfs_node *namei(struct vfs_node *cwd, const char *path)
{
	struct vfs_node *current;
	const char *cursor;

	if (path == 0)
		return 0;

	current = (path[0] == '/') ? vfs_root() : cwd;
	cursor = path;
	if (path[0] == '/')
		++cursor;

	while (*cursor != 0 && current != 0) {
		size_t length = component_length(cursor);
		if (length == 0) {
			++cursor;
			continue;
		}
		current = find_child_n(current, cursor, length);
		cursor += length;
		if (*cursor == '/')
			++cursor;
	}
	return current;
}

struct vfs_node *namei_parent(struct vfs_node *cwd, const char *path,
                                const char **out_name, size_t *out_name_length)
{
	const char *last_slash = 0;
	const char *cursor = path;
	char parent_path[128];
	size_t parent_length;
	size_t i;

	if (path == 0 || out_name == 0 || out_name_length == 0)
		return 0;

	while (*cursor != 0) {
		if (*cursor == '/')
			last_slash = cursor;
		++cursor;
	}

	if (last_slash == 0) {
		*out_name = path;
		*out_name_length = component_length(path);
		return (path[0] == '/') ? vfs_root() : cwd;
	}

	*out_name = last_slash + 1;
	*out_name_length = component_length(*out_name);

	parent_length = (size_t)(last_slash - path);
	if (parent_length == 0)
		return vfs_root();
	if (parent_length >= sizeof(parent_path))
		return 0;

	for (i = 0; i < parent_length; ++i)
		parent_path[i] = path[i];
	parent_path[parent_length] = 0;
	return namei(cwd, parent_path);
}