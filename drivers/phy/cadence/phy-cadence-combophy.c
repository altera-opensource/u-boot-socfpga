// SPDX-License-Identifier: GPL-2.0
/*
 * Cadence COMBOPHY PHY Driver
 *
 * Based on the linux driver provided by Cadence Sierra
 *
 * Copyright (c) 2022 Intel Corporation
 * Author: Lee, Kah Jing <kah.jing.lee@intel.com>
 *
 */
#include <common.h>
#include <clk.h>
#include <generic-phy.h>
#include <reset.h>
#include <dm/device.h>
#include <dm/device-internal.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <dm/read.h>
#include <dm/uclass.h>
#include <dm/devres.h>
#include <linux/io.h>
#include <linux/sizes.h>
#include <dt-bindings/phy/phy.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/reset_manager.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>

/* PHY register offsets */
/* General define */
#define BIT_ZERO	0
#define BIT_ONE		1
#define SD_HOST_CLK 200000000

/* SDIO Card UHS-I modes */
#define SDIO_MODE_SDR12		0
#define SDIO_MODE_SDR25		1
#define SDIO_MODE_SDR50		2
#define SDIO_MODE_SDR104	3
#define SDIO_MODE_DDR50		4
#define SD_MODE_HS			5

/* eMMC modes */
#define eMMC_MODE_SDR		0
#define eMMC_MODE_HS		1
#define eMMC_MODE_HS200		2
#define eMMC_MODE_HS400		3
/* SW_RESET_REG */
#define SDHCI_CDNS_HRS00		0x00
#define SDHCI_CDNS_HRS00_SWR	BIT(0)
/* PHY access port */
#define SDHCI_CDNS_HRS04		0x10
#define SDHCI_CDNS_HRS04_ADDR	GENMASK(5, 0)
/* PHY data access port */
#define SDHCI_CDNS_HRS05		0x14
/* eMMC control registers */
#define SDHCI_CDNS_HRS06		0x18

#define READ_OP				0
#define WRITE_OP			1
#define READ_WRITE_OP		2
#define POLLING_OP			3
#define READ_PHY_OP			4
#define READ_WRITE_PHY_OP	5

#define PHY_TYPE_NAND	0
#define PHY_TYPE_SDMMC	1
#define PHY_REG_LEN		15
/* IO_DELAY_INFO_REG */
#define SDHCI_CDNS_HRS07					0x1c
#define SDHCI_CDNS_HRS07_RW_COMPENSATE		GENMASK(20, 16)
#define SDHCI_CDNS_HRS07_IDELAY_VAL			GENMASK(4, 0)
/* TODO: check DV dfi_init val=9 */
#define SDHCI_CDNS_HRS07_RW_COMPENSATE_DATA 0x9
/* TODO: check DV dfi_init val=8; DDR Mode */
#define SDHCI_CDNS_HRS07_RW_COMPENSATE_DATA_DDR	0x8
#define SDHCI_CDNS_HRS07_IDELAY_VAL_DATA		0x0
/* PHY reset port */
#define SDHCI_CDNS_HRS09					0x24
#define SDHCI_CDNS_HRS09_PHY_SW_RESET		BIT(0)
#define SDHCI_CDNS_HRS09_PHY_INIT_COMPLETE	BIT(1)
#define SDHCI_CDNS_HRS09_EXTENDED_RD_MODE	BIT(2)
#define SDHCI_CDNS_HRS09_EXTENDED_WR_MODE	BIT(3)
#define SDHCI_CDNS_HRS09_RDCMD_EN			BIT(15)
#define SDHCI_CDNS_HRS09_RDDATA_EN			BIT(16)

/* PHY reset port */
#define SDHCI_CDNS_HRS10						0x28
#define SDHCI_CDNS_HRS10_HCSDCLKADJ				GENMASK(19, 16)
#define SDHCI_CDNS_HRS10_HCSDCLKADJ_DATA		0x0
/* HCSDCLKADJ DATA; DDR Mode */
#define SDHCI_CDNS_HRS10_HCSDCLKADJ_DATA_DDR	0x2

