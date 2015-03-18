/*
 * Micrel PHY drivers
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * author Andy Fleming
 * (C) 2012 NetModule AG, David Andrey, added KSZ9031
 */
#include <config.h>
#include <common.h>
#include <micrel.h>
#include <phy.h>

static struct phy_driver KSZ804_driver = {
	.name = "Micrel KSZ804",
	.uid = 0x221510,
	.mask = 0xfffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};

#ifndef CONFIG_PHY_MICREL_KSZ9021
/*
 * I can't believe Micrel used the exact same part number
 * for the KSZ9021. Shame Micrel, Shame!
 */
static struct phy_driver KS8721_driver = {
	.name = "Micrel KS8721BL",
	.uid = 0x221610,
	.mask = 0xfffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};
#endif


/*
 * KSZ9021 - KSZ9031 common
 */

#define MII_KSZ90xx_PHY_CTL		0x1f
#define MIIM_KSZ90xx_PHYCTL_1000	(1 << 6)
#define MIIM_KSZ90xx_PHYCTL_100		(1 << 5)
#define MIIM_KSZ90xx_PHYCTL_10		(1 << 4)
#define MIIM_KSZ90xx_PHYCTL_DUPLEX	(1 << 3)

static int ksz90xx_startup(struct phy_device *phydev)
{
	unsigned phy_ctl;
	genphy_update_link(phydev);
	phy_ctl = phy_read(phydev, MDIO_DEVAD_NONE, MII_KSZ90xx_PHY_CTL);

	if (phy_ctl & MIIM_KSZ90xx_PHYCTL_DUPLEX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	if (phy_ctl & MIIM_KSZ90xx_PHYCTL_1000)
		phydev->speed = SPEED_1000;
	else if (phy_ctl & MIIM_KSZ90xx_PHYCTL_100)
		phydev->speed = SPEED_100;
	else if (phy_ctl & MIIM_KSZ90xx_PHYCTL_10)
		phydev->speed = SPEED_10;
	return 0;
}

#ifdef CONFIG_PHY_MICREL_KSZ9021
/*
 * KSZ9021
 */

/* PHY Registers */
#define MII_KSZ9021_EXTENDED_CTRL	0x0b
#define MII_KSZ9021_EXTENDED_DATAW	0x0c
#define MII_KSZ9021_EXTENDED_DATAR	0x0d

#define CTRL1000_PREFER_MASTER		(1 << 10)
#define CTRL1000_CONFIG_MASTER		(1 << 11)
#define CTRL1000_MANUAL_CONFIG		(1 << 12)

int ksz9021_phy_extended_write(struct phy_device *phydev, int regnum, u16 val)
{
	/* extended registers */
	phy_write(phydev, MDIO_DEVAD_NONE,
		MII_KSZ9021_EXTENDED_CTRL, regnum | 0x8000);
	return phy_write(phydev, MDIO_DEVAD_NONE,
		MII_KSZ9021_EXTENDED_DATAW, val);
}

int ksz9021_phy_extended_read(struct phy_device *phydev, int regnum)
{
	/* extended registers */
	phy_write(phydev, MDIO_DEVAD_NONE, MII_KSZ9021_EXTENDED_CTRL, regnum);
	return phy_read(phydev, MDIO_DEVAD_NONE, MII_KSZ9021_EXTENDED_DATAR);
}


static int ksz9021_phy_extread(struct phy_device *phydev, int addr, int devaddr,
			      int regnum)
{
	return ksz9021_phy_extended_read(phydev, regnum);
}

static int ksz9021_phy_extwrite(struct phy_device *phydev, int addr,
			       int devaddr, int regnum, u16 val)
{
	return ksz9021_phy_extended_write(phydev, regnum, val);
}

/* Micrel ksz9021 */
static int ksz9021_config(struct phy_device *phydev)
{
	unsigned ctrl1000 = 0;
	const unsigned master = CTRL1000_PREFER_MASTER |
			CTRL1000_CONFIG_MASTER | CTRL1000_MANUAL_CONFIG;
	unsigned features = phydev->drv->features;

	if (getenv("disable_giga"))
		features &= ~(SUPPORTED_1000baseT_Half |
				SUPPORTED_1000baseT_Full);
	/* force master mode for 1000BaseT due to chip errata */
	if (features & SUPPORTED_1000baseT_Half)
		ctrl1000 |= ADVERTISE_1000HALF | master;
	if (features & SUPPORTED_1000baseT_Full)
		ctrl1000 |= ADVERTISE_1000FULL | master;
	phydev->advertising = phydev->supported = features;
	phy_write(phydev, MDIO_DEVAD_NONE, MII_CTRL1000, ctrl1000);
	genphy_config_aneg(phydev);
	genphy_restart_aneg(phydev);
	return 0;
}

static struct phy_driver ksz9021_driver = {
	.name = "Micrel ksz9021",
	.uid  = 0x221610,
	.mask = 0xfffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &ksz9021_config,
	.startup = &ksz90xx_startup,
	.shutdown = &genphy_shutdown,
	.writeext = &ksz9021_phy_extwrite,
	.readext = &ksz9021_phy_extread,
};
#endif

/**
 * KSZ9031
 */
/* PHY Registers */
#define MII_KSZ9031_MMD_ACCES_CTRL	0x0d
#define MII_KSZ9031_MMD_REG_DATA	0x0e
#define KSZ9031_PS_TO_REG		60

/* Extended registers */
#define MII_KSZ9031RN_CONTROL_PAD_SKEW  4
#define MII_KSZ9031RN_RX_DATA_PAD_SKEW  5
#define MII_KSZ9031RN_TX_DATA_PAD_SKEW  6
#define MII_KSZ9031RN_CLK_PAD_SKEW      8

/* Accessors to extended registers*/
int ksz9031_phy_extended_write(struct phy_device *phydev,
			       int devaddr, int regnum, u16 mode, u16 val)
{
	/*select register addr for mmd*/
	phy_write(phydev, MDIO_DEVAD_NONE,
		  MII_KSZ9031_MMD_ACCES_CTRL, devaddr);
	/*select register for mmd*/
	phy_write(phydev, MDIO_DEVAD_NONE,
		  MII_KSZ9031_MMD_REG_DATA, regnum);
	/*setup mode*/
	phy_write(phydev, MDIO_DEVAD_NONE,
		  MII_KSZ9031_MMD_ACCES_CTRL, (mode | devaddr));
	/*write the value*/
	return	phy_write(phydev, MDIO_DEVAD_NONE,
		MII_KSZ9031_MMD_REG_DATA, val);
}

int ksz9031_phy_extended_read(struct phy_device *phydev, int devaddr,
			      int regnum, u16 mode)
{
	phy_write(phydev, MDIO_DEVAD_NONE,
		  MII_KSZ9031_MMD_ACCES_CTRL, devaddr);
	phy_write(phydev, MDIO_DEVAD_NONE,
		  MII_KSZ9031_MMD_REG_DATA, regnum);
	phy_write(phydev, MDIO_DEVAD_NONE,
		  MII_KSZ9031_MMD_ACCES_CTRL, (devaddr | mode));
	return phy_read(phydev, MDIO_DEVAD_NONE, MII_KSZ9031_MMD_REG_DATA);
}

static int ksz9031_phy_extread(struct phy_device *phydev, int addr, int devaddr,
			       int regnum)
{
	return ksz9031_phy_extended_read(phydev, devaddr, regnum,
					 MII_KSZ9031_MOD_DATA_NO_POST_INC);
};

static int ksz9031_phy_extwrite(struct phy_device *phydev, int addr,
				int devaddr, int regnum, u16 val)
{
	return ksz9031_phy_extended_write(phydev, devaddr, regnum,
					 MII_KSZ9031_MOD_DATA_POST_INC_RW, val);
};

void
dump_skew_values(const char *hdr, struct phy_device *phydev)
{
	ulong reg4, reg5, reg6, reg8;
	reg4 = ksz9031_phy_extended_read(phydev, 2, 4,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg5 = ksz9031_phy_extended_read(phydev, 2, 5,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg6 = ksz9031_phy_extended_read(phydev, 2, 6,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg8 = ksz9031_phy_extended_read(phydev, 2, 8,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);

	printf("%s", hdr);
	printf("    reg4, rgmii ctrl skew      %lx\n", reg4);
	printf("    reg5, rgmii rx data skew   %lx\n", reg5);
	printf("    reg6, rgmii tx data skew   %lx\n", reg6);
	printf("    reg8, rgmii clock pad skew %lx\n", reg8);
}

u8
ksz9031_get_rxc_skew(struct phy_device *phydev)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 8,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg &= 0x1f;
	return (u8)reg;
}

u8
ksz9031_get_txc_skew(struct phy_device *phydev)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 8,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg = reg >> 5;
	reg &= 0x1f;
	return (u8)reg;
}

u8
ksz9031_get_txen_skew(struct phy_device *phydev)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 4,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg &= 0xf;
	return (u8)reg;
}

u8
ksz9031_get_rxdv_skew(struct phy_device *phydev)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 4,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg = reg >> 4;
	reg &= 0xf;
	return (u8)reg;
}

