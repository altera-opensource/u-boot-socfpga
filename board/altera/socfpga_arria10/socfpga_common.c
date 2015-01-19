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

/* EMAC related setup and only supported in U-Boot */
#if !defined(CONFIG_SOCFPGA_VIRTUAL_TARGET) && \
!defined(CONFIG_SPL_BUILD) && defined(CONFIG_DESIGNWARE_ETH)

#if 0
/*
 * DesignWare Ethernet initialization
 * This function overrides the __weak  version in the driver proper.
 * Our Micrel Phy needs slightly non-conventional setup
 */
int designware_board_phy_init(struct eth_device *dev, int phy_addr,
		int (*mii_write)(struct eth_device *, u8, u8, u16),
		int (*dw_reset_phy)(struct eth_device *))
{
	struct dw_eth_dev *priv = dev->priv;
	struct phy_device *phydev;
	struct mii_dev *bus;

	if ((*dw_reset_phy)(dev) < 0)
		return -1;

	bus = mdio_get_current_dev();
	phydev = phy_connect(bus, phy_addr, dev,
		priv->interface);

	/* Micrel PHY is connected to EMAC1 */
	if (strcasecmp(phydev->drv->name, "Micrel ksz9021") == 0 &&
		((phydev->drv->uid & phydev->drv->mask) ==
		(phydev->phy_id & phydev->drv->mask))) {

		printf("Configuring PHY skew timing for %s\n",
			phydev->drv->name);

		/* min rx data delay */
		if (ksz9021_phy_extended_write(phydev,
			MII_KSZ9021_EXT_RGMII_RX_DATA_SKEW,
			getenv_ulong(CONFIG_KSZ9021_DATA_SKEW_ENV, 16,
				CONFIG_KSZ9021_DATA_SKEW_VAL)) < 0)
			return -1;
		/* min tx data delay */
		if (ksz9021_phy_extended_write(phydev,
			MII_KSZ9021_EXT_RGMII_TX_DATA_SKEW,
			getenv_ulong(CONFIG_KSZ9021_DATA_SKEW_ENV, 16,
				CONFIG_KSZ9021_DATA_SKEW_VAL)) < 0)
			return -1;
		/* max rx/tx clock delay, min rx/tx control */
		if (ksz9021_phy_extended_write(phydev,
			MII_KSZ9021_EXT_RGMII_CLOCK_SKEW,
			getenv_ulong(CONFIG_KSZ9021_CLK_SKEW_ENV, 16,
				CONFIG_KSZ9021_CLK_SKEW_VAL)) < 0)
			return -1;

		if (phydev->drv->config)
			phydev->drv->config(phydev);
	}
	return 0;
}
#endif
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
			error("bad emacbase %x\n", emacbase);
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
			error("bad phymode %x\n", phymode);
			hang();
			break;
	}
	return val;
}

int board_phy_config(struct phy_device *phydev)
{
	u32 ctl, rx, tx, clk;
	ctl = ksz9031_phy_extended_read(phydev, 2,
				   MII_KSZ9031_EXT_RGMII_CTRL_SIG_SKEW,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);

	rx = ksz9031_phy_extended_read(phydev, 2,
				   MII_KSZ9031_EXT_RGMII_RX_DATA_SKEW,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);

        tx = ksz9031_phy_extended_read(phydev, 2,
				   MII_KSZ9031_EXT_RGMII_TX_DATA_SKEW,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	ksz9031_phy_extended_write(phydev, 2,
				   MII_KSZ9031_EXT_RGMII_CLOCK_SKEW,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, 0x3ff);

	clk = ksz9031_phy_extended_read(phydev, 2,
				   MII_KSZ9031_EXT_RGMII_CLOCK_SKEW,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);

	printf("%s: %x %x %x %x\n", __func__, ctl, rx, tx, clk);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}
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

