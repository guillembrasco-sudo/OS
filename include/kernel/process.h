#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stdint.h>
#include <kernel/context.h>

struct process {
	uint32_t pid;
	uint64_t pml4_physical;
	uintptr_t user_entry;
	uintptr_t user_stack;
	struct cpu_context context;
};

int process_create_user(struct process *process,
	                       uintptr_t entry,
	                       uintptr_t user_stack);
int process_activate(const struct process *process);
void process_enter_user(const struct process *process);
void process_switch(struct process *current, struct process *next);
uint32_t current_process_id(void);

#endif