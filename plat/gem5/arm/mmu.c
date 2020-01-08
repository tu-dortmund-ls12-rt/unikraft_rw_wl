#include <arm/arm64/mmu.h>
#include <stdio.h>
#include <stdint.h>
#include <uk/arch/limits.h>
#include <gem5-arm/mm.h>

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

enum plat_mmu_memory_permissions
plat_mmu_get_access_permissions(unsigned long address)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

	// Load the permissions
	unsigned long target_permissions =
	    l3_table[(address - vm_offset) >> 12] & 0xC0;
	return target_permissions >> 6;
}

void plat_mmu_set_access_permissions(
    unsigned long address, enum plat_mmu_memory_permissions permissions)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

	// Load the permissions
	unsigned long target_page = l3_table[(address - vm_offset) >> 12];
	target_page &= ~(0xC0);
	target_page |= ((permissions << 6) & 0xC0);
	l3_table[(address - vm_offset) >> 12] = target_page;
	plat_mmu_flush_tlb();
}

unsigned long plat_mmu_get_pm_mapping(unsigned long address)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

	// Load the address
	unsigned long target_pm_page =
	    l3_table[(address - vm_offset) >> 12] & 0xFFFFFFFFF000;
	return target_pm_page;
}
void plat_mmu_set_pm_mapping(unsigned long address, unsigned long pm_map)
{
	// Determine L3 Table begin
	unsigned long *l3_table =
	    (unsigned long *)(((unsigned long)&_end) + L3_TABLE_OFFSET);

	// Determine the virtual address offset
	unsigned long vm_offset = (unsigned long)(&_start_bin);

	// Load the address
	unsigned long target_page = l3_table[(address - vm_offset) >> 12];
	// Modify the address
	target_page &= ~0xFFFFFFFFF000;
	target_page |= (pm_map & 0xFFFFFFFFF000);
	l3_table[(address - vm_offset) >> 12] = target_page;
	plat_mmu_flush_tlb();
}