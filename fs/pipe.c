// fs/pipe.c
//
// Pipes anonimos (pipe_create) y con nombre (pipe_mkfifo / FIFOs).
// Ambos comparten el mismo tipo de buffer circular de tamano fijo
// (PIPE_BUFFER_SIZE), tomado de un pool estatico - igual que el resto
// de fs/ en esta revision, sin allocator de kernel todavia.
//
// No hay scheduler enganchado aqui: si el buffer esta lleno (write) o
// vacio (read) se devuelve sin bloquear en vez de suspender al
// proceso. Cuando exista un scheduler con colas de espera, sustituir
// esos casos por un yield/wait real es la unica pieza que falta.

#include <fs/pipe.h>
#include <fs/namei.h>

#define NR_PIPE_BUFFERS 32

struct pipe_buffer {
	uint8_t data[PIPE_BUFFER_SIZE];
	uint32_t head;
	uint32_t tail;
	uint32_t count;
	int used;
};

// Pool aparte para los vfs_node de los extremos anonimos (pipe_create):
// no cuelgan de ningun filesystem, asi que ramfs no los puede reservar.
struct pipe_endpoint_storage {
	struct vfs_node node;
	int used;
};

static struct pipe_buffer buffers[NR_PIPE_BUFFERS];
static struct pipe_endpoint_storage endpoints[NR_PIPE_BUFFERS * 2];

static long pipe_read(struct vfs_node *node, void *buffer, size_t length, uint64_t offset);
static long pipe_write(struct vfs_node *node, const void *buffer, size_t length, uint64_t offset);
static int fifo_open(struct vfs_node *node);
static int fifo_close(struct vfs_node *node);

static const struct vfs_operations pipe_read_operations = {
	pipe_read, 0, 0, 0, 0, 0, 0, 0
};

static const struct vfs_operations pipe_write_operations = {
	0, pipe_write, 0, 0, 0, 0, 0, 0
};

static const struct vfs_operations fifo_operations = {
	pipe_read, pipe_write, 0, fifo_open, fifo_close, 0, 0, 0
};

static struct pipe_buffer *alloc_buffer(void)
{
	uint32_t i;
	for (i = 0; i < NR_PIPE_BUFFERS; ++i)
		if (!buffers[i].used) {
			buffers[i].used = 1;
			buffers[i].head = 0;
			buffers[i].tail = 0;
			buffers[i].count = 0;
			return &buffers[i];
		}
	return 0;
}

static struct vfs_node *alloc_endpoint(void)
{
	uint32_t i;
	for (i = 0; i < NR_PIPE_BUFFERS * 2; ++i)
		if (!endpoints[i].used) {
			endpoints[i].used = 1;
			return &endpoints[i].node;
		}
	return 0;
}

static long pipe_read(struct vfs_node *node, void *buffer, size_t length, uint64_t offset)
{
	struct pipe_buffer *pipe = (struct pipe_buffer *)node->private_data;
	uint8_t *out = (uint8_t *)buffer;
	size_t read_count = 0;

	(void)offset;
	if (pipe == 0 || buffer == 0)
		return -1;
	if (pipe->count == 0)
		return 0; // sin datos: no bloquea (ver nota de cabecera), simplemente no lee nada

	while (read_count < length && pipe->count > 0) {
		out[read_count] = pipe->data[pipe->tail];
		pipe->tail = (pipe->tail + 1) % PIPE_BUFFER_SIZE;
		pipe->count--;
		read_count++;
	}
	return (long)read_count;
}

static long pipe_write(struct vfs_node *node, const void *buffer, size_t length, uint64_t offset)
{
	struct pipe_buffer *pipe = (struct pipe_buffer *)node->private_data;
	const uint8_t *in = (const uint8_t *)buffer;
	size_t written = 0;

	(void)offset;
	if (pipe == 0 || buffer == 0)
		return -1;
	if (pipe->count == PIPE_BUFFER_SIZE)
		return -1; // buffer lleno: ver nota de cabecera sobre bloqueo

	while (written < length && pipe->count < PIPE_BUFFER_SIZE) {
		pipe->data[pipe->head] = in[written];
		pipe->head = (pipe->head + 1) % PIPE_BUFFER_SIZE;
		pipe->count++;
		written++;
	}
	return (long)written;
}

int pipe_create(struct vfs_node **read_node, struct vfs_node **write_node)
{
	struct pipe_buffer *pipe;
	struct vfs_node *rnode;
	struct vfs_node *wnode;

	if (read_node == 0 || write_node == 0)
		return -1;

	pipe = alloc_buffer();
	if (pipe == 0)
		return -1;

	rnode = alloc_endpoint();
	wnode = alloc_endpoint();
	if (rnode == 0 || wnode == 0) {
		pipe->used = 0;
		return -1;
	}

	rnode->name = "pipe:read";
	rnode->mode = 0400;
	rnode->type = VFS_TYPE_FIFO;
	rnode->private_data = pipe;
	rnode->operations = &pipe_read_operations;

	wnode->name = "pipe:write";
	wnode->mode = 0200;
	wnode->type = VFS_TYPE_FIFO;
	wnode->private_data = pipe;
	wnode->operations = &pipe_write_operations;

	*read_node = rnode;
	*write_node = wnode;
	return 0;
}

// Reserva el buffer compartido la primera vez que se abre esta ruta
// (node->private_data == 0); las aperturas siguientes de la misma
// ruta reutilizan el mismo buffer, conectando a los procesos que
// abran ese FIFO por separado.
static int fifo_open(struct vfs_node *node)
{
	if (node->private_data != 0)
		return 0;
	node->private_data = alloc_buffer();
	return node->private_data != 0 ? 0 : -1;
}

static int fifo_close(struct vfs_node *node)
{
	(void)node;
	// El buffer se queda vivo mientras el nodo del FIFO exista en el
	// arbol, para que el siguiente open() lo siga encontrando.
	return 0;
}

int pipe_mkfifo(struct vfs_node *cwd, const char *path, uint32_t mode)
{
	const char *name;
	size_t name_length;
	struct vfs_node *parent;
	struct vfs_node *node;
	char name_buffer[VFS_NAME_MAX];
	size_t i;

	parent = namei_parent(cwd, path, &name, &name_length);
	if (parent == 0 || parent->operations == 0 ||
	    parent->operations->create == 0 ||
	    name_length == 0 || name_length >= VFS_NAME_MAX)
		return -1;

	for (i = 0; i < name_length; ++i)
		name_buffer[i] = name[i];
	name_buffer[name_length] = 0;

	if (parent->operations->create(parent, name_buffer, mode, &node) != 0)
		return -1;

	node->type = VFS_TYPE_FIFO;
	node->operations = &fifo_operations;
	node->private_data = 0;
	return 0;
}