/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/u-boot.h>
#include <asm/utils.h>
#include <version.h>
#include <image.h>
#include <malloc.h>
#include <asm/arch/reset_manager.h>
#include <spl.h>
#include <asm/spl.h>
#include <asm/arch/spl.h>
#include <watchdog.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/debug_memory.h>
#include <asm/arch/ecc_ram.h>
#include <asm/arch/freeze_controller.h>
#include <asm/arch/interrupts.h>
#include <asm/arch/nic301.h>
#include <asm/arch/scan_manager.h>
#include <asm/arch/sdram.h>
#include <asm/arch/system_manager.h>

DECLARE_GLOBAL_DATA_PTR;

#if (CONFIG_PRELOADER_WARMRST_SKIP_CFGIO == 1)
u32 rst_mgr_status;
#endif

u32 spl_boot_device(void)
{
#if (CONFIG_PRELOADER_BOOT_FROM_QSPI == 1)
	return BOOT_DEVICE_SPI;
#elif (CONFIG_PRELOADER_BOOT_FROM_RAM == 1)
	return BOOT_DEVICE_RAM;
#else
	return BOOT_DEVICE_MMC1;
#endif
}

/* sdmmc boot mode is raw instead fat */
u32 spl_boot_mode(void)
{
	return MMCSD_MODE_MBR;
}

static void init_boot_params(void)
{
#if (CONFIG_PRELOADER_DEBUG_MEMORY_WRITE == 1)
	/* Pass the debug memory address  to next boot image */
	boot_params_ptr = CONFIG_PRELOADER_DEBUG_MEMORY_ADDR;
#else
	boot_params_ptr = 0;
#endif
}


/*
 * Board initialization after bss clearance
 */