/* CMD_DATA_OUTPUT */
#define SDHCI_CDNS_HRS16					0x40
#define SDHCI_CDNS_HRS16_WRDATA1_SDCLK_DLY	GENMASK(31, 28)
#define SDHCI_CDNS_HRS16_WRDATA0_SDCLK_DLY	GENMASK(27, 24)
#define SDHCI_CDNS_HRS16_WRCMD1_SDCLK_DLY	GENMASK(23, 20)
#define SDHCI_CDNS_HRS16_WRCMD0_SDCLK_DLY	GENMASK(19, 16)
#define SDHCI_CDNS_HRS16_WRDATA1_DLY		GENMASK(15, 12)
#define SDHCI_CDNS_HRS16_WRDATA0_DLY		GENMASK(11, 8)
#define SDHCI_CDNS_HRS16_WRCMD1_DLY			GENMASK(7, 4)
#define SDHCI_CDNS_HRS16_WRCMD0_DLY			GENMASK(3, 0)
#define SDHCI_CDNS_HRS16_WRDATA1_SDCLK_DLY_DATA		0x0
#define SDHCI_CDNS_HRS16_WRDATA1_SDCLK_DLY_DATA_DDR	0x1
#define SDHCI_CDNS_HRS16_WRDATA0_SDCLK_DLY_DATA		0x0
#define SDHCI_CDNS_HRS16_WRDATA0_SDCLK_DLY_DATA_DDR	0x1
#define SDHCI_CDNS_HRS16_WRCMD1_SDCLK_DLY_DATA		0x0
#define SDHCI_CDNS_HRS16_WRCMD0_SDCLK_DLY_DATA		0x0
#define SDHCI_CDNS_HRS16_WRDATA1_DLY_DATA			0x0
/* TODO: check DV dfi_init val=1 */
#define SDHCI_CDNS_HRS16_WRDATA0_DLY_DATA			0x1
#define SDHCI_CDNS_HRS16_WRCMD1_DLY_DATA			0x0
/* TODO: check DV dfi_init val=1 */
#define SDHCI_CDNS_HRS16_WRCMD0_DLY_DATA			0x1

/* SRS - Slot Register Set (SDHCI-compatible) */
#define SDHCI_CDNS_SRS_BASE					0x200
#define SDHCI_CDNS_SRS00					0x200
#define SDHCI_CDNS_SRS01					0x204
#define SDHCI_CDNS_SRS02					0x208
#define SDHCI_CDNS_SRS03					0x20c
#define SDHCI_CDNS_SRS04					0x210
#define SDHCI_CDNS_SRS08					0x220
#define SDHCI_CDNS_SRS09					0x224
#define SDHCI_CDNS_SRS09_CI					BIT(16)
#define SDHCI_CDNS_SRS10					0x228
#define SDHCI_CDNS_SRS10_VOLT_ON FIELD_PREP(GENMASK(11, 9), BIT_ONE)\
	| FIELD_PREP(BIT(8), BIT_ONE) | FIELD_PREP(BIT(0), BIT_ZERO)
#define SDHCI_CDNS_SRS10_VOLT_OFF FIELD_PREP(GENMASK(11, 9), BIT_ONE)\
	| FIELD_PREP(BIT(8), BIT_ZERO)

#define SDHCI_CDNS_SRS11					0x22c
#define SDHCI_CDNS_SRS11_RST				0x0
#define SDHCI_CDNS_SRS12					0x230
#define SDHCI_CDNS_SRS13					0x234
#define SDHCI_CDNS_SRS13_DATA				0xffffffff
#define SDHCI_CDNS_SRS14					0x238
#define SDHCI_CDNS_SRS15					0x23c

