/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/misc.h>
#include <fdtdec.h>
#include <errno.h>

DECLARE_GLOBAL_DATA_PTR;

u32 rst_mgr_status __attribute__ ((section(".data")));

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
	while ((readl(&system_manager_base->noc_idleack) ^
		(ALT_SYSMGR_NOC_H2F_SET_MSK |
		ALT_SYSMGR_NOC_LWH2F_SET_MSK |
		ALT_SYSMGR_NOC_F2H_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR0_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR1_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR2_SET_MSK)))
		;

	/* Poll until all idlestatus to 1 */
	while ((readl(&system_manager_base->noc_idlestatus) ^
		(ALT_SYSMGR_NOC_H2F_SET_MSK |
		ALT_SYSMGR_NOC_LWH2F_SET_MSK |
		ALT_SYSMGR_NOC_F2H_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR0_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR1_SET_MSK |
		ALT_SYSMGR_NOC_F2SDR2_SET_MSK)))
		;

	/* Put all bridges (except NOR DDR scheduler) into reset state */
	setbits_le32(&reset_manager_base->brgmodrst,
		(ALT_RSTMGR_BRGMODRST_H2F_SET_MSK |
		ALT_RSTMGR_BRGMODRST_LWH2F_SET_MSK |
		ALT_RSTMGR_BRGMODRST_F2H_SET_MSK |
		ALT_RSTMGR_BRGMODRST_F2SSDRAM0_SET_MSK |
		ALT_RSTMGR_BRGMODRST_F2SSDRAM1_SET_MSK |
		ALT_RSTMGR_BRGMODRST_F2SSDRAM2_SET_MSK));

	/* Disable NOC timeout */
	writel(0, &system_manager_base->noc_timeout);
}

static int get_bridge_init_val(const void *blob, int compat_id)
{
	int rval = 0;
	int node;
	u32 val;

	node = fdtdec_next_compatible(blob, 0, compat_id);
	if (node >= 0) {
		if (!fdtdec_get_int_array(blob, node, "init-val", &val, 1)) {
			if (val == 1)
				rval = val;
		}
	}
	return rval;
}

struct bridge_cfg {
	int compat_id;
	u32  mask_noc;
	u32  mask_rstmgr;
};

static const struct bridge_cfg bridge_cfg_tbl[] = {
	{
		COMPAT_ARRIA10_H2F_BRG,
		ALT_SYSMGR_NOC_H2F_SET_MSK,
		ALT_RSTMGR_BRGMODRST_H2F_SET_MSK,
	},
	{
		COMPAT_ARRIA10_LWH2F_BRG,
		ALT_SYSMGR_NOC_LWH2F_SET_MSK,
		ALT_RSTMGR_BRGMODRST_LWH2F_SET_MSK,
	},
	{
		COMPAT_ARRIA10_F2H_BRG,
		ALT_SYSMGR_NOC_F2H_SET_MSK,
		ALT_RSTMGR_BRGMODRST_F2H_SET_MSK,
	},
	{
		COMPAT_ARRIA10_F2SDR0,
		ALT_SYSMGR_NOC_F2SDR0_SET_MSK,
		ALT_RSTMGR_BRGMODRST_F2SSDRAM0_SET_MSK,
	},
	{
		COMPAT_ARRIA10_F2SDR1,
		ALT_SYSMGR_NOC_F2SDR1_SET_MSK,
		ALT_RSTMGR_BRGMODRST_F2SSDRAM1_SET_MSK,
	},
	{
		COMPAT_ARRIA10_F2SDR2,
		ALT_SYSMGR_NOC_F2SDR2_SET_MSK,
		ALT_RSTMGR_BRGMODRST_F2SSDRAM2_SET_MSK,
	},
};

