#include <stdint.h>
#include <user/syscall.h>

/* Userland command frontend. It never receives a WindowManager pointer. */
int shell_execute_command(const char *command)
{
	return (int)user_console_command(command);
}