/* PHY register values */
#define PHY_DQ_TIMING_REG					0x2000
#define PHY_DQS_TIMING_REG					0x2004
#define PHY_GATE_LPBK_CTRL_REG				0x2008
#define PHY_DLL_MASTER_CTRL_REG				0x200C
#define PHY_DLL_SLAVE_CTRL_REG				0x2010
#define PHY_CTRL_REG						0x2080
#define USE_EXT_LPBK_DQS					BIT(22)
#define USE_LPBK_DQS						BIT(21)
#define USE_PHONY_DQS						BIT(20)
#define USE_PHONY_DQS_CMD					BIT(19)
#define SYNC_METHOD							BIT(31)
#define SW_HALF_CYCLE_SHIFT					BIT(28)
#define RD_DEL_SEL							GENMASK(24, 19)
#define RD_DEL_SEL_DATA						0x34
#define GATE_CFG_ALWAYS_ON					BIT(6)
#define UNDERRUN_SUPPRESS					BIT(18)
#define PARAM_DLL_BYPASS_MODE				BIT(23)
#define PARAM_PHASE_DETECT_SEL				GENMASK(22, 20)
#define PARAM_DLL_START_POINT				GENMASK(7, 0)
#define PARAM_PHASE_DETECT_SEL_DATA			0x2
#define PARAM_DLL_START_POINT_DATA			0x4
#define PARAM_DLL_START_POINT_DATA_SDR50	254

#define READ_DQS_CMD_DELAY		GENMASK(31, 24)
#define CLK_WRDQS_DELAY			GENMASK(23, 16)
#define CLK_WR_DELAY			GENMASK(15, 8)
#define READ_DQS_DELAY			GENMASK(7, 0)
#define READ_DQS_CMD_DELAY_DATA	0x0
#define CLK_WRDQS_DELAY_DATA	0x0
#define CLK_WR_DELAY_DATA		0x0
#define READ_DQS_DELAY_DATA		0x0

#define PHONY_DQS_TIMING		GENMASK(9, 4)
#define PHONY_DQS_TIMING_DATA	0x0

#define IO_MASK_ALWAYS_ON		BIT(31)
#define IO_MASK_END				GENMASK(29, 27)
#define IO_MASK_START			GENMASK(26, 24)
#define DATA_SELECT_OE_END		GENMASK(2, 0)
/* TODO: check DV dfi_init val=0 */
#define IO_MASK_END_DATA		0x0
/* TODO: check DV dfi_init val=2; DDR Mode */
#define IO_MASK_END_DATA_DDR	0x2
#define IO_MASK_START_DATA		0x0
#define DATA_SELECT_OE_END_DATA 0x1

struct cdns_combophy_reg_pairs {
	u32 reg;
	u32 val;
	u32 op;
};

struct cdns_combophy_plat {
	u32 phy_type;
	void __iomem *hrs_addr;
};

struct cdns_combophy_phy {
	struct udevice *dev;
	void *base;
};

struct cdns_combophy_data {
	u32 speed_mode;
	struct cdns_combophy_reg_pairs *cdns_phy_regs;
};

static int sdhci_cdns_write_phy_reg_mask(u32 addr, u32 data, u32 mask,
					 void __iomem *hrs_addr)
{
	void __iomem *reg = hrs_addr + SDHCI_CDNS_HRS04;
	u32 tmp;

	tmp = addr;

	/* get PHY address */
	writel(tmp, reg);
	debug("%s: register = 0x%08x\n", __func__, readl(hrs_addr +
	SDHCI_CDNS_HRS04));

	/* read current PHY register value, before write */
	reg = hrs_addr + SDHCI_CDNS_HRS05;
	tmp = readl(reg);
	debug("%s: register = 0x%08x\n", __func__, readl(hrs_addr +
	SDHCI_CDNS_HRS05));
	tmp &= ~mask;
	tmp |= data;

	/* write operation */
	writel(tmp, reg);
	debug("%s: register = 0x%08x\n", __func__, readl(hrs_addr +
	SDHCI_CDNS_HRS05));

	return 0;
}

