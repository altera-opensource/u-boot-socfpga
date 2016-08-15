/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <fdtdec.h>
#include <malloc.h>
#include <mmc.h>
#include <nand.h>
#include <watchdog.h>
#include <ns16550.h>
#include <pl330.h>
#include <asm/io.h>
#include <asm/arch/cff.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/sdram.h>

DECLARE_GLOBAL_DATA_PTR;

unsigned long irq_cnt_ecc_sdram = 0;

/* FAWBANK - Number of Bank of a given device involved in the FAW period. */
#define ARRIA10_SDR_ACTIVATE_FAWBANK	(0x1)

#define ARRIA10_EMIF_RST	31	/* fpga_mgr_fpgamgrregs.gpo.31 */
#define ARRIA10_OCT_CAL_REQ	30	/* fpga_mgr_fpgamgrregs.gpo.30 */
#define ARRIA10_OCT_CAL_ACK	31	/* fpga_mgr_fpgamgrregs.gpi.31 */

#define ARRIA10_NIOS_OCT_DONE	7
#define ARRIA10_NIOS_OCT_ACK	7

/* Engineering sample silicon */
#define ARRIA10_ES_SILICON_VER	0x00010001

#define DDR_REG_SEQ2CORE        0xFFD0507C
#define DDR_REG_CORE2SEQ        0xFFD05078
#define DDR_REG_GPOUT           0xFFD03010
#define DDR_REG_GPIN            0xFFD03014
#define DDR_MAX_TRIES		0x00100000
#define IO48_MMR_DRAMSTS	0xFFCFA0EC
#define IO48_MMR_NIOS2_RESERVE0	0xFFCFA110
#define IO48_MMR_NIOS2_RESERVE1	0xFFCFA114
#define IO48_MMR_NIOS2_RESERVE2	0xFFCFA118

#define SEQ2CORE_MASK		0xF
#define CORE2SEQ_INT_REQ	0xF
#define SEQ2CORE_INT_RESP_BIT	3

#define DDR_ECC_DMA_SIZE	1500
#define DDR_READ_LATENCY_DELAY	40

static const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;
static const struct socfpga_noc_ddr_scheduler *socfpga_noc_ddr_scheduler_base =
		(void *)SOCFPGA_SDR_SCHEDULER_ADDRESS;
static const struct socfpga_noc_fw_ddr_mpu_fpga2sdram
		*socfpga_noc_fw_ddr_mpu_fpga2sdram_base =
		(void *)SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS;
static const struct socfpga_noc_fw_ddr_l3 *socfpga_noc_fw_ddr_l3_base =
		(void *)SOCFPGA_SDR_FIREWALL_L3_ADDRESS;
static const struct socfpga_system_manager *socfpga_system_mgr =
		(void *)SOCFPGA_SYSMGR_ADDRESS;
static const struct socfpga_io48_mmr *socfpga_io48_mmr_base =
		(void *)SOCFPGA_HMC_MMR_IO48_ADDRESS;

