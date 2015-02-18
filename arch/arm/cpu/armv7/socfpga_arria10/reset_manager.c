/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/fpga_manager.h>

DECLARE_GLOBAL_DATA_PTR;

unsigned long rst_mgr_status;

static const struct socfpga_reset_manager *reset_manager_base =
		(void *)SOCFPGA_RSTMGR_ADDRESS;

static const struct socfpga_system_manager *system_manager_base =
		(void *)SOCFPGA_SYSMGR_ADDRESS;

/* Release NOC ddr scheduler from reset */
void reset_deassert_noc_ddr_scheduler(void)
{
	clrbits_le32(&reset_manager_base->brgmodrst,
		ALT_RSTMGR_BRGMODRST_DDRSCH_SET_MSK);
}

/* Disable the watchdog (toggle reset to watchdog) */
void watchdog_disable(void)
{
	/* assert reset for watchdog */
	setbits_le32(&reset_manager_base->per1modrst,
		ALT_RSTMGR_PER1MODRST_WD0_SET_MSK);

	/* deassert watchdog from reset (watchdog in not running state) */
//	clrbits_le32(&reset_manager_base->per1modrst,
//		ALT_RSTMGR_PER1MODRST_WD0_SET_MSK);
}

/* Check whether Watchdog in reset state? */
int is_wdt_in_reset(void)
{
	unsigned long val;

	val = readl(&reset_manager_base->per1modrst);
	val &= ALT_RSTMGR_PER1MODRST_WD0_SET_MSK;

	/* return 0x1 if watchdog in reset */
	return val;
}

/* Write the reset manager register to cause reset */
void reset_cpu(ulong addr)
{
	/* request a warm reset */
	writel(ALT_RSTMGR_CTL_SWWARMRSTREQ_SET_MSK,
		&reset_manager_base->ctrl);
	/*
	 * infinite loop here as watchdog will trigger and reset
	 * the processor
	 */
	while (1)
		;
}


/* emacbase: base address of emac to enable/disable reset
 * state: 0 - disable reset, !0 - enable reset
 */
void emac_manage_reset(ulong emacbase, uint state)
{
	ulong eccmask;
	ulong emacmask;
	switch (emacbase) {
		case SOCFPGA_EMAC0_ADDRESS:
			eccmask = ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK;
			emacmask = ALT_RSTMGR_PER0MODRST_EMAC0_SET_MSK;
			break;
		case SOCFPGA_EMAC1_ADDRESS:
			eccmask = ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK;
			emacmask = ALT_RSTMGR_PER0MODRST_EMAC1_SET_MSK;
			break;
		case SOCFPGA_EMAC2_ADDRESS:
			eccmask = ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK;
			emacmask = ALT_RSTMGR_PER0MODRST_EMAC2_SET_MSK;
			break;
		default:
			error("emac base address unexpected! %lx", emacbase);
			hang();
			break;
	}

	if (state) {
		/* Enable ECC OCP first */
		setbits_le32(&reset_manager_base->per0modrst, eccmask);
		setbits_le32(&reset_manager_base->per0modrst, emacmask);
	} else {
		/* Disable ECC OCP first */
		clrbits_le32(&reset_manager_base->per0modrst, emacmask);
		clrbits_le32(&reset_manager_base->per0modrst, eccmask);
	}
}