/* Enable bridges (hps2fpga, lwhps2fpga, fpga2hps, fpga2sdram) per handoff */
int reset_deassert_bridges_handoff(void)
{
	u32 mask_noc = 0, mask_rstmgr = 0;
	int i;
	unsigned start = get_timer(0);

	for (i = 0; i < ARRAY_SIZE(bridge_cfg_tbl); i++) {
		if (get_bridge_init_val(gd->fdt_blob,
					bridge_cfg_tbl[i].compat_id)) {
			mask_noc |= bridge_cfg_tbl[i].mask_noc;
			mask_rstmgr |= bridge_cfg_tbl[i].mask_rstmgr;
		}
	}

	/* clear idle request to all bridges */
	setbits_le32(&system_manager_base->noc_idlereq_clr, mask_noc);

	/* Release bridges from reset state per handoff value */
	clrbits_le32(&reset_manager_base->brgmodrst, mask_rstmgr);

	/* Poll until all idleack to 0, timeout at 1000ms */
	while (readl(&system_manager_base->noc_idleack) & mask_noc) {
		if (get_timer(start) > 1000) {
			printf("Fail: noc_idleack = 0x%08x mask_noc = 0x%08x\n",
				readl(&system_manager_base->noc_idleack),
				mask_noc);
			return -ETIMEDOUT;
		}
	}
	return 0;
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

#define ECC_MASK (ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK|\
	ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK|\
	ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK|\
	ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK|\
	ALT_RSTMGR_PER0MODRST_QSPIECC_SET_MSK|\
	ALT_RSTMGR_PER0MODRST_SDMMCECC_SET_MSK)

void reset_deassert_dedicated_peripherals(void)
{
	int i;
	u32 mask0 = 0;
	u32 mask1 = 0;
	u32 pinmux_addr = SOCFPGA_PINMUX_DEDICATED_IO_ADDRESS;
	u32 mask = 0;
#if defined(CONFIG_MMC)
	mask |= ALT_RSTMGR_PER0MODRST_SDMMCECC_SET_MSK;
#elif defined(CONFIG_CADENCE_QSPI)
	mask |= ALT_RSTMGR_PER0MODRST_QSPIECC_SET_MSK;
#elif defined(CONFIG_NAND_DENALI)
	mask |= ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK;
#else
#error "unsupported dedicated peripherals"
#endif
	mask |= ALT_RSTMGR_PER0MODRST_DMAECC_SET_MSK;

	/* enable ECC OCP first */
	clrbits_le32(&reset_manager_base->per0modrst, mask);

	mask = 0;
#if defined(CONFIG_MMC)
	mask |= ALT_RSTMGR_PER0MODRST_SDMMC_SET_MSK;
#elif defined(CONFIG_CADENCE_QSPI)
	mask |= ALT_RSTMGR_PER0MODRST_QSPI_SET_MSK;
#elif defined(CONFIG_NAND_DENALI)
	mask |= ALT_RSTMGR_PER0MODRST_NAND_SET_MSK;
#else
#error "unsupported dedicated peripherals"
#endif
	mask |= ALT_RSTMGR_PER0MODRST_DMA_SET_MSK;

	clrbits_le32(&reset_manager_base->per0modrst, mask);

	mask = ALT_RSTMGR_PER1MODRST_L4SYSTMR0_SET_MSK;

	clrbits_le32(&reset_manager_base->per1modrst, mask);

	/* start with 4 as first 3 registers are reserved */
	for (i = 4, pinmux_addr += (sizeof(u32) * (i - 1)); i <= 17;
		i++, pinmux_addr += sizeof(u32)) {
		switch (readl(pinmux_addr)) {
		case 0:
			if ((i == 12) || (i == 13))
				mask1 |= ALT_RSTMGR_PER1MODRST_I2C3_SET_MSK;
			else if ((i == 14) || (i == 15))
				mask1 |= ALT_RSTMGR_PER1MODRST_I2C4_SET_MSK;
			else if ((i == 16) || (i == 17))
				mask1 |= ALT_RSTMGR_PER1MODRST_I2C2_SET_MSK;
			break;
		case 1:
			if ((i == 12) || (i == 13))
				mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC1_SET_MSK;
			else if ((i == 14) || (i == 15))
				mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC2_SET_MSK;
			else if ((i == 16) || (i == 17))
				mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC0_SET_MSK;
			break;
		case 2:
			if ((i == 10) || ((i >= 15) && (i <= 17)))
				mask0 |= ALT_RSTMGR_PER0MODRST_SPIS0_SET_MSK;
			break;
		case 3:
			if ((i >= 10) && (i <= 14))
				mask0 |= ALT_RSTMGR_PER0MODRST_SPIM0_SET_MSK;
			break;
		case 13:
			if (i >= 12)
				mask1 |= ALT_RSTMGR_PER1MODRST_UART1_SET_MSK;
			break;
		case 15:
			mask1 |= ALT_RSTMGR_PER1MODRST_GPIO2_SET_MSK;
			break;
		}
	}
	clrbits_le32(&reset_manager_base->per0modrst, mask0 & ECC_MASK);
	clrbits_le32(&reset_manager_base->per1modrst, mask1);
	clrbits_le32(&reset_manager_base->per0modrst, mask0);

}

