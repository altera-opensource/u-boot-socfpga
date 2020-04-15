// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2018 Intel Corporation <www.intel.com>
 *
 */

#include <hang.h>
#include <init.h>
#include <log.h>
#include <asm/global_data.h>
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
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/smmu_s10.h>
#include <asm/arch/system_manager.h>
#include <watchdog.h>
#include <dm/uclass.h>

DECLARE_GLOBAL_DATA_PTR;

u32 spl_boot_device(void)
{
	int ret, size;
	ofnode node;
	const fdt32_t *phandle_p;
	u32 phandle;
	struct udevice *dev;

	node = ofnode_path("/chosen");
	if (!ofnode_valid(node)) {
		debug("%s: /chosen node was not found.\n", __func__);
		goto fallback;
	}

	phandle_p = ofnode_get_property(node, "u-boot,boot0", &size);
	if (!phandle_p) {
		debug("%s: u-boot,boot0 property was not found.\n",
		     __func__);
		goto fallback;
	}

	phandle = fdt32_to_cpu(*phandle_p);

	node = ofnode_get_by_phandle(phandle);

	ret = device_get_global_by_ofnode(node, &dev);
	if (ret) {
		debug("%s: Boot device at not found, error: %d\n", __func__,
		      ret);
		goto fallback;
	}

	debug("%s: Found boot device %s\n", __func__, dev->name);

	switch (device_get_uclass_id(dev)) {
		case UCLASS_SPI_FLASH:
			return BOOT_DEVICE_SPI;
		case UCLASS_MISC:
			return BOOT_DEVICE_NAND;
		case UCLASS_MMC:
			return BOOT_DEVICE_MMC1;
		default:
			debug("%s: Booting from device uclass '%s' is not "
			      "supported\n", __func__,
			      dev_get_uclass_name(dev));
	}

fallback:
	/* Return default boot device */
	return BOOT_DEVICE_MMC1;
}

#ifdef CONFIG_SPL_MMC_SUPPORT
u32 spl_mmc_boot_mode(const u32 boot_device)
{
#if defined(CONFIG_SPL_FS_FAT) || defined(CONFIG_SPL_FS_EXT4)
	return MMCSD_MODE_FS;
#else
	return MMCSD_MODE_RAW;
#endif
}
#endif

void board_init_f(ulong dummy)
{
	const struct cm_config *cm_default_cfg = cm_get_default_config();
	int ret;

	ret = spl_early_init();
	if (ret)
		hang();

	socfpga_get_managers_addr();

	/* Ensure watchdog is paused when debugging is happening */
	writel(SYSMGR_WDDBG_PAUSE_ALL_CPU,
	       socfpga_get_sysmgr_addr() + SYSMGR_SOC64_WDDBG);

#ifdef CONFIG_HW_WATCHDOG
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

	preloader_console_init();
	print_reset_info();
	cm_print_clock_quick_summary();

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