#define ARRIA_DDR_CONFIG(A, B, C, R)	((A<<24)|(B<<16)|(C<<8)|R)
/* The followring are the supported configurations */
u32 ddr_config[] = {
	/* Chip - Row - Bank - Column Style */
	/* All Types */
	ARRIA_DDR_CONFIG(0, 3, 10, 12),
	ARRIA_DDR_CONFIG(0, 3, 10, 13),
	ARRIA_DDR_CONFIG(0, 3, 10, 14),
	ARRIA_DDR_CONFIG(0, 3, 10, 15),
	ARRIA_DDR_CONFIG(0, 3, 10, 16),
	ARRIA_DDR_CONFIG(0, 3, 10, 17),
	/* LPDDR x16 */
	ARRIA_DDR_CONFIG(0, 3, 11, 14),
	ARRIA_DDR_CONFIG(0, 3, 11, 15),
	ARRIA_DDR_CONFIG(0, 3, 11, 16),
	ARRIA_DDR_CONFIG(0, 3, 12, 15),
	/* DDR4 Only */
	ARRIA_DDR_CONFIG(0, 4, 10, 14),
	ARRIA_DDR_CONFIG(0, 4, 10, 15),
	ARRIA_DDR_CONFIG(0, 4, 10, 16),
	ARRIA_DDR_CONFIG(0, 4, 10, 17),	/* 14 */
	/* Chip - Bank - Row - Column Style */
	ARRIA_DDR_CONFIG(1, 3, 10, 12),
	ARRIA_DDR_CONFIG(1, 3, 10, 13),
	ARRIA_DDR_CONFIG(1, 3, 10, 14),
	ARRIA_DDR_CONFIG(1, 3, 10, 15),
	ARRIA_DDR_CONFIG(1, 3, 10, 16),
	ARRIA_DDR_CONFIG(1, 3, 10, 17),
	ARRIA_DDR_CONFIG(1, 3, 11, 14),
	ARRIA_DDR_CONFIG(1, 3, 11, 15),
	ARRIA_DDR_CONFIG(1, 3, 11, 16),
	ARRIA_DDR_CONFIG(1, 3, 12, 15),
	/* DDR4 Only */
	ARRIA_DDR_CONFIG(1, 4, 10, 14),
	ARRIA_DDR_CONFIG(1, 4, 10, 15),
	ARRIA_DDR_CONFIG(1, 4, 10, 16),
	ARRIA_DDR_CONFIG(1, 4, 10, 17),
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

/* Check whether SDRAM is successfully Calibrated */
int is_sdram_cal_success(void)
{
	return readl(&socfpga_ecc_hmc_base->ddrcalstat);
}

unsigned char ddr_get_bit(u32 ereg, unsigned char bit)
{
	unsigned int reg = readl(ereg);

	return (reg & (1 << bit)) ? 1 : 0;
}

unsigned char ddr_wait_bit(u32 ereg, u32 bit,
			   u32 expected, u32 timeout_usec)
{
	unsigned int tmr;

	for (tmr = 0; tmr < timeout_usec; tmr += 100) {
		udelay(100);
		WATCHDOG_RESET();
		if (ddr_get_bit(ereg, bit) == expected)
			return 0;
	}

	return 1;
}

void ddr_set_bit(u32 ereg, u32 bit)
{
	unsigned int tmp = readl(ereg);

	tmp |= (1 << bit);
	writel(tmp, ereg);
}

void ddr_clr_bit(u32 ereg, u32 bit)
{
	unsigned int tmp = readl(ereg);

	tmp &= ~(1 << bit);
	writel(tmp, ereg);
}

void ddr_delay(u32 delay)
{
	int tmr;

	for (tmr = 0; tmr < delay; tmr++) {
		udelay(1000);
		WATCHDOG_RESET();
	}
}

/*
 * Diagram of OCT Workaround:
 *
 * EMIF Core                     HPS Processor              OCT FSM
 * =================================================================
 *
 * seq2core      ==============>
 * [0x?????????]   OCT Request   [0xFFD0507C]
 *
 * core2seq
 * [0x?????????] <==============
 *                 OCT Ready     [0xFFD05078]
 *
 *                               [0xFFD03010] ============> Request
 *                                             OCT Request
 *
 *                               [0xFFD03014] <============ Ready
 *                                              OCT Ready
 * Signal definitions:
 *
 * seq2core[7] - OCT calibration request (act-high)
 * core2seq[7] - Signals OCT FSM is ready (active high)
 * gpout[31]   - EMIF Reset override (active low)
 * gpout[30]   - OCT calibration request (act-high)
 * gpin[31]    - OCT calibration ready (act-high)
 */

int ddr_calibration_es_workaround(void)
{
	ddr_delay(500);
	/* Step 1 - Initiating Reset Sequence */
	ddr_clr_bit(DDR_REG_GPOUT, ARRIA10_EMIF_RST);	/* Reset EMIF */
	ddr_delay(10);

	/* Step 2 - Clearing registers to EMIF core */
	writel(0, DDR_REG_CORE2SEQ);	/*Clear the HPS->NIOS COM reg.*/

	/* Step 3 - Clearing registers to OCT core */
	ddr_clr_bit(DDR_REG_GPOUT, ARRIA10_OCT_CAL_REQ); /* OCT Cal Request */
	ddr_delay(5);

	/* Step 4 - Taking EMIF out of reset */
	ddr_set_bit(DDR_REG_GPOUT, ARRIA10_EMIF_RST);
	ddr_delay(10);

	/* Step 5 - Waiting for OCT circuitry to come out of reset */
	if (ddr_wait_bit(DDR_REG_GPIN, ARRIA10_OCT_CAL_ACK, 1, 1000000))
		return -1;

	/* Step 6 - Allowing EMIF to proceed with OCT calibration */
	ddr_set_bit(DDR_REG_CORE2SEQ, ARRIA10_NIOS_OCT_DONE);

	/* Step 7 - Waiting for EMIF request */
	if (ddr_wait_bit(DDR_REG_SEQ2CORE, ARRIA10_NIOS_OCT_ACK, 1, 2000000))
		return -2;

	/* Step 8 - Acknowledging EMIF OCT request */
	ddr_clr_bit(DDR_REG_CORE2SEQ, ARRIA10_NIOS_OCT_DONE);

	/* Step 9 - Waiting for EMIF response */
	if (ddr_wait_bit(DDR_REG_SEQ2CORE, ARRIA10_NIOS_OCT_ACK, 0, 2000000))
		return -3;

	/* Step 10 - Triggering OCT Calibration */
	ddr_set_bit(DDR_REG_GPOUT, ARRIA10_OCT_CAL_REQ);

	/* Step 11 - Waiting for OCT response */
	if (ddr_wait_bit(DDR_REG_GPIN, ARRIA10_OCT_CAL_ACK, 0, 1000))
		return -4;

	/* Step 12 - Clearing OCT Request bit */
	ddr_clr_bit(DDR_REG_GPOUT, ARRIA10_OCT_CAL_REQ);

	/* Step 13 - Waiting for OCT Engine */
	if (ddr_wait_bit(DDR_REG_GPIN, ARRIA10_OCT_CAL_ACK, 1, 200000))
		return -5;

	/* Step 14 - Proceeding with EMIF calibration */
	ddr_set_bit(DDR_REG_CORE2SEQ, ARRIA10_NIOS_OCT_DONE);

	ddr_delay(100);

	return 0;
}

static int emif_clear(void)
{
	u32 s2c;
	u32 i = DDR_MAX_TRIES;

	writel(0, DDR_REG_CORE2SEQ);
	do {
		ddr_delay(50);
		s2c = readl(DDR_REG_SEQ2CORE);
	} while ((s2c & SEQ2CORE_MASK) && (--i > 0));

	return !i;
}
static int emif_reset(void)
{
	u32 c2s, s2c;

	c2s = readl(DDR_REG_CORE2SEQ);
	s2c = readl(DDR_REG_SEQ2CORE);

	debug("c2s=%08x s2c=%08x nr0=%08x nr1=%08x nr2=%08x dst=%08x\n",
		c2s, s2c, readl(IO48_MMR_NIOS2_RESERVE0),
		readl(IO48_MMR_NIOS2_RESERVE1),
		readl(IO48_MMR_NIOS2_RESERVE2),
		readl(IO48_MMR_DRAMSTS));

	if ((s2c & SEQ2CORE_MASK) && emif_clear()) {
		printf("failed emif_clear()\n");
		return -1;
	}

	writel(CORE2SEQ_INT_REQ, DDR_REG_CORE2SEQ);

	if (ddr_wait_bit(DDR_REG_SEQ2CORE, SEQ2CORE_INT_RESP_BIT, 0, 1000000)) {
		printf("emif_reset failed to see interrupt acknowledge\n");
		return -2;
	} else {
		printf("emif_reset interrupt acknowledged\n");
	}

	if (emif_clear()) {
		printf("emif_clear() failed\n");
		return -3;
	}
	debug("emif_reset interrupt cleared\n");

	debug("nr0=%08x nr1=%08x nr2=%08x\n",
		readl(IO48_MMR_NIOS2_RESERVE0),
		readl(IO48_MMR_NIOS2_RESERVE1),
		readl(IO48_MMR_NIOS2_RESERVE2));

	return 0;
}

int ddr_setup(void)
{
	int i, j, retcode, ddr_setup_complete = 0;

	int chip_version = readl(&socfpga_system_mgr->siliconid1);

	/* Try 3 times to do a calibration */
	for (i = 0; (i < 3) && !ddr_setup_complete; i++) {
		WATCHDOG_RESET();

		/* Only engineering sample needs calibration workaround */
		if (ARRIA10_ES_SILICON_VER == chip_version) {
			retcode = ddr_calibration_es_workaround();
			if (retcode) {
				printf("DDRCAL: Failure: %d\n", retcode);
				continue;
			}
		}

		/* A delay to wait for calibration bit to set */
		for (j = 0; (j < 10) && !ddr_setup_complete; j++) {
			ddr_delay(500);
			ddr_setup_complete = is_sdram_cal_success();
		}

		if (!ddr_setup_complete &&
			(ARRIA10_ES_SILICON_VER != chip_version)) {
			emif_reset();
		}
	}

	if (!ddr_setup_complete) {
		puts("Error: Could Not Calibrate SDRAM\n");
		return -1;
	}

	return 0;
}

int is_sdram_ecc_enabled(void)
{
	return (readl(&socfpga_ecc_hmc_base->eccctrl) &
		ALT_ECC_HMC_OCP_ECCCTL_ECC_EN_SET_MSK) != 0;
}

/* Initialize SDRAM ECC bits to avoid false DBE */
void sdram_init_ecc_bits(void)
{
	struct pl330_transfer_struct pl330;
	u8 buf[DDR_ECC_DMA_SIZE];
	unsigned int start;

	pl330.dst_addr = CONFIG_SYS_SDRAM_BASE;
	pl330.size_byte = gd->ram_size;
	pl330.channel_num = 0;
	pl330.buf_size = sizeof(buf);
	pl330.buf = buf;

	printf("SDRAM: Initializing ECC 0x%08lx - 0x%08lx\n",
	       pl330.dst_addr, pl330.dst_addr + pl330.size_byte);
	reset_timer();
	start = get_timer(0);

	if (pl330_transfer_zeroes(&pl330)) {
		puts("ERROR - DMA setup failed\n");
		hang();
	}

	if (pl330_transfer_start(&pl330)) {
		puts("ERROR - DMA start failed\n");
		hang();
	}

	if (pl330_transfer_finish(&pl330)) {
		puts("ERROR - DMA finish failed\n");
		hang();
	}

	printf("SDRAM-ECC: Initialized success with %d ms\n",
	       (unsigned)get_timer(start));
}

/* Function to startup the SDRAM*/
int sdram_startup(void)
{
	/* Release NOC ddr scheduler from reset */
	reset_deassert_noc_ddr_scheduler();

	/* Bringup the DDR (calibration and configuration) */
	return ddr_setup();
}

u32 sdram_size_calc(void)
{
	union dramaddrw_reg dramaddrw =
		(union dramaddrw_reg)readl(&socfpga_io48_mmr_base->dramaddrw);

	u32 size = (1 << (dramaddrw.cfg_cs_addr_width +
		    dramaddrw.cfg_bank_group_addr_width +
		    dramaddrw.cfg_bank_addr_width +
		    dramaddrw.cfg_row_addr_width +
		    dramaddrw.cfg_col_addr_width));

	size *= (2 << (readl(&socfpga_ecc_hmc_base->ddrioctrl) &
		       ALT_ECC_HMC_OCP_DDRIOCTRL_IO_SIZE_MSK));

	return size;
}

/* Function to initialize SDRAM MMR and NOC DDR scheduler*/
void sdram_mmr_init(void)
{
	u32 update_value, io48_value;
	union ctrlcfg0_reg ctrlcfg0 =
		(union ctrlcfg0_reg)readl(&socfpga_io48_mmr_base->ctrlcfg0);
	union ctrlcfg1_reg ctrlcfg1 =
		(union ctrlcfg1_reg)readl(&socfpga_io48_mmr_base->ctrlcfg1);
	union dramaddrw_reg dramaddrw =
		(union dramaddrw_reg)readl(&socfpga_io48_mmr_base->dramaddrw);
	union caltiming0_reg caltim0 =
		(union caltiming0_reg)readl(&socfpga_io48_mmr_base->caltiming0);
	union caltiming1_reg caltim1 =
		(union caltiming1_reg)readl(&socfpga_io48_mmr_base->caltiming1);
	union caltiming2_reg caltim2 =
		(union caltiming2_reg)readl(&socfpga_io48_mmr_base->caltiming2);
	union caltiming3_reg caltim3 =
		(union caltiming3_reg)readl(&socfpga_io48_mmr_base->caltiming3);
	union caltiming4_reg caltim4 =
		(union caltiming4_reg)readl(&socfpga_io48_mmr_base->caltiming4);
	union caltiming9_reg caltim9 =
		(union caltiming9_reg)readl(&socfpga_io48_mmr_base->caltiming9);
	u32 ddrioctl;

	/*
	 * Configure the DDR IO size [0xFFCFB008]
	 * niosreserve0: Used to indicate DDR width &
	 *	bit[7:0] = Number of data bits (0x20 for 32bit)
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
	if ((socfpga_io48_mmr_base->niosreserve1 >> 6) & 0x1FF) {
		update_value = readl(&socfpga_io48_mmr_base->niosreserve0);
		writel(((update_value & 0xFF) >> 5),
		       &socfpga_ecc_hmc_base->ddrioctrl);
	}

	ddrioctl = readl(&socfpga_ecc_hmc_base->ddrioctrl);

	/* Set the DDR Configuration [0xFFD12400] */
	io48_value = ARRIA_DDR_CONFIG(ctrlcfg1.cfg_addr_order,
				      (dramaddrw.cfg_bank_addr_width +
				      dramaddrw.cfg_bank_group_addr_width),
				      dramaddrw.cfg_col_addr_width,
				      dramaddrw.cfg_row_addr_width);

	update_value = match_ddr_conf(io48_value);
	if (update_value)
		writel(update_value,
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_ddrconf);

	/*
	 * Configure DDR timing [0xFFD1240C]
	 *  RDTOMISS = tRTP + tRP + tRCD - BL/2
	 *  WRTOMISS = WL + tWR + tRP + tRCD and
	 *    WL = RL + BL/2 + 2 - rd-to-wr ; tWR = 15ns  so...
	 *  First part of equation is in memory clock units so divide by 2
	 *  for HMC clock units. 1066MHz is close to 1ns so use 15 directly.
	 *  WRTOMISS = ((RL + BL/2 + 2 + tWR) >> 1)- rd-to-wr + tRP + tRCD
	 */
	update_value = (caltim2.cfg_rd_to_pch +  caltim4.cfg_pch_to_valid +
			caltim0.cfg_act_to_rdwr -
			(ctrlcfg0.cfg_ctrl_burst_len >> 2));
	io48_value = ((((socfpga_io48_mmr_base->dramtiming0 &
		      ALT_IO48_DRAMTIME_MEM_READ_LATENCY_MASK) + 2 + 15 +
		      (ctrlcfg0.cfg_ctrl_burst_len >> 1)) >> 1) -
		      /* Up to here was in memory cycles so divide by 2 */
		      caltim1.cfg_rd_to_wr + caltim0.cfg_act_to_rdwr +
		      caltim4.cfg_pch_to_valid);

	writel(((caltim0.cfg_act_to_act <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_ACTTOACT_LSB) |
		(update_value <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_RDTOMISS_LSB) |
		(io48_value <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_WRTOMISS_LSB) |
		((ctrlcfg0.cfg_ctrl_burst_len >> 2) <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_BURSTLEN_LSB) |
		(caltim1.cfg_rd_to_wr <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_RDTOWR_LSB) |
		(caltim3.cfg_wr_to_rd <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_WRTORD_LSB) |
		(((ddrioctl == 1) ? 1 : 0) <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_BWRATIO_LSB)),
		&socfpga_noc_ddr_scheduler_base->
			ddr_t_main_scheduler_ddrtiming);

	/* Configure DDR mode [0xFFD12410] [precharge = 0] */
	writel(((ddrioctl ? 0 : 1) <<
		ALT_NOC_MPU_DDR_T_SCHED_DDRMOD_BWRATIOEXTENDED_LSB),
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_ddrmode);

	/* Configure the read latency [0xFFD12414] */
	writel(((socfpga_io48_mmr_base->dramtiming0 &
		ALT_IO48_DRAMTIME_MEM_READ_LATENCY_MASK) >> 1) +
		DDR_READ_LATENCY_DELAY,
		&socfpga_noc_ddr_scheduler_base->
			ddr_t_main_scheduler_readlatency);

	/*
	 * Configuring timing values concerning activate commands
	 * [0xFFD12438] [FAWBANK alway 1 because always 4 bank DDR]
	 */
	writel(((caltim0.cfg_act_to_act_db <<
			ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_RRD_LSB) |
		(caltim9.cfg_4_act_to_act <<
			ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_FAW_LSB) |
		(ARRIA10_SDR_ACTIVATE_FAWBANK <<
			ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_FAWBANK_LSB)),
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_activate);

	/*
	 * Configuring timing values concerning device to device data bus
	 * ownership change [0xFFD1243C]
	 */
	writel(((caltim1.cfg_rd_to_rd_dc <<
			ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSRDTORD_LSB) |
		(caltim1.cfg_rd_to_wr_dc <<
			ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSRDTOWR_LSB) |
		(caltim3.cfg_wr_to_rd_dc <<
			ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSWRTORD_LSB)),
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_devtodev);

	/* Enable or disable the SDRAM ECC */
	if (ctrlcfg1.cfg_ctrl_enable_ecc) {
		setbits_le32(&socfpga_ecc_hmc_base->eccctrl,
			     (ALT_ECC_HMC_OCP_ECCCTL_AWB_CNT_RST_SET_MSK |
			      ALT_ECC_HMC_OCP_ECCCTL_CNT_RST_SET_MSK |
			      ALT_ECC_HMC_OCP_ECCCTL_ECC_EN_SET_MSK));
		clrbits_le32(&socfpga_ecc_hmc_base->eccctrl,
			     (ALT_ECC_HMC_OCP_ECCCTL_AWB_CNT_RST_SET_MSK |
			      ALT_ECC_HMC_OCP_ECCCTL_CNT_RST_SET_MSK));
		setbits_le32(&socfpga_ecc_hmc_base->eccctrl2,
			     (ALT_ECC_HMC_OCP_ECCCTL2_RMW_EN_SET_MSK |
			      ALT_ECC_HMC_OCP_ECCCTL2_AWB_EN_SET_MSK));
	} else {
		clrbits_le32(&socfpga_ecc_hmc_base->eccctrl,
			     (ALT_ECC_HMC_OCP_ECCCTL_AWB_CNT_RST_SET_MSK |
			      ALT_ECC_HMC_OCP_ECCCTL_CNT_RST_SET_MSK |
			      ALT_ECC_HMC_OCP_ECCCTL_ECC_EN_SET_MSK));
		clrbits_le32(&socfpga_ecc_hmc_base->eccctrl2,
			     (ALT_ECC_HMC_OCP_ECCCTL2_RMW_EN_SET_MSK |
			      ALT_ECC_HMC_OCP_ECCCTL2_AWB_EN_SET_MSK));
	}
}

struct firewall_entry {
	const char *prop_name;
	const u32 cfg_addr;
	const u32 en_addr;
	const u32 en_bit;
};
#define FW_MPU_FPGA_ADDRESS \
	((const struct socfpga_noc_fw_ddr_mpu_fpga2sdram *)\
	SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS)
const struct firewall_entry firewall_table[] = {
	{
		"mpu0",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
		offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
			 mpuregion0addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
		offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
			 enable),
		ALT_NOC_FW_DDR_SCR_EN_MPUREG0EN_SET_MSK
	},
	{
		"mpu1",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
		offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
			 mpuregion1addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
		offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
			 enable),
		ALT_NOC_FW_DDR_SCR_EN_MPUREG1EN_SET_MSK
	},
	{
		"mpu2",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
		offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
			 mpuregion2addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
		offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
			 enable),
		ALT_NOC_FW_DDR_SCR_EN_MPUREG2EN_SET_MSK
	},
	{
		"mpu3",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
		offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
			 mpuregion3addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
		offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
			 enable),
		ALT_NOC_FW_DDR_SCR_EN_MPUREG3EN_SET_MSK
	},
	{
		"l3-0",
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, hpsregion0addr),
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, enable),
		ALT_NOC_FW_DDR_SCR_EN_HPSREG0EN_SET_MSK
	},
	{
		"l3-1",
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, hpsregion1addr),
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, enable),
		ALT_NOC_FW_DDR_SCR_EN_HPSREG1EN_SET_MSK
	},
	{
		"l3-2",
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, hpsregion2addr),
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, enable),
		ALT_NOC_FW_DDR_SCR_EN_HPSREG2EN_SET_MSK
	},
	{
		"l3-3",
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, hpsregion3addr),
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, enable),
		ALT_NOC_FW_DDR_SCR_EN_HPSREG3EN_SET_MSK
	},
	{
		"l3-4",
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, hpsregion4addr),
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, enable),
		ALT_NOC_FW_DDR_SCR_EN_HPSREG4EN_SET_MSK
	},
	{
		"l3-5",
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, hpsregion5addr),
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, enable),
		ALT_NOC_FW_DDR_SCR_EN_HPSREG5EN_SET_MSK
	},
	{
		"l3-6",
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, hpsregion6addr),
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, enable),
		ALT_NOC_FW_DDR_SCR_EN_HPSREG6EN_SET_MSK
	},
	{
		"l3-7",
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, hpsregion7addr),
		SOCFPGA_SDR_FIREWALL_L3_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_l3, enable),
		ALT_NOC_FW_DDR_SCR_EN_HPSREG7EN_SET_MSK
	},
	{
		"fpga2sdram0-0",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram0region0addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG0EN_SET_MSK
	},
	{
		"fpga2sdram0-1",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram0region1addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG1EN_SET_MSK
	},
	{
		"fpga2sdram0-2",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram0region2addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG2EN_SET_MSK
	},
	{
		"fpga2sdram0-3",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram0region3addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG3EN_SET_MSK
	},
	{
		"fpga2sdram1-0",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram1region0addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG0EN_SET_MSK
	},
	{
		"fpga2sdram1-1",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram1region1addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG1EN_SET_MSK
	},
	{
		"fpga2sdram1-2",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram1region2addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG2EN_SET_MSK
	},
	{
		"fpga2sdram1-3",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram1region3addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG3EN_SET_MSK
	},	{
		"fpga2sdram2-0",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram2region0addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG0EN_SET_MSK
	},
	{
		"fpga2sdram2-1",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram2region1addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG1EN_SET_MSK
	},
	{
		"fpga2sdram2-2",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram2region2addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG2EN_SET_MSK
	},
	{
		"fpga2sdram2-3",
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 fpga2sdram2region3addr),
		SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS +
			offsetof(struct socfpga_noc_fw_ddr_mpu_fpga2sdram,
				 enable),
		ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG3EN_SET_MSK
	},

};

