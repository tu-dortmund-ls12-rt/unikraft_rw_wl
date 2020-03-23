/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Wei Chen <wei.chen@arm.com>
 *
 * Copyright (c) 2018, Arm Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */
#include <sw_ctx.h>
#include <uk/arch/lcpu.h>
#include <uk/asm.h>
#include <uk/config.h>

/*
 * Thread stack memory layout:
 *
 * |-----------------------------| --> SP
 * |    thread main function     | In init_sp, we have pushed thread main
 * |-----------------------------| function and arguments to stackfunction
 * |    arguments                | and arguments to stack.
 * |-----------------------------| --> ctx->sp = SP - function - arguments
 * |    Reversed room for Arm64  |
 * |    Callee saved registers:  |
 * |    x19 ~ x28                |
 * |-----------------------------|
 * |    x29 (frame pointer)      |
 * |-----------------------------|
 * |    ...                      |
 * |    ...                      |
 * |-----------------------------|
 * |    thread                   | --> used for uk_thread_current
 * |-----------------------------|
 */

/*
 * This function will jump to thread main function and set up
 * thread exit handler.
 */
ENTRY(asm_thread_starter)
	mov x2, sp
	ldp x0, x1, [x2]		/* Load func to x1, args to x0 */
	ldr x30, =uk_sched_thread_exit	/* Set thread exit handler */
	br  x1				/* Jump to thread main function */
ENDPROC(asm_thread_starter)

/*
 * x0 = ctx->sp
 * x1 = ctx->ip = asm_thread_starter
 *
 * This function will set the SP to thread's SP and jump to
 * ctx->ip: asm_thread_starter
 */
ENTRY(asm_ctx_start)
	mov sp, x0	/* set SP */
	br x1		/* jump to asm_thread_starter */
ENDPROC(asm_ctx_start)

/*
 * x0 = prectx
 * x1 = nextctx
 * This function will switch to the next thread context
 */
ENTRY(asm_sw_ctx_switch)
	/* Save callee-saved registers to prevctx's stack */
	sub x2, sp, #__CALLEE_SAVED_SIZE
	stp x19, x20, [x2, #16 * 0]
	stp x21, x22, [x2, #16 * 1]
	stp x23, x24, [x2, #16 * 2]
	stp x25, x26, [x2, #16 * 3]
	stp x27, x28, [x2, #16 * 4]
	stp x29, x30, [x2, #16 * 5]

	/*
	 * Record the restore point for switch out thread to restore
	 * its called-saved registers in next switch to time.
	 */
	ldr x30, =restore_point

	/* Save sp and restore point to previous context */
	stp x2, x30, [x0]

	/* Restire sp and restore point from next context */
	ldp x2, x30, [x1]
	mov sp, x2

	ret

restore_point:
	/* Restore the callee-saved registers */
	ldp x19, x20, [x2, #16 * 0]
	ldp x21, x22, [x2, #16 * 1]
	ldp x23, x24, [x2, #16 * 2]
	ldp x25, x26, [x2, #16 * 3]
	ldp x27, x28, [x2, #16 * 4]
	ldp x29, x30, [x2, #16 * 5]

	add sp, x2, #__CALLEE_SAVED_SIZE
	ret
ENDPROC(asm_sw_ctx_switch)

#ifdef CONFIG_FLOAT_POINT
.macro fpsimd_save state, tmpnr
	stp	q0, q1, [\state, #16 * 0]
	stp	q2, q3, [\state, #16 * 2]
	stp	q4, q5, [\state, #16 * 4]
	stp	q6, q7, [\state, #16 * 6]
	stp	q8, q9, [\state, #16 * 8]
	stp	q10, q11, [\state, #16 * 10]
	stp	q12, q13, [\state, #16 * 12]
	stp	q14, q15, [\state, #16 * 14]
	stp	q16, q17, [\state, #16 * 16]
	stp	q18, q19, [\state, #16 * 18]
	stp	q20, q21, [\state, #16 * 20]
	stp	q22, q23, [\state, #16 * 22]
	stp	q24, q25, [\state, #16 * 24]
	stp	q26, q27, [\state, #16 * 26]
	stp	q28, q29, [\state, #16 * 28]
	stp	q30, q31, [\state, #16 * 30]!
	mrs	x\tmpnr, fpsr
	str	w\tmpnr, [\state, #16 * 2]
	mrs	x\tmpnr, fpcr
	str	w\tmpnr, [\state, #16 * 2 + 4]
.endm

.macro fpsimd_restore_fpcr state, tmp
	/*
	 * Writes to fpcr may be self-synchronising, so avoid restoring
	 * the register if it hasn't changed.
	 */
	mrs	\tmp, fpcr
	cmp	\tmp, \state
	b.eq	9999f
	msr	fpcr, \state
9999:
.endm

/* Clobbers \state */
.macro fpsimd_restore state, tmpnr
	ldp	q0, q1, [\state, #16 * 0]
	ldp	q2, q3, [\state, #16 * 2]
	ldp	q4, q5, [\state, #16 * 4]
	ldp	q6, q7, [\state, #16 * 6]
	ldp	q8, q9, [\state, #16 * 8]
	ldp	q10, q11, [\state, #16 * 10]
	ldp	q12, q13, [\state, #16 * 12]
	ldp	q14, q15, [\state, #16 * 14]
	ldp	q16, q17, [\state, #16 * 16]
	ldp	q18, q19, [\state, #16 * 18]
	ldp	q20, q21, [\state, #16 * 20]
	ldp	q22, q23, [\state, #16 * 22]
	ldp	q24, q25, [\state, #16 * 24]
	ldp	q26, q27, [\state, #16 * 26]
	ldp	q28, q29, [\state, #16 * 28]
	ldp	q30, q31, [\state, #16 * 30]!
	ldr	w\tmpnr, [\state, #16 * 2]
	msr	fpsr, x\tmpnr
	ldr	w\tmpnr, [\state, #16 * 2 + 4]
	fpsimd_restore_fpcr x\tmpnr, \state
.endm


/*
 * Save the FP registers.
 *
 * x0 - pointer to struct fpsimd_state
 */
ENTRY(fpsimd_save_state)
	fpsimd_save x0, 8
	ret
ENDPROC(fpsimd_save_state)

/*
 * Load the FP registers.
 *
 * x0 - pointer to struct fpsimd_state
 */
ENTRY(fpsimd_restore_state)
	fpsimd_restore x0, 8
	ret
ENDPROC(fpsimd_restore_state)
#endif /* CONFIG_FLOAT_POINT) */