void spl_board_init(void)
{
#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
	cm_config_t cm_default_cfg = {
		/* main group */
		MAIN_VCO_BASE,
		CLKMGR_MAINPLLGRP_MPUCLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_MPUCLK_CNT),
		CLKMGR_MAINPLLGRP_MAINCLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_MAINCLK_CNT),
		CLKMGR_MAINPLLGRP_DBGATCLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_DBGATCLK_CNT),
		CLKMGR_MAINPLLGRP_MAINQSPICLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_MAINQSPICLK_CNT),
		CLKMGR_PERPLLGRP_PERNANDSDMMCCLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_MAINNANDSDMMCCLK_CNT),
		CLKMGR_MAINPLLGRP_CFGS2FUSER0CLK_CNT_SET(
			CONFIG_HPS_MAINPLLGRP_CFGS2FUSER0CLK_CNT),
		CLKMGR_MAINPLLGRP_MAINDIV_L3MPCLK_SET(
			CONFIG_HPS_MAINPLLGRP_MAINDIV_L3MPCLK) |
		CLKMGR_MAINPLLGRP_MAINDIV_L3SPCLK_SET(
			CONFIG_HPS_MAINPLLGRP_MAINDIV_L3SPCLK) |
		CLKMGR_MAINPLLGRP_MAINDIV_L4MPCLK_SET(
			CONFIG_HPS_MAINPLLGRP_MAINDIV_L4MPCLK) |
		CLKMGR_MAINPLLGRP_MAINDIV_L4SPCLK_SET(
			CONFIG_HPS_MAINPLLGRP_MAINDIV_L4SPCLK),
		CLKMGR_MAINPLLGRP_DBGDIV_DBGATCLK_SET(
			CONFIG_HPS_MAINPLLGRP_DBGDIV_DBGATCLK) |
		CLKMGR_MAINPLLGRP_DBGDIV_DBGCLK_SET(
			CONFIG_HPS_MAINPLLGRP_DBGDIV_DBGCLK),
		CLKMGR_MAINPLLGRP_TRACEDIV_TRACECLK_SET(
			CONFIG_HPS_MAINPLLGRP_TRACEDIV_TRACECLK),
		CLKMGR_MAINPLLGRP_L4SRC_L4MP_SET(
			CONFIG_HPS_MAINPLLGRP_L4SRC_L4MP) |
		CLKMGR_MAINPLLGRP_L4SRC_L4SP_SET(
			CONFIG_HPS_MAINPLLGRP_L4SRC_L4SP),

		/* peripheral group */
		PERI_VCO_BASE,
		CLKMGR_PERPLLGRP_EMAC0CLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_EMAC0CLK_CNT),
		CLKMGR_PERPLLGRP_EMAC1CLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_EMAC1CLK_CNT),
		CLKMGR_PERPLLGRP_PERQSPICLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_PERQSPICLK_CNT),
		CLKMGR_PERPLLGRP_PERNANDSDMMCCLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_PERNANDSDMMCCLK_CNT),
		CLKMGR_PERPLLGRP_PERBASECLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_PERBASECLK_CNT),
		CLKMGR_PERPLLGRP_S2FUSER1CLK_CNT_SET(
			CONFIG_HPS_PERPLLGRP_S2FUSER1CLK_CNT),
		CLKMGR_PERPLLGRP_DIV_USBCLK_SET(
			CONFIG_HPS_PERPLLGRP_DIV_USBCLK) |
		CLKMGR_PERPLLGRP_DIV_SPIMCLK_SET(
			CONFIG_HPS_PERPLLGRP_DIV_SPIMCLK) |
		CLKMGR_PERPLLGRP_DIV_CAN0CLK_SET(
			CONFIG_HPS_PERPLLGRP_DIV_CAN0CLK) |
		CLKMGR_PERPLLGRP_DIV_CAN1CLK_SET(
			CONFIG_HPS_PERPLLGRP_DIV_CAN1CLK),
		CLKMGR_PERPLLGRP_GPIODIV_GPIODBCLK_SET(
			CONFIG_HPS_PERPLLGRP_GPIODIV_GPIODBCLK),
		CLKMGR_PERPLLGRP_SRC_QSPI_SET(
			CONFIG_HPS_PERPLLGRP_SRC_QSPI) |
		CLKMGR_PERPLLGRP_SRC_NAND_SET(
			CONFIG_HPS_PERPLLGRP_SRC_NAND) |
		CLKMGR_PERPLLGRP_SRC_SDMMC_SET(
			CONFIG_HPS_PERPLLGRP_SRC_SDMMC),

		/* sdram pll group */
		SDR_VCO_BASE,
		CLKMGR_SDRPLLGRP_DDRDQSCLK_PHASE_SET(
			CONFIG_HPS_SDRPLLGRP_DDRDQSCLK_PHASE) |
		CLKMGR_SDRPLLGRP_DDRDQSCLK_CNT_SET(
			CONFIG_HPS_SDRPLLGRP_DDRDQSCLK_CNT),
		CLKMGR_SDRPLLGRP_DDR2XDQSCLK_PHASE_SET(
			CONFIG_HPS_SDRPLLGRP_DDR2XDQSCLK_PHASE) |
		CLKMGR_SDRPLLGRP_DDR2XDQSCLK_CNT_SET(
			CONFIG_HPS_SDRPLLGRP_DDR2XDQSCLK_CNT),
		CLKMGR_SDRPLLGRP_DDRDQCLK_PHASE_SET(
			CONFIG_HPS_SDRPLLGRP_DDRDQCLK_PHASE) |
		CLKMGR_SDRPLLGRP_DDRDQCLK_CNT_SET(
			CONFIG_HPS_SDRPLLGRP_DDRDQCLK_CNT),
		CLKMGR_SDRPLLGRP_S2FUSER2CLK_PHASE_SET(
			CONFIG_HPS_SDRPLLGRP_S2FUSER2CLK_PHASE) |
		CLKMGR_SDRPLLGRP_S2FUSER2CLK_CNT_SET(
			CONFIG_HPS_SDRPLLGRP_S2FUSER2CLK_CNT),
	};
#endif /* CONFIG_SOCFPGA_VIRTUAL_TARGET */

#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	debug("Freezing all I/O banks\n");
	/* freeze all IO banks */
	sys_mgr_frzctrl_freeze_req(FREEZE_CHANNEL_0,
		FREEZE_CONTROLLER_FSM_SW);
	sys_mgr_frzctrl_freeze_req(FREEZE_CHANNEL_1,
		FREEZE_CONTROLLER_FSM_SW);
	sys_mgr_frzctrl_freeze_req(FREEZE_CHANNEL_2,
		FREEZE_CONTROLLER_FSM_SW);
	sys_mgr_frzctrl_freeze_req(FREEZE_CHANNEL_3,
		FREEZE_CONTROLLER_FSM_SW);
#endif /* CONFIG_SOCFPGA_VIRTUAL_TARGET */


#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	debug("Asserting reset to all except L4WD and SDRAM\n");
	/*
	 * assert all peripherals and bridges to reset. This is
	 * to ensure no glitch happen during PLL re-configuration
	 */
	reset_assert_all_peripherals_except_l4wd0();
