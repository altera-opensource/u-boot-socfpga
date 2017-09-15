/*
 * Copyright (C) 2016-2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <errno.h>
#include <div64.h>
#include <asm/io.h>
#include <watchdog.h>
#include <asm/arch/sdram_s10.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/reset_manager.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;
static const struct socfpga_noc_ddr_scheduler *socfpga_noc_ddr_scheduler_base =
		(void *)SOCFPGA_SDR_SCHEDULER_ADDRESS;
static const struct socfpga_io48_mmr *socfpga_io48_mmr_base =
		(void *)SOCFPGA_HMC_MMR_IO48_ADDRESS;
static const struct socfpga_system_manager *sysmgr_regs =
		(void *)SOCFPGA_SYSMGR_ADDRESS;

#define DDR_CONFIG(A, B, C, R)	((A<<24)|(B<<16)|(C<<8)|R)

/* The followring are the supported configurations */
u32 ddr_config[] = {
	/* DDR_CONFIG(Address order,Bank,Column,Row) */
	/* List for DDR3 or LPDDR3 (pinout order > chip, row, bank, column) */
	DDR_CONFIG(0, 3, 10, 12),
	DDR_CONFIG(0, 3,  9, 13),
	DDR_CONFIG(0, 3, 10, 13),
	DDR_CONFIG(0, 3,  9, 14),
	DDR_CONFIG(0, 3, 10, 14),
	DDR_CONFIG(0, 3, 10, 15),
	DDR_CONFIG(0, 3, 11, 14),
	DDR_CONFIG(0, 3, 11, 15),
	DDR_CONFIG(0, 3, 10, 16),
	DDR_CONFIG(0, 3, 11, 16),
	DDR_CONFIG(0, 3, 12, 15),	/* 0xa */
	/* List for DDR4 only (pinout order > chip, bank, row, column) */
	DDR_CONFIG(1, 3, 10, 14),
	DDR_CONFIG(1, 4, 10, 14),
	DDR_CONFIG(1, 3, 10, 15),
	DDR_CONFIG(1, 4, 10, 15),
	DDR_CONFIG(1, 3, 10, 16),
	DDR_CONFIG(1, 4, 10, 16),
	DDR_CONFIG(1, 3, 10, 17),
	DDR_CONFIG(1, 4, 10, 17),
};

#define DDR_CONFIG_ELEMENTS	(sizeof(ddr_config)/sizeof(u32))

int match_ddr_conf(u32 ddr_conf)
{
	int i;
	for (i = 0; i < DDR_CONFIG_ELEMENTS; i++) {
		if (ddr_conf == ddr_config[i])
			return i;
	}
	return 0;
}

static int emif_clear(void)
{
	u32 s2c, i;

	writel(0, &socfpga_ecc_hmc_base->rsthandshakectrl);
	s2c = readl(&socfpga_ecc_hmc_base->rsthandshakestat) &
	      DDR_HMC_RSTHANDSHAKE_MASK;

	for (i = 1000; (i > 0) && s2c; i--) {
		WATCHDOG_RESET();
		mdelay(1);
		s2c = readl(&socfpga_ecc_hmc_base->rsthandshakestat) &
		      DDR_HMC_RSTHANDSHAKE_MASK;
	}
	return !s2c;
}

static int emif_reset(void)
{
	u32 c2s, s2c, i;

	c2s = readl(&socfpga_ecc_hmc_base->rsthandshakectrl) &
	      DDR_HMC_RSTHANDSHAKE_MASK;
	s2c = readl(&socfpga_ecc_hmc_base->rsthandshakestat) &
	      DDR_HMC_RSTHANDSHAKE_MASK;

	debug("DDR: c2s=%08x s2c=%08x nr0=%08x nr1=%08x nr2=%08x dst=%08x\n",
		c2s, s2c, readl(&socfpga_io48_mmr_base->niosreserve0),
		readl(&socfpga_io48_mmr_base->niosreserve1),
		readl(&socfpga_io48_mmr_base->niosreserve2),
		readl(&socfpga_io48_mmr_base->dramsts));

	if (s2c && emif_clear()) {
		printf("DDR: emif_clear() failed\n");
		return -1;
	}

	puts("DDR: Triggerring emif_reset\n");
	writel(DDR_HMC_CORE2SEQ_INT_REQ,
	       &socfpga_ecc_hmc_base->rsthandshakectrl);

	for (i = 1000; i > 0; i--) {
		/* if seq2core[3] = 0, we are good */
		if (!(readl(&socfpga_ecc_hmc_base->rsthandshakestat) &
		    DDR_HMC_SEQ2CORE_INT_RESP_MASK))
			break;
		WATCHDOG_RESET();
		mdelay(1);
	}

	if (!i) {
		printf("DDR: failed to get ack from EMIF\n");
		return -2;
	}

	if (emif_clear()) {
		printf("DDR: emif_clear() failed\n");
		return -3;
	}

	printf("DDR: emif_reset triggered successly\n");
	return 0;
}

