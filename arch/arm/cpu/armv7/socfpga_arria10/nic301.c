/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/nic301.h>

/*
 * Convert all slave from secure to non secure
 */
void nic301_slave_ns(void)
{
#ifdef TEST_AT_ASIMOV
	writel(0x1, (SOCFPGA_L3REGS_ADDRESS +
		L3REGS_SECGRP_LWHPS2FPGAREGS_ADDRESS));
	writel(0x1, (SOCFPGA_L3REGS_ADDRESS +
		L3REGS_SECGRP_HPS2FPGAREGS_ADDRESS));
	writel(0x1, (SOCFPGA_L3REGS_ADDRESS +
		L3REGS_SECGRP_ACP_ADDRESS));
	writel(0x1, (SOCFPGA_L3REGS_ADDRESS +
		L3REGS_SECGRP_ROM_ADDRESS));
	writel(0x1, (SOCFPGA_L3REGS_ADDRESS +
		L3REGS_SECGRP_OCRAM_ADDRESS));
	writel(0x1, (SOCFPGA_L3REGS_ADDRESS +
		L3REGS_SECGRP_SDRDATA_ADDRESS));
#endif
	return;
}
