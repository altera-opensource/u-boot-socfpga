/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


 MEMORY { .fpga : ORIGIN = CONFIG_SPL_TEXT_BASE,\
		LENGTH = CONFIG_FPGA_MAX_SIZE }
 MEMORY { .sram : ORIGIN = CONFIG_FPGA_DATA_BASE,\
		LENGTH = CONFIG_FPGA_DATA_MAX_SIZE }

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
	.text	:
	{
		arch/arm/cpu/armv7/start.o	(.text)
		*(.text*)
	} >.fpga

	. = ALIGN(4);
	.rodata : { *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*))) } >.fpga

	. = ALIGN(4);
	__data_start = .;
	.data : AT (__data_start) { *(SORT_BY_ALIGNMENT(.data*)) } >.sram

	. = ALIGN(4);
	__image_copy_end = .;
	_end = .;
	__ecc_padding_start = .;

	.bss : {
		. = ALIGN(4);
		__bss_start = .;
		__ecc_padding_start = .;
		*(.bss*)
		. = ALIGN(4);
		__bss_end__ = .;
		__ecc_padding_end = .;
	} >.sram

	. = ALIGN(8);
	.malloc :
	{
		__malloc_start = .;
		. = . + CONFIG_SPL_MALLOC_SIZE;
		__malloc_end = .;
	} >.sram

	.stack :
	{
		. = . + CONFIG_SPL_STACK_SIZE;
		. = ALIGN(8);
		__stack_start = .;
#ifndef CONFIG_USE_IRQ
		. = . + 0x8;
		__ecc_padding_end = .;
#endif
	} >.sram

#ifdef CONFIG_USE_IRQ
	.spl_irq_stack :
	{
		. = . + CONFIG_STACKSIZE_IRQ;
		. = ALIGN(8);
		__irq_stack_start = .;
		. = . + 0x8;
		__ecc_padding_end = .;
	} >.sram
#endif
}
