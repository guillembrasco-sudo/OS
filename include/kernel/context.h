#ifndef KERNEL_CONTEXT_H
#define KERNEL_CONTEXT_H

#include <stdint.h>

struct cpu_context {
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
};

void context_switch(struct cpu_context *old_context,
                    const struct cpu_context *new_context);

#endif
