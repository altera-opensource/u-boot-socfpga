/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/sdram.h>
#include <altera.h>
#include <errno.h>
#include <watchdog.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_fpga_manager *fpga_manager_base =
		(void *)SOCFPGA_FPGAMGRREGS_ADDRESS;

#if defined(CONFIG_CMD_FPGA_LOAD)
static const struct socfpga_system_manager *system_manager_base =
		(void *)SOCFPGA_SYSMGR_ADDRESS;
#endif

static void fpgamgr_set_cd_ratio(unsigned long ratio);

static uint32_t fpgamgr_get_msel(void)
{
	uint32_t reg;

	reg = readl(&fpga_manager_base->imgcfg_stat);
	reg = ((reg & ALT_FPGAMGR_IMGCFG_STAT_F2S_MSEL_SET_MSD) >>
		ALT_FPGAMGR_IMGCFG_STAT_F2S_MSEL0_LSB);

	return reg;
}

static void fpgamgr_set_cfgwdth(int width)
{
	if (width)
		setbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
			ALT_FPGAMGR_IMGCFG_CTL_02_CFGWIDTH_SET_MSK);
	else
		clrbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
			ALT_FPGAMGR_IMGCFG_CTL_02_CFGWIDTH_SET_MSK);
}

/* Check whether FPGA Init_Done signal is high */
int is_fpgamgr_initdone_high(void)
{
	return (readl(&fpga_manager_base->imgcfg_stat) &
		ALT_FPGAMGR_IMGCFG_STAT_F2S_INITDONE_OE_SET_MSK) != 0;
}

int is_fpgamgr_user_mode(void)
{
	return (readl(&fpga_manager_base->imgcfg_stat) &
		ALT_FPGAMGR_IMGCFG_STAT_F2S_USERMODE_SET_MSK) != 0;
}

static int wait_for_user_mode(void)
{
	unsigned start = get_timer(0);

	while (!(is_fpgamgr_user_mode())) {
		if (get_timer(start) > FPGA_TIMEOUT_MSEC)
			return -ETIMEDOUT;
	}

	return 0;
}

int is_fpgamgr_early_user_mode(void)
{
	return (readl(&fpga_manager_base->imgcfg_stat) &
		ALT_FPGAMGR_IMGCFG_STAT_F2S_EARLY_USERMODE_SET_MSK) != 0;
}

int fpgamgr_wait_early_user_mode(void)
{
	u32 sync_data = 0xffffffff;
	u32 i = 0;
	unsigned start = get_timer(0);
	unsigned long cd_ratio;

	/* Getting existing CDRATIO */
	cd_ratio = (readl(&fpga_manager_base->imgcfg_ctrl_02) &
		ALT_FPGAMGR_IMGCFG_CTL_02_CDRATIO_SET_MSK) >>
		ALT_FPGAMGR_IMGCFG_CTL_02_CDRATIO_LSB;

	/* Using CDRATIO_X1 for better compatibility */
	fpgamgr_set_cd_ratio(CDRATIO_x1);

	while (!(is_fpgamgr_early_user_mode())) {
		if (get_timer(start) > FPGA_TIMEOUT_MSEC)
			return -ETIMEDOUT;
		fpgamgr_program_write((const long unsigned int *)&sync_data,
				sizeof(sync_data));
		udelay(1000);
		i++;
	}

	debug("Additional %i sync word needed\n", i);

	/* restoring original CDRATIO */
	fpgamgr_set_cd_ratio(cd_ratio);

	return 0;
}

/* send sync words to clock data through the control block */
void fpgamgr_program_sync(void)
{
	u32 sync_data = 0xffffffff;
	int i;
	for (i = 0; i < 10; i++) {
		fpgamgr_program_write((const long unsigned int *)&sync_data,
				sizeof(sync_data));
	}
}

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

/* Read f2s_nconfig_pin and f2s_nstatus_pin; loop until de-asserted */
static int wait_for_nconfig_pin_and_nstatus_pin(void)
{
	return wait_for_imgcfg_stat(
		ALT_FPGAMGR_IMGCFG_STAT_F2S_NCONFIG_PIN_SET_MSK |
		ALT_FPGAMGR_IMGCFG_STAT_F2S_NSTATUS_PIN_SET_MSK);
}

