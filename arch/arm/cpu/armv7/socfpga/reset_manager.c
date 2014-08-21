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
#include <asm/arch/reset_manager.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/nic301.h>
#include <watchdog.h>
#include <asm/arch/debug_memory.h>
#include <asm/arch/system_manager.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_reset_manager *reset_manager_base =
		(void *)SOCFPGA_RSTMGR_ADDRESS;

/* Disable the watchdog (toggle reset to watchdog) */
void watchdog_disable(void)
{
	/* assert reset for watchdog */
	setbits_le32(&reset_manager_base->per_mod_reset,
		(1<<RSTMGR_PERMODRST_L4WD0_LSB));
	/* deassert watchdog from reset (watchdog in not running state) */
	clrbits_le32(&reset_manager_base->per_mod_reset,
		(1<<RSTMGR_PERMODRST_L4WD0_LSB));
}

/* Check whether Watchdog in reset state? */
int is_wdt_in_reset(void)
{
	unsigned long val;
	val = readl(&reset_manager_base->per_mod_reset);
	val = ((val >> RSTMGR_PERMODRST_L4WD0_LSB) & 0x1);
	/* return 0x1 if watchdog in reset */
	return val;
}

/* Write the reset manager register to cause reset */
void reset_cpu(ulong addr)
{
	/* request a warm reset */
	setbits_le32(&reset_manager_base->ctrl,
		     (1 << RSTMGR_CTRL_SWWARMRSTREQ_LSB));
	/*
	 * infinite loop here as watchdog will trigger and reset
	 * the processor
	 */
	while (1)
		;
}

/* Release all peripherals from reset through reset manager */
void reset_deassert_all_peripherals(void)
{
	writel(0x0, &reset_manager_base->per_mod_reset);
}

/* Release all bridges from reset through reset manager */
void reset_deassert_all_bridges(void)
{
#if !defined(CONFIG_SOCFPGA_VIRTUAL_TARGET)
	/* check signal from FPGA */
	if (poll_fpgamgr_fpga_ready() == 0) {
		/* FPGA not ready. Not much can be done but let WD timeout */
		for (;;)
			;
	}
	/* brdmodrst */
	writel(0, &reset_manager_base->brg_mod_reset);

	/* remap the bridges into memory map */
	writel((L3REGS_REMAP_LWHPS2FPGA_MASK | L3REGS_REMAP_HPS2FPGA_MASK |
		L3REGS_REMAP_OCRAM_MASK), SOCFPGA_L3REGS_ADDRESS);
#endif
}

/* Release second processor cpu1 from reset through reset manager */
void reset_deassert_cpu1(void)
{
	writel(0, &reset_manager_base->mpu_mod_reset);
}

/* Release L4 OSC1 Timer0 from reset through reset manager */
void reset_deassert_osc1timer0(void)
{
	clrbits_le32(&reset_manager_base->per_mod_reset,
		(1<<RSTMGR_PERMODRST_OSC1TIMER0_LSB));
}

/* Release L4 OSC1 Watchdog Timer 0 from reset through reset manager */
void reset_deassert_osc1wd0(void)
{
	clrbits_le32(&reset_manager_base->per_mod_reset,
		(1<<RSTMGR_PERMODRST_L4WD0_LSB));
}

/* Assert reset to all peripherals through reset manager */
void reset_assert_all_peripherals(void)
{
	writel(~0, &reset_manager_base->per_mod_reset);
}

/* Assert reset to all bridges through reset manager */
void reset_assert_all_bridges(void)
{
#if !defined(CONFIG_SOCFPGA_VIRTUAL_TARGET)
	writel(~0, &reset_manager_base->brg_mod_reset);
#endif
}

/* Assert reset to all peripherals except l4wd0 through reset manager */
void reset_assert_all_peripherals_except_l4wd0(void)
{
	writel(~(1 << RSTMGR_PERMODRST_L4WD0_LSB),
		&reset_manager_base->per_mod_reset);
}

/* Assert reset to all peripherals except l4wd0 and sdr through reset manager */
void reset_assert_all_peripherals_except_l4wd0_and_sdr(void)
{
	writel(~((1 << RSTMGR_PERMODRST_L4WD0_LSB) |
	       (1 << RSTMGR_PERMODRST_SDR_LSB)),
	       &reset_manager_base->per_mod_reset);
}

/* Below function only applicable for SPL */
#ifdef CONFIG_SPL_BUILD

