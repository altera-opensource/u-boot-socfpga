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

MEMORY { .sram : ORIGIN = CONFIG_SPL_TEXT_BASE,\
		LENGTH = CONFIG_SPL_MAX_SIZE }
#ifdef CONFIG_SPL_FAT_SUPPORT
/* Allocate the malloc and bss (when FAT enabled) to last 1MB of SDRAM */
MEMORY { .sdram : ORIGIN = (PHYS_SDRAM_1_SIZE - 0x100000 \
		+ CONFIG_SYS_SDRAM_BASE), LENGTH = 0x100000 }
#endif

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
	.text	:
	{
		arch/arm/cpu/armv7/start.o	(.text)
		*(.text*)
	} >.sram

	. = ALIGN(4);
	.rodata : { *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*))) } >.sram

	. = ALIGN(4);
	.data : { *(SORT_BY_ALIGNMENT(.data*)) } >.sram

	__ecc_padding_start = .;
	. = ALIGN(4);
	__image_copy_end = .;
	_end = .;

#ifdef CONFIG_SPL_FAT_SUPPORT
	.bss_fat : {
#if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN == 1)
		. = ALIGN(CONFIG_SPL_SDRAM_ECC_PADDING);
#endif
		__bss_fat_start = .;
		fs/fat/libfat.o (.bss*)
#if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN == 1)
		. = ALIGN(CONFIG_SPL_SDRAM_ECC_PADDING);
#endif
		__bss_fat_end = .;
	} >.sdram
#endif

	.bss : {
		. = ALIGN(4);
		__bss_start = .;
		*(.bss*)
		. = ALIGN(4);
		__bss_end__ = .;
	} >.sram

#ifdef CONFIG_SPL_FAT_SUPPORT
	__malloc_start = .;
	__malloc_end = .;
	.malloc :
	{
#if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN == 1)
		. = ALIGN(CONFIG_SPL_SDRAM_ECC_PADDING);
#endif
		__malloc_fat_start = .;
		. = . + CONFIG_SPL_MALLOC_SIZE;
#if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN == 1)
		. = ALIGN(CONFIG_SPL_SDRAM_ECC_PADDING);
#endif
		__malloc_fat_end = .;
	} >.sdram
#else
	.malloc :
	{
		. = ALIGN(8);
		__malloc_start = .;
		. = . + CONFIG_SPL_MALLOC_SIZE;
		__malloc_end = .;
	} >.sram
#endif

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

#ifdef CONFIG_SPL_FAT_SUPPORT
	.sdram_stack :
	{
#if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN == 1)
		. = ALIGN(CONFIG_SPL_SDRAM_ECC_PADDING);
#endif
		__sdram_stack_end = .;
		. = . + CONFIG_SPL_SDRAM_STACK_SIZE;
#if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN == 1)
		. = ALIGN(CONFIG_SPL_SDRAM_ECC_PADDING);
#endif
		__sdram_stack_start = .;
	} >.sdram
#endif
}
