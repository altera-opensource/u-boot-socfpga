/*
 * Copyright (C) 2016-2017 Intel Corporation <www.intel.com>
 * Copyright (C) 2012-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/timer.h>

#define TIMER_LOAD_VAL		0xFFFFFFFF

#ifdef CONFIG_TARGET_SOCFPGA_GEN5
static const struct socfpga_timer *timer_base = (void *)CONFIG_SYS_TIMERBASE;
#endif

/*
 * Timer initialization
 */
int timer_init(void)
{
#if defined(CONFIG_TARGET_SOCFPGA_STRATIX10)
	int enable = 0x3;	/* timer enable + output signal masked */
	int loadval = ~0;

	/* enable system counter */
	writel(enable, SOCFPGA_GTIMER_SEC_ADDRESS);
	/* enable processor pysical counter */
	asm volatile("msr cntp_ctl_el0, %0" : : "r" (enable));
	asm volatile("msr cntp_tval_el0, %0" : : "r" (loadval));

#else
	writel(TIMER_LOAD_VAL, &timer_base->load_val);
	writel(TIMER_LOAD_VAL, &timer_base->curr_val);
	writel(readl(&timer_base->ctrl) | 0x3, &timer_base->ctrl);
#endif
	return 0;
}
