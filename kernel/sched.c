#include <kernel/sched.h>

static struct sched_entity *run_tree;

static void rotate_left(struct sched_entity *node)
{
	struct sched_entity *child = node->right;
	node->right = child->left;
	if (child->left != 0)
		child->left->parent = node;
	child->parent = node->parent;
	if (node->parent == 0)
		run_tree = child;
	else if (node == node->parent->left)
		node->parent->left = child;
	else
		node->parent->right = child;
	child->left = node;
	node->parent = child;
}

static void rotate_right(struct sched_entity *node)
{
	struct sched_entity *child = node->left;
	node->left = child->right;
	if (child->right != 0)
		child->right->parent = node;
	child->parent = node->parent;
	if (node->parent == 0)
		run_tree = child;
	else if (node == node->parent->right)
		node->parent->right = child;
	else
		node->parent->left = child;
	child->right = node;
	node->parent = child;
}

void sched_init(void)
{
	run_tree = 0;
}

void sched_enqueue(struct sched_entity *entity)
{
	struct sched_entity *parent = 0;
	struct sched_entity *cursor = run_tree;
	entity->left = 0;
	entity->right = 0;
	entity->red = 1;
	while (cursor != 0) {
		parent = cursor;
		cursor = entity->vruntime < cursor->vruntime ? cursor->left : cursor->right;
	}
	entity->parent = parent;
	if (parent == 0)
		run_tree = entity;
	else if (entity->vruntime < parent->vruntime)
		parent->left = entity;
	else
		parent->right = entity;
	if (entity->parent != 0 && entity->parent->parent == 0) {
		if (entity == entity->parent->right)
			rotate_left(entity->parent);
		else
			rotate_right(entity->parent);
		entity->red = 0;
	}
}

void sched_dequeue(struct sched_entity *entity)
{
	struct sched_entity *replacement = entity->left != 0 ? entity->left : entity->right;
	if (replacement != 0)
		replacement->parent = entity->parent;
	if (entity->parent == 0)
		run_tree = replacement;
	else if (entity == entity->parent->left)
		entity->parent->left = replacement;
	else
		entity->parent->right = replacement;
	entity->left = 0;
	entity->right = 0;
	entity->parent = 0;
}

struct sched_entity *sched_pick_next(uint32_t cpu, uint32_t numa_node)
{
	struct sched_entity *cursor = run_tree;
	struct sched_entity *best = 0;
	while (cursor != 0) {
		if (cursor->cpu == cpu || cursor->numa_node == numa_node)
			best = cursor;
		cursor = cursor->left;
	}
	if (best != 0)
		return best;
	cursor = run_tree;
	while (cursor != 0 && cursor->left != 0)
		cursor = cursor->left;
	return cursor;
}

void sched_tick(struct sched_entity *entity, uint64_t elapsed_ns)
{
	entity->vruntime += elapsed_ns;
}