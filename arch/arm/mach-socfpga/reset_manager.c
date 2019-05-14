// SPDX-License-Identifier: GPL-2.0+
/*
 *  Copyright (C) 2013 Altera Corporation <www.altera.com>
 */


#include <common.h>
#include <asm/io.h>
#include <asm/arch/reset_manager.h>

#if defined(CONFIG_TARGET_SOCFPGA_STRATIX10)
#include <asm/arch/mailbox_s10.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#if !defined(CONFIG_TARGET_SOCFPGA_STRATIX10)
static const struct socfpga_reset_manager *reset_manager_base =
		(void *)SOCFPGA_RSTMGR_ADDRESS;
#endif

/*
 * Write the reset manager register to cause reset
 */
void reset_cpu(ulong addr)
{
#if defined(CONFIG_TARGET_SOCFPGA_STRATIX10)
#ifndef CONFIG_SPL_BUILD
	const char *reset = env_get("reset");

	if (reset && !strcmp(reset, "warm")) {
		/* flush dcache */
		flush_dcache_all();

		/* request a warm reset */
		puts("Do warm reset now...\n");
		l2_reset_cpu();
	} else {
#endif
		puts("Mailbox: Issuing mailbox cmd REBOOT_HPS\n");
		mbox_reset_cold();
#ifndef CONFIG_SPL_BUILD
	}
#endif
#else
	/* request a warm reset */
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