#if (CONFIG_PRELOADER_EXE_ON_FPGA == 0)
	reset_assert_all_bridges();
#endif

	DEBUG_MEMORY
	debug("Deassert reset for OSC1 Timer\n");
	/*
	 * deassert reset for osc1timer0. We need this for delay
	 * function that required during PLL re-configuration
	 */
	reset_deassert_osc1timer0();

	DEBUG_MEMORY
	debug("Init timer\n");
	/* init timer for enabling delay function */
	timer_init();


#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	debug("Reconfigure Clock Manager\n");
	/* reconfigure the PLLs */
	cm_basic_init(&cm_default_cfg);

#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	/* Skip configuration is warm reset happen and WARMRSTCFGIO set */
#if (CONFIG_PRELOADER_WARMRST_SKIP_CFGIO == 1)
	if (((readl(CONFIG_SYSMGR_ROMCODEGRP_CTRL) &
	SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGIO) == 0) ||
	((rst_mgr_status & RSTMGR_WARMRST_MASK) == 0)) {
#endif /* CONFIG_PRELOADER_WARMRST_SKIP_CFGIO */
#if (CONFIG_PRELOADER_BOOTROM_HANDSHAKE_CFGIO == 1)
		/* Enable handshake bit with BootROM */
		setbits_le32(CONFIG_SYSMGR_ROMCODEGRP_CTRL,
			SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGIO);
#endif /* CONFIG_PRELOADER_BOOTROM_HANDSHAKE_CFGIO */
		debug("Configure IOCSR\n");
		/* configure the IOCSR through scan chain */
		scan_mgr_io_scan_chain_prg(
			IO_SCAN_CHAIN_0,
			CONFIG_HPS_IOCSR_SCANCHAIN0_LENGTH,
			iocsr_scan_chain0_table);
		scan_mgr_io_scan_chain_prg(
			IO_SCAN_CHAIN_1,
			CONFIG_HPS_IOCSR_SCANCHAIN1_LENGTH,
			iocsr_scan_chain1_table);
		scan_mgr_io_scan_chain_prg(
			IO_SCAN_CHAIN_2,
			CONFIG_HPS_IOCSR_SCANCHAIN2_LENGTH,
			iocsr_scan_chain2_table);
		scan_mgr_io_scan_chain_prg(
			IO_SCAN_CHAIN_3,
			CONFIG_HPS_IOCSR_SCANCHAIN3_LENGTH,
			iocsr_scan_chain3_table);
#if (CONFIG_PRELOADER_BOOTROM_HANDSHAKE_CFGIO == 1)
		/* Clear handshake bit with BootROM */
		DEBUG_MEMORY
		clrbits_le32(CONFIG_SYSMGR_ROMCODEGRP_CTRL,
			SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGIO);
#endif /* CONFIG_PRELOADER_BOOTROM_HANDSHAKE_CFGIO */
#if (CONFIG_PRELOADER_WARMRST_SKIP_CFGIO == 1)
	}
#endif /* CONFIG_PRELOADER_WARMRST_SKIP_CFGIO */

#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
#if (CONFIG_PRELOADER_WARMRST_SKIP_CFGIO == 1)
	/* Skip configuration is warm reset happen and WARMRSTCFGPINMUX set */
	if (((readl(CONFIG_SYSMGR_ROMCODEGRP_CTRL) &
	SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGPINMUX) == 0) ||
	((rst_mgr_status & RSTMGR_WARMRST_MASK) == 0)) {
#endif /* CONFIG_PRELOADER_WARMRST_SKIP_CFGIO */
#if (CONFIG_PRELOADER_BOOTROM_HANDSHAKE_CFGIO == 1)
		/* Enable handshake bit with BootROM */
		setbits_le32(CONFIG_SYSMGR_ROMCODEGRP_CTRL,
			SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGPINMUX);
#endif /* CONFIG_PRELOADER_BOOTROM_HANDSHAKE_CFGIO */
		/* configure the pin muxing through system manager */
		sysmgr_pinmux_init();
#if (CONFIG_PRELOADER_BOOTROM_HANDSHAKE_CFGIO == 1)
		/* Clear handshake bit with BootROM */
		DEBUG_MEMORY
		clrbits_le32(CONFIG_SYSMGR_ROMCODEGRP_CTRL,
			SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGPINMUX);
#endif /* CONFIG_PRELOADER_BOOTROM_HANDSHAKE_CFGIO */
#if (CONFIG_PRELOADER_WARMRST_SKIP_CFGIO == 1)
	}
