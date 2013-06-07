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
#include <asm/arch/system_manager.h>
#include <asm/io.h>

#include <netdev.h>
#include <mmc.h>
#include <linux/dw_mmc.h>
#include <asm/arch/interrupts.h>
#include <asm/arch/sdram.h>
#include <asm/arch/sdmmc.h>
#include <phy.h>
#include <micrel.h>
#include <miiphy.h>
#include <../drivers/net/designware.h>

#include <altera.h>
#include <fpga.h>


DECLARE_GLOBAL_DATA_PTR;

/*
 * Declaration for DW MMC structure
 */
#ifdef CONFIG_DW_MMC
struct dw_host socfpga_dw_mmc_host = {
	.need_init_cmd = 1,
	.clock_in = CONFIG_HPS_CLK_SDMMC_HZ / 4,
	.reg = (struct dw_registers *)CONFIG_SDMMC_BASE,
#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
	.set_timing = NULL,
	.use_hold_reg = NULL,
#else
	.set_timing = sdmmc_set_clk_timing,
	.use_hold_reg = sdmmc_use_hold_reg,
#endif
};
#endif

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
	/* add device descriptor to FPGA device table */
	socfpga_fpga_add();
	return 0;
}

#if defined(CONFIG_SYS_CONSOLE_IS_IN_ENV) && defined(CONFIG_SYS_CONSOLE_OVERWRITE_ROUTINE)
int overwrite_console(void)
{
	return 0;
}
#endif

/* EMAC related setup */
#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET

#define MICREL_KSZ9021_EXTREG_CTRL 11
#define MICREL_KSZ9021_EXTREG_DATA_WRITE 12


/*
 * Write the extended registers in the PHY
 */
static int eth_emdio_write(struct eth_device *dev, u8 addr, u16 reg, u16 val,
		int (*mii_write)(struct eth_device *, u8, u8, u16))

{
	int ret = (*mii_write)(dev, addr,
		MICREL_KSZ9021_EXTREG_CTRL, 0x8000|reg);

	if (0 != ret) {
		printf("eth_emdio_read write0 failed %d\n", ret);
		return ret;
	}
	ret = (*mii_write)(dev, addr, MICREL_KSZ9021_EXTREG_DATA_WRITE, val);
	if (0 != ret) {
		printf("eth_emdio_read write1 failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int eth_set_ksz9021_skew(struct eth_device *dev, int phy_addr,
		int (*mii_write)(struct eth_device *, u8, u8, u16))
{
	/*  RXC PAD Skew andTXC PAD Skew */
	if (eth_emdio_write(dev, phy_addr,
		MII_KSZ9021_EXT_RGMII_CLOCK_SKEW,
		getenv_ulong(CONFIG_KSZ9021_CLK_SKEW_ENV, 16,
			CONFIG_KSZ9021_CLK_SKEW_VAL),
		mii_write) < 0)
		return -1;

	/* set no PAD skew for data */
	if (eth_emdio_write(dev, phy_addr,
		MII_KSZ9021_EXT_RGMII_RX_DATA_SKEW,
		getenv_ulong(CONFIG_KSZ9021_DATA_SKEW_ENV, 16,
			CONFIG_KSZ9021_DATA_SKEW_VAL),
		mii_write) < 0)
		return -1;

	return 0;
}

/*
 * DesignWare Ethernet initialization
 * This function overrides the __weak  version in the driver proper.
 * Our Micrel Phy needs slightly non-conventional setup
 */
int designware_board_phy_init(struct eth_device *dev, int phy_addr,
		int (*mii_write)(struct eth_device *, u8, u8, u16),
		int (*dw_reset_phy)(struct eth_device *))
{
	struct mii_dev *bus;
	unsigned int phyid;
	int ret = 0;

	if ((*dw_reset_phy)(dev) < 0)
		return -1;

	bus = miiphy_get_active_dev(dev->name);
	if (!bus)
		return 1;

	if (get_phy_id(bus, phy_addr, MDIO_DEVAD_NONE, &phyid) != 0) {
		puts("Net:   failed to get phyid\n");
		return -1;
	}
	printf("Net PHY ID: 0x%08x\n", phyid);

	if ((MICREL_PHY_ID_KSZ9021 | MICREL_PHY_50MHZ_CLK) == phyid)
		ret = eth_set_ksz9021_skew(dev, phy_addr, mii_write);

	return ret;
}
#endif

/* We know all the init functions have been run now */
int board_eth_init(bd_t *bis)
{
#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET

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
#elif (CONFIG_EMAC_BASE == CONFIG_EMAC1_BASE)
		SYSMGR_EMACGRP_CTRL_PHYSEL1_LSB));
#endif

	/* Release the EMAC controller from reset */
#if (CONFIG_EMAC_BASE == CONFIG_EMAC0_BASE)
	emac0_reset_enable(0);
#elif (CONFIG_EMAC_BASE == CONFIG_EMAC1_BASE)
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
#ifdef CONFIG_DW_MMC
	return dw_mmc_init(&socfpga_dw_mmc_host);
#else
	return 0;
#endif
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
#if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN == 1)
	/* clear first any pending interrupt */
	sdram_enable_interrupt(0);
	/* create event for tracking SDRAM ECC SBE count */
	setenv_ulong("sdram_ecc_sbe", 0);
	/* register SDRAM ECC handler */
	irq_register(IRQ_ECC_SDRAM,
		irq_handler_ecc_sdram,
		(void *)&irq_cnt_ecc_sdram, 0);
	sdram_enable_interrupt(1);
	puts("SDRAM: ECC Enabled\n");
#endif 	/* CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN */
#endif
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
