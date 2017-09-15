/*
 * Copyright (C) 2016-2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <common.h>
#include <asm/arch/clock_manager.h>
#include <asm/io.h>
#include <asm/arch/handoff_s10.h>

const struct cm_config * const cm_get_default_config(void)
{
	struct cm_config *cm_handoff_cfg = (struct cm_config *)
		(CONFIG_HANDOFF_CLOCK + CONFIG_HANDOFF_OFFSET_DATA);
	u32 *conversion = (u32 *)cm_handoff_cfg;
	u32 i;

	if (swab32(readl(CONFIG_HANDOFF_CLOCK)) == CONFIG_HANDOFF_MAGIC_CLOCK) {
		writel(swab32(readl(CONFIG_HANDOFF_CLOCK)),
			CONFIG_HANDOFF_CLOCK);
		for (i = 0; i < (sizeof(*cm_handoff_cfg) / sizeof(u32)); i++)
			conversion[i] = swab32(conversion[i]);
		return cm_handoff_cfg;
	}
	else if (readl(CONFIG_HANDOFF_CLOCK) == CONFIG_HANDOFF_MAGIC_CLOCK) {
		return cm_handoff_cfg;
	}
	else
		return 0;
}

const unsigned int cm_get_osc_clk_hz(const int osc)
{
	return 25000000;
}

const unsigned int cm_get_intosc_clk_hz(void)
{
	/* theory maximum internal osc clock */
	return 460000000;
}

const unsigned int cm_get_fpga_clk_hz(void)
{
	/* assuming 50MHz */
	return 50000000;
}