#endif /* CONFIG_PRELOADER_WARMRST_SKIP_CFGIO */
#endif /* CONFIG_SOCFPGA_VIRTUAL_TARGET */


#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	debug("Deasserting resets\n");
	/* de-assert reset for peripherals and bridges based on handoff */
#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
	reset_deassert_peripherals_handoff();
#else
	reset_deassert_all_peripherals();
#endif
#if (CONFIG_PRELOADER_EXE_ON_FPGA == 0)
	reset_deassert_bridges_handoff();
#endif


#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	debug("Unfreezing/Thaw all I/O banks\n");
	/* unfreeze / thaw all IO banks */
	sys_mgr_frzctrl_thaw_req(FREEZE_CHANNEL_0,
		FREEZE_CONTROLLER_FSM_SW);
	sys_mgr_frzctrl_thaw_req(FREEZE_CHANNEL_1,
		FREEZE_CONTROLLER_FSM_SW);
	sys_mgr_frzctrl_thaw_req(FREEZE_CHANNEL_2,
		FREEZE_CONTROLLER_FSM_SW);
	sys_mgr_frzctrl_thaw_req(FREEZE_CHANNEL_3,
		FREEZE_CONTROLLER_FSM_SW);
#endif	/* CONFIG_SOCFPGA_VIRTUAL_TARGET */


#ifdef CONFIG_SPL_SERIAL_SUPPORT
#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	/* enable console uart printing */
	preloader_console_init();
#endif


#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
#ifndef CONFIG_PRELOADER_SKIP_SDRAM
#error "CONFIG_PRELOADER_SKIP_SDRAM must be defined"
#endif /* CONFIG_PRELOADER_SKIP_SDRAM */
#if (CONFIG_PRELOADER_SKIP_SDRAM == 0)
#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	puts("SDRAM : Initializing MMR registers\n");
	/* SDRAM MMR initialization */
	if (sdram_mmr_init_full() != 0)
		hang();

#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	puts("SDRAM : Calibrating PHY\n");
	/* SDRAM calibration */
	if (sdram_calibration_full() == 0)
		hang();
#endif	/* CONFIG_PRELOADER_SKIP_SDRAM */

#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif
	DEBUG_MEMORY
	/* configure the interconnect NIC-301 security */
	nic301_slave_ns();
#endif	/* CONFIG_SOCFPGA_VIRTUAL_TARGET */


#ifdef CONFIG_USE_IRQ
	debug("Setup interrupt controller... IRQ SP at 0x%08x "
		"with size 0x%08x\n", __irq_stack_start,
		CONFIG_STACKSIZE_IRQ);
	/* setup the stack pointer for IRQ */
	gd->irq_sp = (unsigned long)&__irq_stack_start;
	DEBUG_MEMORY
	/* set up exceptions */
	interrupt_init();
	/* enable exceptions */
	enable_interrupts();
	/* register on-chip RAM ECC handler */
	irq_register(IRQ_ECC_OCRAM_CORRECTED, irq_handler_ecc_ocram_corrected,
		(void *)&irq_cnt_ecc_ocram_corrected, 0);
	irq_register(IRQ_ECC_OCRAM_UNCORRECTED,
		irq_handler_ecc_ocram_uncorrected,
		(void *)&irq_cnt_ecc_ocram_uncorrected, 0);
	/* register SDRAM ECC handler */
	irq_register(IRQ_ECC_SDRAM,
		irq_handler_ecc_sdram,
		(void *)&irq_cnt_ecc_sdram, 0);
#endif

#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif

	/*
	 * if not boot specified, its special mode where user wish to loop
	 * here after all hardware being initialized include the SDRAM
	 */
#if !defined(CONFIG_SPL_RAM_DEVICE) && !defined(CONFIG_SPL_MMC_SUPPORT) && \
!defined(CONFIG_SPL_NAND_SUPPORT) && !defined(CONFIG_SPL_SPI_SUPPORT)
	puts("\nNo boot device selected. Just loop here...\n");
	for (;;) {
		__udelay(1);
#ifdef CONFIG_HW_WATCHDOG
		WATCHDOG_RESET();
#endif
	}
#endif

	DEBUG_MEMORY
	init_boot_params();
}