int of_sdram_firewall_setup(const void *blob)
{
	int child, i, node;
	u32 start_end[2];

	node = fdtdec_next_compatible(blob, 0, COMPAT_ARRIA10_NOC);
	if (node < 0)
		return 2;

	child = fdt_first_subnode(blob, node);
	if (child < 0)
		return 1;

	/* set to default state */
	writel(0, &socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable);
	writel(0, &socfpga_noc_fw_ddr_l3_base->enable);


	for (i = 0; i < ARRAY_SIZE(firewall_table); i++) {
		if (!fdtdec_get_int_array(blob, child,
					  firewall_table[i].prop_name,
					  start_end, 2)) {
			writel((start_end[0] & ALT_NOC_FW_DDR_ADDR_MASK) |
				(start_end[1] << ALT_NOC_FW_DDR_END_ADDR_LSB),
				 firewall_table[i].cfg_addr);
			setbits_le32(firewall_table[i].en_addr,
				     firewall_table[i].en_bit);
		}
	}

	return 0;
}

int ddr_calibration_sequence(void)
{
	WATCHDOG_RESET();

	/* Check to see if SDRAM cal was success */
	if (sdram_startup()) {
		puts("DDRCAL: Failed\n");
		return -1;
	}

	puts("DDRCAL: Success\n");

	WATCHDOG_RESET();

	/* initialize the MMR register */
	sdram_mmr_init();

	/* assigning the SDRAM size */
	gd->ram_size = sdram_size_calc();

	/* If a weird value, use default Config size */
	if (gd->ram_size <= 0)
		gd->ram_size = PHYS_SDRAM_1_SIZE;

	/* setup the dram info within bd */
	dram_init_banksize();

	if (of_sdram_firewall_setup(gd->fdt_blob))
		puts("FW: Error Configuring Firewall\n");

	if (is_sdram_ecc_enabled())
		sdram_init_ecc_bits();

	return 0;
}