static u32 sdhci_cdns_read_phy_reg(u32 addr, void __iomem *hrs_addr)
{
	void __iomem *reg = hrs_addr + SDHCI_CDNS_HRS04;
	u32 tmp;

	tmp = addr;

	/* get PHY address */
	writel(tmp, reg);
	debug("%s: register = 0x%08x\n", __func__, readl(reg));

	/* read current PHY register value, before write */
	reg = hrs_addr + SDHCI_CDNS_HRS05;
	tmp = readl(reg);
	debug("%s: register = 0x%08x\n", __func__, readl(reg));

	return tmp;
}

/* initialize clock */
static void sdhci_set_clk(u32 freq_khz, void __iomem *hrs_addr)
{
	void __iomem *reg = hrs_addr + SDHCI_CDNS_SRS11;

	/* calculate the value to set */
	u32 sdclkfs = (SD_HOST_CLK / 2000) / freq_khz;
	u32 dtcvval = 0xe;

	/* disable SDCE */
	writel(0, reg);
	printf("Disable SDCE\n");

	/* Enable ICE (internal clock enable) */
	writel(dtcvval << 16 | sdclkfs << 8 | 1 << 0, reg);
	printf("Enable ICE: SRS11: %08x\n", readl(reg));

	/* Wait for ICS (internal clock stable) */
	while ((readl(reg)
		& BIT(1)) == 0) {
		/* polling for ICS = 1 */
	}

	reg = hrs_addr + SDHCI_CDNS_HRS09;
	/* Enable DLL reset */
	writel(readl(reg) & ~0x00000001, reg);
	debug("Enable DLL reset\n");

	/* Set extended_wr_mode */
	writel((readl(reg) & 0xFFFFFFF7) |
		((sdclkfs > 0 ? 1 << 3 : 0 << 3)), reg);
	debug("Set extended_wr_mode\n");

	/* Release DLL reset */
	writel(readl(reg) | BIT(0), reg);
	writel(readl(reg) | 3 << 15, reg);
	debug("Release DLL reset\n");

	/* polling for phy_init = 1 */
	while ((readl(reg)
		& BIT(1)) == 0) {
	}

	printf("PHY init complete\n");

	writel(dtcvval << 16 | sdclkfs << 8 | 1 << 0 | 1 << 2, reg);
	printf("SD clock enabled\n");
}

static int sdhci_cdns_detect_card(void __iomem *hrs_addr)
{
	u32 tmp;
	void __iomem *reg = hrs_addr + SDHCI_CDNS_SRS09;

	/* step 1, polling for sdcard */
	while ((readl(reg)
		& SDHCI_CDNS_SRS09_CI) == 0) {
		/* polling for SRS9.CI = 1 */
	}

	printf("SDcard detected\n");

	debug("ON voltage\n");
	tmp = SDHCI_CDNS_SRS10_VOLT_ON;
	reg = hrs_addr + SDHCI_CDNS_SRS10;
	writel(tmp, reg);

	debug("OFF Voltage\n");
	tmp = SDHCI_CDNS_SRS10_VOLT_OFF;
	writel(tmp, reg);

	debug("ON voltage\n");
	tmp = SDHCI_CDNS_SRS10_VOLT_ON;
	reg = hrs_addr + SDHCI_CDNS_SRS10;
	writel(tmp, reg);

	/* sdhci_set_clk - 50000 */
	debug("SD clock\n");
	sdhci_set_clk(50000, hrs_addr);

	/* set SRS13 */
	tmp = SDHCI_CDNS_SRS13_DATA;
	reg = hrs_addr + SDHCI_CDNS_SRS13;
	writel(tmp, reg);
	debug("SRS13: %08x\n", readl(reg));

	return 0;
}

