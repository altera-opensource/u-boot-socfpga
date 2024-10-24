// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019-2022 Intel Corporation <www.intel.com>
 *
 */

#include <init.h>
#include <log.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/u-boot.h>
#include <asm/utils.h>
#include <hang.h>
#include <image.h>
#include <spl.h>
#include <asm/arch/base_addr_soc64.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/firewall.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/smmu_s10.h>
#include <asm/arch/system_manager.h>
#include <wdt.h>
#include <dm/uclass.h>

DECLARE_GLOBAL_DATA_PTR;

void board_init_f(ulong dummy)
{
	int ret;
	struct udevice *dev;

	ret = spl_early_init();
	if (ret)
		hang();

	socfpga_get_managers_addr();

	sysmgr_pinmux_init();

	if (!(IS_ENABLED(CONFIG_TARGET_SOCFPGA_AGILEX5_SIMICS) ||
	      IS_ENABLED(CONFIG_TARGET_SOCFPGA_AGILEX5_EMU))) {
		/* Ensure watchdog is paused when debugging is happening */
		writel(SYSMGR_WDDBG_PAUSE_ALL_CPU,
		       socfpga_get_sysmgr_addr() + SYSMGR_SOC64_WDDBG);
	}

	timer_init();

	ret = uclass_get_device(UCLASS_CLK, 0, &dev);
	if (ret) {
		debug("Clock init failed: %d\n", ret);
		hang();
	}

	if (!(IS_ENABLED(CONFIG_TARGET_SOCFPGA_AGILEX5_SIMICS) ||
	      IS_ENABLED(CONFIG_TARGET_SOCFPGA_AGILEX5_EMU))) {
		/*
		 * Enable watchdog as early as possible before initializing other
		 * component. Watchdog need to be enabled after clock driver because
		 * it will retrieve the clock frequency from clock driver.
		 */
		if (CONFIG_IS_ENABLED(WDT))
			initr_watchdog();
	}

	preloader_console_init();
	print_reset_info();
	cm_print_clock_quick_summary();

	ret = uclass_get_device_by_name(UCLASS_NOP, "socfpga-secreg", &dev);
	if (ret) {
		printf("Firewall & secure settings init failed: %d\n", ret);
		hang();
	}

#if CONFIG_IS_ENABLED(ALTERA_SDRAM)
	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret) {
		debug("DRAM init failed: %d\n", ret);
		hang();
	}
#endif

#if CONFIG_IS_ENABLED(PHY_CADENCE_COMBOPHY)
	u32 tmp = SYSMGR_SOC64_COMBOPHY_DFISEL_SDMMC;

	/* manually deassert for COMBOPHY & SDMMC for only RAM boot */
	clrbits_le32(SOCFPGA_RSTMGR_ADDRESS + RSTMGR_SOC64_PER0MODRST, BIT(6));
	clrbits_le32(SOCFPGA_RSTMGR_ADDRESS + RSTMGR_SOC64_PER0MODRST, BIT(7));

	/* configure DFI_SEL for SDMMC */
	writel(tmp, socfpga_get_sysmgr_addr() + SYSMGR_SOC64_COMBOPHY_DFISEL);
#endif
	/* configure default base clkmgr clock - 200MHz */
	writel((readl(socfpga_get_clkmgr_addr() + CLKMGR_MAINPLL_NOCDIV)
		& 0xfffcffff) |
		(CLKMGR_NOCDIV_SOFTPHY_DIV_ONE << CLKMGR_NOCDIV_SOFTPHY_OFFSET),
		socfpga_get_clkmgr_addr() + CLKMGR_MAINPLL_NOCDIV);


	mbox_init();

#ifdef CONFIG_CADENCE_QSPI
	mbox_qspi_open();
#endif

	/* Enable non secure access to ocram */
	clrbits_le32(SOCFPGA_OCRAM_FIREWALL_ADDRESS + 0x18, BIT(0));
}
