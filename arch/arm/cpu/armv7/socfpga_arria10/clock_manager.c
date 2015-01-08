/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/clock_manager.h>
#include <fdtdec.h>

static const struct socfpga_clock_manager *clock_manager_base =
		(void *)SOCFPGA_CLKMGR_ADDRESS;

#define LOCKED_MASK \
	(CLKMGR_CLKMGR_STAT_MAINPLLLOCKED_SET_MSK  | \
	CLKMGR_CLKMGR_STAT_PERPLLLOCKED_SET_MSK)

static inline void cm_wait_for_lock(uint32_t mask)
{
	register uint32_t inter_val;
	do {
		inter_val = readl(&clock_manager_base->stat) & mask;
	} while (inter_val != mask);
}

/* function to poll in the fsm busy bit */
static inline void cm_wait4fsm(void)
{
	register uint32_t inter_val;
	do {
		inter_val = readl(&clock_manager_base->stat) &
			CLKMGR_CLKMGR_STAT_BUSY_SET_MSK;
	} while (inter_val);
}
unsigned int cm_get_mmc_controller_clk_hz(void)
{
#ifdef TEST_AT_ASIMOV
	return 50000000;
#else	
	return CONFIG_HPS_CLK_SDMMC_HZ;
#endif
}


/*
 * Setup clocks while making no assumptions of the
 * previous state of the clocks.
 *
 * Start by being paranoid and gate all sw managed clocks
 *
 * Put all plls in bypass
 *
 * Put all plls VCO registers back to reset value (bgpwr dwn).
 *
 * Put peripheral and main pll src to reset value to avoid glitch.
 *
 * Delay 5 us.
 *
 * Deassert bg pwr dn and set numerator and denominator
 *
 * Start 7 us timer.
 *
 * set internal dividers
 *
 * Wait for 7 us timer.
 *
 * Enable plls
 *
 * Set external dividers while plls are locking
 *
 * Wait for pll lock
 *
 * Assert/deassert outreset all.
 *
 * Take all pll's out of bypass
 *
 * Clear safe mode
 *
 * set source main and peripheral clocks
 *
 * Ungate clocks
 */