static int poll_hmc_clock_status(void)
{
	u32 status, i;

	for (i = 1000; i > 0; i--) {
		status = readl(&sysmgr_regs->hmc_clk) &
			 SYSMGR_HMC_CLK_STATUS_MSK;
		udelay(1);
		if (status)
			break;
		WATCHDOG_RESET();
	}
	return status;
}

/**
 * sdram_mmr_init_full() - Function to initialize SDRAM MMR
 *
 * Initialize the SDRAM MMR.
 */
int sdram_mmr_init_full(unsigned int unused)
{
	u32 update_value, io48_value, ddrioctl;
	u32 i, j, cal_success;

	/* Enable access to DDR from CPU master */
	clrbits_le32(CCU_CPU0_MPRT_ADBASE_DDRREG_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_CPU0_MPRT_ADBASE_MEMSPACE0_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_CPU0_MPRT_ADBASE_MEMSPACE1A_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_CPU0_MPRT_ADBASE_MEMSPACE1B_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_CPU0_MPRT_ADBASE_MEMSPACE1C_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_CPU0_MPRT_ADBASE_MEMSPACE1D_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_CPU0_MPRT_ADBASE_MEMSPACE1E_ADDR, CCU_ADBASE_DI_MASK);

	/* Enable access to DDR from IO master */
	clrbits_le32(CCU_IOM_MPRT_ADBASE_MEMSPACE0_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_IOM_MPRT_ADBASE_MEMSPACE1A_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_IOM_MPRT_ADBASE_MEMSPACE1B_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_IOM_MPRT_ADBASE_MEMSPACE1C_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_IOM_MPRT_ADBASE_MEMSPACE1D_ADDR, CCU_ADBASE_DI_MASK);
	clrbits_le32(CCU_IOM_MPRT_ADBASE_MEMSPACE1E_ADDR, CCU_ADBASE_DI_MASK);

	/* this enables nonsecure access to DDR */
	/* mpuregion0addr_limit */
	writel(0xFFFF0000, 0xF8020118);
	writel(0x1F, 0xF802011c);

	/* nonmpuregion0addr_limit */
	writel(0xFFFF0000, 0xF8020198);
	writel(0x1F, 0xF802019C);

	/* Enable mpuregion0enable and nonmpuregion0enable */
	writel(BIT(0) | BIT(8), 0xF8020100);

	/* Ensure HMC clock is running */
	if (!poll_hmc_clock_status()) {
		puts("DDR: Error as HMC clock not running\n");
		return -1;
	}

	/* release DDR scheduler from reset */
	socfpga_per_reset(SOCFPGA_RESET(SDR), 0);

	/* Try 3 times to do a calibration */
	for (i = 0; i < 3; i++) {
		cal_success = readl(&socfpga_ecc_hmc_base->ddrcalstat) &
			      DDR_HMC_DDRCALSTAT_CAL_MSK;
		/* A delay to wait for calibration bit to set */
		for (j = 0; (j < 1000) && !cal_success; j++) {
			WATCHDOG_RESET();
			mdelay(1);
			cal_success = readl(&socfpga_ecc_hmc_base->ddrcalstat)
				      & DDR_HMC_DDRCALSTAT_CAL_MSK;
		}

		if (cal_success)
			break;
		else
			emif_reset();
	}

	if (!cal_success) {
		puts("DDR: Error as SDRAM calibration failed\n");
		return -1;
	}
	puts("DDR: Calibration success\n");

	union ctrlcfg0_reg ctrlcfg0 = (union ctrlcfg0_reg)
				readl(&socfpga_io48_mmr_base->ctrlcfg0);
	union ctrlcfg1_reg ctrlcfg1 = (union ctrlcfg1_reg)
				readl(&socfpga_io48_mmr_base->ctrlcfg1);
	union dramaddrw_reg dramaddrw = (union dramaddrw_reg)
				readl(&socfpga_io48_mmr_base->dramaddrw);
	union dramtiming0_reg dramtim0 = (union dramtiming0_reg)
				readl(&socfpga_io48_mmr_base->dramtiming0);
	union caltiming0_reg caltim0 = (union caltiming0_reg)
				readl(&socfpga_io48_mmr_base->caltiming0);
	union caltiming1_reg caltim1 = (union caltiming1_reg)
				readl(&socfpga_io48_mmr_base->caltiming1);
	union caltiming2_reg caltim2 = (union caltiming2_reg)
				readl(&socfpga_io48_mmr_base->caltiming2);
	union caltiming3_reg caltim3 = (union caltiming3_reg)
				readl(&socfpga_io48_mmr_base->caltiming3);
	union caltiming4_reg caltim4 = (union caltiming4_reg)
				readl(&socfpga_io48_mmr_base->caltiming4);
	union caltiming9_reg caltim9 = (union caltiming9_reg)
				readl(&socfpga_io48_mmr_base->caltiming9);

	/*
	 * Configure the DDR IO size [0xFFCFB008]
	 * niosreserve0: Used to indicate DDR width &
	 *	bit[7:0] = Number of data bits (bit[6:5] 0x01=32bit, 0x10=64bit)
	 *	bit[8]   = 1 if user-mode OCT is present
	 *	bit[9]   = 1 if warm reset compiled into EMIF Cal Code
	 *	bit[10]  = 1 if warm reset is on during generation in EMIF Cal
	 * niosreserve1: IP ADCDS version encoded as 16 bit value
	 *	bit[2:0] = Variant (0=not special,1=FAE beta, 2=Customer beta,
	 *			    3=EAP, 4-6 are reserved)
	 *	bit[5:3] = Service Pack # (e.g. 1)
	 *	bit[9:6] = Minor Release #
	 *	bit[14:10] = Major Release #
	 */
	update_value = readl(&socfpga_io48_mmr_base->niosreserve0);
	writel(((update_value & 0xFF) >> 5), &socfpga_ecc_hmc_base->ddrioctrl);
	ddrioctl = readl(&socfpga_ecc_hmc_base->ddrioctrl);

	/* enable HPS interface to HMC */
	writel(DDR_HMC_HPSINTFCSEL_ENABLE_MASK,
	       &socfpga_ecc_hmc_base->hpsintfcsel);

	/* Set the DDR Configuration */
	io48_value = DDR_CONFIG(ctrlcfg1.cfg_addr_order,
				(dramaddrw.cfg_bank_addr_width +
				 dramaddrw.cfg_bank_group_addr_width),
				dramaddrw.cfg_col_addr_width,
				dramaddrw.cfg_row_addr_width);

	update_value = match_ddr_conf(io48_value);
	if (update_value)
		writel(update_value,
		&socfpga_noc_ddr_scheduler_base->main_scheduler_ddrconf);

	/* Configure HMC dramaddrw */
	writel(readl(&socfpga_io48_mmr_base->dramaddrw),
		&socfpga_ecc_hmc_base->dramaddrwidth);

	/*
	 * Configure DDR timing
	 *  RDTOMISS = tRTP + tRP + tRCD - BL/2
	 *  WRTOMISS = WL + tWR + tRP + tRCD and
	 *    WL = RL + BL/2 + 2 - rd-to-wr ; tWR = 15ns  so...
	 *  First part of equation is in memory clock units so divide by 2
	 *  for HMC clock units. 1066MHz is close to 1ns so use 15 directly.
	 *  WRTOMISS = ((RL + BL/2 + 2 + tWR) >> 1)- rd-to-wr + tRP + tRCD
	 */
	update_value = caltim2.cfg_rd_to_pch + caltim4.cfg_pch_to_valid +
		       caltim0.cfg_act_to_rdwr -
		       (ctrlcfg0.cfg_ctrl_burst_len >> 2);
	io48_value = (((dramtim0.cfg_tcl + 2 + DDR_TWR +
		      (ctrlcfg0.cfg_ctrl_burst_len >> 1)) >> 1) -
		      /* Up to here was in memory cycles so divide by 2 */
		      caltim1.cfg_rd_to_wr + caltim0.cfg_act_to_rdwr +
		      caltim4.cfg_pch_to_valid);

	writel(((caltim0.cfg_act_to_act << DDR_SCHED_DDRTIMING_ACTTOACT_OFFSET) |
		(update_value << DDR_SCHED_DDRTIMING_RDTOMISS_OFFSET) |
		(io48_value << DDR_SCHED_DDRTIMING_WRTOMISS_OFFSET) |
		((ctrlcfg0.cfg_ctrl_burst_len >> 2) <<
			DDR_SCHED_DDRTIMING_BURSTLEN_OFFSET) |
		(caltim1.cfg_rd_to_wr << DDR_SCHED_DDRTIMING_RDTOWR_OFFSET) |
		(caltim3.cfg_wr_to_rd << DDR_SCHED_DDRTIMING_WRTORD_OFFSET) |
		(((ddrioctl == 1) ? 1 : 0) <<
			DDR_SCHED_DDRTIMING_BWRATIO_OFFSET)),
		&socfpga_noc_ddr_scheduler_base->main_scheduler_ddrtiming);

	/* Configure DDR mode [precharge = 0] */
	writel(((ddrioctl ? 0 : 1) << DDR_SCHED_DDRMOD_BWRATIOEXTENDED_OFFSET),
	       &socfpga_noc_ddr_scheduler_base->main_scheduler_ddrmode);

	/* Configure the read latency */
	writel((dramtim0.cfg_tcl >> 1) + DDR_READ_LATENCY_DELAY,
	       &socfpga_noc_ddr_scheduler_base->main_scheduler_readlatency);

	/*
	 * Configuring timing values concerning activate commands
	 * [FAWBANK alway 1 because always 4 bank DDR]
	 */
	writel(((caltim0.cfg_act_to_act_db << DDR_SCHED_ACTIVATE_RRD_OFFSET) |
	       (caltim9.cfg_4_act_to_act << DDR_SCHED_ACTIVATE_FAW_OFFSET) |
	       (DDR_ACTIVATE_FAWBANK << DDR_SCHED_ACTIVATE_FAWBANK_OFFSET)),
	       &socfpga_noc_ddr_scheduler_base->main_scheduler_activate);

	/*
	 * Configuring timing values concerning device to device data bus
	 * ownership change
	 */
	writel(((caltim1.cfg_rd_to_rd_dc <<
			DDR_SCHED_DEVTODEV_BUSRDTORD_OFFSET) |
	       (caltim1.cfg_rd_to_wr_dc <<
			DDR_SCHED_DEVTODEV_BUSRDTOWR_OFFSET) |
	       (caltim3.cfg_wr_to_rd_dc <<
			DDR_SCHED_DEVTODEV_BUSWRTORD_OFFSET)),
	       &socfpga_noc_ddr_scheduler_base->main_scheduler_devtodev);

	/* Enable or disable the SDRAM ECC */
	if (ctrlcfg1.cfg_ctrl_enable_ecc) {
		setbits_le32(&socfpga_ecc_hmc_base->eccctrl,
			     (DDR_HMC_ECCCTL_AWB_CNT_RST_SET_MSK |
			      DDR_HMC_ECCCTL_CNT_RST_SET_MSK |
			      DDR_HMC_ECCCTL_ECC_EN_SET_MSK));
		clrbits_le32(&socfpga_ecc_hmc_base->eccctrl,
			     (DDR_HMC_ECCCTL_AWB_CNT_RST_SET_MSK |
			      DDR_HMC_ECCCTL_CNT_RST_SET_MSK));
		setbits_le32(&socfpga_ecc_hmc_base->eccctrl2,
			     (DDR_HMC_ECCCTL2_RMW_EN_SET_MSK |
			      DDR_HMC_ECCCTL2_AWB_EN_SET_MSK));
	} else {
		clrbits_le32(&socfpga_ecc_hmc_base->eccctrl,
			     (DDR_HMC_ECCCTL_AWB_CNT_RST_SET_MSK |
			      DDR_HMC_ECCCTL_CNT_RST_SET_MSK |
			      DDR_HMC_ECCCTL_ECC_EN_SET_MSK));
		clrbits_le32(&socfpga_ecc_hmc_base->eccctrl2,
			     (DDR_HMC_ECCCTL2_RMW_EN_SET_MSK |
			      DDR_HMC_ECCCTL2_AWB_EN_SET_MSK));
	}

	puts("DDR: HMC init success\n");
	return 0;
}

/**
 * sdram_calculate_size() - Calculate SDRAM size
 *
 * Calculate SDRAM device size based on SDRAM controller parameters.
 * Size is specified in bytes.
 */
unsigned long sdram_calculate_size(void)
{
	union dramaddrw_reg dramaddrw =
		(union dramaddrw_reg)readl(&socfpga_io48_mmr_base->dramaddrw);

	u32 size = (1 << (dramaddrw.cfg_cs_addr_width +
		    dramaddrw.cfg_bank_group_addr_width +
		    dramaddrw.cfg_bank_addr_width +
		    dramaddrw.cfg_row_addr_width +
		    dramaddrw.cfg_col_addr_width));

	size *= (2 << (readl(&socfpga_ecc_hmc_base->ddrioctrl) &
		       DDR_HMC_DDRIOCTRL_IOSIZE_MSK));

	return size;
}
