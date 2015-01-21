/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/sdram.h>
#include <altera.h>
#include <errno.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_fpga_manager *fpga_manager_base =
		(void *)SOCFPGA_FPGAMGRREGS_ADDRESS;

static const struct socfpga_system_manager *system_manager_base =
		(void *)SOCFPGA_SYSMGR_ADDRESS;

/* Check whether FPGA Init_Done signal is high */
int is_fpgamgr_initdone_high(void)
{
	unsigned long val;

#ifdef TEST_AT_ASIMOV
	val = readl(SOCFPGA_FPGAMGRREGS_ADDRESS +
		FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS);
	if (val & FPGAMGRREGS_MON_GPIO_EXT_PORTA_ID_MASK)
#else
	val = readl(&fpga_manager_base->imgcfg_stat);
	if (val & ALT_FPGAMGR_IMGCFG_STAT_F2S_INITDONE_OE_SET_MSK)
#endif
		return 1;
	else
		return 0;
}

#ifdef TEST_AT_ASIMOV
/* Get the FPGA mode */
static int fpgamgr_get_mode(void)
{
	unsigned long val;
	val = readl(&fpga_manager_base->stat);
	val = val & FPGAMGRREGS_STAT_MODE_MASK;
	return val;
}
#endif

static uint32_t fpgamgr_get_msel(void)
{
	uint32_t reg;
#ifdef TEST_AT_ASIMOV
	reg = readl(&fpga_manager_base->stat);
	reg = ((reg & FPGAMGRREGS_STAT_MSEL_MASK) >> FPGAMGRREGS_STAT_MSEL_LSB);
#else
	reg = readl(&fpga_manager_base->imgcfg_stat);
	reg = ((reg & ALT_FPGAMGR_IMGCFG_STAT_F2S_MSEL_SET_MSD) >>
		ALT_FPGAMGR_IMGCFG_STAT_F2S_MSEL0_LSB);
#endif
	return reg;
}

static void fpgamgr_set_cfgwdth(int width)
{
#ifdef TEST_AT_ASIMOV
	if (width)
		setbits_le32(&fpga_manager_base->ctrl,
			FPGAMGRREGS_CTRL_CFGWDTH_MASK);
	else
		clrbits_le32(&fpga_manager_base->ctrl,
			FPGAMGRREGS_CTRL_CFGWDTH_MASK);
#else
	if (width)
		setbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
			ALT_FPGAMGR_IMGCFG_CTL_02_CFGWIDTH_SET_MSK);
	else
		clrbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
			ALT_FPGAMGR_IMGCFG_CTL_02_CFGWIDTH_SET_MSK);
#endif
}

int is_fpgamgr_user_mode(void)
{
	int rval = 0;
#ifdef TEST_AT_ASIMOV
	if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_USERMODE)
		rval = 1;
#else
	if (readl(&fpga_manager_base->imgcfg_stat) &
		ALT_FPGAMGR_IMGCFG_STAT_F2S_USERMODE_SET_MSK)
		rval = 1;
#endif
	return rval;
}

#ifndef TEST_AT_ASIMOV
static int wait_for_user_mode(void)
{
	unsigned long i;
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (is_fpgamgr_user_mode())
			break;
	}
	return i;
}

/*
 * Poll imgcfg_stat register.  Waiting for all bits set in mask to be set.
 *
 * returns FPGA_TIMEOUT_CNT if timeout
 */
static int wait_for_imgcfg_stat(unsigned long mask)
{
	unsigned long reg, i;

	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		reg = readl(&fpga_manager_base->imgcfg_stat);
		if ((reg & mask) == mask)
			break;
	}

	return i;
}

static int wait_for_condone_pin(void)
{
	return wait_for_imgcfg_stat(
		ALT_FPGAMGR_IMGCFG_STAT_F2S_CONDONE_PIN_SET_MSK);
}