static int wait_for_f2s_nstatus_pin(unsigned long value)
{
	unsigned long reg, i, desired;
	unsigned long mask = ALT_FPGAMGR_IMGCFG_STAT_F2S_NSTATUS_PIN_SET_MSK;

	if (value == 0)
		desired = 0;
	else
		desired = mask;

	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		reg = readl(&fpga_manager_base->imgcfg_stat);
		if ((reg & mask) == desired)
			break;
	}

	return i;
}

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
	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
		ALT_FPGAMGR_IMGCFG_CTL_02_CDRATIO_SET_MSK);

	setbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
		(ratio << ALT_FPGAMGR_IMGCFG_CTL_02_CDRATIO_LSB) &
		ALT_FPGAMGR_IMGCFG_CTL_02_CDRATIO_SET_MSK);
}

static int fpgamgr_dclkcnt_set(unsigned long cnt)
{
	unsigned long i;

	/* clear any existing done status */
	if (readl(&fpga_manager_base->dclkstat))
		writel(0x1, &fpga_manager_base->dclkstat);

	/* write the dclkcnt */
	writel(cnt, &fpga_manager_base->dclkcnt);

	/* wait till the dclkcnt done */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (readl(&fpga_manager_base->dclkstat)) {
			writel(0x1, &fpga_manager_base->dclkstat);
			return 0;
		}
	}

	return FPGA_TIMEOUT_CNT;
}

/* get the MSEL value, verify we are set for FPP configuration mode */
static int fpgamgr_verify_msel(void)
{
	unsigned int msel = fpgamgr_get_msel();

	if ((msel != 0) && (msel != 1)) {
		printf("Fail: read msel=%d\n", msel);
		return -1;
	}

	return 0;
}

/*
 * Write cdratio and cdwidth based on whether the bitstream is compressed
 * and/or encoded
 */
static int fpgamgr_set_cdratio_cdwidth(unsigned int cfg_width, u32 *rbf_data,
				       u32 rbf_size)
{
	unsigned int cd_ratio;
	bool encrypt, compress;

	if (rbf_size < 230)
		return -1;

	encrypt = (rbf_data[69] >> 2) & 3;
	encrypt = encrypt != 0;

	compress = (rbf_data[229] >> 1) & 1;
	compress = !compress;

#if 0
	printf("header word %d = %08x\n", 69, rbf_data[69]);
	printf("header word %d = %08x\n", 229, rbf_data[229]);
	printf("read from rbf header: encrypt=%d compress=%d\n", encrypt, compress);
#endif

	/*
	 * from the register map description of cdratio in imgcfg_ctrl_02:
	 *  Normal Configuration    : 32bit Passive Parallel
	 *  Partial Reconfiguration : 16bit Passive Parallel
	 */

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

	return 0;
}

int fpgamgr_reset(void)
{
	unsigned long reg;

	/* S2F_NCONFIG = 0 */
	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NCONFIG_SET_MSK);

	/* Wait for f2s_nstatus == 0 */
	if (wait_for_f2s_nstatus_pin(0) == FPGA_TIMEOUT_CNT)
		return -5;

	/* S2F_NCONFIG = 1 */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NCONFIG_SET_MSK);

	/* Wait for f2s_nstatus == 1 */
	if (wait_for_f2s_nstatus_pin(1) == FPGA_TIMEOUT_CNT)
		return -6;

	/* read and confirm f2s_condone_pin = 0 and f2s_condone_oe = 1 */
	reg = readl(&fpga_manager_base->imgcfg_stat);
	if ((reg & ALT_FPGAMGR_IMGCFG_STAT_F2S_CONDONE_PIN_SET_MSK) != 0)
		return -7;

	if ((reg & ALT_FPGAMGR_IMGCFG_STAT_F2S_CONDONE_OE_SET_MSK) == 0)
		return -8;

	return 0;
}