static int sdhci_cdns_combophy_phy_prog(void __iomem *hrs_addr)
{
	u32 ret, tmp;
	u32 mask = 0x0;

	/* step 1, switch on SW_RESET */
	tmp = SDHCI_CDNS_SRS11_RST;
	writel(tmp, hrs_addr + SDHCI_CDNS_SRS11);

	tmp = SDHCI_CDNS_HRS00_SWR;
	writel(tmp, hrs_addr);
	while ((readl(hrs_addr) &
		SDHCI_CDNS_HRS00_SWR) == 1) {
		/* polling for HRS0.0 (SWR) = 0 */
	}
	debug("SW reset\n");

	/* switch on DLL_RESET */
	tmp = readl(hrs_addr + SDHCI_CDNS_HRS09);
	tmp &= ~SDHCI_CDNS_HRS09_PHY_SW_RESET;
	tmp |= FIELD_PREP(SDHCI_CDNS_HRS09_PHY_SW_RESET, BIT_ZERO);

	writel(tmp, hrs_addr + SDHCI_CDNS_HRS09);
	debug("PHY_SW_RESET: 0x%08x\n", readl(hrs_addr + SDHCI_CDNS_HRS09));

	/* program PHY_DQS_TIMING_REG */
	tmp = FIELD_PREP(USE_EXT_LPBK_DQS, BIT_ONE) |
			FIELD_PREP(USE_LPBK_DQS, BIT_ONE) |
			FIELD_PREP(USE_PHONY_DQS, BIT_ONE) |
			FIELD_PREP(USE_PHONY_DQS_CMD, BIT_ONE);
	ret = sdhci_cdns_write_phy_reg_mask(PHY_DQS_TIMING_REG, tmp, tmp, hrs_addr);

	/* program PHY_GATE_LPBK_CTRL_REG */
	tmp = FIELD_PREP(SYNC_METHOD, BIT_ONE) |
			FIELD_PREP(SW_HALF_CYCLE_SHIFT, BIT_ZERO) |
			FIELD_PREP(RD_DEL_SEL, RD_DEL_SEL_DATA) |
			FIELD_PREP(UNDERRUN_SUPPRESS, BIT_ONE) |
			FIELD_PREP(GATE_CFG_ALWAYS_ON, BIT_ONE);
	mask = SYNC_METHOD | SW_HALF_CYCLE_SHIFT | RD_DEL_SEL | UNDERRUN_SUPPRESS |
			GATE_CFG_ALWAYS_ON;
	ret = sdhci_cdns_write_phy_reg_mask(PHY_GATE_LPBK_CTRL_REG, tmp, mask,
					    hrs_addr);

	/* program PHY_DLL_MASTER_CTRL_REG */
	tmp = FIELD_PREP(PARAM_DLL_BYPASS_MODE, BIT_ONE) |
			FIELD_PREP(PARAM_PHASE_DETECT_SEL, PARAM_PHASE_DETECT_SEL_DATA) |
			FIELD_PREP(PARAM_DLL_START_POINT, PARAM_DLL_START_POINT_DATA);
	mask = PARAM_DLL_BYPASS_MODE | PARAM_PHASE_DETECT_SEL |
			PARAM_DLL_START_POINT;
	ret = sdhci_cdns_write_phy_reg_mask(PHY_DLL_MASTER_CTRL_REG, tmp, mask,
					    hrs_addr);

	/* program PHY_DLL_SLAVE_CTRL_REG */
	tmp = FIELD_PREP(READ_DQS_CMD_DELAY, READ_DQS_CMD_DELAY_DATA) |
			FIELD_PREP(CLK_WRDQS_DELAY, CLK_WRDQS_DELAY_DATA) |
			FIELD_PREP(CLK_WR_DELAY, CLK_WR_DELAY_DATA) |
			FIELD_PREP(READ_DQS_DELAY, READ_DQS_DELAY_DATA);
	mask = READ_DQS_CMD_DELAY | CLK_WRDQS_DELAY | CLK_WR_DELAY | READ_DQS_DELAY;
	ret = sdhci_cdns_write_phy_reg_mask(PHY_DLL_SLAVE_CTRL_REG, tmp, mask,
					    hrs_addr);

	/* program PHY_CTRL_REG */
	tmp = FIELD_PREP(PHONY_DQS_TIMING, PHONY_DQS_TIMING_DATA);
	mask = PHONY_DQS_TIMING;
	ret = sdhci_cdns_write_phy_reg_mask(PHY_CTRL_REG, tmp, mask, hrs_addr);

	/* switch off DLL_RESET */
	tmp = readl(hrs_addr + SDHCI_CDNS_HRS09);
	tmp &= ~SDHCI_CDNS_HRS09_PHY_SW_RESET;
	tmp |= FIELD_PREP(SDHCI_CDNS_HRS09_PHY_SW_RESET, BIT_ONE);

	writel(tmp, hrs_addr + SDHCI_CDNS_HRS09);

	/* polling PHY_INIT_COMPLETE */
	while ((readl(hrs_addr + SDHCI_CDNS_HRS09) &
		SDHCI_CDNS_HRS09_PHY_INIT_COMPLETE) !=
		SDHCI_CDNS_HRS09_PHY_INIT_COMPLETE) {
		/* polling for PHY_INIT_COMPLETE bit */
	}

	/* program PHY_DQ_TIMING_REG */
	tmp = sdhci_cdns_read_phy_reg(PHY_DQ_TIMING_REG, hrs_addr) & 0x07FFFF8;
	tmp |= FIELD_PREP(IO_MASK_ALWAYS_ON, BIT_ZERO) |
			FIELD_PREP(IO_MASK_END, IO_MASK_END_DATA) |
			FIELD_PREP(IO_MASK_START, IO_MASK_START_DATA) |
			FIELD_PREP(DATA_SELECT_OE_END, DATA_SELECT_OE_END_DATA);
	mask = IO_MASK_ALWAYS_ON | IO_MASK_END | IO_MASK_START | DATA_SELECT_OE_END;
	ret = sdhci_cdns_write_phy_reg_mask(PHY_DQ_TIMING_REG, tmp, mask, hrs_addr);

	/* program HRS09, register 42 */
	tmp = readl(hrs_addr + SDHCI_CDNS_HRS09) & 0xFFFE7FF3;
	tmp &= ~(SDHCI_CDNS_HRS09_RDDATA_EN |
			SDHCI_CDNS_HRS09_RDCMD_EN |
			SDHCI_CDNS_HRS09_EXTENDED_WR_MODE |
			SDHCI_CDNS_HRS09_EXTENDED_RD_MODE);
	tmp |= FIELD_PREP(SDHCI_CDNS_HRS09_RDDATA_EN, BIT_ONE) |
			FIELD_PREP(SDHCI_CDNS_HRS09_RDCMD_EN, BIT_ONE) |
			FIELD_PREP(SDHCI_CDNS_HRS09_EXTENDED_WR_MODE, BIT_ONE) |
			FIELD_PREP(SDHCI_CDNS_HRS09_EXTENDED_RD_MODE, BIT_ONE);

	writel(tmp, hrs_addr + SDHCI_CDNS_HRS09);
	debug("%s: register = 0x%x\n", __func__, readl(hrs_addr +
			SDHCI_CDNS_HRS09));

	/* program HRS10, register 43 */
	tmp = readl(hrs_addr + SDHCI_CDNS_HRS10) & 0xFFF0FFFF;
	tmp &= ~SDHCI_CDNS_HRS10_HCSDCLKADJ;
	tmp |= FIELD_PREP(SDHCI_CDNS_HRS10_HCSDCLKADJ,
			SDHCI_CDNS_HRS10_HCSDCLKADJ_DATA);

	writel(tmp, hrs_addr + SDHCI_CDNS_HRS10);
	debug("%s: register = 0x%x\n", __func__, readl(hrs_addr +
			SDHCI_CDNS_HRS10));

	/* program HRS16, register 48 */
	tmp = readl(hrs_addr + SDHCI_CDNS_HRS16);
	tmp &= ~(SDHCI_CDNS_HRS16_WRDATA1_SDCLK_DLY |
			SDHCI_CDNS_HRS16_WRDATA0_SDCLK_DLY |
			SDHCI_CDNS_HRS16_WRCMD1_SDCLK_DLY |
			SDHCI_CDNS_HRS16_WRDATA1_DLY |
			SDHCI_CDNS_HRS16_WRDATA0_DLY |
			SDHCI_CDNS_HRS16_WRCMD1_DLY |
			SDHCI_CDNS_HRS16_WRCMD0_DLY);
	tmp |= FIELD_PREP(SDHCI_CDNS_HRS16_WRDATA1_SDCLK_DLY,
				SDHCI_CDNS_HRS16_WRDATA1_SDCLK_DLY_DATA) |
			FIELD_PREP(SDHCI_CDNS_HRS16_WRDATA0_SDCLK_DLY,
				   SDHCI_CDNS_HRS16_WRDATA0_SDCLK_DLY_DATA) |
			FIELD_PREP(SDHCI_CDNS_HRS16_WRCMD1_SDCLK_DLY,
				   SDHCI_CDNS_HRS16_WRCMD1_SDCLK_DLY_DATA) |
			FIELD_PREP(SDHCI_CDNS_HRS16_WRCMD0_SDCLK_DLY,
				   SDHCI_CDNS_HRS16_WRCMD0_SDCLK_DLY_DATA) |
			FIELD_PREP(SDHCI_CDNS_HRS16_WRDATA1_DLY,
				   SDHCI_CDNS_HRS16_WRDATA1_DLY_DATA) |
			FIELD_PREP(SDHCI_CDNS_HRS16_WRDATA0_DLY,
				   SDHCI_CDNS_HRS16_WRDATA0_DLY_DATA) |
			FIELD_PREP(SDHCI_CDNS_HRS16_WRCMD1_DLY,
				   SDHCI_CDNS_HRS16_WRCMD1_DLY_DATA) |
			FIELD_PREP(SDHCI_CDNS_HRS16_WRCMD0_DLY,
				   SDHCI_CDNS_HRS16_WRCMD0_DLY_DATA);

	writel(tmp, hrs_addr + SDHCI_CDNS_HRS16);
	debug("%s: register = 0x%x\n", __func__, readl(hrs_addr +
		SDHCI_CDNS_HRS16));

	/* program HRS07, register 40 */
	tmp = readl(hrs_addr + SDHCI_CDNS_HRS07);
	tmp &= ~(SDHCI_CDNS_HRS07_RW_COMPENSATE |
			SDHCI_CDNS_HRS07_IDELAY_VAL);
	tmp |= FIELD_PREP(SDHCI_CDNS_HRS07_RW_COMPENSATE,
				   SDHCI_CDNS_HRS07_RW_COMPENSATE_DATA) |
			FIELD_PREP(SDHCI_CDNS_HRS07_IDELAY_VAL,
				   SDHCI_CDNS_HRS07_IDELAY_VAL_DATA);
	writel(tmp, hrs_addr + SDHCI_CDNS_HRS07);
	debug("%s: register = 0x%x\n", __func__, readl(hrs_addr +
			SDHCI_CDNS_HRS07));
	/* end of combophy init */

	/* initialize sdcard/eMMC & set clock*/
	ret = sdhci_cdns_detect_card(hrs_addr);
	debug("eMMC Mode: 0x%08x\n", readl(hrs_addr + SDHCI_CDNS_HRS06));

	return 0;
}