/* Disable all the bridges (hps2fpga, lwhps2fpga, fpga2hps, fpga2sdram) */
void reset_assert_all_bridges(void)
{
	/* set idle request to all bridges */
	writel(ALT_SYSMGR_NOC_H2F_SET_MSK |
		ALT_SYSMGR_NOC_LWH2F_SET_MSK |
		ALT_SYSMGR_NOC_F2H_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR0_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR1_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR2_SET_MSK,
		&system_manager_base->noc_idlereq_set);

	/* Enable the NOC timeout */
	writel(ALT_SYSMGR_NOC_TMO_EN_SET_MSK,
		&system_manager_base->noc_timeout);

	/* Poll until all idleack to 1 */
	while ((readl(&system_manager_base->noc_idleack) &
		(ALT_SYSMGR_NOC_H2F_SET_MSK |
		ALT_SYSMGR_NOC_LWH2F_SET_MSK |
		ALT_SYSMGR_NOC_F2H_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR0_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR1_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR2_SET_MSK)) == 0)
		;

	/* Poll until all idlestatus to 1 */
	while ((readl(&system_manager_base->noc_idlestatus) &
		(ALT_SYSMGR_NOC_H2F_SET_MSK |
		ALT_SYSMGR_NOC_LWH2F_SET_MSK |
		ALT_SYSMGR_NOC_F2H_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR0_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR1_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR2_SET_MSK)) == 0)
		;

	/* Put all bridges (except NOR DDR scheduler) into reset state */
	setbits_le32(&reset_manager_base->brgmodrst,
		(ALT_RSTMGR_BRGMODRST_H2F_SET_MSK |
		ALT_RSTMGR_BRGMODRST_LWH2F_SET_MSK |
		ALT_RSTMGR_BRGMODRST_F2H_SET_MSK |
		ALT_RSTMGR_BRGMODRST_F2SSDRAM0_SET_MSK |
		ALT_RSTMGR_BRGMODRST_F2SSDRAM1_SET_MSK |
		ALT_RSTMGR_BRGMODRST_F2SSDRAM2_SET_MSK));
}

/* Enable bridges (hps2fpga, lwhps2fpga, fpga2hps, fpga2sdram) per handoff */
void reset_deassert_bridges_handoff(void)
{
	unsigned long mask_noc = 0, mask_rstmgr = 0;

	/* skip bridge releasing if FPGA is blank */
	if (is_fpgamgr_fpga_ready() == 0)
		return;

	/* setup the mask for NOC register based on handoff */
#if (CONFIG_HPS_RESET_ASSERT_HPS2FPGA == 0)
	mask_noc |= ALT_SYSMGR_NOC_H2F_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_LWHPS2FPGA == 0)
	mask_noc |= ALT_SYSMGR_NOC_LWH2F_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA2HPS == 0)
	mask_noc |= ALT_SYSMGR_NOC_F2H_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA2SDRAM0 == 0)
	mask_noc |= ALT_SYSMGR_NOC_F2SDR0_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA2SDRAM1 == 0)
	mask_noc |= ALT_SYSMGR_NOC_F2SDR1_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA2SDRAM2 == 0)
	mask_noc |= ALT_SYSMGR_NOC_F2SDR2_SET_MSK;
#endif

	/* setup the mask for Reset Manager register based on handoff */
#if (CONFIG_HPS_RESET_ASSERT_HPS2FPGA == 0)
	mask_rstmgr |= ALT_RSTMGR_BRGMODRST_H2F_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_LWHPS2FPGA == 0)
	mask_rstmgr |= ALT_RSTMGR_BRGMODRST_LWH2F_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA2HPS == 0)
	mask_rstmgr |= ALT_RSTMGR_BRGMODRST_F2H_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA2SDRAM0 == 0)
	mask_rstmgr |= ALT_RSTMGR_BRGMODRST_F2SSDRAM0_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA2SDRAM1 == 0)
	mask_rstmgr |= ALT_RSTMGR_BRGMODRST_F2SSDRAM1_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA2SDRAM2 == 0)
	mask_rstmgr |= ALT_RSTMGR_BRGMODRST_F2SSDRAM2_SET_MSK;
#endif

	/* clear idle request to all bridges */
	setbits_le32(&system_manager_base->noc_idlereq_clr, mask_noc);

	/* Release bridges from reset state per handoff value */
	clrbits_le32(&reset_manager_base->brgmodrst, mask_rstmgr);

	/* Poll until all idleack to 0 */
	while (readl(&system_manager_base->noc_idleack) & mask_noc)
		;
}