static int wait_for_nconfig_pin_and_nstatus_pin(void)
{
	return wait_for_imgcfg_stat(
		ALT_FPGAMGR_IMGCFG_STAT_F2S_NCONFIG_PIN_SET_MSK |
		ALT_FPGAMGR_IMGCFG_STAT_F2S_NSTATUS_PIN_SET_MSK);
}
#endif /* ! TEST_AT_ASIMOV */

/* Check whether FPGA is ready to be accessed */
int is_fpgamgr_fpga_ready(void)
{
	/* check for init done signal */
	if (is_fpgamgr_initdone_high() == 0)
		return 0;
	/* check again to avoid false glitches */
	if (is_fpgamgr_initdone_high() == 0)
		return 0;
	if (!is_fpgamgr_user_mode())
		return 0;
	return 1;
}

/* Poll until FPGA is ready to be accessed or timeout occurred */
int poll_fpgamgr_fpga_ready(void)
{
	unsigned long i;
	/* If FPGA is blank, wait till WD invoke warm reset */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		/* check for init done signal */
		if (is_fpgamgr_initdone_high() == 0)
			continue;
		/* check again to avoid false glitches */
		if (is_fpgamgr_initdone_high() == 0)
			continue;
		return 1;
	}
	return 0;
}

/* set CD ratio */
static void fpgamgr_set_cd_ratio(unsigned long ratio)
{
#ifdef TEST_AT_ASIMOV
	unsigned long reg;
	reg = readl(&fpga_manager_base->ctrl);
	reg = (reg & ~(0x3 << FPGAMGRREGS_CTRL_CDRATIO_LSB)) |
		((ratio & 0x3) << FPGAMGRREGS_CTRL_CDRATIO_LSB);
	writel(reg, &fpga_manager_base->ctrl);
#else
	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
		ALT_FPGAMGR_IMGCFG_CTL_02_CDRATIO_SET_MSK);
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
		(ratio << ALT_FPGAMGR_IMGCFG_CTL_02_CDRATIO_LSB) &
		ALT_FPGAMGR_IMGCFG_CTL_02_CDRATIO_SET_MSK);
#endif
}

static int fpgamgr_dclkcnt_set(unsigned long cnt)
{
	unsigned long i;

	/* clear any existing done status */
	if (readl(&fpga_manager_base->dclkstat))
		writel(0x1, &fpga_manager_base->dclkstat);
	/* write the dclkcnt */
	writel(cnt, &fpga_manager_base->dclkcnt);
	/*
	 * wait till the dclkcnt done
	 */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (readl(&fpga_manager_base->dclkstat)) {
			writel(0x1, &fpga_manager_base->dclkstat);
			return 0;
		}
	}

	return -1;
}