static int cdns_combophy_phy_init(struct phy *gphy)
{
	u32 tmp;
	struct cdns_combophy_plat *plat = dev_get_plat(gphy->dev);

	if (plat->phy_type == PHY_TYPE_SDMMC) {
		/* SDMMC warm reset */
		writel(0x80, socfpga_get_rstmgr_addr() + RSTMGR_SOC64_PER0MODRST);
		writel(0x0, socfpga_get_rstmgr_addr() + RSTMGR_SOC64_PER0MODRST);

		/* SDMMC comboPHY warm reset */
		writel(0x40, socfpga_get_rstmgr_addr() + RSTMGR_SOC64_PER0MODRST);
		writel(0x0, socfpga_get_rstmgr_addr() + RSTMGR_SOC64_PER0MODRST);

		/* configure DFI_SEL for SDMMC */
		tmp = SYSMGR_SOC64_COMBOPHY_DFISEL_SDMMC;
		writel(tmp, socfpga_get_sysmgr_addr() + SYSMGR_SOC64_COMBOPHY_DFISEL);
		debug("DFISEL: %08x\nSDMMC_USEFPGA: %08x\n",
		      readl(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_COMBOPHY_DFISEL),
		      readl(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_SDMMC_USEFPGA));

		/* reset clkmgr for SDMMC & COMBOPHY */
		writel(0x0000007f, socfpga_get_clkmgr_addr() + 0x28);
		writel(0x04000020, socfpga_get_clkmgr_addr() + 0x7c);
		writel(0x04000020, socfpga_get_clkmgr_addr() + 0x80);
		debug("clkmgr.mainpllgrp.en: 0x%08x\nclkmgr.mainpllgrp.ens: 0x%08x\n",
		      readl(socfpga_get_clkmgr_addr() + 0x24),
			  readl(socfpga_get_clkmgr_addr() + 0x28));
		debug("clkmgr.perpllgrp.en: 0x%08x\nclkmgr.perpllgrp.ens: 0x%08x\n",
		      readl(socfpga_get_clkmgr_addr() + 0x7c),
			  readl(socfpga_get_clkmgr_addr() + 0x80));

		/* TODO: add speed-mode checking in device tree */
		sdhci_cdns_combophy_phy_prog(plat->hrs_addr);

	} else if (plat->phy_type == PHY_TYPE_NAND) {
		/* configure DFI_SEL for NAND */
	} else {
		return -EINVAL;
	}

	return 0;
}

