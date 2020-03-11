// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2018 Intel Corporation <www.intel.com>
 *
 */

#include <asm/io.h>
#include <asm/u-boot.h>
#include <asm/utils.h>
#include <common.h>
#include <debug_uart.h>
#include <dm.h>
#include <dm/ofnode.h>
#include <image.h>
#include <spl.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/firewall.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/smmu_s10.h>
#include <watchdog.h>
#include <dm/uclass.h>

DECLARE_GLOBAL_DATA_PTR;

static struct socfpga_system_manager *sysmgr_regs =
	(struct socfpga_system_manager *)SOCFPGA_SYSMGR_ADDRESS;

void board_init_f(ulong dummy)
{
	const struct cm_config *cm_default_cfg = cm_get_default_config();
	int ret;

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

	socfpga_per_reset(SOCFPGA_RESET(OSC1TIMER0), 0);
	timer_init();

	sysmgr_pinmux_init();

	/* configuring the HPS clocks */
	cm_basic_init(cm_default_cfg);

#ifdef CONFIG_DEBUG_UART
	socfpga_per_reset(SOCFPGA_RESET(UART0), 0);
	debug_uart_init();
#endif
	ret = spl_early_init();
	if (ret) {
		debug("spl_early_init() failed: %d\n", ret);
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

	/* disable ocram security at CCU for non secure access */
	clrbits_le32(CCU_REG_ADDR(CCU_CPU0_MPRT_ADMASK_MEM_RAM0),
		     CCU_ADMASK_P_MASK | CCU_ADMASK_NS_MASK);
	clrbits_le32(CCU_REG_ADDR(CCU_IOM_MPRT_ADMASK_MEM_RAM0),
		     CCU_ADMASK_P_MASK | CCU_ADMASK_NS_MASK);

#if CONFIG_IS_ENABLED(ALTERA_SDRAM)
		struct udevice *dev;

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

/* board specific function prior loading SSBL / U-Boot */
void spl_board_prepare_for_boot(void)
{
	mbox_hps_stage_notify(HPS_EXECUTION_STATE_SSBL);
}
