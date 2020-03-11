#include <arm/arm64/mmu.h>
#include <stdio.h>
#include <stdint.h>
#include <uk/arch/limits.h>
#include <gem5-arm/mm.h>

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
extern unsigned long uk_so_wl_text_spare_vm_begin;
unsigned long *plat_mmu_sparevm_l3_table;
#endif

// This is the very end of the memory
extern unsigned long _end;
extern unsigned long _start_bin;

void plat_mmu_flush_tlb()
{
	asm volatile("dsb ishst;"
		     "tlbi vmalle1;"
		     "dsb ish;"
		     "isb;");
}

#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
#define PLAT_MMU_STACK_L3_SIZE                                                 \
	((((CONFIG_APPLICATION_STACK_SIZE) >> 12) + 1) * 2)
#define PLAT_MMU_STACK_L2_SIZE ((PLAT_MMU_STACK_L3_SIZE / 512) + 1)

unsigned long plat_mmu_stack_l2_table[PLAT_MMU_STACK_L2_SIZE]
    __attribute((aligned(0x1000)));
unsigned long plat_mmu_stack_l3_table[PLAT_MMU_STACK_L3_SIZE]
    __attribute((aligned(0x1000)));

void plat_mmu_setup_stack_pages()
{
	// Figure out L1 Entry first
	unsigned long *l1_table =
	    (unsigned long *)(((unsigned long)&_end) + L1_TABLE_OFFSET);
	// Write L1 Table entry to L2 (1G)
	l1_table[PLAT_MMU_VSTACK_BASE >> 30] =
	    ((unsigned long)plat_mmu_stack_l2_table) | 0b11;

	// Fill the L2 Table with further 2MB Table entries
	unsigned long number_l2_entries =
	    ((CONFIG_APPLICATION_STACK_SIZE >> 21) * 2) + 1;
	for (unsigned long i = 0; i < number_l2_entries; i++) {
		plat_mmu_stack_l2_table[i] =
		    (((unsigned long)plat_mmu_stack_l3_table) + i * 4096)
		    | 0b11;
	}

	// Fill the L3 Table with 4k block entries
	unsigned long number_l3_entries =
	    (CONFIG_APPLICATION_STACK_SIZE >> 12) * 2;
	extern unsigned long __NVMSYMBOL__APPLICATION_STACK_BEGIN;
	unsigned long real_stack_begin =
	    (unsigned long)(&__NVMSYMBOL__APPLICATION_STACK_BEGIN);
	for (unsigned long i = 0; i < number_l3_entries; i++) {
		plat_mmu_stack_l3_table[i] =
		    (real_stack_begin
		     + (i % (CONFIG_APPLICATION_STACK_SIZE >> 12)) * 4096)
		    | 0b11100100111;
	}

	plat_mmu_flush_tlb();
}
#endif

#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES

extern unsigned long plat_mmu_text_l2_table[];
extern unsigned long plat_mmu_text_l3_table[];

extern unsigned long plat_mmu_text_l2_size;
extern unsigned long plat_mmu_text_l3_size;

extern unsigned long uk_app_text_size;
extern unsigned long uk_app_got_size;

void plat_mmu_setup_text_pages()
{
	// Figure out L1 Entry first
	unsigned long *l1_table =
	    (unsigned long *)(((unsigned long)&_end) + L1_TABLE_OFFSET);
	// Write L1 Table entry to L2 (1G)
	l1_table[PLAT_MMU_VTEXT_BASE >> 30] =
	    ((unsigned long)plat_mmu_text_l2_table) | 0b11;

	unsigned long total_size = (uk_app_text_size * 2 + uk_app_got_size);

	// Fill the L2 Table with further 2MB Table entries
	unsigned long number_l2_entries = (total_size >> 21) + 1;
	for (unsigned long i = 0; i < number_l2_entries; i++) {
		plat_mmu_text_l2_table[i] =
		    (((unsigned long)plat_mmu_text_l3_table) + i * 4096) | 0b11;
	}

	// Fill the L3 Table with 4k block entries
	unsigned long number_l3_text_entries = (uk_app_text_size >> 12) * 2;
	extern unsigned long __NVMSYMBOL__APPLICATION_DATA_BEGIN;
	unsigned long real_text_begin =
	    ((unsigned long)(&__NVMSYMBOL__APPLICATION_DATA_BEGIN)) + 0x1000;
	for (unsigned long i = 0; i < number_l3_text_entries; i++) {
		plat_mmu_text_l3_table[i] =
		    (real_text_begin + (i % (uk_app_text_size >> 12)) * 4096)
		    | 0b11101100111;
		// printf("Mapping text 0x%lx to 0x%lx\n",
		//        &plat_mmu_text_l3_table[i],
		//        plat_mmu_text_l3_table[i]);
	}
	unsigned long number_l3_got_entries = (uk_app_got_size >> 12);
	for (unsigned long i = 0; i < number_l3_got_entries; i++) {
		plat_mmu_text_l3_table[number_l3_text_entries + i] =
		    (real_text_begin + uk_app_text_size + i * 0x1000)
		    | 0b11101100111;
		// printf("Mapping got 0x%lx to 0x%lx\n",
		//        &plat_mmu_text_l3_table[number_l3_text_entries + i],
		//        plat_mmu_text_l3_table[number_l3_text_entries + i]);
	}

	plat_mmu_flush_tlb();
}
#endif