/* Initialise the DRAM by telling the DRAM Size */
int dram_init(void)
{
	bd_t *bd;
	unsigned long addr;
	int rval = 0;

	WATCHDOG_RESET();

	/* enable cache as we want to speed up CFF process */
#if !(defined(CONFIG_SYS_ICACHE_OFF) && defined(CONFIG_SYS_DCACHE_OFF))
	/* reserve TLB table */
	gd->arch.tlb_size = 4096 * 4;
	/* page table is located at last 16kB of OCRAM */
	addr = CONFIG_SYS_INIT_SP_ADDR;
	gd->arch.tlb_addr = addr;

	/*
	 * We need to setup the bd for the dram info too. We will use same
	 * memory layout in later setup
	 */
	addr -= (CONFIG_OCRAM_STACK_SIZE + CONFIG_OCRAM_MALLOC_SIZE);

	/*
	 * (permanently) allocate a Board Info struct
	 * and a permanent copy of the "global" data
	 */
	addr -= sizeof(bd_t);
	bd = (bd_t *)addr;
	gd->bd = bd;

	/* enable the cache */
	enable_caches();
#endif

	WATCHDOG_RESET();

	if (is_external_fpga_config(gd->fdt_blob)) {
		ddr_calibration_sequence();
	} else {
#if defined(CONFIG_MMC)
		rval = cff_from_sdmmc_env();
#elif defined(CONFIG_CADENCE_QSPI)
		rval = cff_from_qspi_env();
#elif defined(CONFIG_NAND_DENALI)
		nand_init();
		rval = cff_from_nand_env();
#else
#error "unsupported config"
#endif
		if (rval > 0) {
			config_pins(gd->fdt_blob, "shared");

			reset_deassert_shared_connected_peripherals();
			/*
			 * If UART shared IO pin mux is found, print out all
			 * buffer stored messages to console.
			 */
			shared_uart_buffer_to_console();
			if (!is_early_release_fpga_config(gd->fdt_blob)) {
				config_pins(gd->fdt_blob, "fpga");
				reset_deassert_fpga_connected_peripherals();
			}
			ddr_calibration_sequence();
		}
	}

	/* Skip relocation as U-Boot cannot run on SDRAM for secure boot */
	skip_relocation();
	WATCHDOG_RESET();
	return 0;
}

