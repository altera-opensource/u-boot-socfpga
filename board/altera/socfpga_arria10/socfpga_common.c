/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <watchdog.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/reset_manager.h>
#include <phy.h>
#include <micrel.h>
#include <miiphy.h>
#include <netdev.h>
#include "../../../drivers/net/designware.h"

DECLARE_GLOBAL_DATA_PTR;

/*
 * Initialization function which happen at early stage of c code
 */
int board_early_init_f(void)
{
	WATCHDOG_RESET();
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

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	return 0;
}
#endif

ulong
socfpga_get_emac_control(unsigned long emacbase)
{
	ulong base = 0;
	switch (emacbase) {
		case SOCFPGA_EMAC0_ADDRESS:
			base = CONFIG_SYSMGR_EMAC0_CTRL;
			break;
		case SOCFPGA_EMAC1_ADDRESS:
			base = CONFIG_SYSMGR_EMAC1_CTRL;
			break;
		case SOCFPGA_EMAC2_ADDRESS:
			base = CONFIG_SYSMGR_EMAC2_CTRL;
			break;
		default:
			error("bad emacbase %lx\n", emacbase);
			hang();
			break;
	}
	return base;
}

ulong
socfpga_get_phy_mode(ulong phymode)
{
	ulong val;
	switch (phymode) {
		case PHY_INTERFACE_MODE_GMII:
			val = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII;
			break;
		case PHY_INTERFACE_MODE_MII:
			val = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII;
			break;
		case PHY_INTERFACE_MODE_RGMII:
			val = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII;
			break;
		case PHY_INTERFACE_MODE_RMII:
			val = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII;
			break;
		default:
			error("bad phymode %lx\n", phymode);
			hang();
			break;
	}
	return val;
}

int is_ksz9031(struct phy_device *phydev)
{
	unsigned short phyid1;
	unsigned short phyid2;

	phyid1 = phy_read(phydev, MDIO_DEVAD_NONE, MII_PHYSID1);
	phyid2 = phy_read(phydev, MDIO_DEVAD_NONE, MII_PHYSID2);

	phyid2 = phyid2 & MICREL_KSZ9031_PHYID2_REVISION_MASK;

	debug("phyid1 %04x, phyid2 %04x\n", phyid1, phyid2);

	if ((phyid1 == MICREL_KSZ9031_PHYID1) &&
	    (phyid2 == MICREL_KSZ9031_PHYID2))
		return 1;
	return 0;
}



int board_phy_config(struct phy_device *phydev)
{
	int reg;
	int devad = MDIO_DEVAD_NONE;

	reg = phy_read(phydev, devad, MII_BMCR);
	if (reg < 0) {
		debug("PHY status read failed\n");
		return -1;
	}

	if (reg & BMCR_PDOWN) {
		reg &= ~BMCR_PDOWN;
		if (phy_write(phydev, devad, MII_BMCR, reg) < 0) {
			debug("PHY power up failed\n");
			return -1;
		}
		udelay(1500);
	}


	if (is_ksz9031(phydev)) {
		unsigned short reg4;
		unsigned short reg5;
		unsigned short reg6;
		unsigned short reg8;

		reg4 = getenv_ulong("ksz9031-rgmii-ctrl-skew", 16, 0x77);
		reg5 = getenv_ulong("ksz9031-rgmii-rxd-skew", 16, 0x7777);
		reg6 = getenv_ulong("ksz9031-rgmii-txd-skew", 16, 0x7777);
		reg8 = getenv_ulong("ksz9031-rgmii-clock-skew", 16, 0x1ef);

		ksz9031_phy_extended_write(phydev, 2, 4,
					   MII_KSZ9031_MOD_DATA_NO_POST_INC,
					   reg4);
		ksz9031_phy_extended_write(phydev, 2, 5,
					   MII_KSZ9031_MOD_DATA_NO_POST_INC,
					   reg5);
		ksz9031_phy_extended_write(phydev, 2, 6,
					   MII_KSZ9031_MOD_DATA_NO_POST_INC,
					   reg6);
		ksz9031_phy_extended_write(phydev, 2, 8,
					   MII_KSZ9031_MOD_DATA_NO_POST_INC,
					   reg8);

		ksz9031_phy_extended_write(phydev, 0,
			MII_KSZ9031RN_FLP_BURST_TX_HI,
			MII_KSZ9031_MOD_DATA_NO_POST_INC, 0x0006);

		ksz9031_phy_extended_write(phydev, 0,
			MII_KSZ9031RN_FLP_BURST_TX_LO,
			MII_KSZ9031_MOD_DATA_NO_POST_INC, 0x1A80);
	}
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

#ifdef CONFIG_DESIGNWARE_ETH
/* We know all the init functions have been run now */
int board_eth_init(bd_t *bis)
{
	ulong emacctrlreg;
	ulong reg32;

	emacctrlreg = socfpga_get_emac_control(CONFIG_EMAC_BASE);

	/* Put the emac we're using into reset. 
	 * This is required before configuring the PHY interface
	 */
	emac_manage_reset(CONFIG_EMAC_BASE, 1);

	reg32 = readl(emacctrlreg);
	reg32 &= ~SYSMGR_EMACGRP_CTRL_PHYSEL_MASK;

	reg32 |= socfpga_get_phy_mode(CONFIG_PHY_INTERFACE_MODE);

	writel(reg32, emacctrlreg);

	/* Now that the PHY interface is configured, release
	 * the EMAC from reset. Delay a little bit afterwards
	 * just to make sure reset is completed before first access
	 * to EMAC CSRs. 
	 */
	emac_manage_reset(CONFIG_EMAC_BASE, 0);

	/* initialize and register the emac */
	return designware_initialize(CONFIG_EMAC_BASE,
					CONFIG_PHY_INTERFACE_MODE);
}
#endif

