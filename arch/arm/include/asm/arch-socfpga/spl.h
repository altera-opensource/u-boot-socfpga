/*
 *  Copyright (C) 2012 Pavel Machek <pavel@denx.de>
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

#ifndef _SOCFPGA_SPL_H_
#define _SOCFPGA_SPL_H_

/* Symbols from linker script */
extern char __malloc_start, __malloc_end, __stack_start;
extern char __ecc_padding_start, __ecc_padding_end;
#ifdef CONFIG_SPL_FAT_SUPPORT
extern char __bss_fat_start, __bss_fat_end;
extern char __malloc_fat_start, __malloc_fat_end;
extern char __sdram_stack_start, __sdram_stack_end;
#endif

#ifdef CONFIG_USE_IRQ
extern char __irq_stack_start;
#endif /* CONFIG_USE_IRQ */

#if (CONFIG_PRELOADER_WARMRST_SKIP_CFGIO == 1) || \
(CONFIG_HPS_RESET_WARMRST_HANDSHAKE_SDRAM == 1)
extern u32 rst_mgr_status __attribute__ ((section(".data")));
#endif

#ifdef CONFIG_SPL_FAT_SUPPORT
void relocate_stack_to_sdram(void);
#endif

#define BOOT_DEVICE_RAM 1
#define BOOT_DEVICE_SPI 2
#define BOOT_DEVICE_MMC1 3
#define BOOT_DEVICE_MMC2 4
#define BOOT_DEVICE_MMC2_2 5
#define BOOT_DEVICE_NAND 6

#endif