void reset_assert_uart(void)
{
	u32 mask = 0;
	unsigned int com_port;

	com_port = uart_com_port(gd->fdt_blob);

	if (SOCFPGA_UART1_ADDRESS == com_port)
		mask |= ALT_RSTMGR_PER1MODRST_UART1_SET_MSK;
	else if (SOCFPGA_UART0_ADDRESS == com_port)
		mask |= ALT_RSTMGR_PER1MODRST_UART0_SET_MSK;

	setbits_le32(&reset_manager_base->per1modrst, mask);
}

void reset_deassert_uart(void)
{
	u32 mask = 0;
	unsigned int com_port;

	com_port = uart_com_port(gd->fdt_blob);

	if (SOCFPGA_UART1_ADDRESS == com_port)
		mask |= ALT_RSTMGR_PER1MODRST_UART1_SET_MSK;
	else if (SOCFPGA_UART0_ADDRESS == com_port)
		mask |= ALT_RSTMGR_PER1MODRST_UART0_SET_MSK;

	clrbits_le32(&reset_manager_base->per1modrst, mask);
}

static const u32 per0fpgamasks[] = {
	ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK |
	ALT_RSTMGR_PER0MODRST_EMAC0_SET_MSK,
	ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK |
	ALT_RSTMGR_PER0MODRST_EMAC1_SET_MSK,
	ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK |
	ALT_RSTMGR_PER0MODRST_EMAC2_SET_MSK,
	0, /* i2c0 per1mod */
	0, /* i2c1 per1mod */
	0, /* i2c0_emac */
	0, /* i2c1_emac */
	0, /* i2c2_emac */
	ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK |
	ALT_RSTMGR_PER0MODRST_NAND_SET_MSK,
	ALT_RSTMGR_PER0MODRST_QSPIECC_SET_MSK |
	ALT_RSTMGR_PER0MODRST_QSPI_SET_MSK,
	ALT_RSTMGR_PER0MODRST_SDMMCECC_SET_MSK |
	ALT_RSTMGR_PER0MODRST_SDMMC_SET_MSK,
	ALT_RSTMGR_PER0MODRST_SPIM0_SET_MSK,
	ALT_RSTMGR_PER0MODRST_SPIM1_SET_MSK,
	ALT_RSTMGR_PER0MODRST_SPIS0_SET_MSK,
	ALT_RSTMGR_PER0MODRST_SPIS1_SET_MSK,
	0, /* uart0 per1mod */
	0, /* uart1 per1mod */
};

static const u32 per1fpgamasks[] = {
	0, /* emac0 per0mod */
	0, /* emac1 per0mod */
	0, /* emac2 per0mod */
	ALT_RSTMGR_PER1MODRST_I2C0_SET_MSK,
	ALT_RSTMGR_PER1MODRST_I2C1_SET_MSK,
	ALT_RSTMGR_PER1MODRST_I2C2_SET_MSK, /* i2c0_emac */
	ALT_RSTMGR_PER1MODRST_I2C3_SET_MSK, /* i2c1_emac */
	ALT_RSTMGR_PER1MODRST_I2C4_SET_MSK, /* i2c2_emac */
	0, /* nand per0mod */
	0, /* qspi per0mod */
	0, /* sdmmc per0mod */
	0, /* spim0 per0mod */
	0, /* spim1 per0mod */
	0, /* spis0 per0mod */
	0, /* spis1 per0mod */
	ALT_RSTMGR_PER1MODRST_UART0_SET_MSK,
	ALT_RSTMGR_PER1MODRST_UART1_SET_MSK,
};

