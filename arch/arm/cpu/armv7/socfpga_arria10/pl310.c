/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/pl310.h>
#include <asm/armv7.h>

static const struct pl310_regs *pl310_regs_base =
	(void *)CONFIG_SYS_PL310_BASE;


#ifndef CONFIG_SYS_L2CACHE_OFF
void v7_outer_cache_enable(void)
{
	/* disable the L2 cache */
	writel(0, &pl310_regs_base->pl310_ctrl);

	/* enable BRESP, instruction and data prefetch, full line of zeroes */
	setbits_le32(&pl310_regs_base->pl310_aux_ctrl,
		PL310_AUX_CTRL_FULL_LINE_ZERO_MASK |
		PL310_AUX_CTRL_DATA_PREFETCH_MASK |
		PL310_AUX_CTRL_INST_PREFETCH_MASK |
		PL310_AUX_CTRL_EARLY_BRESP_MASK);

	/* enable double linefills and prefetch for better performance */
	setbits_le32(&pl310_regs_base->pl310_prefetch_ctrl,
		PL310_PREFETCH_CTRL_DATA_PREFETCH_MASK	|
		PL310_PREFETCH_CTRL_INST_PREFETCH_MASK	|
		PL310_PREFETCH_CTRL_DBL_LINEFILL_MASK);

	/* setup tag ram latency */
	writel(0, &pl310_regs_base->pl310_tag_latency_ctrl);

	/* setup data ram latency */
	writel(0x10, &pl310_regs_base->pl310_data_latency_ctrl);

	/* invalidate the cache before enable */
	v7_outer_cache_inval_all();

	/* enable the L2 cache */
	writel(0x1, &pl310_regs_base->pl310_ctrl);
}

void v7_outer_cache_disable(void)
{
	writel(0, &pl310_regs_base->pl310_ctrl);
}
#endif /* !CONFIG_SYS_L2CACHE_OFF */
