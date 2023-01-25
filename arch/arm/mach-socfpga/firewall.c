// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2022 Intel Corporation <www.intel.com>
 *
 */

#include <asm/io.h>
#include <common.h>
#include <asm/arch/firewall.h>
#include <asm/arch/system_manager.h>

static void firewall_l4_per_disable(void)
{
	const struct socfpga_firwall_l4_per *firwall_l4_per_base =
		(struct socfpga_firwall_l4_per *)SOCFPGA_FIREWALL_L4_PER;
	u32 i;
	const u32 *addr[] = {
			&firwall_l4_per_base->nand,
			&firwall_l4_per_base->nand_data,
			&firwall_l4_per_base->usb0,
			&firwall_l4_per_base->usb1,
			&firwall_l4_per_base->spim0,
			&firwall_l4_per_base->spim1,
			&firwall_l4_per_base->emac0,
			&firwall_l4_per_base->emac1,
			&firwall_l4_per_base->emac2,
			&firwall_l4_per_base->sdmmc,
			&firwall_l4_per_base->gpio0,
			&firwall_l4_per_base->gpio1,
			&firwall_l4_per_base->i2c0,
			&firwall_l4_per_base->i2c1,
			&firwall_l4_per_base->i2c2,
			&firwall_l4_per_base->i2c3,
			&firwall_l4_per_base->i2c4,
			&firwall_l4_per_base->timer0,
			&firwall_l4_per_base->timer1,
			&firwall_l4_per_base->uart0,
			&firwall_l4_per_base->uart1
			};

	/*
	 * The following lines of code will enable non-secure access
	 * to nand, usb, spi, emac, sdmmc, gpio, i2c, timers and uart. This
	 * is needed as most OS run in non-secure mode. Thus we need to
	 * enable non-secure access to these peripherals in order for the
	 * OS to use these peripherals.
	 */
	for (i = 0; i < ARRAY_SIZE(addr); i++)
		writel(FIREWALL_L4_DISABLE_ALL, addr[i]);
}

static void firewall_l4_sys_disable(void)
{
	const struct socfpga_firwall_l4_sys *firwall_l4_sys_base =
		(struct socfpga_firwall_l4_sys *)SOCFPGA_FIREWALL_L4_SYS;
	u32 i;
	const u32 *addr[] = {
			&firwall_l4_sys_base->dma_ecc,
			&firwall_l4_sys_base->emac0rx_ecc,
			&firwall_l4_sys_base->emac0tx_ecc,
			&firwall_l4_sys_base->emac1rx_ecc,
			&firwall_l4_sys_base->emac1tx_ecc,
			&firwall_l4_sys_base->emac2rx_ecc,
			&firwall_l4_sys_base->emac2tx_ecc,
			&firwall_l4_sys_base->nand_ecc,
			&firwall_l4_sys_base->nand_read_ecc,
			&firwall_l4_sys_base->nand_write_ecc,
			&firwall_l4_sys_base->ocram_ecc,
			&firwall_l4_sys_base->sdmmc_ecc,
			&firwall_l4_sys_base->usb0_ecc,
			&firwall_l4_sys_base->usb1_ecc,
			&firwall_l4_sys_base->clock_manager,
			&firwall_l4_sys_base->io_manager,
			&firwall_l4_sys_base->reset_manager,
			&firwall_l4_sys_base->system_manager,
			&firwall_l4_sys_base->watchdog0,
			&firwall_l4_sys_base->watchdog1,
			&firwall_l4_sys_base->watchdog2,
			&firwall_l4_sys_base->watchdog3
		};

	for (i = 0; i < ARRAY_SIZE(addr); i++)
		writel(FIREWALL_L4_DISABLE_ALL, addr[i]);
}

static void firewall_bridge_disable(void)
{
	/* disable lwsocf2fpga and soc2fpga bridge security */
	writel(FIREWALL_BRIDGE_DISABLE_ALL, SOCFPGA_FIREWALL_SOC2FPGA);
	writel(FIREWALL_BRIDGE_DISABLE_ALL, SOCFPGA_FIREWALL_LWSOC2FPGA);
}

void firewall_setup(void)
{
	firewall_l4_per_disable();
	firewall_l4_sys_disable();
	firewall_bridge_disable();

	/* disable SMMU security */
	writel(FIREWALL_L4_DISABLE_ALL, SOCFPGA_FIREWALL_TCU);

	/* enable non-secure interface to DMA330 DMA and peripherals */
	writel(SYSMGR_DMA_IRQ_NS | SYSMGR_DMA_MGR_NS,
	       socfpga_get_sysmgr_addr() + SYSMGR_SOC64_DMA);
	writel(SYSMGR_DMAPERIPH_ALL_NS,
	       socfpga_get_sysmgr_addr() + SYSMGR_SOC64_DMA_PERIPH);

#if IS_ENABLED(CONFIG_TARGET_SOCFPGA_AGILEX) || \
		IS_ENABLED(CONFIG_TARGET_SOCFPGA_N5X)
	/* Disable the MPFE Firewall for SMMU */
	writel(FIREWALL_MPFE_SCR_DISABLE_ALL, SOCFPGA_FW_MPFE_SCR_ADDRESS +
					      FW_MPFE_SCR_HMC);
	/* Disable MPFE Firewall for HMC adapter (ECC) */
	writel(FIREWALL_MPFE_SCR_DISABLE_MPU, SOCFPGA_FW_MPFE_SCR_ADDRESS +
					      FW_MPFE_SCR_HMC_ADAPTOR);
#endif

#if defined(CONFIG_TARGET_SOCFPGA_AGILEX) || \
	defined(CONFIG_TARGET_SOCFPGA_STRATIX10)
	/*
	 * Enable both priviledged & non-prviledged access to various
	 * peripherals
	 */
	writel(0xffffffff, SOCFPGA_FIREWALL_PRIV_MEMORYMAP_PRIV);
#endif

#ifdef CONFIG_TARGET_SOCFPGA_STRATIX10
	 /* Enable access to DDR from FPGA */
	clrbits_le32(CCU_REG_ADDR(CCU_FPGA_MPRT_ADBASE_MEMSPACE0),
		     CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_REG_ADDR(CCU_FPGA_MPRT_ADBASE_MEMSPACE1A),
		     CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_REG_ADDR(CCU_FPGA_MPRT_ADBASE_MEMSPACE1B),
		     CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_REG_ADDR(CCU_FPGA_MPRT_ADBASE_MEMSPACE1C),
		     CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_REG_ADDR(CCU_FPGA_MPRT_ADBASE_MEMSPACE1D),
		     CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_REG_ADDR(CCU_FPGA_MPRT_ADBASE_MEMSPACE1E),
		     CCU_ADBASE_DI_MASK);
#endif
}
