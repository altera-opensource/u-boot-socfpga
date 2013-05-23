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
#include <asm/arch/system_manager.h>
#include <pinmux_config.h>

/******************************************************************************
 * Configure all the pin mux
 *****************************************************************************/
void sysmgr_pinmux_init(void)
{
	unsigned long offset = CONFIG_SYSMGR_PINMUXGRP_OFFSET;

	const unsigned long *pval = sys_mgr_init_table;
	unsigned long i;

	for (i = 0;
		i < ((sizeof(sys_mgr_init_table)) /
			sizeof(sys_mgr_pinmux_entry_t));
		i++, offset += sizeof(sys_mgr_pinmux_entry_t)) {
		writel(*pval++, (SOCFPGA_SYSMGR_ADDRESS + offset));
	}

	/* disable all signals from fpga fabric to hps modules */
	writel(0, CONFIG_SYSMGR_FPGAINTF_MODULE);

	/* enable the signal for those hps peripheral use fpga */
	if (readl(CONFIG_SYSMGR_NANDUSEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(CONFIG_SYSMGR_FPGAINTF_MODULE,
			SYSMGR_FPGAINTF_NAND);

	if (readl(CONFIG_SYSMGR_EMAC1USEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(CONFIG_SYSMGR_FPGAINTF_MODULE,
			SYSMGR_FPGAINTF_EMAC1);

	if (readl(CONFIG_SYSMGR_SDMMCUSEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(CONFIG_SYSMGR_FPGAINTF_MODULE,
			SYSMGR_FPGAINTF_SDMMC);

	if (readl(CONFIG_SYSMGR_EMAC0USEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(CONFIG_SYSMGR_FPGAINTF_MODULE,
			SYSMGR_FPGAINTF_EMAC0);

	if (readl(CONFIG_SYSMGR_SPIM1USEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(CONFIG_SYSMGR_FPGAINTF_MODULE,
			SYSMGR_FPGAINTF_SPIM1);

	if (readl(CONFIG_SYSMGR_SPIM0USEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(CONFIG_SYSMGR_FPGAINTF_MODULE,
			SYSMGR_FPGAINTF_SPIM0);
}
