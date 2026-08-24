#include <stdint.h>
#include <kernel/syscall.h>
#include <kernel/handles.h>
#include <drivers/gpu_ioctl.h>
#include <arch/uaccess.h>

/* The process ID must come from the current task, never from user registers. */
extern uint32_t current_process_id(void);

uint64_t syscall_dispatch(uint64_t number, uint64_t arg0, uint64_t arg1,
						  uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	uint32_t process_id = current_process_id();
	(void)arg3;
	(void)arg4;
	(void)arg4;
	if (number == SYS_HANDLE_CLOSE)
		return handle_close(process_id, (handle_t)arg0);
	if (number == SYS_GPU_IOCTL) {
		uint8_t argument[256];
		size_t size = arg2 > sizeof(argument) ? sizeof(argument) : (size_t)arg2;
		if (arg1 == 0 || copy_from_user(argument, (const void *)arg1, size) != 0)
			return (uint64_t)-1;
		return gpu_ioctl_dispatch(process_id, (uint32_t)arg0, argument);
	}
	if (number == SYS_YIELD)
		return 0;
	return (uint64_t)-1;
}
