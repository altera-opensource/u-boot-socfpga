// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <dm.h>
#include <env.h>
#include <errno.h>
#include <sysreset.h>
#include <asm/arch/reset_manager.h>
#include <asm/system.h>

static int socfpga_sysreset_request(struct udevice *dev,
				    enum sysreset_t type)
{
#if !defined(CONFIG_SPL_BUILD) && defined(CONFIG_SPL_ATF)
	const char *reset = env_get("reset");

	if (reset && !strcmp(reset, "warm")) {
		/* request a warm reset */
		puts("Do warm reset now...\n");

		/* doing architecture system reset */
		psci_system_reset2(0, 0);
	} else {
		puts("Issuing cold reset REBOOT_HPS\n");
		psci_system_reset();
	}
#endif

	return -EINPROGRESS;
}

static struct sysreset_ops socfpga_sysreset = {
	.request = socfpga_sysreset_request,
};

U_BOOT_DRIVER(sysreset_socfpga) = {
	.id	= UCLASS_SYSRESET,
	.name	= "socfpga_sysreset",
	.ops	= &socfpga_sysreset,
};
