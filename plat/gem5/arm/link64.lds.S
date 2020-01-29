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

/*OUTPUT_FORMAT("elf64-littleaarch64")*/
OUTPUT_ARCH(aarch64)
ENTRY(_libgem5plat_entry)

#include <uk/arch/limits.h>
#include <common.lds.h>

/* GEM5 ARK64 RAM base is at  0x80000000, where also the init code has to be*/
#define RAM_BASE_ADDR 0x80000000

/* Reserve first 1MB for DTB */
#define DTB_RESERVED_SIZE 0x100000

/*
 * We use mmu to control each section's attribute, so the boundaries
 * of sections with different memory attributes must be 4KiB alignment.
 * For instance, the dtb section is readonly, but the text section
 * is readonly+exec. So the _text must start at a 4KiB alignment
 * address.
 */
SECTIONS {
	. = RAM_BASE_ADDR;

	_start_bin = .;

	/* Code */
	_text = .;
	.text :
	{
		/*Place the bootcode as the very first thing, as gem5 starts to execute here */
		*(.gem5_arm_bootcode)
		*(.text)
		*(.text.*)
		. = ALIGN(__PAGE_SIZE);
	}

	. = ALIGN(__PAGE_SIZE);
	_etext = .;

	EXCEPTION_SECTIONS

	CTORTAB_SECTION

	INITTAB_SECTION

	/* Read-only data */
	. = ALIGN(__PAGE_SIZE);
	_rodata = .;
	.rodata :
	{
		*(.rodata)
		*(.rodata.*)
	}


	_erodata = .;

	/* Constructor tables (read-only) */
	. = ALIGN(0x8);
	_ctors = .;
	.preinit_array : {
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	}

	. = ALIGN(0x8);
	.init_array : {
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array .ctors))
		PROVIDE_HIDDEN (__init_array_end = .);
	}
	_ectors = .;
	. = ALIGN(__PAGE_SIZE);

	TLS_SECTIONS

	/* Read-write data that is initialized explicitly in code */
	_data = .;
	.data :
	{
		*(.data)
		*(.data.*)
	}
	_edata = .;

	. = ALIGN(__PAGE_SIZE);

	__bss_start = .;
	/* Read-write data that is not initialized explicitly in code */
	.bss :
	{
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(__PAGE_SIZE);
	/*
	 * TODO
	 * We can define the stack and page-table here, and free them once
	 * the boot is done.
	 */
	}

#ifdef CONFIG_SEPARATE_CODE_LINKING
	. = ALIGN(__PAGE_SIZE);
	/* Place the app code here */
	__NVMSYMBOL__APPLICATION_TEXT_BEGIN = .;
	.app_text :
	{
		*(.app_text)
	}
	. = ALIGN(__PAGE_SIZE);
	__NVMSYMBOL__APPLICATION_TEXT_END = .;
	__NVMSYMBOL__APPLICATION_DATA_BEGIN = .;
	.app_data :
	{
		*(.app_data)
	}
	. = ALIGN(__PAGE_SIZE);
	__NVMSYMBOL__APPLICATION_DATA_END = .;
	__NVMSYMBOL__APPLICATION_BSS_BEGIN = .;
	.app_bss :
	{
		*(.app_bss)
	}
	. = ALIGN(__PAGE_SIZE);
	__NVMSYMBOL__APPLICATION_BSS_END = .;
	__NVMSYMBOL__APPLICATION_STACK_BEGIN = .;
    .app_stack : {
        . = ALIGN(0x8);
        . += CONFIG_APPLICATION_STACK_SIZE;
        __appstack_top = .;
    }
__NVMSYMBOL__APPLICATION_STACK_END = .;
#endif


#ifdef CONFIG_SEPARATE_CODE_LINKING
	__NVMSYMBOL__STUFF_BEGIN = .;
#endif

#ifdef CONFIG_SEPARATE_CODE_LINKING
__IRQ_STACK_BEGIN = .;
    .irq_stack : {
        . = ALIGN(0x8);
        . += 0x8000; /* 1MB */
        __irqstack_top = .;
    }
__IRQ_STACK_END = .;
#endif

#ifdef CONFIG_SEPARATE_CODE_LINKING
	. = ALIGN(__PAGE_SIZE);
	/* Place the wear-leveling code here */
	__WL_TEXT_SECTION_BEGIN = .;
	.wl_text :
	{
		*(.wl_text)
	}
	. = ALIGN(__PAGE_SIZE);
	__WL_TEXT_SECTION_END = .;
	__WL_DATA_SECTION_BEGIN = .;
	.wl_data :
	{
		*(.wl_data)
	}
	. = ALIGN(__PAGE_SIZE);
	__WL_DATA_SECTION_END = .;
#endif

	/* Place DTB binary here, can be leter done by gem5 at  0x88000000 statically*/
	. = ALIGN(__PAGE_SIZE);
	_dtb = .;
	__armv8_gem5_v2_1cpu_dtb : {
		*(__armv8_gem5_v2_1cpu_dtb)
	}
	_end_dtb = .;

#ifdef CONFIG_SEPARATE_CODE_LINKING
	__NVMSYMBOL__STUFF_END = .;
#endif

	. = ALIGN(__PAGE_SIZE);

	_end = .;

	/*gem5 needs this to load the image */
	PROVIDE (end = .);

	.comment       0 : { *(.comment) }
	DEBUG_SYMBOLS

	/DISCARD/ : { *(.note.gnu.build-id) }
}
