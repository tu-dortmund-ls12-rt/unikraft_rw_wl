/* SPDX-License-Identifier: ISC */
/*
 * Authors: Wei Chen <Wei.Chen@arm.com>
 *
 * Copyright (c) 2018 Arm Ltd.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <string.h>
#include <uk/print.h>
#include <uk/assert.h>
#include <gic/gic-v2.h>

static const char *exception_modes[] = {"Synchronous Abort", "IRQ", "FIQ",
					"Error"};

static void dump_registers(struct __regs *regs, uint64_t far)
{
	unsigned char idx;

	uk_pr_crit("Unikraft: Dump registers:\n");
	uk_pr_crit("\t SP       : 0x%016lx\n", regs->sp);
	uk_pr_crit("\t ESR_EL1  : 0x%016lx\n", regs->esr_el1);
	uk_pr_crit("\t ELR_EL1  : 0x%016lx\n", regs->elr_el1);
	uk_pr_crit("\t LR (x30) : 0x%016lx\n", regs->lr);
	uk_pr_crit("\t PSTATE   : 0x%016lx\n", regs->spsr_el1);
	uk_pr_crit("\t FAR_EL1  : 0x%016lx\n", far);

	for (idx = 0; idx < 28; idx += 4)
		uk_pr_crit(
		    "\t x%02d ~ x%02d: 0x%016lx 0x%016lx 0x%016lx 0x%016lx\n",
		    idx, idx + 3, regs->x[idx], regs->x[idx + 1],
		    regs->x[idx + 2], regs->x[idx + 3]);

	uk_pr_crit("\t x28 ~ x29: 0x%016lx 0x%016lx\n", regs->x[28],
		   regs->x[29]);
}

void invalid_trap_handler(struct __regs *regs, uint32_t el, uint32_t reason,
			  uint64_t far)
{
	uk_pr_crit("Unikraft: EL%d invalid %s trap caught\n", el,
		   exception_modes[reason]);
	dump_registers(regs, far);
	ukplat_crash();
}

void trap_el0_sync(struct __regs *regs, uint64_t far)
{
	// Check for a wanted SVC first
	unsigned long esr_el1;
	asm volatile("mrs %0, esr_el1" : "=r"(esr_el1));
	if (((esr_el1 >> 26) & 0b111111) == 0b010101) {
		// SVC, may be desired
		if (uk_plat_arm_common_handle_syscall(esr_el1 & 0xFFFF)) {
			return;
		}
	}

	/**
	 * Page faults are sync traps but actually not that bad. Some apps or
	 * libs may produce page faults on purpose to do something. In case this
	 * feature is available, a page fault is forwared to an implementable
	 * handler instead of crashing the entire system
	 */
#ifdef CONFIG_FORWARD_PAGEFAULT
	// Check the exception syndrome register first, to figure out if there
	// was a page fault
	// unsigned long esr_el1;
	asm volatile("mrs %0, esr_el1" : "=r"(esr_el1));
	// Check for MMU data error first
	if ((esr_el1 & 0xFC000000) == 0x90000000) {
		// Check if it was a permission fault on Table level 3
		if ((esr_el1 & 0x3F) == 0x0F) {
			extern void uk_upper_level_page_fault_handler(
			    unsigned long *register_stack);
			uk_upper_level_page_fault_handler(
			    (unsigned long *)regs);
			return;
		}
	}

#endif
	uk_pr_crit("Unikraft: EL0 sync trap caught\n");

	dump_registers(regs, far);
	ukplat_crash();
}

void trap_el1_sync(struct __regs *regs, uint64_t far)
{
	uk_pr_crit("Unikraft: EL1 sync trap caught\n");

	dump_registers(regs, far);
	ukplat_crash();
}

void trap_el1_irq(void)
{
	gic_handle_irq();
}
