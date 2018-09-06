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

void l2_reset_cpu(void)
{
	asm volatile(
		"str	%0, [%1]\n"
		/* Increase timeout in rstmgr.hdsktimeout */
		"ldr	x2, =0xFFFFFF\n"
		"str	w2, [%2, #0x64]\n"
		"str	w2, [%2, #0x10]\n"
		/*
		 * Set l2flushen = 1, etrstallen = 1,
		 * fpgahsen = 1 and sdrselfrefen = 1
		 * in rstmgr.hdsken to perform handshake
		 * in certain peripherals before trigger
		 * L2 reset.
		 */
		"ldr	x3, =0x10D\n"
		"orr	x2, x2, x3\n"
		"str	w2, [%2, #0x10]\n"
		/* Trigger L2 reset in rstmgr.coldmodrst */
		"ldr	w2, [%2, #0x34]\n"
		"orr	x2, x2, #0x100\n"
		"isb\n"
		"dsb	sy\n"
		"str	w2, [%2, #0x34]\n"
		/* Put all cores into WFI mode */
		"wfi_loop:\n"
		"	wfi\n"
		"	b	wfi_loop\n"
		: : "r" (L2_RESET_DONE_STATUS),
		    "r" (L2_RESET_DONE_REG),
		    "r" (SOCFPGA_RSTMGR_ADDRESS)
		: "x1", "x2", "x3");
}