/* Disable all the peripherals except L4 watchdog0 and L4 Timer 0 */
void reset_assert_all_peripherals_except_l4wd0_l4timer0(void)
{
	unsigned mask_ecc_ocp =
		ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK |
		ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK |
		ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK |
		ALT_RSTMGR_PER0MODRST_USBECC0_SET_MSK |
		ALT_RSTMGR_PER0MODRST_USBECC1_SET_MSK |
		ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK |
		ALT_RSTMGR_PER0MODRST_QSPIECC_SET_MSK |
		ALT_RSTMGR_PER0MODRST_SDMMCECC_SET_MSK;

	/* disable all components except ECC_OCP, L4 Timer0 and L4 WD0 */
	writel(~(ALT_RSTMGR_PER1MODRST_WD0_SET_MSK |
	       ALT_RSTMGR_PER1MODRST_L4SYSTMR0_SET_MSK),
	       &reset_manager_base->per1modrst);
	setbits_le32(&reset_manager_base->per0modrst, ~mask_ecc_ocp);

	/* Finally disable the ECC_OCP */
	setbits_le32(&reset_manager_base->per0modrst, mask_ecc_ocp);
}

void reset_deassert_peripherals_handoff(void)
{
	unsigned mask = 0;

	/* enable ECC OCP first */
#if (CONFIG_HPS_EMAC0 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK;
#endif
#if (CONFIG_HPS_EMAC1 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK;
#endif
#if (CONFIG_HPS_EMAC2 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK;
#endif
#if (CONFIG_HPS_USB0 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_USBECC0_SET_MSK;
#endif
#if (CONFIG_HPS_USB1 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_USBECC1_SET_MSK;
#endif
#if (CONFIG_HPS_NAND == 1 || CONFIG_PRELOADER_BOOT_FROM_NAND == 1)
	mask |= ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK;
#endif
#if (CONFIG_HPS_QSPI == 1 || CONFIG_PRELOADER_BOOT_FROM_QSPI == 1)
	mask |= ALT_RSTMGR_PER0MODRST_QSPIECC_SET_MSK;
#endif
#if (CONFIG_HPS_SDMMC == 1 || CONFIG_PRELOADER_BOOT_FROM_SDMMC == 1)
	mask |= ALT_RSTMGR_PER0MODRST_SDMMCECC_SET_MSK;
#endif
	clrbits_le32(&reset_manager_base->per0modrst, mask);

	/* then release the IP from reset */
	mask = 0;
#if (CONFIG_HPS_EMAC0 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_EMAC0_SET_MSK;
#endif
#if (CONFIG_HPS_EMAC1 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_EMAC1_SET_MSK;
#endif
#if (CONFIG_HPS_EMAC2 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_EMAC2_SET_MSK;
#endif
#if (CONFIG_HPS_USB0 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_USB0_SET_MSK;
#endif
#if (CONFIG_HPS_USB1 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_USB1_SET_MSK;
#endif
#if (CONFIG_HPS_NAND == 1 || CONFIG_PRELOADER_BOOT_FROM_NAND == 1)
	mask |= ALT_RSTMGR_PER0MODRST_NAND_SET_MSK;
#endif
#if (CONFIG_HPS_QSPI == 1 || CONFIG_PRELOADER_BOOT_FROM_QSPI == 1)
	mask |= ALT_RSTMGR_PER0MODRST_QSPI_SET_MSK;
#endif
#if (CONFIG_HPS_SDMMC == 1 || CONFIG_PRELOADER_BOOT_FROM_SDMMC == 1)
	mask |= ALT_RSTMGR_PER0MODRST_SDMMC_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_DMA == 0)
	mask |= ALT_RSTMGR_PER0MODRST_DMA_SET_MSK;
#endif
#if (CONFIG_HPS_SPIM0 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_SPIM0_SET_MSK;
#endif
#if (CONFIG_HPS_SPIM1 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_SPIM1_SET_MSK;
#endif
#if (CONFIG_HPS_SPIS0 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_SPIS0_SET_MSK;
#endif
#if (CONFIG_HPS_SPIS1 == 1)
	mask |= ALT_RSTMGR_PER0MODRST_SPIS1_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_EMACPTP == 0)
	mask |= ALT_RSTMGR_PER0MODRST_EMACPTP_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA_DMA0 == 0)
	mask |= ALT_RSTMGR_PER0MODRST_DMAIF0_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA_DMA1 == 0)
	mask |= ALT_RSTMGR_PER0MODRST_DMAIF1_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA_DMA2 == 0)
	mask |= ALT_RSTMGR_PER0MODRST_DMAIF2_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA_DMA3 == 0)
	mask |= ALT_RSTMGR_PER0MODRST_DMAIF3_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA_DMA4 == 0)
	mask |= ALT_RSTMGR_PER0MODRST_DMAIF4_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA_DMA5 == 0)
	mask |= ALT_RSTMGR_PER0MODRST_DMAIF5_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA_DMA6 == 0)
	mask |= ALT_RSTMGR_PER0MODRST_DMAIF6_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA_DMA7 == 0)
	mask |= ALT_RSTMGR_PER0MODRST_DMAIF7_SET_MSK;