void dram_bank_mmu_setup(int bank)
{
	bd_t *bd = gd->bd;
	int	i;

	debug("%s: bank: %d\n", __func__, bank);
	for (i = bd->bi_dram[bank].start >> 20;
	     i < (bd->bi_dram[bank].start + bd->bi_dram[bank].size) >> 20;
	     i++) {
#if defined(CONFIG_SYS_ARM_CACHE_WRITETHROUGH)
		set_section_dcache(i, DCACHE_WRITETHROUGH);
#else
		set_section_dcache(i, DCACHE_WRITEBACK);
#endif
	}

	/* same as above but just that we would want cacheable for ocram too */
	i = CONFIG_SYS_INIT_RAM_ADDR >> 20;
#if defined(CONFIG_SYS_ARM_CACHE_WRITETHROUGH)
	set_section_dcache(i, DCACHE_WRITETHROUGH);
#else
	set_section_dcache(i, DCACHE_WRITEBACK);
#endif
}

/* Enable and disable external address parity error interrupt */
void ext_addrparity_err_enable_interrupt(unsigned enable)
{
	const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;

	if (enable) /* Enable generating interrupt when external parity error
		       is detected */
		setbits_le32(&socfpga_ecc_hmc_base->intmode,
				ALT_ECC_HMC_OCP_INTMOD_EXT_ADDRPARITY_SET_MSK);
	else /* Disable address parity on DERR interrupt */
		clrbits_le32(&socfpga_ecc_hmc_base->intmode,
				ALT_ECC_HMC_OCP_INTMOD_EXT_ADDRPARITY_SET_MSK);

	return;
}

