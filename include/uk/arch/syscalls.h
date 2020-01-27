#pragma once

#define UK_PLAT_SYSCALL_HANDLER(number)                                        \
	void uk_svc_handler_##number_func();                                   \
	struct uk_plat_arm_common_syscall_handlers uk_svc_handler_##number = { \
	    number, uk_svc_handler_##number_func, 0};                          \
	void uk_svc_handler_##number_func()

#define UK_PLAT_REGISTER_SYSCALL(number)                              \
	extern struct uk_plat_arm_common_syscall_handlers                      \
	    *uk_platform_arm_common_first_handler;                             \
	extern struct uk_plat_arm_common_syscall_handlers                      \
	    uk_svc_handler_##number;                                           \
	uk_svc_handler_##number.next = uk_platform_arm_common_first_handler;   \
	uk_platform_arm_common_first_handler = &uk_svc_handler_##number;

struct uk_plat_arm_common_syscall_handlers {
	unsigned long syscall_number;
	void (*handler)();
	struct uk_plat_arm_common_syscall_handlers *next;
};

int uk_plat_arm_common_handle_syscall(unsigned long syscall_number);