/* Start the FPGA programming by initialize the FPGA Manager */
int fpgamgr_program_init(void)
{
#ifdef TEST_AT_ASIMOV
	unsigned long i, reg;

	/* get the MSEL value */
	reg = fpgamgr_get_msel();

	/*
	 * Set the cfg width
	 * If MSEL[3] = 1, cfg width = 32 bit
	 */
	fpgamgr_set_cfgwdth(reg & 0x8);

	/* To determine the CD ratio */
	/* MSEL[3] = 1 & MSEL[1:0] = 0, CD Ratio = 1 */
	if ((reg & 0xb) == 0x8)
		fpgamgr_set_cd_ratio(CDRATIO_x1);
	/* MSEL[3] = 1 & MSEL[1:0] = 1, CD Ratio = 4 */
	else if ((reg & 0xb) == 0x9)
		fpgamgr_set_cd_ratio(CDRATIO_x4);
	/* MSEL[3] = 1 & MSEL[1:0] = 2, CD Ratio = 8 */
	else if ((reg & 0xb) == 0xa)
		fpgamgr_set_cd_ratio(CDRATIO_x8);
	/* MSEL[3] = 0 & MSEL[1:0] = 0, CD Ratio = 1 */
	else if ((reg & 0xb) == 0x0)
		fpgamgr_set_cd_ratio(CDRATIO_x1);
	/* MSEL[3] = 0 & MSEL[1:0] = 1, CD Ratio = 2 */
	else if ((reg & 0xb) == 0x1)
		fpgamgr_set_cd_ratio(CDRATIO_x2);
	/* MSEL[3] = 0 & MSEL[1:0] = 2, CD Ratio = 4 */
	else if ((reg & 0xb) == 0x2)
		fpgamgr_set_cd_ratio(CDRATIO_x4);
#else
	unsigned int msel, cfg_width, cd_ratio;
	bool encrypt, compress;

	/* get the MSEL value, verify we are set for FPP configuration mode */
	msel = fpgamgr_get_msel();
	if ((msel != 0) && (msel != 1))
		return -1;

//todo hardcoded for now
	encrypt = 0;
	compress = 0;

	/*
	 * from the register map description of cdratio in imgcfg_ctrl_02:
	 *  Normal Configuration    : 32bit Passive Parallel
	 *  Partial Reconfiguration : 16bit Passive Parallel
	 */
//todo hardcoded for now
	cfg_width = CFGWDTH_32;

	/*
	 * cd ratio is dependent on cfg width and whether the bitstream
	 * is encrypted and/or compressed.
	 *
	 * | width | encr. | compr. | cd ratio |
	 * |  16   |   0   |   0    |     1    |
	 * |  16   |   0   |   1    |     4    |
	 * |  16   |   1   |   0    |     2    |
	 * |  16   |   1   |   1    |     4    |
	 * |  32   |   0   |   0    |     1    |
	 * |  32   |   0   |   1    |     8    |
	 * |  32   |   1   |   0    |     4    |
	 * |  32   |   1   |   1    |     8    |
	 */
	if (!compress && !encrypt) {
		cd_ratio = CDRATIO_x1;
	} else {
		if (compress)
			cd_ratio = CDRATIO_x4;
		else
			cd_ratio = CDRATIO_x2;

		/* if 32 bit, double the cd ratio (so register
		   field setting is incremented) */
		if (cfg_width == CFGWDTH_32)
			cd_ratio += 1;
	}		

	fpgamgr_set_cfgwdth(cfg_width);
	fpgamgr_set_cd_ratio(cd_ratio);
#endif

#ifdef TEST_AT_ASIMOV
	/* to enable FPGA Manager configuration */
	clrbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_NCE_MASK);

	/* to enable FPGA Manager drive over configuration line */
	setbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_EN_MASK);

	/* put FPGA into reset phase */
	setbits_le32(&fpga_manager_base->ctrl,
		FPGAMGRREGS_CTRL_NCONFIGPULL_MASK);

	/* (1) wait until FPGA enter reset phase */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_RESETPHASE)
			break;
	}
	/* if not in reset state, return error */
	if (fpgamgr_get_mode() != FPGAMGRREGS_MODE_RESETPHASE)
		return -1;

	/* release FPGA from reset phase */
	clrbits_le32(&fpga_manager_base->ctrl,
		FPGAMGRREGS_CTRL_NCONFIGPULL_MASK);

	/* (2) wait until FPGA enter configuration phase */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_CFGPHASE)
			break;
	}
	/* if not in configuration state, return error */
	if (fpgamgr_get_mode() != FPGAMGRREGS_MODE_CFGPHASE)
		return -2;

	/* clear all interrupt in CB Monitor */
	writel(0xFFF, (SOCFPGA_FPGAMGRREGS_ADDRESS +
		FPGAMGRREGS_MON_GPIO_PORTA_EOI_ADDRESS));

	/* enable AXI configuration */
	setbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_AXICFGEN_MASK);
#else
	/*
	 * Read f2s_nconfig_pin and f2s_nstatus_pin, loop until
	 * de-asserted at value 1.
	 */
	if (wait_for_nconfig_pin_and_nstatus_pin() == FPGA_TIMEOUT_CNT)
		return -2;

	/* Write S2F_NCE=1; S2F_PR_REQUEST=0; EN_CFG_DATA=0; (deasserted values). */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_01,
		ALT_FPGAMGR_IMGCFG_CTL_01_S2F_NCE_SET_MSK);

	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_01,
		ALT_FPGAMGR_IMGCFG_CTL_01_S2F_PR_REQUEST_SET_MSK);