/* Enable and disable double bit error interrupt in HMI */
void db_err_enable_interrupt(unsigned enable)
{
	const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;

	if (enable)
		writel(ALT_ECC_HMC_OCP_ERRINTEN_DERR_SET_MSK,
			&socfpga_ecc_hmc_base->errintens);
	else
		writel(ALT_ECC_HMC_OCP_ERRINTEN_DERR_SET_MSK,
			&socfpga_ecc_hmc_base->errintenr);

	return;
}

/* Enable and disable single bit error interrupt */
void sb_err_enable_interrupt(unsigned enable)
{
	const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;

	if (enable) {
		writel(ALT_ECC_HMC_OCP_ERRINTEN_SERR_SET_MSK,
			&socfpga_ecc_hmc_base->errintens);
		/* Enable generating interrupt on every SERR */
		setbits_le32(&socfpga_ecc_hmc_base->intmode,
				ALT_ECC_HMC_OCP_INTMOD_SERR_SET_MSK);
	} else {
		writel(ALT_ECC_HMC_OCP_ERRINTEN_SERR_SET_MSK,
			&socfpga_ecc_hmc_base->errintenr);
		/* Disable generating interrupt on every SERR */
		clrbits_le32(&socfpga_ecc_hmc_base->intmode,
				ALT_ECC_HMC_OCP_INTMOD_SERR_SET_MSK);
	}

	return;
}