static int cdns_combophy_phy_on(struct phy *gphy)
{
	return 0;
}

static int cdns_combophy_phy_off(struct phy *gphy)
{
	return 0;
}

static int cdns_combophy_phy_reset(struct phy *gphy)
{
	return 0;
};

static const struct phy_ops ops = {
	.init		= cdns_combophy_phy_init,
	.power_on	= cdns_combophy_phy_on,
	.power_off	= cdns_combophy_phy_off,
	.reset		= cdns_combophy_phy_reset,
};

static int cdns_combophy_phy_probe(struct udevice *dev)
{
	struct cdns_combophy_plat *plat = dev_get_plat(dev);
	int ret = 0;

	plat->phy_type = dev_read_u32_default(dev, "phy-type", 0);
	plat->hrs_addr = dev_remap_addr_index(dev, 0);

	printf("%s: ComboPHY probed\n", __func__);

	return ret;
}

static int cdns_combophy_phy_remove(struct udevice *dev)
{
	return 0;
}

static const struct udevice_id cdns_combophy_id_table[] = {
	{ .compatible = "cdns,combophy" },
	{}
};

U_BOOT_DRIVER(combophy_phy_provider) = {
	.name		= "combophy",
	.id		= UCLASS_PHY,
	.of_match	= cdns_combophy_id_table,
	.probe		= cdns_combophy_phy_probe,
	.remove		= cdns_combophy_phy_remove,
	.ops		= &ops,
	.priv_auto	= sizeof(struct cdns_combophy_phy),
	.plat_auto  = sizeof(struct cdns_combophy_plat),
};
