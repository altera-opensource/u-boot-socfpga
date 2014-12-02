/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <watchdog.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/ecc_ram.h>
#include <asm/sections.h>
#include <fdtdec.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * First C function to initialize the critical hardware early
 */
void s_init(void)
{
#ifdef CONFIG_OF_CONTROL
       int node;
#endif

#ifndef TEST_AT_ASIMOV
	/* Clear fake OCRAM ECC first as might triggered during power on */
	//clear_ecc_ocram_ecc_status();
#endif /***************** TEST_AT_ASIMOV *****************/

	/* Configure the L2 controller to make SDRAM start at 0	*/
	writel(0x1, SOCFPGA_MPUL2_ADRFLTR_START);

	/* re-setup watchdog */
	if (!(is_wdt_in_reset())) {
		/*
		 * only disabled if wdt not in reset state
		 * disable the watchdog prior PLL reconfiguration
		 */
		watchdog_disable();
	}

#ifdef CONFIG_HW_WATCHDOG
	/* release osc1 watchdog timer 0 from reset */
	reset_deassert_osc1wd0();

	/* reconfigure and enable the watchdog */
	hw_watchdog_init();
	WATCHDOG_RESET();
#endif /* CONFIG_HW_WATCHDOG */

#ifdef CONFIG_OF_CONTROL
	/* We need to access to FDT as this stage */
	memset((void *)gd, 0, sizeof(gd_t));
	/* FDT is at end of image */
	gd->fdt_blob = (void *)(_end);
	/* Check whether we have a valid FDT or not. */
	if (fdtdec_prepare_fdt()) {
		panic("** CONFIG_OF_CONTROL defined but no FDT - please see "
			"doc/README.fdt-control");
	}
#endif /* CONFIG_OF_CONTROL */

#ifndef TEST_AT_ASIMOV
	/* assert reset to all except L4WD0 and L4TIMER0 */
	reset_assert_all_peripherals_except_l4wd0_l4timer0();
#endif

	/* Initialize the timer */
	timer_init();

	/* configuring the clock based on handoff */
	cm_basic_init();
	WATCHDOG_RESET();

#ifndef TEST_AT_ASIMOV
	/* configure the pin muxing */
#if (CONFIG_PRELOADER_OVERWRITE_DEDICATED == 1)
	sysmgr_pinmux_init_dedicated(SOCFPGA_PINMUX_DEDICATED_IO_ADDRESS,
		SOCFPGA_PINMUX_DEDICATED_IO_CFG_ADDRESS);
#else
	sysmgr_pinmux_init(SOCFPGA_PINMUX_DEDICATED_IO_ADDRESS,
		sys_mgr_init_table_dedicated,
		CONFIG_HPS_PINMUX_NUM_DEDICATED);
	sysmgr_pinmux_init(SOCFPGA_PINMUX_DEDICATED_IO_CFG_ADDRESS,
		sys_mgr_init_table_dedicated_cfg,
		CONFIG_HPS_PINMUX_NUM_DEDICATED_CFG);
#endif /* CONFIG_PRELOADER_OVERWRITE_DEDICATED */

	sysmgr_pinmux_init(SOCFPGA_PINMUX_SHARED_3V_IO_ADDRESS,
		sys_mgr_init_table_shared,
		CONFIG_HPS_PINMUX_NUM_SHARED);
	sysmgr_pinmux_init(SOCFPGA_PINMUX_FPGA_INTERFACE_ADDRESS,
		sys_mgr_init_table_fpga,
		CONFIG_HPS_PINMUX_NUM_FPGA);
	WATCHDOG_RESET();

	/* configure the Reset Manager */
	reset_deassert_peripherals_handoff();
	reset_deassert_bridges_handoff();

#else
	/* DTB pinmux test area at Asimov */
#ifdef CONFIG_OF_CONTROL
	node = fdtdec_prepare_fdt();
	printf("fdtdec_prepare_fdt() returned %d\n", node);
#endif /* CONFIG_OF_CONTROL */

#endif /* TEST_AT_ASIMOV */
}
