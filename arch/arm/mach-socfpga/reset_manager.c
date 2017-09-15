/*
 *  Copyright (C) 2013 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */


#include <common.h>
#include <asm/io.h>
#include <asm/arch/reset_manager.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_reset_manager *reset_manager_base =
		(void *)SOCFPGA_RSTMGR_ADDRESS;

/*
 * Write the reset manager register to cause reset
 */
void reset_cpu(ulong addr)
{
	/* request a warm reset */
#if defined(CONFIG_TARGET_SOCFPGA_STRATIX10)
	writel((1 << RSTMGR_MPUMODRST_CORE0),
		&reset_manager_base->mpu_mod_reset);
#else
	writel(1 << RSTMGR_CTRL_SWWARMRSTREQ_LSB,
	       &reset_manager_base->ctrl);
#endif
	/*
	 * infinite loop here as watchdog will trigger and reset
	 * the processor
	 */
	while (1)
		;
}