//todo register map says "en_cfg_data is an unused software bit"
//so we are departing from the recommendationded steps in the spec here
	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
		ALT_FPGAMGR_IMGCFG_CTL_02_EN_CFG_CTRL_SET_MSK);

	/* Write S2F_NCONFIG=1, S2F_NSTATUS_OE=0, S2F_CONDONE_OE=0 (deasserted values). */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NCONFIG_SET_MSK);

	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NSTATUS_OE_SET_MSK |
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_CONDONE_OE_SET_MSK);

	/*
	 * Enable override for the above signals.
	 * S2F_NENABLE_CONFIG=0
	 * S2F_NENABLE_NCONFIG=0
	 * S2F_NENABLE_NSTATUS=0 (optional).
	 * S2F_NENABLE_CONDONE=0 (optional).
	 */
	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NENABLE_NCONFIG_SET_MSK |
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NENABLE_NSTATUS_SET_MSK |
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NENABLE_CONDONE_SET_MSK);

	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_01,
		ALT_FPGAMGR_IMGCFG_CTL_01_S2F_NENABLE_CONFIG_SET_MSK);

	/* Optionally wait for enough period of time for the CSS settle. */

//todo register map says "en_cfg_data is an unused software bit"
//so we are departing from the recommendationded steps in the spec here
	/* Write EN_CFG_DATA=1 */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
		ALT_FPGAMGR_IMGCFG_CTL_02_EN_CFG_CTRL_SET_MSK);
												
	if (fpgamgr_dclkcnt_set(0x7ff) != 0)
		return -3;

	/* Write S2F_NCONFIG=0 */
	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NCONFIG_SET_MSK);

	if (fpgamgr_dclkcnt_set(10) != 0)
		return -4;

	/* Write S2F_NCONFIG=1 */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NCONFIG_SET_MSK);

	if (fpgamgr_dclkcnt_set(10) != 0)
		return -5;
#endif

	return 0;
}

/* Write the RBF data to FPGA Manager */
void fpgamgr_program_write(const unsigned long *rbf_data,
	unsigned long rbf_size)
{
#ifdef TEST_AT_ASIMOV
	fpgamgr_axi_write(rbf_data, SOCFPGA_FPGAMGRDATA_ADDRESS, rbf_size);
#else
	/* Write sof/pof data to img_data_w */
	fpgamgr_axi_write(rbf_data, SOCFPGA_FPGAMGRDATA_ADDRESS, rbf_size);
#endif
}

/* Ensure the FPGA entering config done */
int fpgamgr_program_poll_cd(void)
{
#ifdef TEST_AT_ASIMOV
	unsigned long reg, i;

	/* (3) wait until full config done */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		reg = readl(SOCFPGA_FPGAMGRREGS_ADDRESS +
			FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS);
		/* config error */
		if (!(reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_NS_MASK) &&
			!(reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_CD_MASK))
			return -3;
		/* config done without error */
		if ((reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_NS_MASK) &&
			(reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_CD_MASK))
			break;
	}
	/* tiemout happen, return error */
	if (i == FPGA_TIMEOUT_CNT)
		return -4;

	/* disable AXI configuration */
	clrbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_AXICFGEN_MASK);
#else
	if (wait_for_condone_pin() == FPGA_TIMEOUT_CNT)
		return -3;
#endif
	return 0;
}

/* Ensure the FPGA entering init phase */
int fpgamgr_program_poll_initphase(void)
{
#ifdef TEST_AT_ASIMOV
	unsigned long i;

	/* additional clocks for the CB to enter initialization phase */
	if (fpgamgr_dclkcnt_set(0x4) != 0)
		return -5;

	/* (4) wait until FPGA enter init phase or user mode */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_INITPHASE)
			break;
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_USERMODE)
			break;
	}
	/* if not in configuration state, return error */
	if (i == FPGA_TIMEOUT_CNT)
		return -6;
#endif
	return 0;
}