u8
ksz9031_get_rxdx_skew(struct phy_device *phydev, int index)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 5,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg = reg >> (index*4);
	reg &= 0xf;
	return (u8)reg;
}

u8
ksz9031_get_txdx_skew(struct phy_device *phydev, int index)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 6,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg = reg >> (index*4);
	reg &= 0xf;
	return (u8)reg;
}


void
ksz9031_set_rxc_skew(struct phy_device *phydev, u8 val)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 8,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg = reg & 0xffe0;
	reg |= (u16) val;

        ksz9031_phy_extended_write(phydev, 2, 8,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, reg);
}

void
ksz9031_set_txc_skew(struct phy_device *phydev, u8 val)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 8,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg = reg & 0xfc1f;
	reg |= ((u16) val) << 5;

        ksz9031_phy_extended_write(phydev, 2, 8,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, reg);
}

void
ksz9031_set_txen_skew(struct phy_device *phydev, u8 val)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 4,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);

	reg = reg & 0xfff0;
	reg |= ((u16) val);
        ksz9031_phy_extended_write(phydev, 2, 4,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, reg);
}

void
ksz9031_set_rxdv_skew(struct phy_device *phydev, u8 val)
{
	u16 reg;
	reg = ksz9031_phy_extended_read(phydev, 2, 4,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg = reg & 0xff0f;
	reg |= ((u16) val) << 4;
        ksz9031_phy_extended_write(phydev, 2, 4,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, reg);
}

void
ksz9031_set_rxdx_skew(struct phy_device *phydev, int index, u8 val)
{
	u16 reg;
	u16 mask;
	mask = ~(0xf << (index * 4));

	reg = ksz9031_phy_extended_read(phydev, 2, 5,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);
	reg = reg & mask;
	reg |= ((u16) val) << (4 * index);

        ksz9031_phy_extended_write(phydev, 2, 5,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, reg);
}

void
ksz9031_set_txdx_skew(struct phy_device *phydev, int index, u8 val)
{
	u16 reg;
	u16 mask;
	mask = ~(0xf << (index * 4));
	reg = ksz9031_phy_extended_read(phydev, 2, 6,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC);

	reg = reg & mask;
	reg |= ((u16) val) << (4 * index);

        ksz9031_phy_extended_write(phydev, 2, 6,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, reg);
}


long ksz9031_range_check(const char *name, long val, long lower, long upper)
{
	long returnval = val;
	if (val < lower) {
		printf("Warning: %s value %ld less than lower bound %ld, clipping to %ld\n",
			name, val, lower, lower);
		returnval = lower;
	}
	if (val > upper) {
		printf("Warning: %s value %ld greater than upper bound %ld, clipping to %ld\n",
			name, val, upper, upper);
		returnval = upper;
	}
	return returnval;
}

#define KSZ9031_DEFAULT_RXC_SKEW_PS	0
#define KSZ9031_DEFAULT_TXC_SKEW_PS	0
#define KSZ9031_DEFAULT_TXEN_SKEW_PS	0
#define KSZ9031_DEFAULT_RXDV_SKEW_PS	0

#define KSZ9031_DEFAULT_RXD_SKEW_PS	0
#define KSZ9031_DEFAULT_TXD_SKEW_PS	0


int ksz9031_config_skews(struct phy_device *phydev)
{
	long rxc_skew_ps;	/* -900ps through 960ps */
	long txc_skew_ps;	/* -900ps through 960ps */
	long txen_skew_ps;	/* -420ps through 480ps */
	long rxdv_skew_ps;	/* -420ps through 480ps */
	long rxdx_skew_ps[4];	/* -420ps through 480ps */
	long txdx_skew_ps[4];	/* -420ps through 480ps */

	u8 rxc_skew_regval;	/* 5 bit range */
	u8 txc_skew_regval;	/* 5 bit range */
	u8 txen_skew_regval;	/* 4 bit range */
	u8 rxdv_skew_regval;	/* 4 bit range */
	u8 rxdx_skew_regval[4]; /* 4 bit range */
	u8 txdx_skew_regval[4]; /* 4 bit range */

	long upper, lower;

	int i;

        char *rx_data_skews[4] = {
                "rxd0-skew-ps", "rxd1-skew-ps",
                "rxd2-skew-ps", "rxd3-skew-ps"
        };
        char *tx_data_skews[4] = {
                "txd0-skew-ps", "txd1-skew-ps",
                "txd2-skew-ps", "txd3-skew-ps"
        };

	/* Get values from the environment */
	rxc_skew_ps = getenv_long("rxc-skew-ps", 10, KSZ9031_DEFAULT_RXC_SKEW_PS);
	txc_skew_ps = getenv_long("txc-skew-ps", 10, KSZ9031_DEFAULT_TXC_SKEW_PS);
	txen_skew_ps = getenv_long("txen-skew-ps", 10, KSZ9031_DEFAULT_TXEN_SKEW_PS);
	rxdv_skew_ps = getenv_long("rxdv-skew-ps", 10, KSZ9031_DEFAULT_RXDV_SKEW_PS);

	for (i=0; i<4; i++) {
		rxdx_skew_ps[i] = getenv_long(rx_data_skews[i], 10, KSZ9031_DEFAULT_RXD_SKEW_PS);
		txdx_skew_ps[i] = getenv_long(tx_data_skews[i], 10, KSZ9031_DEFAULT_TXD_SKEW_PS);
	}

	/* Range check, normalize values */
	lower = -900;
	upper = 960;
	rxc_skew_ps = ksz9031_range_check("rxc-skew-ps", rxc_skew_ps, lower, upper);
	txc_skew_ps = ksz9031_range_check("txc-skew-ps", txc_skew_ps, lower, upper);

	lower = -420;
	upper = 480;
	txen_skew_ps = ksz9031_range_check("txen-skew-ps", txen_skew_ps, lower, upper);
	rxdv_skew_ps = ksz9031_range_check("rxdv-skew-ps", rxdv_skew_ps, lower, upper);

	for (i=0; i<4; i++) {
		rxdx_skew_ps[i] = ksz9031_range_check("rxd[x]-skew-ps", rxdx_skew_ps[i], lower, upper);
		txdx_skew_ps[i] = ksz9031_range_check("txd[x]-skew-ps", txdx_skew_ps[i], lower, upper);
	}

	/* Assign values to register variables */
	rxc_skew_regval = (rxc_skew_ps + 900)/60;
	txc_skew_regval = (txc_skew_ps + 900)/60;

	txen_skew_regval = (txen_skew_ps + 420)/60;
	rxdv_skew_regval = (rxdv_skew_ps + 420)/60;

	for (i=0; i<4; i++) {
		rxdx_skew_regval[i] = (rxdx_skew_ps[i] + 420)/60;
		txdx_skew_regval[i] = (txdx_skew_ps[i] + 420)/60;
	}

	/* Write the values to the registers */
	ksz9031_set_rxc_skew(phydev, rxc_skew_regval);
	ksz9031_set_txc_skew(phydev, txc_skew_regval);
	ksz9031_set_txen_skew(phydev, txen_skew_regval);
	ksz9031_set_rxdv_skew(phydev, rxdv_skew_regval);

	for (i=0; i<4; i++) {
		ksz9031_set_rxdx_skew(phydev, i, rxdx_skew_regval[i]);
		ksz9031_set_txdx_skew(phydev, i, txdx_skew_regval[i]);
	}

	return 0;
}


int ksz9031_config_init(struct phy_device *phydev)
{
	genphy_config_aneg(phydev);
	genphy_restart_aneg(phydev);

        return 0;
}

static struct phy_driver ksz9031_driver = {
	.name = "Micrel ksz9031",
	.uid  = 0x221620,
	.mask = 0xfffff0,
	.features = PHY_GBIT_FEATURES,
	.config   = &ksz9031_config_init,
	.startup  = &ksz90xx_startup,
	.shutdown = &genphy_shutdown,
	.writeext = &ksz9031_phy_extwrite,
	.readext = &ksz9031_phy_extread,
};

int phy_micrel_init(void)
{
	phy_register(&KSZ804_driver);
#ifdef CONFIG_PHY_MICREL_KSZ9021
	phy_register(&ksz9021_driver);
#else
	phy_register(&KS8721_driver);
#endif
	phy_register(&ksz9031_driver);
	return 0;
}