/* Start the FPGA programming by initialize the FPGA Manager */
int fpgamgr_program_init(u32 * rbf_data, u32 rbf_size)
{
	int ret;

	/* Step 1 */
	if (fpgamgr_verify_msel())
		return -1;

	/* Step 2 */
	if (fpgamgr_set_cdratio_cdwidth(CFGWDTH_32, rbf_data, rbf_size))
		return -2;

	/*
	 * Step 3:
	 * Make sure no other external devices are trying to interfere with
	 * programming:
	 */
	if (wait_for_nconfig_pin_and_nstatus_pin() == FPGA_TIMEOUT_CNT)
		return -3;

	/*
	 * Step 4:
	 * Deassert the signal drives from HPS
	 *
	 * S2F_NCE = 1
	 * S2F_PR_REQUEST = 0
	 * EN_CFG_CTRL = 0
	 * EN_CFG_DATA = 0
	 * S2F_NCONFIG = 1
	 * S2F_NSTATUS_OE = 0
	 * S2F_CONDONE_OE = 0
	 */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_01,
		ALT_FPGAMGR_IMGCFG_CTL_01_S2F_NCE_SET_MSK);

	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_01,
		ALT_FPGAMGR_IMGCFG_CTL_01_S2F_PR_REQUEST_SET_MSK);

	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
		ALT_FPGAMGR_IMGCFG_CTL_02_EN_CFG_DATA_SET_MSK |
		ALT_FPGAMGR_IMGCFG_CTL_02_EN_CFG_CTRL_SET_MSK);

	setbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NCONFIG_SET_MSK);

	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NSTATUS_OE_SET_MSK |
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_CONDONE_OE_SET_MSK);

	/*
	 * Step 5:
	 * Enable overrides
	 * S2F_NENABLE_CONFIG = 0
	 * S2F_NENABLE_NCONFIG = 0
	 */
	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_01,
		ALT_FPGAMGR_IMGCFG_CTL_01_S2F_NENABLE_CONFIG_SET_MSK);
	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NENABLE_NCONFIG_SET_MSK);

	/*
	 * Disable driving signals that HPS doesn't need to drive.
	 * S2F_NENABLE_NSTATUS = 1
	 * S2F_NENABLE_CONDONE = 1
	 */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NENABLE_NSTATUS_SET_MSK |
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NENABLE_CONDONE_SET_MSK);

	/*
	 * Step 6:
	 * Drive chip select S2F_NCE = 0
	 */
	 clrbits_le32(&fpga_manager_base->imgcfg_ctrl_01,
		ALT_FPGAMGR_IMGCFG_CTL_01_S2F_NCE_SET_MSK);

	/* Step 7 */
	if (wait_for_nconfig_pin_and_nstatus_pin() == FPGA_TIMEOUT_CNT)
		return -4;

	/* Step 8 */
	ret = fpgamgr_reset();
	if (ret)
		return ret;

	/*
	 * Step 9:
	 * EN_CFG_CTRL and EN_CFG_DATA = 1
	 */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
		ALT_FPGAMGR_IMGCFG_CTL_02_EN_CFG_DATA_SET_MSK |
		ALT_FPGAMGR_IMGCFG_CTL_02_EN_CFG_CTRL_SET_MSK);

	return 0;
}

int fpgamgr_program_fini(void)
{
	/* Ensure the FPGA entering config done */
	int status = fpgamgr_program_poll_cd();
	if (status) {
		printf("FPGA: Poll CD failed with error code %d\n", status);
		return -3;
	}
	WATCHDOG_RESET();

	/* Ensure the FPGA entering init phase */
	status = fpgamgr_program_poll_initphase();
	if (status) {
		printf("FPGA: Poll initphase failed with error code %d\n",
			status);
		return -4;
	}
	WATCHDOG_RESET();

	/* Ensure the FPGA entering user mode */
	status = fpgamgr_program_poll_usermode();
	if (status) {
		printf("FPGA: Poll usermode failed with error code %d\n",
			status);
		return -5;
	}

	printf("Full Configuration Succeeded.\n");

	return 0;
}

/* Write the RBF data to FPGA Manager */
void fpgamgr_program_write(const unsigned long *rbf_data,
	unsigned long rbf_size)
{
	/* Write sof/pof data to img_data_w */
	fpgamgr_axi_write(rbf_data, SOCFPGA_FPGAMGRDATA_ADDRESS, rbf_size);
}