/* Ensure the FPGA entering user mode */
int fpgamgr_program_poll_usermode(void)
{
#ifdef TEST_AT_ASIMOV
	unsigned long i;

	/* additional clocks for the CB to exit initialization phase */
	if (fpgamgr_dclkcnt_set(0x5000) != 0)
		return -7;

	/* (5) wait until FPGA enter user mode */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_USERMODE)
			break;
	}
	/* if not in configuration state, return error */
	if (i == FPGA_TIMEOUT_CNT)
		return -8;

	/* to release FPGA Manager drive over configuration line */
	clrbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_EN_MASK);
#else
	if (fpgamgr_dclkcnt_set(0xf) != 0)
		return -7;

	if (wait_for_user_mode() == FPGA_TIMEOUT_CNT)
		return -8;
#endif
	return 0;
}

/*
 * Using FPGA Manager to program the FPGA
 * Return 0 for sucess
 */
int fpgamgr_program_fpga(const unsigned long *rbf_data,	unsigned long rbf_size)
{
	unsigned long status;

	/* prior programming the FPGA, all bridges need to be shut off */

#ifndef TEST_AT_ASIMOV
	/* disable all signals from hps peripheral controller to fpga */
	writel(0, &system_manager_base->fpgaintf_en_global);

	/* disable all axi bridge (hps2fpga, lwhps2fpga & fpga2hps) */
	reset_assert_all_bridges();
#endif

	/* initialize the FPGA Manager */
	status = fpgamgr_program_init();
	if (status)
		return status;

	/* Write the RBF data to FPGA Manager */
	fpgamgr_program_write(rbf_data, rbf_size);

	/* Ensure the FPGA entering config done */
	status = fpgamgr_program_poll_cd();
	if (status)
		return status;

	/* Ensure the FPGA entering init phase */
	status = fpgamgr_program_poll_initphase();
	if (status)
		return status;

	/* Ensure the FPGA entering user mode */
	return fpgamgr_program_poll_usermode();
}
/*
 * FPGA Manager to program the FPGA. This is the interface used by FPGA driver.
 * Return 0 for sucess, non-zero for error.
 */
int socfpga_load(Altera_desc *desc, const void *rbf_data, size_t rbf_size)
{
	unsigned long status;

#ifdef TEST_AT_ASIMOV
	if ((uint32_t)rbf_data & 0x3) {
		puts("FPGA: Unaligned data, realign to 32bit boundary.\n");
		return -EINVAL;
	}

	/* Prior programming the FPGA, all bridges need to be shut off */

	/* Disable all signals from hps peripheral controller to fpga */
	writel(0, &system_manager_base->fpgaintfgrp_module);

	/* Disable all signals from FPGA to HPS SDRAM */
#define SDR_CTRLGRP_FPGAPORTRST_ADDRESS	0x5080
	writel(0, SOCFPGA_SDR_ADDRESS + SDR_CTRLGRP_FPGAPORTRST_ADDRESS);

	/* Disable all axi bridge (hps2fpga, lwhps2fpga & fpga2hps) */
	socfpga_bridges_reset(1);

	/* Unmap the bridges from NIC-301 */
	writel(0x1, SOCFPGA_L3REGS_ADDRESS);
#else
	/* disable all signals from hps peripheral controller to fpga */
	writel(0, &system_manager_base->fpgaintf_en_global);

	/* disable all axi bridge (hps2fpga, lwhps2fpga & fpga2hps) */
	reset_assert_all_bridges();
#endif
	/* Initialize the FPGA Manager */
	status = fpgamgr_program_init();
	if (status)
		return status;

	/* Write the RBF data to FPGA Manager */
	fpgamgr_program_write(rbf_data, rbf_size);

	/* Ensure the FPGA entering config done */
	status = fpgamgr_program_poll_cd();
	if (status)
		return status;

	/* Ensure the FPGA entering init phase */
	status = fpgamgr_program_poll_initphase();
	if (status)
		return status;

	/* Ensure the FPGA entering user mode */
	return fpgamgr_program_poll_usermode();
}
