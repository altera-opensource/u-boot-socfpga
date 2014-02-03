/*
 * Copyright (C) 2013 Altera Corporation <www.altera.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,  MA 02111-1307 USA
 *
 */

#include <common.h>
#include <malloc.h>
#include <dwmmc.h>
#include <asm/arch/dwmmc.h>

#define CLKMGR_PERPLLGRP_EN_REG		(SOCFPGA_CLKMGR_ADDRESS + 0xA0)
#define CLKMGR_SDMMC_CLK_ENABLE		(1 << 8)
#define SYSMGR_SDMMCGRP_CTRL_REG	(SOCFPGA_SYSMGR_ADDRESS + 0x108)
#define SYSMGR_SDMMC_CTRL_GET_DRVSEL(x)	(((x) >> 0) & 0x7)
#define SYSMGR_SDMMC_CTRL_SET(smplsel, drvsel)	\
	((((drvsel) << 0) & 0x7) | (((smplsel) << 3) & 0x38))

static char *ALTERA_NAME = "ALTERA DWMMC";

static void altera_dwmci_clksel(struct dwmci_host *host)
{
	unsigned int en;
	unsigned int drvsel;
	unsigned int smplsel;

	/* Disable SDMMC clock. */
	en = readl(CLKMGR_PERPLLGRP_EN_REG);
	en &= ~CLKMGR_SDMMC_CLK_ENABLE;
	writel(en, CLKMGR_PERPLLGRP_EN_REG);

	/* Configures drv_sel and smpl_sel */
	drvsel = 3;
	smplsel = 0;

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

int altera_dwmmc_init(u32 regbase, int bus_width, int index)
{
	struct dwmci_host *host = NULL;
	host = calloc(sizeof(struct dwmci_host), 1);
	if (!host) {
		printf("dwmci_host calloc fail!\n");
		return 1;
	}

	host->name = ALTERA_NAME;
	host->ioaddr = (void *)regbase;
	host->buswidth = bus_width;
	host->clksel = altera_dwmci_clksel;
	host->dev_index = index;
	/* fixed clock divide by 4 which due to the SDMMC wrapper */
	host->bus_hz = CONFIG_DWMMC_BUS_HZ;
	host->fifoth_val = MSIZE(0x2) |
		RX_WMARK(CONFIG_DWMMC_FIFO_DEPTH / 2 - 1) |
		TX_WMARK(CONFIG_DWMMC_FIFO_DEPTH / 2);

	add_dwmci(host, host->bus_hz, 400000);

	return 0;
}

