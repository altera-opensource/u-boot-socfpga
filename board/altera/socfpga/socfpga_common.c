/*
 *  Copyright Altera Corporation (C) 2013. All rights reserved
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
#ifndef CONFIG_SPL_BUILD
#include <phy.h>
#include <micrel.h>
#include <miiphy.h>
#include "../../../drivers/net/designware.h"
#endif

DECLARE_GLOBAL_DATA_PTR;

/* EMAC related setup and only supported in U-Boot */
#if !defined(CONFIG_SOCFPGA_VIRTUAL_TARGET) && \
!defined(CONFIG_SPL_BUILD)

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