/* Release peripherals from reset based on handoff */
void reset_deassert_peripherals_handoff(void)
{
	unsigned val = 0;

#if defined(CONFIG_SOCFPGA_VIRTUAL_TARGET)
	DEBUG_MEMORY
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_EMAC0_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_EMAC0);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_EMAC1_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_EMAC1);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_USB0_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_USB0);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_USB1_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_USB1);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_NAND_FLASH_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_NAND);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_QSPI_FLASH_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_QSPI);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_WATCHDOG1_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_L4WD1);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_OSC1_TIMER1_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_OSC1TIMER1);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_SP_TIMER0_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_SPTIMER0);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_SP_TIMER1_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_SPTIMER1);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_I2C0_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_I2C0);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_I2C1_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_I2C1);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_I2C2_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_I2C2);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_I2C3_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_I2C3);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_UART0_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_UART0);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_UART1_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_UART1);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_SPI_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_SPIM0);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_SDMMC_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_SDMMC);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_CAN0_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_CAN0);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_CAN1_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_CAN1);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_GPIO0_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_GPIO0);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_GPIO1_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_GPIO1);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_GPIO2_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_GPIO2);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_DMA_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_DMA);
	val |= RESET_MANAGER_REGS_PERIPH_MODULE_RST_REG_SDRAM_RST_FLD_SET(
		CONFIG_HPS_RESET_ASSERT_SDR);
	writel(val, &reset_manager_base->per_mod_reset);

#else
	DEBUG_MEMORY
	/* permodrst */
	val |= RSTMGR_PERMODRST_EMAC0_SET(CONFIG_HPS_RESET_ASSERT_EMAC0);
	val |= RSTMGR_PERMODRST_EMAC1_SET(CONFIG_HPS_RESET_ASSERT_EMAC1);
	val |= RSTMGR_PERMODRST_USB0_SET(CONFIG_HPS_RESET_ASSERT_USB0);
	val |= RSTMGR_PERMODRST_USB1_SET(CONFIG_HPS_RESET_ASSERT_USB1);
	val |= RSTMGR_PERMODRST_NAND_SET(CONFIG_HPS_RESET_ASSERT_NAND);
	val |= RSTMGR_PERMODRST_QSPI_SET(CONFIG_HPS_RESET_ASSERT_QSPI);
	val |= RSTMGR_PERMODRST_L4WD1_SET(CONFIG_HPS_RESET_ASSERT_L4WD1);
	val |= RSTMGR_PERMODRST_OSC1TIMER1_SET(
		CONFIG_HPS_RESET_ASSERT_OSC1TIMER1);
	val |= RSTMGR_PERMODRST_SPTIMER0_SET(CONFIG_HPS_RESET_ASSERT_SPTIMER0);
	val |= RSTMGR_PERMODRST_SPTIMER1_SET(CONFIG_HPS_RESET_ASSERT_SPTIMER1);
	val |= RSTMGR_PERMODRST_I2C0_SET(CONFIG_HPS_RESET_ASSERT_I2C0);
	val |= RSTMGR_PERMODRST_I2C1_SET(CONFIG_HPS_RESET_ASSERT_I2C1);
	val |= RSTMGR_PERMODRST_I2C2_SET(CONFIG_HPS_RESET_ASSERT_I2C2);
	val |= RSTMGR_PERMODRST_I2C3_SET(CONFIG_HPS_RESET_ASSERT_I2C3);
	val |= RSTMGR_PERMODRST_UART0_SET(CONFIG_HPS_RESET_ASSERT_UART0);
	val |= RSTMGR_PERMODRST_UART1_SET(CONFIG_HPS_RESET_ASSERT_UART1);
	val |= RSTMGR_PERMODRST_SPIM0_SET(CONFIG_HPS_RESET_ASSERT_SPIM0);
	val |= RSTMGR_PERMODRST_SPIM1_SET(CONFIG_HPS_RESET_ASSERT_SPIM1);
	val |= RSTMGR_PERMODRST_SPIS0_SET(CONFIG_HPS_RESET_ASSERT_SPIS0);
	val |= RSTMGR_PERMODRST_SPIS1_SET(CONFIG_HPS_RESET_ASSERT_SPIS1);
	val |= RSTMGR_PERMODRST_SDMMC_SET(CONFIG_HPS_RESET_ASSERT_SDMMC);
	val |= RSTMGR_PERMODRST_CAN0_SET(CONFIG_HPS_RESET_ASSERT_CAN0);
	val |= RSTMGR_PERMODRST_CAN1_SET(CONFIG_HPS_RESET_ASSERT_CAN1);
	val |= RSTMGR_PERMODRST_GPIO0_SET(CONFIG_HPS_RESET_ASSERT_GPIO0);
	val |= RSTMGR_PERMODRST_GPIO1_SET(CONFIG_HPS_RESET_ASSERT_GPIO1);
	val |= RSTMGR_PERMODRST_GPIO2_SET(CONFIG_HPS_RESET_ASSERT_GPIO2);
	val |= RSTMGR_PERMODRST_DMA_SET(CONFIG_HPS_RESET_ASSERT_DMA);
	val |= RSTMGR_PERMODRST_SDR_SET(CONFIG_HPS_RESET_ASSERT_SDR);
	writel(val, &reset_manager_base->per_mod_reset);

	/* permodrst2 */
	val = 0;
	val |= RSTMGR_PER2MODRST_DMAIF0_SET(CONFIG_HPS_RESET_ASSERT_FPGA_DMA0);
	val |= RSTMGR_PER2MODRST_DMAIF1_SET(CONFIG_HPS_RESET_ASSERT_FPGA_DMA1);
	val |= RSTMGR_PER2MODRST_DMAIF2_SET(CONFIG_HPS_RESET_ASSERT_FPGA_DMA2);
	val |= RSTMGR_PER2MODRST_DMAIF3_SET(CONFIG_HPS_RESET_ASSERT_FPGA_DMA3);
	val |= RSTMGR_PER2MODRST_DMAIF4_SET(CONFIG_HPS_RESET_ASSERT_FPGA_DMA4);
	val |= RSTMGR_PER2MODRST_DMAIF5_SET(CONFIG_HPS_RESET_ASSERT_FPGA_DMA5);
	val |= RSTMGR_PER2MODRST_DMAIF6_SET(CONFIG_HPS_RESET_ASSERT_FPGA_DMA6);
	val |= RSTMGR_PER2MODRST_DMAIF7_SET(CONFIG_HPS_RESET_ASSERT_FPGA_DMA7);
	writel(val, &reset_manager_base->per2_mod_reset);

