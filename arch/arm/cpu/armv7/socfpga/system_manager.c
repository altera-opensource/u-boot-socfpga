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
#include <asm/arch/fpga_manager.h>

/*
 * Populate the value for SYSMGR.FPGAINTF.MODULE based on pinmux setting.
 * The value is not wrote to SYSMGR.FPGAINTF.MODULE but
 * CONFIG_SYSMGR_ISWGRP_HANDOFF.
 */
static unsigned long populate_sysmgr_fpgaintf_module(void)
{
	writel(0, ISWGRP_HANDOFF_FPGAINTF);

	/* enable the signal for those hps peripheral use fpga */
	if (readl(SYSMGR_PINMUXGRP_NANDUSEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(ISWGRP_HANDOFF_FPGAINTF,
			SYSMGR_FPGAINTF_NAND);

	if (readl(SYSMGR_PINMUXGRP_EMAC1USEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(ISWGRP_HANDOFF_FPGAINTF,
			SYSMGR_FPGAINTF_EMAC1);

	if (readl(SYSMGR_PINMUXGRP_SDMMCUSEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(ISWGRP_HANDOFF_FPGAINTF,
			SYSMGR_FPGAINTF_SDMMC);

	if (readl(SYSMGR_PINMUXGRP_EMAC0USEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(ISWGRP_HANDOFF_FPGAINTF,
			SYSMGR_FPGAINTF_EMAC0);

	if (readl(SYSMGR_PINMUXGRP_SPIM1USEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(ISWGRP_HANDOFF_FPGAINTF,
			SYSMGR_FPGAINTF_SPIM1);

	if (readl(SYSMGR_PINMUXGRP_SPIM0USEFPGA) == SYSMGR_FPGAINTF_USEFPGA)
		setbits_le32(ISWGRP_HANDOFF_FPGAINTF,
			SYSMGR_FPGAINTF_SPIM0);

	return readl(ISWGRP_HANDOFF_FPGAINTF);
}

/******************************************************************************
 * Configure all the pin mux
 *****************************************************************************/
void sysmgr_pinmux_init(void)
{
	unsigned long offset = CONFIG_SYSMGR_PINMUXGRP_OFFSET;

	const unsigned long *pval = sys_mgr_init_table;
	unsigned long i, reg_value;

	for (i = 0;
		i < ((sizeof(sys_mgr_init_table)) /
			sizeof(sys_mgr_pinmux_entry_t));
		i++, offset += sizeof(sys_mgr_pinmux_entry_t)) {
		writel(*pval++, (SOCFPGA_SYSMGR_ADDRESS + offset));
	}

	/* populate (not writing) the value for SYSMGR.FPGAINTF.MODULE
	based on pinmux setting */
	reg_value = populate_sysmgr_fpgaintf_module();

	if (is_fpgamgr_fpga_ready()) {
		/* enable the required signals only */
		writel(reg_value, SYSMGR_FPGAINTF_MODULE);
	}
}

/*
 * If SDMMC PWREN is used, we need to ensure BootROM always reconfigure
 * IOCSR and pinmux after warm reset. This is to cater the use case
 * of board design which is using SDMMC PWREN pins.
 */
void sysmgr_sdmmc_pweren_mux_check(void)
{
	if (readl(SYSMGR_PINMUXGRP_FLASHIO1) == 3)
		writel(SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGPINMUX |
		       SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGIO,
		       CONFIG_SYSMGR_ROMCODEGRP_CTRL);
}