/* Ensure the FPGA entering config done */
int fpgamgr_program_poll_cd(void)
{
	unsigned long reg, i;

	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		reg = readl(&fpga_manager_base->imgcfg_stat);
		if (reg & ALT_FPGAMGR_IMGCFG_STAT_F2S_CONDONE_PIN_SET_MSK)
			return 0;

		if ((reg & ALT_FPGAMGR_IMGCFG_STAT_F2S_NSTATUS_PIN_SET_MSK) == 0) {
			printf("nstatus == 0 while waiting for condone\n");
			return -9;
		}
	}

	if (i == FPGA_TIMEOUT_CNT)
		return -10;

	return 0;
}

/* Ensure the FPGA entering init phase */
int fpgamgr_program_poll_initphase(void)
{
	return 0;
}

/* Ensure the FPGA entering user mode */
int fpgamgr_program_poll_usermode(void)
{
	unsigned long reg;
	int ret = 0;

	if (fpgamgr_dclkcnt_set(0xf) == FPGA_TIMEOUT_CNT)
		return -11;

	ret = wait_for_user_mode();

	if (ret < 0) {
		printf("%s: Failed to enter user mode with ", __func__);
		printf("error code %d\n", ret);
		return ret;
	}

	/*
	 * Step 14:
	 * Stop DATA path and Dclk
	 * EN_CFG_CTRL and EN_CFG_DATA = 0
	 */
	clrbits_le32(&fpga_manager_base->imgcfg_ctrl_02,
		ALT_FPGAMGR_IMGCFG_CTL_02_EN_CFG_DATA_SET_MSK |
		ALT_FPGAMGR_IMGCFG_CTL_02_EN_CFG_CTRL_SET_MSK);

	/*
	 * Step 15:
	 * Disable overrides
	 * S2F_NENABLE_CONFIG = 1
	 * S2F_NENABLE_NCONFIG = 1
	 */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_01,
		ALT_FPGAMGR_IMGCFG_CTL_01_S2F_NENABLE_CONFIG_SET_MSK);
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_00,
		ALT_FPGAMGR_IMGCFG_CTL_00_S2F_NENABLE_NCONFIG_SET_MSK);

	/* Disable chip select S2F_NCE = 1 */
	setbits_le32(&fpga_manager_base->imgcfg_ctrl_01,
		ALT_FPGAMGR_IMGCFG_CTL_01_S2F_NCE_SET_MSK);

	/*
	 * Step 16:
	 * Final check
	 */
	reg = readl(&fpga_manager_base->imgcfg_stat);
	if (((reg & ALT_FPGAMGR_IMGCFG_STAT_F2S_USERMODE_SET_MSK) == 0) ||
	    ((reg & ALT_FPGAMGR_IMGCFG_STAT_F2S_CONDONE_PIN_SET_MSK) == 0) ||
	    ((reg & ALT_FPGAMGR_IMGCFG_STAT_F2S_NSTATUS_PIN_SET_MSK) == 0))
		return -13;

	return 0;
}

#if defined(CONFIG_CMD_FPGA_LOAD)
/*
 * FPGA Manager to program the FPGA. This is the interface used by FPGA driver.
 * Return 0 for sucess, non-zero for error.
 */
int socfpga_load(Altera_desc *desc, const void *rbf_data, size_t rbf_size)
{
	unsigned long status;

	/* disable all signals from hps peripheral controller to fpga */
	writel(0, &system_manager_base->fpgaintf_en_global);

	/* disable all axi bridge (hps2fpga, lwhps2fpga & fpga2hps) */
	reset_assert_all_bridges();

	/* Initialize the FPGA Manager */
	status = fpgamgr_program_init((u32 *)rbf_data, rbf_size);
	if (status)
		return status;

	/* Write the RBF data to FPGA Manager */
	fpgamgr_program_write(rbf_data, rbf_size);

	return fpgamgr_program_fini();
}
#endif /* CONFIG_CMD_FPGA_LOAD */