#endif
	clrbits_le32(&reset_manager_base->per0modrst, mask);

	/* we always release timer 1 */
	mask = ALT_RSTMGR_PER1MODRST_L4SYSTMR0_SET_MSK;
#if (CONFIG_HPS_RESET_ASSERT_L4WD1 == 0)
	mask |= ALT_RSTMGR_PER1MODRST_WD1_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_OSC1TIMER1 == 0)
	mask |= ALT_RSTMGR_PER1MODRST_L4SYSTMR1_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_SPTIMER0 == 0)
	mask |= ALT_RSTMGR_PER1MODRST_SPTMR0_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_SPTIMER1 == 0)
	mask |= ALT_RSTMGR_PER1MODRST_SPTMR1_SET_MSK;
#endif
#if (CONFIG_HPS_I2C0 == 1)
	mask |= ALT_RSTMGR_PER1MODRST_I2C0_SET_MSK;
#endif
#if (CONFIG_HPS_I2C1 == 1)
	mask |= ALT_RSTMGR_PER1MODRST_I2C1_SET_MSK;
#endif
#if (CONFIG_HPS_I2CEMAC0 == 1)
	mask |= ALT_RSTMGR_PER1MODRST_I2C2_SET_MSK;
#endif
#if (CONFIG_HPS_I2CEMAC1 == 1)
	mask |= ALT_RSTMGR_PER1MODRST_I2C3_SET_MSK;
#endif
#if (CONFIG_HPS_I2CEMAC2 == 1)
	mask |= ALT_RSTMGR_PER1MODRST_I2C4_SET_MSK;
#endif
#if (CONFIG_HPS_UART0 == 1)
	mask |= ALT_RSTMGR_PER1MODRST_UART0_SET_MSK;
#endif
#if (CONFIG_HPS_UART1 == 1 || CONFIG_PRELOADER_OVERWRITE_DEDICATED == 1)
	mask |= ALT_RSTMGR_PER1MODRST_UART1_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_GPIO0 == 0)
	mask |= ALT_RSTMGR_PER1MODRST_GPIO0_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_GPIO1 == 0)
	mask |= ALT_RSTMGR_PER1MODRST_GPIO1_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_GPIO2 == 0)
	mask |= ALT_RSTMGR_PER1MODRST_GPIO2_SET_MSK;
#endif
	clrbits_le32(&reset_manager_base->per1modrst, mask);

	mask = 0;
#if (CONFIG_HPS_RESET_WARMRST_HANDSHAKE_SDRAM == 1)
	mask |= ALT_RSTMGR_HDSKEN_SDRSELFREFEN_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_WARMRST_HANDSHAKE_FPGAMGR == 1)
	mask |= ALT_RSTMGR_HDSKEN_FPGAMGRHSEN_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_WARMRST_HANDSHAKE_FPGA == 1)
	mask |= ALT_RSTMGR_HDSKEN_FPGAHSEN_SET_MSK;
#endif
#if (CONFIG_HPS_RESET_WARMRST_HANDSHAKE_ETR == 1)
	mask |= ALT_RSTMGR_HDSKEN_ETRSTALLEN_SET_MSK;
#endif
	setbits_le32(&reset_manager_base->hdsken, mask);
}

/* Release L4 OSC1 Watchdog Timer 0 from reset through reset manager */
void reset_deassert_osc1wd0(void)
{
	clrbits_le32(&reset_manager_base->per1modrst,
		ALT_RSTMGR_PER1MODRST_WD0_SET_MSK);
}
