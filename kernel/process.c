#include <stdint.h>
#include <kernel/process.h>
#include <arch/paging.h>
#include <mm/pmm.h>

#define USER_ADDRESS_LIMIT 0x0000800000000000ULL
#define USER_PAGE_SIZE 0x1000ULL

static uint32_t next_process_id = 1;

int process_create_user(struct process *process, uintptr_t entry,
	                       uintptr_t user_stack)
{
	if (!process || entry >= 0x0000800000000000ULL ||
	    user_stack >= USER_ADDRESS_LIMIT || user_stack < USER_PAGE_SIZE ||
	    (user_stack & (USER_PAGE_SIZE - 1)) != 0)
		return -1;
	if (paging_create_user_space(&process->pml4_physical) != 0)
		return -1;
	process->pid = next_process_id++;
	process->user_entry = entry;
	process->user_stack = user_stack;
	uint64_t stack_page = pmm_alloc_page();
	if (!stack_page || paging_map_page_in_space(
			process->pml4_physical, user_stack - USER_PAGE_SIZE,
			stack_page, PAGING_WRITE) != 0)
		return -1;
	return 0;
}

int process_activate(const struct process *process)
{
	if (!process || !process->pml4_physical)
		return -1;
	return paging_activate_space(process->pml4_physical);
}

void process_enter_user(const struct process *process)
{
	if (!process || !process->user_entry || !process->user_stack)
		return;
	enter_usermode(process->user_entry, process->user_stack);
}

void process_switch(struct process *current, struct process *next)
{
	if (!current || !next || process_activate(next) != 0)
		return;
	context_switch(&current->context, &next->context);
}

uint32_t current_process_id(void)
{
	return next_process_id - 1;
}
