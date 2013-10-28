/*
 *  Copyright Altera Corporation (C) 2012-2013. All rights reserved
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms and conditions of the GNU General Public License,
 *  version 2, as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/interrupts.h>
#include <asm/arch/sdram.h>
#include <asm/arch/dwmmc.h>
#include <mmc.h>
#include <dwmmc.h>
#include <altera.h>
#include <fpga.h>
#ifndef CONFIG_SPL_BUILD
#include <netdev.h>
#include <phy.h>
#endif


DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_reset_manager *reset_manager_base =
		(void *)SOCFPGA_RSTMGR_ADDRESS;

/*
 * FPGA programming support for SoC FPGA Cyclone V
 */
/* currently only single FPGA device avaiable on dev kit */
Altera_desc altera_fpga[CONFIG_FPGA_COUNT] = {
	{Altera_SoCFPGA, /* family */
	fast_passive_parallel, /* interface type */
	-1,		/* no limitation as
			additional data will be ignored */
	NULL,		/* no device function table */
	NULL,		/* base interface address specified in driver */
	0}		/* no cookie implementation */
};

/* add device descriptor to FPGA device table */
void socfpga_fpga_add(void)
{
	int i;
	fpga_init();
	for (i = 0; i < CONFIG_FPGA_COUNT; i++)
		fpga_add(fpga_altera, &altera_fpga[i]);
}

/*
 * Print CPU information
 */
int print_cpuinfo(void)
{
	puts("CPU   : Altera SOCFPGA Platform\n");
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
	/* adress of boot parameters for ATAG (if ATAG is used) */
	gd->bd->bi_boot_params = 0x00000100;
	return 0;
}

int misc_init_r(void)
{
	/* add device descriptor to FPGA device table */
	socfpga_fpga_add();
	return 0;
}

#if defined(CONFIG_SYS_CONSOLE_IS_IN_ENV) && \
defined(CONFIG_SYS_CONSOLE_OVERWRITE_ROUTINE)
int overwrite_console(void)
{
	return 0;
}
#endif


/* We know all the init functions have been run now */
int board_eth_init(bd_t *bis)
{
#if !defined(CONFIG_SOCFPGA_VIRTUAL_TARGET) && \
!defined(CONFIG_SPL_BUILD)

	/* Initialize EMAC */

	/*
	 * Putting the EMAC controller to reset when configuring the PHY
	 * interface select at System Manager
	*/
	emac0_reset_enable(1);
	emac1_reset_enable(1);

	/* Clearing emac0 PHY interface select to 0 */
	clrbits_le32(CONFIG_SYSMGR_EMAC_CTRL,
		(SYSMGR_EMACGRP_CTRL_PHYSEL_MASK <<
#if (CONFIG_EMAC_BASE == CONFIG_EMAC0_BASE)
		SYSMGR_EMACGRP_CTRL_PHYSEL0_LSB));
#elif (CONFIG_EMAC_BASE == CONFIG_EMAC1_BASE)
		SYSMGR_EMACGRP_CTRL_PHYSEL1_LSB));
#endif

	/* configure to PHY interface select choosed */
	setbits_le32(CONFIG_SYSMGR_EMAC_CTRL,
#if (CONFIG_PHY_INTERFACE_MODE == SOCFPGA_PHYSEL_ENUM_GMII)
		(SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII <<
#elif (CONFIG_PHY_INTERFACE_MODE == SOCFPGA_PHYSEL_ENUM_MII)
		(SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII <<
#elif (CONFIG_PHY_INTERFACE_MODE == SOCFPGA_PHYSEL_ENUM_RGMII)
		(SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII <<
#elif (CONFIG_PHY_INTERFACE_MODE == SOCFPGA_PHYSEL_ENUM_RMII)
		(SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII <<
#endif
#if (CONFIG_EMAC_BASE == CONFIG_EMAC0_BASE)
		SYSMGR_EMACGRP_CTRL_PHYSEL0_LSB));
	/* Release the EMAC controller from reset */
	emac0_reset_enable(0);
#elif (CONFIG_EMAC_BASE == CONFIG_EMAC1_BASE)
		SYSMGR_EMACGRP_CTRL_PHYSEL1_LSB));
	/* Release the EMAC controller from reset */
	emac1_reset_enable(0);
#endif

	/* initialize and register the emac */
	int rval = designware_initialize(0, CONFIG_EMAC_BASE,
		CONFIG_EPHY_PHY_ADDR,
#if (CONFIG_PHY_INTERFACE_MODE == SOCFPGA_PHYSEL_ENUM_GMII)
		PHY_INTERFACE_MODE_GMII);
#elif (CONFIG_PHY_INTERFACE_MODE == SOCFPGA_PHYSEL_ENUM_MII)
		PHY_INTERFACE_MODE_MII);
