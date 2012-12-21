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
#include <asm/io.h>

#define CLKMGR_PERPLLGRP_EN_REG		(SOCFPGA_CLKMGR_ADDRESS + 0xA0)
#define CLKMGR_SDMMC_CLK_ENABLE		(1 << 8)

#define SYSMGR_SDMMCGRP_CTRL_REG	(SOCFPGA_SYSMGR_ADDRESS + 0x108)

#define SYSMGR_SDMMC_CTRL_GET_DRVSEL(x)	(((x) >> 0) & 0x7)
#define SYSMGR_SDMMC_CTRL_SET(smplsel, drvsel)	\
	((((drvsel) << 0) & 0x7) | (((smplsel) << 3) & 0x38))

/*
 * Assuming the caller is disable the controller clock before call to this
 * function.
 */
void sdmmc_set_clk_timing(unsigned int clk_out)
{
	unsigned int en;
	unsigned int drvsel;
	unsigned int smplsel;

	debug("%s: clk_out %d\n", __FUNCTION__, clk_out);
	/* Disable SDMMC clock. */
	en = readl(CLKMGR_PERPLLGRP_EN_REG);
	en &= ~CLKMGR_SDMMC_CLK_ENABLE;
	writel(en, CLKMGR_PERPLLGRP_EN_REG);

	/* Configures drv_sel and smpl_sei */
	/*
	 * TODO: These drv_sel and smpl_sel values are not finalized yet.
	 * Need verify with hardware team. Noted, we don't support USH-I mode
	 * in u-Boot.
	 */
	if (clk_out > 25000000) {
		drvsel = 3;
		smplsel = 7;
	} else {
		drvsel = 3;
		smplsel = 0;
	}

	debug("%s: drvsel %d smplsel %d\n", __FUNCTION__, drvsel, smplsel);
	writel(SYSMGR_SDMMC_CTRL_SET(smplsel, drvsel),
		SYSMGR_SDMMCGRP_CTRL_REG);

	debug("%s: SYSMGR_SDMMCGRP_CTRL_REG = 0x%x\n", __FUNCTION__,
		readl(SYSMGR_SDMMCGRP_CTRL_REG));
	/* Enable SDMMC clock */
	en = readl(CLKMGR_PERPLLGRP_EN_REG);
	en |= CLKMGR_SDMMC_CLK_ENABLE;
	writel(en, CLKMGR_PERPLLGRP_EN_REG);
}

/* Return 1 if need to use hold register. */
int sdmmc_use_hold_reg(void)
{
	unsigned int ctrl;

	ctrl = readl(SYSMGR_SDMMCGRP_CTRL_REG);

	/* Use hold reg if drv_sel is set. */
	if (SYSMGR_SDMMC_CTRL_GET_DRVSEL(ctrl))
		return 1;

	return 0;
}