#ifdef CONFIG_MAP_SPARE_VM_SPACE

extern unsigned long plat_mmu_sparevm_l1_table[];

void plat_mmu_setup_sparevm_pages()
{
	// Figure out L0 Entries
	unsigned long *l0_table =
	    (unsigned long *)(((unsigned long)&_end) + L0_TABLE_OFFSET);

	unsigned long findex = CONFIG_SPARE_VM_BASE >> 39;

	// Populate L0 with specific L1 table
	for (unsigned long start = CONFIG_SPARE_VM_BASE;
	     start < (0x1000000000000UL); start += 0x8000000000UL) {
		unsigned long index = start >> 39;
		l0_table[index] =
		    0b11
		    | (((unsigned long)(&plat_mmu_sparevm_l1_table))
		       + 4096 * (index - findex));
	}
}
#endif

enum plat_mmu_memory_permissions
plat_mmu_get_access_permissions(unsigned long address)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES
	if (address >= PLAT_MMU_VTEXT_BASE) {
		l3_table = plat_mmu_text_l3_table;
		vm_offset = PLAT_MMU_VTEXT_BASE;
	}
#endif
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		l3_table = plat_mmu_stack_l3_table;
		vm_offset = PLAT_MMU_VSTACK_BASE;
	}
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
	if (address >= uk_so_wl_text_spare_vm_begin) {
		l3_table = plat_mmu_sparevm_l3_table;
		vm_offset = uk_so_wl_text_spare_vm_begin & ~(0xFFF);
	}
#endif

	// Load the permissions
	unsigned long target_permissions =
	    l3_table[(address - vm_offset) >> 12] & 0xC0;
	return target_permissions >> 6;
}

void plat_mmu_set_access_permissions(unsigned long address,
				     unsigned long permissions,
				     int unprivieged_execution)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES
	if (address >= PLAT_MMU_VTEXT_BASE) {
		l3_table = plat_mmu_text_l3_table;
		vm_offset = PLAT_MMU_VTEXT_BASE;
	}
#endif
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		l3_table = plat_mmu_stack_l3_table;
		vm_offset = PLAT_MMU_VSTACK_BASE;
	}
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
	if (address >= uk_so_wl_text_spare_vm_begin) {
		l3_table = plat_mmu_sparevm_l3_table;
		vm_offset = uk_so_wl_text_spare_vm_begin & ~(0xFFF);
	}
#endif

	// Load the permissions
	unsigned long target_page = l3_table[(address - vm_offset) >> 12];
	target_page &= ~(0xC0);
	target_page |= ((permissions << 6) & 0xC0);
	if (unprivieged_execution) {
		target_page &= ~(0b1 << 54);
	} else {
		target_page |= (0b1 << 54);
	}
	l3_table[(address - vm_offset) >> 12] = target_page;

#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES
	if (address >= PLAT_MMU_VTEXT_BASE
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	    && address < PLAT_MMU_VSTACK_BASE
#endif
	) {
		if ((address - vm_offset) < uk_app_text_size) {
			address += uk_app_text_size;
		} else {
			address -= uk_app_text_size;
		}
		target_page = l3_table[(address - vm_offset) >> 12];
		target_page &= ~(0xC0);
		target_page |= ((permissions << 6) & 0xC0);
		if (unprivieged_execution) {
			target_page &= ~(0b1 << 54);
		} else {
			target_page |= (0b1 << 54);
		}
		l3_table[(address - vm_offset) >> 12] = target_page;
	}
#endif
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE)
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
		&&address < CONFIG_SPARE_VM_BASE
#endif
		{
			if ((address - vm_offset)
			    < CONFIG_APPLICATION_STACK_SIZE) {
				address += CONFIG_APPLICATION_STACK_SIZE;
			} else {
				address -= CONFIG_APPLICATION_STACK_SIZE;
			}
			target_page = l3_table[(address - vm_offset) >> 12];
			target_page &= ~(0xC0);
			target_page |= ((permissions << 6) & 0xC0);
			if (unprivieged_execution) {
				target_page &= ~(0b1 << 54);
			} else {
				target_page |= (0b1 << 54);
			}
			l3_table[(address - vm_offset) >> 12] = target_page;
		}
#endif

