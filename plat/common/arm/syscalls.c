#include <uk/arch/syscalls.h>
#include <stdio.h>

struct uk_plat_arm_common_syscall_handlers
    *uk_platform_arm_common_first_handler = 0;

int uk_plat_arm_common_handle_syscall(unsigned long syscall_number)
{
	struct uk_plat_arm_common_syscall_handlers *handler =
	    uk_platform_arm_common_first_handler;
	while (handler != 0) {
		if (handler->syscall_number == syscall_number) {
			handler->handler();
			return 1;
		}
	}

	return 0;
}