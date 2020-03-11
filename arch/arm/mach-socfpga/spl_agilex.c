// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Intel Corporation <www.intel.com>
 *
 */

#include <asm/io.h>
#include <asm/u-boot.h>
#include <asm/utils.h>
#include <common.h>
#include <image.h>
#include <spl.h>
#include <asm/arch/ccu_agilex.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/firewall.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/smmu_s10.h>
#include <asm/arch/system_manager.h>
#include <watchdog.h>
#include <dm/uclass.h>

DECLARE_GLOBAL_DATA_PTR;

static struct socfpga_system_manager *sysmgr_regs =
	(struct socfpga_system_manager *)SOCFPGA_SYSMGR_ADDRESS;

void board_init_f(ulong dummy)
{
	int ret;
	struct udevice *dev;

#ifdef CONFIG_HW_WATCHDOG
	/* Ensure watchdog is paused when debugging is happening */
	writel(SYSMGR_WDDBG_PAUSE_ALL_CPU, &sysmgr_regs->wddbg);

	/* Enable watchdog before initializing the HW */
	socfpga_per_reset(SOCFPGA_RESET(L4WD0), 1);
	socfpga_per_reset(SOCFPGA_RESET(L4WD0), 0);
	hw_watchdog_init();
#endif

	/* ensure all processors are not released prior Linux boot */
	writeq(0, CPU_RELEASE_ADDR);

	timer_init();

	sysmgr_pinmux_init();

	ret = spl_early_init();
	if (ret) {
		debug("spl_early_init() failed: %d\n", ret);
		hang();
	}

	ret = uclass_get_device(UCLASS_CLK, 0, &dev);
	if (ret) {
		debug("Clock init failed: %d\n", ret);
		hang();
	}

	preloader_console_init();
	print_reset_info();
	cm_print_clock_quick_summary();

	/* enable non-secure interface to DMA330 DMA and peripherals */
	writel(SYSMGR_DMA_IRQ_NS | SYSMGR_DMA_MGR_NS, &sysmgr_regs->dma);
	writel(SYSMGR_DMAPERIPH_ALL_NS, &sysmgr_regs->dma_periph);

	firewall_setup();

	/* Setup and Initialize SMMU */
	socfpga_init_smmu();

	ccu_init();

#if CONFIG_IS_ENABLED(ALTERA_SDRAM)
	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret) {
		debug("DRAM init failed: %d\n", ret);
		hang();
	}
#endif

	mbox_init();

#ifdef CONFIG_CADENCE_QSPI
	mbox_qspi_open();
#endif
}