void reset_assert_fpga_connected_peripherals(void)
{
	u32 mask0 = 0;
	u32 mask1 = 0;
	u32 fpga_pinux_addr = SOCFPGA_PINMUX_FPGA_INTERFACE_ADDRESS;
	int i;

	for (i = 0; i < ARRAY_SIZE(per1fpgamasks); i++) {
		if (readl(fpga_pinux_addr)) {
			mask0 |= per0fpgamasks[i];
			mask1 |= per1fpgamasks[i];
		}
		fpga_pinux_addr += sizeof(u32);
	}

	setbits_le32(&reset_manager_base->per0modrst, mask0 & ECC_MASK);
	setbits_le32(&reset_manager_base->per1modrst, mask1);
	setbits_le32(&reset_manager_base->per0modrst, mask0);
}

void reset_deassert_fpga_connected_peripherals(void)
{
	u32 mask0 = 0;
	u32 mask1 = 0;
	u32 fpga_pinux_addr = SOCFPGA_PINMUX_FPGA_INTERFACE_ADDRESS;
	int i;

	for (i = 0; i < ARRAY_SIZE(per1fpgamasks); i++) {
		if (readl(fpga_pinux_addr)) {
			mask0 |= per0fpgamasks[i];
			mask1 |= per1fpgamasks[i];
		}
		fpga_pinux_addr += sizeof(u32);
	}

	clrbits_le32(&reset_manager_base->per0modrst, mask0 & ECC_MASK);
	clrbits_le32(&reset_manager_base->per1modrst, mask1);
	clrbits_le32(&reset_manager_base->per0modrst, mask0);
}

void reset_deassert_shared_connected_peripherals_q1(u32 *mask0, u32 *mask1,
						    u32 *pinmux_addr)
{
	int i;

	for (i = 1; i <= 12; i++, *pinmux_addr += sizeof(u32)) {
		switch (readl(*pinmux_addr)) {
		case 15:
			*mask1 |= ALT_RSTMGR_PER1MODRST_GPIO0_SET_MSK;
			break;
		case 14:
			*mask0 |= ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK|
				ALT_RSTMGR_PER0MODRST_NAND_SET_MSK;
			break;
		case 13:
			if ((i >= 1) && (i <= 4))
				*mask1 |= ALT_RSTMGR_PER1MODRST_UART0_SET_MSK;
			else if ((i >= 5) && (i <= 8))
				*mask1 |= ALT_RSTMGR_PER1MODRST_UART1_SET_MSK;
			break;
		case 12:
			if ((i >= 5) && (i <= 6))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_QSPIECC_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_QSPI_SET_MSK;
			break;
		case 8:
			*mask0 |= ALT_RSTMGR_PER0MODRST_USBECC0_SET_MSK |
			    ALT_RSTMGR_PER0MODRST_USB0_SET_MSK;
			break;
		case 4:
			if ((i >= 1) && (i <= 10))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_SDMMCECC_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_SDMMC_SET_MSK;
			break;
		case 3:
			if ((i == 1) || ((i >= 5) && (i <= 8)))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIM0_SET_MSK;
			else if ((i == 2) || ((i >= 9) && (i <= 12)))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIM1_SET_MSK;
			break;
		case 2:
			if ((i >= 1) && (i <= 4))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIS0_SET_MSK;
			else if ((i >= 9) && (i <= 12))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIS1_SET_MSK;
			break;
		case 1:
			if ((i == 7) || (i == 8))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC2_SET_MSK;
			else if ((i == 9) || (i == 10))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC1_SET_MSK;
			else if ((i == 11) || (1 == 12))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC0_SET_MSK;
			break;
		case 0:
			if ((i == 3) || (i == 4))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C1_SET_MSK;
			else if ((i == 5) || (i == 6))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C0_SET_MSK;
			else if ((i == 7) || (i == 8))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C4_SET_MSK;
			else if ((i == 9) || (i == 10))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C3_SET_MSK;
			else if ((i == 11) || (i == 12))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C2_SET_MSK;
			break;
		}
	}
}