/* Handler for DDR sb interrupt */
void irq_handler_DDR_ecc_serr(void)
{
	unsigned int reg_value = 0;
	const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;

	sb_err_enable_interrupt(0);

	/* Check whether SBE happen */
	reg_value = readl(&socfpga_ecc_hmc_base->intstat);

	if (reg_value & ALT_ECC_HMC_OCP_INTSTAT_SERR_MSK) {
		/* Clear the interrupt signal from DRAM controller */
		writel(ALT_ECC_HMC_OCP_INTSTAT_SERR_MSK,
			&socfpga_ecc_hmc_base->intstat);
		irq_cnt_ecc_sdram++;
#ifndef CONFIG_ENV_IS_NOWHERE
		setenv_ulong("sdram_ecc_sbe", irq_cnt_ecc_sdram);
#endif
		/* word address is 16 bytes, shift left by 4 to byte address
		   translation */
		printf("Info: DRAM ECC SBE @ 0x%08x\n",
		readl(&socfpga_ecc_hmc_base->serraddra) << 4);
		printf("Info: DRAM ECC AUTO CORRECTION SBE ADDRESS @ 0x%08x\n",
		readl(&socfpga_ecc_hmc_base->autowb_corraddr) << 4);
		printf("decoder0: erraddr = %08x\n",
		readl(&socfpga_ecc_hmc_base->ecc_errgenaddr_0) << 4);
		printf("decoder1: erraddr = %08x\n",
		readl(&socfpga_ecc_hmc_base->ecc_errgenaddr_1) << 4);
		printf("decoder2: erraddr = %08x\n",
		readl(&socfpga_ecc_hmc_base->ecc_errgenaddr_2) << 4);
		printf("decoder3: erraddr = %08x\n",
		readl(&socfpga_ecc_hmc_base->ecc_errgenaddr_3) << 4);
	}

	sb_err_enable_interrupt(1);

	return;
}

