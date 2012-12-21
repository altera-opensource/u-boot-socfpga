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

#include <common.h>
#include <asm/arch/reset_manager.h>
#include <asm/io.h>

#include <netdev.h>
#include <mmc.h>
#include <linux/dw_mmc.h>
#include <asm/arch/interrupts.h>
#include <asm/arch/sdram.h>
#include <asm/arch/sdmmc.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Declaration for DW MMC structure
 */
#ifdef CONFIG_DW_MMC
struct dw_host socfpga_dw_mmc_host = {
	.need_init_cmd = 1,
	.clock_in = CONFIG_HPS_CLK_SDMMC_HZ / 4,
	.reg = (struct dw_registers *)CONFIG_SDMMC_BASE,
#ifdef __CONFIG_SOCFPGA_VTDEV5XS1_H_
	.set_timing = NULL,
	.use_hold_reg = NULL,
#else
	.set_timing = sdmmc_set_clk_timing,
	.use_hold_reg = sdmmc_use_hold_reg,
#endif
};
#endif

/*
 * Print CPU information
 */
int print_cpuinfo(void)
{
	puts("CPU   : Altera SOCFPGA Platform\n");
	return 0;
}

/*
 * Print Board information
 */
int checkboard(void)
{
#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
	puts("BOARD : Altera VTDEV5XS1 Virtual Board\n");
#else
	puts("BOARD : Altera SOCFPGA Cyclone 5 Board\n");
#endif
	return 0;
}

/*
 * Initialization function which happen at early stage of c code
 */
int board_early_init_f(void)
{
#ifdef CONFIG_HW_WATCHDOG
	/* disable the watchdog when entering U-Boot */
	watchdog_disable();
#endif
	return 0;
}

/*
 * Miscellaneous platform dependent initialisations
 */
int board_init(void)
{
	/* adress of boot parameters (ATAG or FDT blob) */
	gd->bd->bi_boot_params = 0x00000100;
	return 0;
}

int misc_init_r(void)
{
	return 0;
}

#if defined(CONFIG_SYS_CONSOLE_IS_IN_ENV) && defined(CONFIG_SYS_CONSOLE_OVERWRITE_ROUTINE)
int overwrite_console(void)
{
	return 0;
}
#endif

/*
 * DesignWare Ethernet initialization
 */
/* We know all the init functions have been run now */
int board_eth_init(bd_t *bis)
{
	return 0;
}

/*
 * Initializes MMC controllers.
 * to override, implement board_mmc_init()
 */
int cpu_mmc_init(bd_t *bis)
{
#ifdef CONFIG_DW_MMC
	return dw_mmc_init(&socfpga_dw_mmc_host);
#else
	return 0;
#endif
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	/* create event for tracking ECC count */
	setenv_ulong("ECC_SDRAM", 0);
#ifndef __CONFIG_SOCFPGA_VTDEV5XS1_H_
	setenv_ulong("ECC_SDRAM_SBE", 0);
	setenv_ulong("ECC_SDRAM_DBE", 0);
#endif
	/* register SDRAM ECC handler */
	irq_register(IRQ_ECC_SDRAM,
		irq_handler_ecc_sdram,
		(void *)&irq_cnt_ecc_sdram, 0);
	return 0;
}
#endif

#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	/* Enable D-cache. I-cache is already enabled in start.S */
	dcache_enable();
}
#endif