void reset_deassert_shared_connected_peripherals_q2(u32 *mask0, u32 *mask1,
						    u32 *pinmux_addr)
{
	int i;

	for (i = 1; i <= 12; i++, *pinmux_addr += sizeof(u32)) {
		switch (readl(*pinmux_addr)) {
		case 15:
			*mask1 |= ALT_RSTMGR_PER1MODRST_GPIO0_SET_MSK;
			break;
		case 14:
			if ((i != 4) && (i != 5))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_NAND_SET_MSK;
			break;
		case 13:
			if ((i >= 9) && (i <= 12))
				*mask1 |= ALT_RSTMGR_PER1MODRST_UART0_SET_MSK;
			break;
		case 8:
			*mask0 |= ALT_RSTMGR_PER0MODRST_USBECC1_SET_MSK |
			    ALT_RSTMGR_PER0MODRST_USB1_SET_MSK;
			break;
		case 4:
			*mask0 |= ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK |
			    ALT_RSTMGR_PER0MODRST_EMAC0_SET_MSK;
			break;
		case 3:
			if ((i >= 8) && (i <= 12))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIM1_SET_MSK;
			break;
		case 2:
			if ((i >= 9) && (i <= 12))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIS0_SET_MSK;
			break;
		case 0:
			if ((i == 9) || (i == 10))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C1_SET_MSK;
			else if ((i == 11) || (i == 12))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C0_SET_MSK;
			break;
		}
	}
}

void reset_deassert_shared_connected_peripherals_q3(u32 *mask0, u32 *mask1,
						    u32 *pinmux_addr)
{
	int i;

	for (i = 1; i <= 12; i++, *pinmux_addr += sizeof(u32)) {
		switch (readl(*pinmux_addr)) {
		case 15:
			*mask1 |= ALT_RSTMGR_PER1MODRST_GPIO1_SET_MSK;
			break;
		case 14:
			*mask0 |= ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK |
			    ALT_RSTMGR_PER0MODRST_NAND_SET_MSK;
			break;
		case 13:
			if ((i >= 1) && (i <= 4))
				*mask1 |= ALT_RSTMGR_PER1MODRST_UART0_SET_MSK;
			else if ((i >= 5) && (i <= 8))
				*mask1 |= ALT_RSTMGR_PER1MODRST_UART1_SET_MSK;
			break;
		case 8:
			*mask0 |= ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK |
			    ALT_RSTMGR_PER0MODRST_EMAC1_SET_MSK;
			break;
		case 3:
			if ((i >= 1) && (i <= 5))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIM1_SET_MSK;
			break;
		case 2:
			if ((i >= 5) && (i <= 8))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIS1_SET_MSK;
			else if ((i >= 9) && (i <= 12))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIS0_SET_MSK;
			break;
		case 1:
			if ((i == 9) || (i == 10))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC2_SET_MSK;
			else if ((i == 11) || (i == 12))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC0_SET_MSK;
			break;
		case 0:
			if ((i == 7) || (i == 8))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C1_SET_MSK;
			else if ((i == 3) || (i == 4))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C0_SET_MSK;
			else if ((i == 9) || (i == 10))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C4_SET_MSK;
			else if ((i == 11) || (i == 12))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C2_SET_MSK;
			break;
		}
	}
}