int cm_basic_init(void)
{
#ifndef TEST_AT_ASIMOV
	/* gate off all mainpll clock excpet HW managed clock */
	writel(CLKMGR_MAINPLL_EN_S2FUSER0CLKEN_SET_MSK |
		CLKMGR_MAINPLL_EN_HMCPLLREFCLKEN_SET_MSK,
		&clock_manager_base->main_pll_enr);

	/* now we can gate off the rest of the peripheral clocks */
	writel(0, &clock_manager_base->per_pll_en);

	/* Put all plls in external bypass */
	writel(CLKMGR_MAINPLL_BYPASS_RESET,
		&clock_manager_base->main_pll_bypasss);
	writel(CLKMGR_PERPLL_BYPASS_RESET,
		&clock_manager_base->per_pll_bypasss);

	/*
	 * Put all plls VCO registers back to reset value.
	 * Some code might have messed with them. At same time set the
	 * desired clock source
	 */
	if ((readl(&clock_manager_base->stat) &
		CLKMGR_CLKMGR_STAT_BOOTCLKSRC_SET_MSK) == 0) {
		/* non secure clock */
		writel(CLKMGR_MAINPLL_VCO0_RESET |
			(CONFIG_HPS_MAINPLLGRP_VCO_PSRC <<
				CLKMGR_MAINPLL_VCO0_PSRC_LSB),
			&clock_manager_base->main_pll_vco0);
	} else {
		/* secure clock always use int_osc */
		writel(CLKMGR_MAINPLL_VCO0_RESET |
			(CLKMGR_MAINPLL_VCO0_PSRC_E_INTOSC <<
				CLKMGR_MAINPLL_VCO0_PSRC_LSB),
			&clock_manager_base->main_pll_vco0);
	}
	writel(CLKMGR_PERPLL_VCO0_RESET |
		(CONFIG_HPS_PERPLLGRP_VCO_PSRC <<
			CLKMGR_PERPLL_VCO0_PSRC_LSB),
		&clock_manager_base->per_pll_vco0);

	writel(CLKMGR_MAINPLL_VCO1_RESET,
		&clock_manager_base->main_pll_vco1);
	writel(CLKMGR_PERPLL_VCO1_RESET,
		&clock_manager_base->per_pll_vco1);

	/* clear the interrupt register status register */
	writel(CLKMGR_CLKMGR_INTR_MAINPLLLOST_SET_MSK |
		CLKMGR_CLKMGR_INTR_PERPLLLOST_SET_MSK |
		CLKMGR_CLKMGR_INTR_MAINPLLRFSLIP_SET_MSK |
		CLKMGR_CLKMGR_INTR_PERPLLRFSLIP_SET_MSK |
		CLKMGR_CLKMGR_INTR_MAINPLLFBSLIP_SET_MSK |
		CLKMGR_CLKMGR_INTR_PERPLLFBSLIP_SET_MSK |
		CLKMGR_CLKMGR_INTR_MAINPLLACHIEVED_SET_MSK |
		CLKMGR_CLKMGR_INTR_PERPLLACHIEVED_SET_MSK,
		&clock_manager_base->intr);

	/* Program VCO “Numerator” and “Denominator” */
	writel((CONFIG_HPS_MAINPLLGRP_VCO_DENOM <<
		CLKMGR_MAINPLL_VCO1_DENOM_LSB) |
		CONFIG_HPS_MAINPLLGRP_VCO_NUMER,
		&clock_manager_base->main_pll_vco1);
	writel((CONFIG_HPS_PERPLLGRP_VCO_DENOM <<
		CLKMGR_PERPLL_VCO1_DENOM_LSB) |
		CONFIG_HPS_PERPLLGRP_VCO_NUMER,
		&clock_manager_base->per_pll_vco1);

	/* Wait for at least 5 us */
	udelay(5);

	/* Now deassert BGPWRDN and PWRDN */
	clrbits_le32(&clock_manager_base->main_pll_vco0,
		CLKMGR_MAINPLL_VCO0_BGPWRDN_SET_MSK |
		CLKMGR_MAINPLL_VCO0_PWRDN_SET_MSK);
	clrbits_le32(&clock_manager_base->per_pll_vco0,
		CLKMGR_PERPLL_VCO0_BGPWRDN_SET_MSK |
		CLKMGR_PERPLL_VCO0_PWRDN_SET_MSK);
#endif /***************** TEST_AT_ASIMOV *****************/

	/* Wait for at least 7 us */
	udelay(7);

#ifndef TEST_AT_ASIMOV
	/* enable the VCO and disable the external regulator to PLL */
	writel((readl(&clock_manager_base->main_pll_vco0) &
		~CLKMGR_MAINPLL_VCO0_REGEXTSEL_SET_MSK) |
		CLKMGR_MAINPLL_VCO0_EN_SET_MSK,
		&clock_manager_base->main_pll_vco0);
	writel((readl(&clock_manager_base->per_pll_vco0) &
		~CLKMGR_PERPLL_VCO0_REGEXTSEL_SET_MSK) |
		CLKMGR_PERPLL_VCO0_EN_SET_MSK,
		&clock_manager_base->per_pll_vco0);

	/* setup all the main PLL counter and clock source */
	writel(0x50005, SOCFPGA_CLKMGR_ADDRESS + 0x144);

	/* main_emaca_clk divider */
	writel(CONFIG_HPS_MAINPLLGRP_EMACA_CNT,
		&clock_manager_base->main_pll_cntr2clk);
	/* main_emacb_clk divider */
	writel(CONFIG_HPS_MAINPLLGRP_EMACB_CNT,
		&clock_manager_base->main_pll_cntr3clk);
	/* main_emac_ptp_clk divider */
	writel(CONFIG_HPS_MAINPLLGRP_EMAC_PTP_CNT,
		&clock_manager_base->main_pll_cntr4clk);
	/* main_gpio_db_clk divider */
	writel(CONFIG_HPS_MAINPLLGRP_GPIO_DB_CNT,
		&clock_manager_base->main_pll_cntr5clk);
	/* main_sdmmc_clk divider */
	writel(CONFIG_HPS_MAINPLLGRP_SDMMC_CNT,
		&clock_manager_base->main_pll_cntr6clk);
	/* main_s2f_user0_clk divider */
	writel(CONFIG_HPS_MAINPLLGRP_S2F_USER0_CNT |
		(CONFIG_HPS_S2F_USER0_SRC <<
			CLKMGR_MAINPLL_CNTR7CLK_SRC_LSB),
		&clock_manager_base->main_pll_cntr7clk);
	/* main_s2f_user1_clk divider */
	writel(CONFIG_HPS_MAINPLLGRP_S2F_USER1_CNT,
		&clock_manager_base->main_pll_cntr8clk);
	/* main_hmc_pll_clk divider */
	writel(CONFIG_HPS_MAINPLLGRP_HMC_PLL_REF_CNT |
		(CONFIG_HPS_HMC_PLL_REF_SRC <<
			CLKMGR_MAINPLL_CNTR9CLK_SRC_LSB),
		&clock_manager_base->main_pll_cntr9clk);
	/* main_periph_ref_clk divider */
	writel(CONFIG_HPS_MAINPLLGRP_PERIPH_REF_CNT,
		&clock_manager_base->main_pll_cntr15clk);

	/* setup all the peripheral PLL counter and clock source */
	/* peri_emaca_clk divider */
	writel(CONFIG_HPS_PERPLLGRP_EMACA_CNT |
		(CONFIG_HPS_EMACA_SRC <<
			CLKMGR_PERPLL_CNTR2CLK_SRC_LSB),
		&clock_manager_base->per_pll_cntr2clk);
	/* peri_emacb_clk divider */
	writel(CONFIG_HPS_PERPLLGRP_EMACB_CNT |
		(CONFIG_HPS_EMACB_SRC <<
			CLKMGR_PERPLL_CNTR3CLK_SRC_LSB),
		&clock_manager_base->per_pll_cntr3clk);
	/* peri_emac_ptp_clk divider */
	writel(CONFIG_HPS_PERPLLGRP_EMAC_PTP_CNT |
		(CONFIG_HPS_EMAC_PTP_SRC <<
			CLKMGR_PERPLL_CNTR4CLK_SRC_LSB),
		&clock_manager_base->per_pll_cntr4clk);
	/* peri_gpio_db_clk divider */
	writel(CONFIG_HPS_PERPLLGRP_GPIO_DB_CNT |
		(CONFIG_HPS_GPIO_DB_SRC <<
			CLKMGR_PERPLL_CNTR5CLK_SRC_LSB),
		&clock_manager_base->per_pll_cntr5clk);
	/* peri_sdmmc_clk divider */
	writel(CONFIG_HPS_PERPLLGRP_SDMMC_CNT |
		(CONFIG_HPS_SDMMC_SRC <<
			CLKMGR_PERPLL_CNTR6CLK_SRC_LSB),
		&clock_manager_base->per_pll_cntr6clk);
	/* peri_s2f_user0_clk divider */
	writel(CONFIG_HPS_PERPLLGRP_S2F_USER0_CNT,
		&clock_manager_base->per_pll_cntr7clk);
	/* peri_s2f_user1_clk divider */
	writel(CONFIG_HPS_PERPLLGRP_S2F_USER1_CNT |
		(CONFIG_HPS_S2F_USER1_SRC <<
			CLKMGR_PERPLL_CNTR8CLK_SRC_LSB),
		&clock_manager_base->per_pll_cntr8clk);
	/* peri_hmc_pll_clk divider */
	writel(CONFIG_HPS_PERPLLGRP_HMC_PLL_REF_CNT,
		&clock_manager_base->per_pll_cntr9clk);

	/* setup all the external PLL counter */
	/* mpu wrapper / external divider */
	writel(CONFIG_HPS_DIV_MPU |
		(CONFIG_HPS_MPUCLK_SRC <<
			CLKMGR_MAINPLL_MPUCLK_SRC_LSB),
		&clock_manager_base->main_pll_mpuclk);
	/* NOC wrapper / external divider */
	writel(CONFIG_HPS_DIV_NOC |
		(CONFIG_HPS_NOCCLK_SRC <<
			CLKMGR_MAINPLL_NOCCLK_SRC_LSB),
		&clock_manager_base->main_pll_nocclk);
	/* NOC subclock divider such as l4 */
	writel(CONFIG_HPS_NOCDIV_L4MAINCLK |
		(CONFIG_HPS_NOCDIV_L4MPCLK <<
			CLKMGR_MAINPLL_NOCDIV_L4MPCLK_LSB) |
		(CONFIG_HPS_NOCDIV_L4SPCLK <<
			CLKMGR_MAINPLL_NOCDIV_L4SPCLK_LSB) |
		(CONFIG_HPS_NOCDIV_CS_ATCLK <<
			CLKMGR_MAINPLL_NOCDIV_CSATCLK_LSB) |
		(CONFIG_HPS_NOCDIV_CS_TRACECLK <<
			CLKMGR_MAINPLL_NOCDIV_CSTRACECLK_LSB) |
		(CONFIG_HPS_NOCDIV_CS_PDBGCLK <<
			CLKMGR_MAINPLL_NOCDIV_CSPDBGCLK_LSB),
		&clock_manager_base->main_pll_nocdiv);
	/* gpio_db external divider */
	writel(CONFIG_HPS_DIV_GPIO, &clock_manager_base->per_pll_gpiodiv);

	/* setup the EMAC clock mux select */
	writel((CONFIG_HPS_EMAC0SEL <<
			CLKMGR_PERPLL_EMACCTL_EMAC0SEL_LSB) |
		(CONFIG_HPS_EMAC1SEL <<
			CLKMGR_PERPLL_EMACCTL_EMAC1SEL_LSB) |
		(CONFIG_HPS_EMAC2SEL <<
			CLKMGR_PERPLL_EMACCTL_EMAC2SEL_LSB),
		&clock_manager_base->per_pll_emacctl);

	/* at this stage, check for PLL lock status */
	cm_wait_for_lock(LOCKED_MASK);

	/*
	 * after locking, but before taking out of bypass,
	 * assert/deassert outresetall
	 */
	/* assert mainpll outresetall */
	setbits_le32(&clock_manager_base->main_pll_vco0,
		CLKMGR_MAINPLL_VCO0_OUTRSTALL_SET_MSK);
	/* assert perpll outresetall */
	setbits_le32(&clock_manager_base->per_pll_vco0,
		CLKMGR_PERPLL_VCO0_OUTRSTALL_SET_MSK);
	/* de-assert mainpll outresetall */
	clrbits_le32(&clock_manager_base->main_pll_vco0,
		CLKMGR_MAINPLL_VCO0_OUTRSTALL_SET_MSK);
	/* de-assert perpll outresetall */
	clrbits_le32(&clock_manager_base->per_pll_vco0,
		CLKMGR_PERPLL_VCO0_OUTRSTALL_SET_MSK);

	/* Take all PLLs out of bypass when boot mode is cleared. */
	/* release mainpll from bypass */
	writel(CLKMGR_MAINPLL_BYPASS_RESET,
		&clock_manager_base->main_pll_bypassr);
	/* wait till Clock Manager is not busy */
	cm_wait4fsm();

	/* release perpll from bypass */
	writel(CLKMGR_PERPLL_BYPASS_RESET,
		&clock_manager_base->per_pll_bypassr);
	/* wait till Clock Manager is not busy */
	cm_wait4fsm();

	/* clear boot mode */
	clrbits_le32(&clock_manager_base->ctrl,
		CLKMGR_CLKMGR_CTL_BOOTMOD_SET_MSK);
	/* wait till Clock Manager is not busy */
	cm_wait4fsm();

	/* Now ungate non-hw-managed clocks */
	writel(CLKMGR_MAINPLL_EN_S2FUSER0CLKEN_SET_MSK |
		CLKMGR_MAINPLL_EN_HMCPLLREFCLKEN_SET_MSK,
		&clock_manager_base->main_pll_ens);
	writel(CLKMGR_PERPLL_EN_RESET,
		&clock_manager_base->per_pll_ens);

	/* Clear the loss lock and slip bits as they might set during
	clock reconfiguration */
	writel(CLKMGR_CLKMGR_INTR_MAINPLLLOST_SET_MSK |
		CLKMGR_CLKMGR_INTR_PERPLLLOST_SET_MSK |
		CLKMGR_CLKMGR_INTR_MAINPLLRFSLIP_SET_MSK |
		CLKMGR_CLKMGR_INTR_PERPLLRFSLIP_SET_MSK |
		CLKMGR_CLKMGR_INTR_MAINPLLFBSLIP_SET_MSK |
		CLKMGR_CLKMGR_INTR_PERPLLFBSLIP_SET_MSK,
		&clock_manager_base->intr);
#endif /***************** TEST_AT_ASIMOV *****************/
	return 0;
}
