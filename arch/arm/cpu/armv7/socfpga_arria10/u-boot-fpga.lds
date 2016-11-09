/*
 *  Copyright (C) 2016 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier: 	GPL-2.0
 */

#include <config.h>

MEMORY { .fpga : ORIGIN = CONFIG_SYS_TEXT_BASE,\
		LENGTH = CONFIG_SYS_INIT_RAM_SIZE }
MEMORY { .sram : ORIGIN = CONFIG_FPGA_DATA_BASE,\
		LENGTH = CONFIG_FPGA_DATA_MAX_SIZE }

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(reset)
SECTIONS
{
	. = 0x00000000;

	. = ALIGN(4);
	.text :
	{
		start = .;
		*(.vectors)
		*(.__image_copy_start)
		CPUDIR/start.o (.text*)
		*(.text*)
	} >.fpga

	. = ALIGN(4);
	.rodata : { *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*))) } >.fpga

	. = ALIGN(4);
	__data_start = .;
	.data : AT (__data_start) {
		__data_reloc_start = .;
		*(.data*)
		. = ALIGN(4);
		__data_reloc_end = .;
	} >.sram

	. = __data_start + (__data_reloc_end - __data_reloc_start);
	. = ALIGN(4);
	.u_boot_list . : {
		KEEP(*(SORT(.u_boot_list*)));
	} >.fpga

	. = ALIGN(4);
	.image_copy_end :
	{
		*(.__image_copy_end)
	} >.fpga

	_end = .;

	/* allocate empty space for DTB as we will cat dtb after bin file */
	. = . + CONFIG_UBOOT_DTB_SIZE;

	. = ALIGN(8);
	.bss_start . : {
		KEEP(*(.__bss_start));
		__bss_base = .;
	} >.fpga

	.bss __bss_base (OVERLAY) : {
		*(.bss*)
		 . = ALIGN(4);
		 __bss_limit = .;
	} >.fpga

	.bss_end __bss_limit (OVERLAY) : {
		KEEP(*(.__bss_end));
	} >.fpga

	/DISCARD/ : { *(.dynsym) }
	/DISCARD/ : { *(.dynstr*) }
	/DISCARD/ : { *(.dynamic*) }
	/DISCARD/ : { *(.plt*) }
	/DISCARD/ : { *(.interp*) }
	/DISCARD/ : { *(.gnu*) }
}

/*
 * This compile time check to ensure all binary files generated doesn't
 * exceed the memory size set by BootROM.  The check includes,
 * .vectors, .text, .data, and room for dtb.
 */
ASSERT((__bss_start - start) < CONFIG_U_BOOT_BINARY_MAX_SIZE, \
"*** [size check error] U-Boot binary image bigger than 200KB");

/*
 * This compile time check to ensure all the memory region used during
 * run time doesn't exceed the total memory space available in OCRAM.
 */
ASSERT(((__bss_end - start) + SIZEOF_MMUPAGETABLE + \
CONFIG_OCRAM_STACK_SIZE + CONFIG_PRE_CON_BUF_SZ + CONFIG_OCRAM_MALLOC_SIZE + \
SIZEOF_GD + CONFIG_STACKSIZE_IRQ + CONFIG_STACKSIZE_FIQ) < \
CONFIG_SYS_INIT_RAM_SIZE, \
"*** [size check error] U-Boot run time consume more than OCRAM size");