void reset_deassert_shared_connected_peripherals_q4(u32 *mask0, u32 *mask1,
						    u32 *pinmux_addr)
{
	int i;

	for (i = 1; i <= 12; i++, *pinmux_addr += sizeof(u32)) {
		switch (readl(*pinmux_addr)) {
		case 15:
			*mask1 |= ALT_RSTMGR_PER1MODRST_GPIO1_SET_MSK;
			break;
		case 14:
			if (i != 4)
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_NAND_SET_MSK;
			break;
		case 13:
			if ((i >= 3) && (i <= 6))
				*mask1 |= ALT_RSTMGR_PER1MODRST_UART1_SET_MSK;
			break;
		case 12:
			if ((i == 5) || (i == 6))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_QSPIECC_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_QSPI_SET_MSK;
			break;
		case 8:
			*mask0 |= ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK |
			    ALT_RSTMGR_PER0MODRST_EMAC2_SET_MSK;
			break;
		case 4:
			if ((i >= 1) && (i <= 6))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_SDMMCECC_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_SDMMC_SET_MSK;
			break;
		case 3:
			if ((i >= 6) && (i <= 12))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIM0_SET_MSK;
			break;
		case 2:
			if ((i >= 9) && (i <= 12))
				*mask0 |= ALT_RSTMGR_PER0MODRST_SPIS1_SET_MSK;
			break;
		case 1:
			if ((i == 7) || (i == 8))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC1_SET_MSK;
			else if ((i == 11) || (i == 12))
				*mask0 |=
				    ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK |
				    ALT_RSTMGR_PER0MODRST_EMAC0_SET_MSK;
			break;
		case 0:
			if ((i == 1) || (i == 2))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C1_SET_MSK;
			else if ((i == 7) || (i == 8))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C3_SET_MSK;
			else if ((i == 9) || (i == 10))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C4_SET_MSK;
			else if ((i == 11) || (i == 12))
				*mask1 |= ALT_RSTMGR_PER1MODRST_I2C2_SET_MSK;
			break;
		}
	}
}

void reset_assert_shared_connected_peripherals(void)
{
	u32 mask0 = 0;
	u32 mask1 = 0;
	u32 pinmux_addr = SOCFPGA_PINMUX_SHARED_3V_IO_ADDRESS;

	reset_deassert_shared_connected_peripherals_q1(&mask0, &mask1,
						       &pinmux_addr);
	reset_deassert_shared_connected_peripherals_q2(&mask0, &mask1,
						       &pinmux_addr);
	reset_deassert_shared_connected_peripherals_q3(&mask0, &mask1,
						       &pinmux_addr);
	reset_deassert_shared_connected_peripherals_q4(&mask0, &mask1,
						       &pinmux_addr);
	mask1 |= ALT_RSTMGR_PER1MODRST_WD1_SET_MSK |
		ALT_RSTMGR_PER1MODRST_L4SYSTMR1_SET_MSK |
		ALT_RSTMGR_PER1MODRST_SPTMR0_SET_MSK |
		ALT_RSTMGR_PER1MODRST_SPTMR1_SET_MSK;

	setbits_le32(&reset_manager_base->per0modrst, mask0 & ECC_MASK);
	setbits_le32(&reset_manager_base->per1modrst, mask1);
	setbits_le32(&reset_manager_base->per0modrst, mask0);
}

void reset_deassert_shared_connected_peripherals(void)
{
	u32 mask0 = 0;
	u32 mask1 = 0;
	u32 pinmux_addr = SOCFPGA_PINMUX_SHARED_3V_IO_ADDRESS;

	reset_deassert_shared_connected_peripherals_q1(&mask0, &mask1,
						       &pinmux_addr);
	reset_deassert_shared_connected_peripherals_q2(&mask0, &mask1,
						       &pinmux_addr);
	reset_deassert_shared_connected_peripherals_q3(&mask0, &mask1,
						       &pinmux_addr);
	reset_deassert_shared_connected_peripherals_q4(&mask0, &mask1,
						       &pinmux_addr);
	mask1 |= ALT_RSTMGR_PER1MODRST_WD1_SET_MSK |
		ALT_RSTMGR_PER1MODRST_L4SYSTMR1_SET_MSK |
		ALT_RSTMGR_PER1MODRST_SPTMR0_SET_MSK |
		ALT_RSTMGR_PER1MODRST_SPTMR1_SET_MSK;

	clrbits_le32(&reset_manager_base->per0modrst, mask0 & ECC_MASK);
	clrbits_le32(&reset_manager_base->per1modrst, mask1);
	clrbits_le32(&reset_manager_base->per0modrst, mask0);
}

/* Release L4 OSC1 Watchdog Timer 0 from reset through reset manager */
void reset_deassert_osc1wd0(void)
{
	clrbits_le32(&reset_manager_base->per1modrst,
		ALT_RSTMGR_PER1MODRST_WD0_SET_MSK);
}