#endif

	/* warm reset handshake support */
	DEBUG_MEMORY
#if (CONFIG_HPS_RESET_WARMRST_HANDSHAKE_FPGA == 1)
	setbits_le32(&reset_manager_base->ctrl, RSTMGR_CTRL_FPGAHSEN_MASK);
#else
	clrbits_le32(&reset_manager_base->ctrl, RSTMGR_CTRL_FPGAHSEN_MASK);
#endif

#if (CONFIG_HPS_RESET_WARMRST_HANDSHAKE_ETR == 1)
	setbits_le32(&reset_manager_base->ctrl, RSTMGR_CTRL_ETRSTALLEN_MASK);
#else
	clrbits_le32(&reset_manager_base->ctrl, RSTMGR_CTRL_ETRSTALLEN_MASK);
#endif

#if (CONFIG_HPS_RESET_WARMRST_HANDSHAKE_SDRAM == 1)
	setbits_le32(&reset_manager_base->ctrl, RSTMGR_CTRL_SDRSELFREFEN_MASK);
#else
	clrbits_le32(&reset_manager_base->ctrl, RSTMGR_CTRL_SDRSELFREFEN_MASK);
#endif
}

/* Release bridge from reset based on handoff */
void reset_deassert_bridges_handoff(void)
{
#if !defined(CONFIG_SOCFPGA_VIRTUAL_TARGET)
	unsigned brgmodrst = 0;
	unsigned remap_val = L3REGS_REMAP_OCRAM_MASK;

#if (CONFIG_HPS_RESET_ASSERT_HPS2FPGA == 1 && CONFIG_PRELOADER_EXE_ON_FPGA == 0)
	brgmodrst |= RSTMGR_BRGMODRST_HPS2FPGA_MASK;
#else
	remap_val |= L3REGS_REMAP_HPS2FPGA_MASK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_LWHPS2FPGA == 1)
	brgmodrst |= RSTMGR_BRGMODRST_LWHPS2FPGA_MASK;
#else
	remap_val |= L3REGS_REMAP_LWHPS2FPGA_MASK;
#endif
#if (CONFIG_HPS_RESET_ASSERT_FPGA2HPS == 1)
	brgmodrst |= RSTMGR_BRGMODRST_FPGA2HPS_MASK;
#endif

	writel(brgmodrst, ISWGRP_HANDOFF_AXIBRIDGE);
	writel(remap_val, ISWGRP_HANDOFF_L3REMAP);

	DEBUG_MEMORY
	if (is_fpgamgr_fpga_ready()) {
		DEBUG_MEMORY
		/* enable the axi bridges if FPGA programmed */
		writel(brgmodrst, &reset_manager_base->brg_mod_reset);

		/* remap the enabled bridge into NIC-301 */
		writel(remap_val, SOCFPGA_L3REGS_ADDRESS);
	}
#endif /* CONFIG_SOCFPGA_VIRTUAL_TARGET */
}

#endif /* CONFIG_SPL_BUILD */


/* Change the reset state for EMAC0 */
void emac0_reset_enable(uint state)
{
	if (state)
		setbits_le32(&reset_manager_base->per_mod_reset,
			(1 << RSTMGR_PERMODRST_EMAC0_LSB));
	else
		clrbits_le32(&reset_manager_base->per_mod_reset,
			(1 << RSTMGR_PERMODRST_EMAC0_LSB));
}

/* Change the reset state for EMAC1 */
void emac1_reset_enable(uint state)
{
	if (state)
		setbits_le32(&reset_manager_base->per_mod_reset,
			(1 << RSTMGR_PERMODRST_EMAC1_LSB));
	else
		clrbits_le32(&reset_manager_base->per_mod_reset,
			(1 << RSTMGR_PERMODRST_EMAC1_LSB));
}