/* Handler for db interrupt */
void irq_handler_DDR_ecc_derr(void)
{
	unsigned int reg_value = 0;
	const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;

	db_err_enable_interrupt(0);

	/* Check whether DDR DBE happen */
	reg_value = readl(&socfpga_ecc_hmc_base->intstat);

	if (reg_value & ALT_ECC_HMC_OCP_INTSTAT_DERR_MSK) {
		/* Address mismatch */
		if (reg_value & ALT_ECC_HMC_OCP_INTSTAT_ADDRMTCFLG_MSK) {
			/* clear the address mismatch error */
			writel(ALT_ECC_HMC_OCP_INTSTAT_ADDRMTCFLG_MSK,
				&socfpga_ecc_hmc_base->intstat);
			/* word address is 16 bytes, shift left by 4 to byte
			    address translation */
			puts("Error: DRAM address mismatch error occurred\n");
			printf("decoder0: erraddr = %08x\n",
			readl(&socfpga_ecc_hmc_base->ecc_errgenaddr_0) << 4);
			printf("decoder1: erraddr = %08x\n",
			readl(&socfpga_ecc_hmc_base->ecc_errgenaddr_1) << 4);
			printf("decoder2: erraddr = %08x\n",
			readl(&socfpga_ecc_hmc_base->ecc_errgenaddr_2) << 4);
			printf("decoder3: erraddr = %08x\n",
			readl(&socfpga_ecc_hmc_base->ecc_errgenaddr_3) << 4);
		} /* External address parity error */
		else if (reg_value & ALT_ECC_HMC_OCP_INTSTAT_ADDRPARFLG_MSK) {
			/* clear the address parity error */
			writel(ALT_ECC_HMC_OCP_INTSTAT_ADDRPARFLG_MSK,
				&socfpga_ecc_hmc_base->intstat);
			puts("Error: DRAM external parity error occurred\n");
		} /* Double bit bus error */
		else if (reg_value & ALT_ECC_HMC_OCP_INTSTAT_DERRBUSFLG_MSK) {
			/* clear the double bit bus error */
			writel(ALT_ECC_HMC_OCP_INTSTAT_DERRBUSFLG_MSK,
				&socfpga_ecc_hmc_base->intstat);
			puts("Error: DRAM ECC DBE occurred\n");
			printf("erraddr = %08x\n",
			readl(&socfpga_ecc_hmc_base->derraddra) << 4);
		}

		/* Clear the double bit error interrupt signal */
		writel(ALT_ECC_HMC_OCP_INTSTAT_DERR_MSK,
			&socfpga_ecc_hmc_base->intstat);

		puts("### ERROR ### Please RESET the board ###\n");
	}

	db_err_enable_interrupt(1);

	return;
}