#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
	extern unsigned long uk_so_wl_text_spare_vm_begin;
	extern unsigned long uk_app_text_size;
	if (address >= uk_so_wl_text_spare_vm_begin) {
		if ((address - vm_offset) < uk_app_text_size) {
			address += uk_app_text_size;
		} else {
			address -= uk_app_text_size;
		}
		target_page = l3_table[(address - vm_offset) >> 12];
		target_page &= ~(0xC0);
		target_page |= ((permissions << 6) & 0xC0);
		if (unprivieged_execution) {
			target_page &= ~(0b1 << 54);
		} else {
			target_page |= (0b1 << 54);
		}
		l3_table[(address - vm_offset) >> 12] = target_page;
	}
#endif

	plat_mmu_flush_tlb();
}

unsigned long plat_mmu_get_pm_mapping(unsigned long address)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES
	if (address >= PLAT_MMU_VTEXT_BASE) {
		l3_table = plat_mmu_text_l3_table;
		vm_offset = PLAT_MMU_VTEXT_BASE;
	}
#endif
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		l3_table = plat_mmu_stack_l3_table;
		vm_offset = PLAT_MMU_VSTACK_BASE;
	}
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
	if (address >= uk_so_wl_text_spare_vm_begin) {
		l3_table = plat_mmu_sparevm_l3_table;
		vm_offset = uk_so_wl_text_spare_vm_begin & ~(0xFFF);
	}
#endif

	// Load the address
	unsigned long target_pm_page =
	    l3_table[(address - vm_offset) >> 12] & 0xFFFFFFFFF000;
	return target_pm_page;
}
void plat_mmu_set_pm_mapping(unsigned long address, unsigned long pm_map)
{
	// printf("Mapping 0x%lx to 0x%lx\n", address, pm_map);
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES
	if (address >= PLAT_MMU_VTEXT_BASE) {
		l3_table = plat_mmu_text_l3_table;
		vm_offset = PLAT_MMU_VTEXT_BASE;
	}
#endif
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE) {
		l3_table = plat_mmu_stack_l3_table;
		vm_offset = PLAT_MMU_VSTACK_BASE;
	}
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
	if (address >= uk_so_wl_text_spare_vm_begin) {
		l3_table = plat_mmu_sparevm_l3_table;
		vm_offset = uk_so_wl_text_spare_vm_begin & ~(0xFFF);
	}
#endif

	// Load the address
	unsigned long target_page = l3_table[(address - vm_offset) >> 12];
	// Modify the address
	target_page &= ~0xFFFFFFFFF000;
	target_page |= (pm_map & 0xFFFFFFFFF000);
	l3_table[(address - vm_offset) >> 12] = target_page;

#ifdef CONFIG_SEPARATE_TEXT_PAGETABLES
	if (address >= PLAT_MMU_VTEXT_BASE
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	    && address < PLAT_MMU_VSTACK_BASE
#endif
	) {
		if ((address - vm_offset) < uk_app_text_size) {
			address += uk_app_text_size;
		} else {
			address -= uk_app_text_size;
		}
		// printf("Is a text page, therefore also mapping 0x%lx\n",
		//    address);
		// Load the address
		target_page = l3_table[(address - vm_offset) >> 12];
		// Modify the address
		target_page &= ~0xFFFFFFFFF000;
		target_page |= (pm_map & 0xFFFFFFFFF000);
		l3_table[(address - vm_offset) >> 12] = target_page;
	}
#endif
#ifdef CONFIG_SEPARATE_STACK_PAGETABLES
	if (address >= PLAT_MMU_VSTACK_BASE
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
	    && address < CONFIG_SPARE_VM_BASE
#endif
	) {
		if ((address - vm_offset) < CONFIG_APPLICATION_STACK_SIZE) {
			address += CONFIG_APPLICATION_STACK_SIZE;
		} else {
			address -= CONFIG_APPLICATION_STACK_SIZE;
		}
		// printf("Is a stack page, therefore also mapping 0x%lx\n",
		//    address);
		// Load the address
		target_page = l3_table[(address - vm_offset) >> 12];
		// Modify the address
		target_page &= ~0xFFFFFFFFF000;
		target_page |= (pm_map & 0xFFFFFFFFF000);
		l3_table[(address - vm_offset) >> 12] = target_page;
	}
#endif
#ifdef CONFIG_SOFTONLYWEARLEVELINGLIB_DO_TEXT_PAGE_CONSITENCY
	extern unsigned long uk_so_wl_text_spare_vm_begin;
	extern unsigned long uk_app_text_size;
	if (address >= uk_so_wl_text_spare_vm_begin) {
		if ((address - vm_offset) < uk_app_text_size) {
			address += uk_app_text_size;
		} else {
			address -= uk_app_text_size;
		}
		// printf("Is a stack page, therefore also mapping 0x%lx\n",
		//    address);
		// Load the address
		target_page = l3_table[(address - vm_offset) >> 12];
		// Modify the address
		target_page &= ~0xFFFFFFFFF000;
		target_page |= (pm_map & 0xFFFFFFFFF000);
		l3_table[(address - vm_offset) >> 12] = target_page;
	}
#endif
	plat_mmu_flush_tlb();
}