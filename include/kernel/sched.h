#ifndef KERNEL_SCHED_H
#define KERNEL_SCHED_H

#include <stdint.h>

struct sched_entity {
    struct sched_entity *left;
    struct sched_entity *right;
    struct sched_entity *parent;
    uint64_t vruntime;
    uint32_t cpu;
    uint32_t numa_node;
    uint8_t red;
};

#define SCHED_MAX_CPUS 64

void sched_init(void);
void sched_enqueue(struct sched_entity *entity);
void sched_dequeue(struct sched_entity *entity);
struct sched_entity *sched_pick_next(uint32_t cpu, uint32_t numa_node);
void sched_tick(struct sched_entity *entity, uint64_t elapsed_ns);

#endif