#elif (CONFIG_PHY_INTERFACE_MODE == SOCFPGA_PHYSEL_ENUM_RGMII)
		PHY_INTERFACE_MODE_RGMII);
#elif (CONFIG_PHY_INTERFACE_MODE == SOCFPGA_PHYSEL_ENUM_RMII)
		PHY_INTERFACE_MODE_RMII);
#endif
	debug("board_eth_init %d\n", rval);
	return rval;
#else
	return 0;
#endif
}

/*
 * Initializes MMC controllers.
 * to override, implement board_mmc_init()
 */
int cpu_mmc_init(bd_t *bis)
{
#ifdef CONFIG_DWMMC
	return altera_dwmmc_init(CONFIG_SDMMC_BASE,
		CONFIG_DWMMC_BUS_WIDTH, 0);
#else
	return 0;
#endif
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	char buf[32];

	/* create environment for bridges and handoff */

	/* hps peripheral controller to fgpa */
	setenv_addr("fpgaintf", (void *)SYSMGR_FPGAINTF_MODULE);
	sprintf(buf, "0x%08x", readl(ISWGRP_HANDOFF_FPGAINTF));
	setenv("fpgaintf_handoff", buf);

	/* fpga2sdram port */
	setenv_addr("fpga2sdram", (void *)(SOCFPGA_SDR_ADDRESS +
		SDR_CTRLGRP_FPGAPORTRST_ADDRESS));
	sprintf(buf, "0x%08x", readl(ISWGRP_HANDOFF_FPGA2SDR));
	setenv("fpga2sdram_handoff", buf);
	setenv_addr("fpga2sdram_apply", (void *)sdram_applycfg_uboot);

	/* axi bridges (hps2fpga, lwhps2fpga and fpga2hps) */
	setenv_addr("axibridge", (void *)&reset_manager_base->brg_mod_reset);
	sprintf(buf, "0x%08x", readl(ISWGRP_HANDOFF_AXIBRIDGE));
	setenv("axibridge_handoff", buf);

	/* l3 remap register */
	setenv_addr("l3remap", (void *)SOCFPGA_L3REGS_ADDRESS);
	sprintf(buf, "0x%08x", readl(ISWGRP_HANDOFF_L3REMAP));
	setenv("l3remap_handoff", buf);


	/* add signle command to enable all bridges based on handoff */
	setenv("bridge_enable_handoff",
		"mw $fpgaintf ${fpgaintf_handoff}; "
		"mw $fpga2sdram ${fpga2sdram_handoff}; "
		"go $fpga2sdram_apply; "
		"mw $axibridge ${axibridge_handoff}; "
		"mw $l3remap ${l3remap_handoff}; "
		"echo fpgaintf; "
		"md $fpgaintf 1; "
		"echo fpga2sdram; "
		"md $fpga2sdram 1; "
		"echo axibridge; "
		"md $axibridge 1");

	/* add signle command to disable all bridges */
	setenv("bridge_disable",
		"mw $fpgaintf 0; "
		"mw $fpga2sdram 0; "
		"go $fpga2sdram_apply; "
		"mw $axibridge 0; "
		"mw $l3remap 0x1; "
		"echo fpgaintf; "
		"md $fpgaintf 1; "
		"echo fpga2sdram; "
		"md $fpga2sdram 1; "
		"echo axibridge; "
		"md $axibridge 1");

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

