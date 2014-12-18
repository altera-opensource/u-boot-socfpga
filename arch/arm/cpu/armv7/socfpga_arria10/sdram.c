/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/cff.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/sdram.h>
#include <watchdog.h>
#include <fdtdec.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;
static const struct socfpga_noc_ddr_scheduler *socfpga_noc_ddr_scheduler_base =
		(void *)SOCFPGA_SDR_SCHEDULER_ADDRESS;
static const struct socfpga_noc_fw_ddr_mpu_fpga2sdram
		*socfpga_noc_fw_ddr_mpu_fpga2sdram_base =
		(void *)SOCFPGA_SDR_FIREWALL_MPU_FPGA_ADDRESS;
static const struct socfpga_noc_fw_ddr_l3 *socfpga_noc_fw_ddr_l3_base =
		(void *)SOCFPGA_SDR_FIREWALL_L3_ADDRESS;

unsigned long irq_cnt_ecc_sdram;

struct sdr_cfg {
	u32 ecc_en;
	u32 serrcnt;
	u32 io_size;
	u32 ddrconf;
	u32 ddrtiming;
	u32 ddrmode;
	u32 readlatency;
	u32 activate;
	u32 devtodev;
};
struct of_sdr_cfg {
	const char *prop_name;
	const u32 offset;
};
const struct of_sdr_cfg sdr_cfg_tab[] = {
	{ "serrcnt", offsetof(struct sdr_cfg, serrcnt) },
	{ "io-size", offsetof(struct sdr_cfg, io_size) },
	{ "ddrconf", offsetof(struct sdr_cfg, ddrconf) },
	{ "ddrtiming", offsetof(struct sdr_cfg, ddrtiming) },
	{ "ddrmode", offsetof(struct sdr_cfg, ddrmode) },
	{ "readlatency", offsetof(struct sdr_cfg, readlatency) },
	{ "activate", offsetof(struct sdr_cfg, activate) },
	{ "devtodev", offsetof(struct sdr_cfg, devtodev) },
};

static int of_get_sdr_cfg(const void *blob, struct sdr_cfg *cfg) {
	int node, err, i;
	u32 val;
	void *vcfg = cfg;

	memset(cfg, 0, sizeof(*cfg));

	node = fdtdec_next_compatible(blob, 0, COMPAT_ARRIA10_SDR_CTL);

	if (node < 0) {
		printf("failed to find %s compatible field\n",
			fdtdec_get_compatible(COMPAT_ARRIA10_SDR_CTL));
		return -1;
	}

	if (fdt_getprop(blob, node, "ecc-en", NULL)) {
		cfg->ecc_en = 1;
	} else {
		cfg->ecc_en = 0;
	}
	for (i = 0; i < ARRAY_SIZE(sdr_cfg_tab); i++) {
		err = fdtdec_get_int_array(blob, node,
			sdr_cfg_tab[i].prop_name, &val, 1);
		if (err) {
			printf("failed to find %s %d\n", 
				sdr_cfg_tab[i].prop_name, err);
			continue;
		}
		*(u32*)(vcfg +  sdr_cfg_tab[i].offset) = val;
	}
	return node;
}
/* Enable and disable SDRAM interrupt */
void sdram_enable_interrupt(unsigned enable)
{
	/* Clear the internal counter (write 1 to clear) */
	setbits_le32(&socfpga_ecc_hmc_base->eccctrl,
		ALT_ECC_HMC_OCP_ECCCTL_CNT_RST_SET_MSK);

	/* clear the ECC prior enable or even disable (write 1 to clear) */
	setbits_le32(&socfpga_ecc_hmc_base->intstat,
		ALT_ECC_HMC_OCP_INTSTAT_SERRPENA_SET_MSK |
		ALT_ECC_HMC_OCP_INTSTAT_DERRPENA_SET_MSK);

	/*
	 * We want the serr trigger after a number of count instead of
	 * triggered every single bit error event which cost cpu time
	 */
	writel(CONFIG_HPS_SDR_SERRCNT,	&socfpga_ecc_hmc_base->serrcntreg);

	/* Enable the interrupt on compare */
	setbits_le32(&socfpga_ecc_hmc_base->intmode,
		ALT_ECC_HMC_OCP_INTMOD_INTONCMP_SET_MSK);

	if (enable)
		writel(ALT_ECC_HMC_OCP_ERRINTEN_SERRINTEN_SET_MSK |
			ALT_ECC_HMC_OCP_ERRINTEN_DERRINTEN_SET_MSK,
			&socfpga_ecc_hmc_base->errintens);
	else
		writel(ALT_ECC_HMC_OCP_ERRINTEN_SERRINTEN_SET_MSK |
			ALT_ECC_HMC_OCP_ERRINTEN_DERRINTEN_SET_MSK,
			&socfpga_ecc_hmc_base->errintenr);
}

/* handler for SDRAM ECC interrupt */
void irq_handler_ecc_sdram(void *arg)
{
	unsigned reg_value;

	/* check whether SBE happen */
	reg_value = readl(&socfpga_ecc_hmc_base->intstat);
	if (reg_value & ALT_ECC_HMC_OCP_INTSTAT_SERRPENA_SET_MSK) {
		printf("Info: SDRAM ECC SBE @ 0x%08x\n",
			readl(&socfpga_ecc_hmc_base->serraddra));
		irq_cnt_ecc_sdram += readl(&socfpga_ecc_hmc_base->serrcntreg);
		setenv_ulong("sdram_ecc_sbe", irq_cnt_ecc_sdram);
	}

	/* check whether DBE happen */
	if (reg_value & ALT_ECC_HMC_OCP_ERRINTEN_DERRINTEN_SET_MSK) {
		puts("Error: SDRAM ECC DBE occurred\n");
		printf("sbecount = %lu\n", irq_cnt_ecc_sdram);
		printf("erraddr = %08x\n",
			readl(&socfpga_ecc_hmc_base->derraddra));
	}

	/* Clear the internal counter (write 1 to clear) */
	setbits_le32(&socfpga_ecc_hmc_base->eccctrl,
		ALT_ECC_HMC_OCP_ECCCTL_CNT_RST_SET_MSK);

	/* clear the ECC prior enable or even disable (write 1 to clear) */
	setbits_le32(&socfpga_ecc_hmc_base->intstat,
		ALT_ECC_HMC_OCP_INTSTAT_SERRPENA_SET_MSK |
		ALT_ECC_HMC_OCP_INTSTAT_DERRPENA_SET_MSK);

	/* if DBE, going into hang */
	if (reg_value & ALT_ECC_HMC_OCP_ERRINTEN_DERRINTEN_SET_MSK) {
		sdram_enable_interrupt(0);
		hang();
	}
}

/* Function to initialize SDRAM MMR and NOC DDR scheduler*/
void sdram_mmr_init(void)
{
	/* configureing the DDR IO size */
	writel(CONFIG_HPS_SDR_IO_SIZE,	&socfpga_ecc_hmc_base->ddrioctrl);

	/* enable or disable the SDRAM ECC */
#if (CONFIG_HPS_SDR_ECC_EN == 1)
	setbits_le32(&socfpga_ecc_hmc_base->eccctrl,
		ALT_ECC_HMC_OCP_ECCCTL_ECC_EN_SET_MSK);
	setbits_le32(&socfpga_ecc_hmc_base->eccctrl2,
		ALT_ECC_HMC_OCP_ECCCTL2_RMW_EN_SET_MSK);
#else
	clrbits_le32(&socfpga_ecc_hmc_base->eccctrl,
		ALT_ECC_HMC_OCP_ECCCTL_ECC_EN_SET_MSK);
	clrbits_le32(&socfpga_ecc_hmc_base->eccctrl2,
		ALT_ECC_HMC_OCP_ECCCTL2_RMW_EN_SET_MSK);
#endif

	/* Release NOC ddr scheduler from reset */
	reset_deassert_noc_ddr_scheduler();

#ifndef CONFIG_HPS_SDR_SKIP_HANDOFF
	/* Function to grab settings from handoff */

	/* configuring the DDR configuration */
	writel(CONFIG_HPS_SDR_DDRCONF,
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_ddrconf);

	/* configuring DDR timing */
	writel(((CONFIG_HPS_SDR_DDRTIMING_ACTTOACT <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_ACTTOACT_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_RDTOMISS <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_RDTOMISS_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_WRTOMISS <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_WRTOMISS_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_BURSTLEN <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_BURSTLEN_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_RDTOWR <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_RDTOWR_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_WRTORD <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_WRTORD_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_BWRATIO <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_BWRATIO_LSB)),
		&socfpga_noc_ddr_scheduler_base->
			ddr_t_main_scheduler_ddrtiming);

	/* configuring DDR mode */
	writel(((CONFIG_HPS_SDR_DDRMODE_AUTOPRECHARGE <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRMOD_AUTOPRECHARGE_LSB) |
		(CONFIG_HPS_SDR_DDRMODE_BWRATIOEXTENDED <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRMOD_BWRATIOEXTENDED_LSB)),
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_ddrmode);

	/* configuring the read latency */
	writel(CONFIG_HPS_SDR_READLATENCY,
		&socfpga_noc_ddr_scheduler_base->
			ddr_t_main_scheduler_readlatency);

	/* configuring timing values concerning activate commands */
	writel(((CONFIG_HPS_SDR_ACTIVATE_RRD <<
			ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_RRD_LSB) |
		(CONFIG_HPS_SDR_ACTIVATE_FAW <<
			ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_FAW_LSB) |
		(CONFIG_HPS_SDR_ACTIVATE_FAWBANK <<
			ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_FAWBANK_LSB)),
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_activate);

	/* configuring timing values concerning device to device data bus
	ownership change */
	writel(((CONFIG_HPS_SDR_DEVTODEV_BUSRDTORD <<
			ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSRDTORD_LSB) |
		(CONFIG_HPS_SDR_DEVTODEV_BUSRDTOWR <<
			ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSRDTOWR_LSB) |
		(CONFIG_HPS_SDR_DEVTODEV_BUSWRTORD <<
			ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSWRTORD_LSB)),
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_devtodev);
#else
	/* Function to grab settings from IOCSR, skipping the need of handoff */

	/* configuring DDR timing */
	writel(((CONFIG_HPS_SDR_DDRTIMING_ACTTOACT <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_ACTTOACT_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_RDTOMISS <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_RDTOMISS_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_WRTOMISS <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_WRTOMISS_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_BURSTLEN <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_BURSTLEN_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_RDTOWR <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_RDTOWR_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_WRTORD <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_WRTORD_LSB) |
		(CONFIG_HPS_SDR_DDRTIMING_BWRATIO <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_BWRATIO_LSB)),
		&socfpga_noc_ddr_scheduler_base->
			ddr_t_main_scheduler_ddrtiming);

	/* configuring DDR mode */
	writel(((CONFIG_HPS_SDR_DDRMODE_AUTOPRECHARGE <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRMOD_AUTOPRECHARGE_LSB) |
		(CONFIG_HPS_SDR_DDRMODE_BWRATIOEXTENDED <<
			ALT_NOC_MPU_DDR_T_SCHED_DDRMOD_BWRATIOEXTENDED_LSB)),
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_ddrmode);

	/* configuring the read latency */
	writel(CONFIG_HPS_SDR_READLATENCY,
		&socfpga_noc_ddr_scheduler_base->
			ddr_t_main_scheduler_readlatency);

	/* configuring timing values concerning activate commands */
	writel(((CONFIG_HPS_SDR_ACTIVATE_RRD <<
			ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_RRD_LSB) |
		(CONFIG_HPS_SDR_ACTIVATE_FAW <<
			ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_FAW_LSB) |
		(CONFIG_HPS_SDR_ACTIVATE_FAWBANK <<
			ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_FAWBANK_LSB)),
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_activate);

	/* configuring timing values concerning device to device data bus
	ownership change */
	writel(((CONFIG_HPS_SDR_DEVTODEV_BUSRDTORD <<
			ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSRDTORD_LSB) |
		(CONFIG_HPS_SDR_DEVTODEV_BUSRDTOWR <<
			ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSRDTOWR_LSB) |
		(CONFIG_HPS_SDR_DEVTODEV_BUSWRTORD <<
			ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSWRTORD_LSB)),
		&socfpga_noc_ddr_scheduler_base->ddr_t_main_scheduler_devtodev);

#endif	/* CONFIG_HPS_SDR_SKIP_HANDOFF */
}

/* quick check for firewall value */
#if (CONFIG_HPS_SDR_MPU0_START > CONFIG_HPS_SDR_MPU0_END) | \
(CONFIG_HPS_SDR_MPU1_START > CONFIG_HPS_SDR_MPU1_END) | \
(CONFIG_HPS_SDR_MPU2_START > CONFIG_HPS_SDR_MPU2_END) | \
(CONFIG_HPS_SDR_MPU3_START > CONFIG_HPS_SDR_MPU3_END) | \
(CONFIG_HPS_SDR_HPS_L3_0_START > CONFIG_HPS_SDR_HPS_L3_0_END) | \
(CONFIG_HPS_SDR_HPS_L3_1_START > CONFIG_HPS_SDR_HPS_L3_1_END) | \
(CONFIG_HPS_SDR_HPS_L3_2_START > CONFIG_HPS_SDR_HPS_L3_2_END) | \
(CONFIG_HPS_SDR_HPS_L3_3_START > CONFIG_HPS_SDR_HPS_L3_3_END) | \
(CONFIG_HPS_SDR_HPS_L3_4_START > CONFIG_HPS_SDR_HPS_L3_4_END) | \
(CONFIG_HPS_SDR_HPS_L3_5_START > CONFIG_HPS_SDR_HPS_L3_5_END) | \
(CONFIG_HPS_SDR_HPS_L3_6_START > CONFIG_HPS_SDR_HPS_L3_6_END) | \
(CONFIG_HPS_SDR_HPS_L3_7_START > CONFIG_HPS_SDR_HPS_L3_7_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM0_0_START > CONFIG_HPS_SDR_FPGA2SDRAM0_0_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM0_1_START > CONFIG_HPS_SDR_FPGA2SDRAM0_1_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM0_2_START > CONFIG_HPS_SDR_FPGA2SDRAM0_2_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM0_3_START > CONFIG_HPS_SDR_FPGA2SDRAM0_3_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM1_0_START > CONFIG_HPS_SDR_FPGA2SDRAM1_0_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM1_1_START > CONFIG_HPS_SDR_FPGA2SDRAM1_1_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM1_2_START > CONFIG_HPS_SDR_FPGA2SDRAM1_2_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM1_3_START > CONFIG_HPS_SDR_FPGA2SDRAM1_3_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM2_0_START > CONFIG_HPS_SDR_FPGA2SDRAM2_0_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM2_1_START > CONFIG_HPS_SDR_FPGA2SDRAM2_1_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM2_2_START > CONFIG_HPS_SDR_FPGA2SDRAM2_2_END) | \
(CONFIG_HPS_SDR_FPGA2SDRAM2_3_START > CONFIG_HPS_SDR_FPGA2SDRAM2_3_END)
#error "sdram_config.h handoff file contain invalid DDR firewall value"
#endif
struct firewall_entry {
	const char *prop_name;
	const u32 cfg_addr;
	const u32 en_addr;
	const u32 en_bit;
};
#define FW_MPU_FPGA_ADDRESS \
	((const struct socfpga_noc_fw_ddr_mpu_fpga2sdram*)\
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
int of_sdram_firewall_setup(void *blob, int node)
{
	int child, i;
	u32 start_end[2];

	child = fdt_first_subnode(blob, node);
	if (child < 0)
		return 1;

	/* set to default state */
	writel(0, &socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable);
	writel(0, &socfpga_noc_fw_ddr_l3_base->enable);


	for (i = 0; i < ARRAY_SIZE(firewall_table); i++) {
		if (!fdtdec_get_int_array(blob, child,
			firewall_table[i].prop_name, start_end, 2)) {
			writel((start_end[0] & ALT_NOC_FW_DDR_ADDR_MASK) |
				(start_end[1] << ALT_NOC_FW_DDR_END_ADDR_LSB),
				firewall_table[i].cfg_addr);
			setbits_le32(firewall_table[i].en_addr,
					firewall_table[i].en_bit);
		}
	}

	return 0;
}
/* Function to initialize SDRAM MMR and NOC DDR scheduler*/
void sdram_firewall_setup(void)
{
	/* set to default state */
	writel(0, &socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable);
	writel(0, &socfpga_noc_fw_ddr_l3_base->enable);

	/* setup MPU region firewall */
#if (CONFIG_HPS_SDR_MPU0_ENABLE)
	writel((CONFIG_HPS_SDR_MPU0_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_MPU0_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->mpuregion0addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_MPUREG0EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_MPU1_ENABLE)
	writel((CONFIG_HPS_SDR_MPU1_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_MPU1_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->mpuregion1addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_MPUREG1EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_MPU2_ENABLE)
	writel((CONFIG_HPS_SDR_MPU2_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_MPU2_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->mpuregion2addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_MPUREG2EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_MPU3_ENABLE)
	writel((CONFIG_HPS_SDR_MPU3_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_MPU3_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->mpuregion3addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_MPUREG3EN_SET_MSK);
#endif

	/* setup HPS L3 region firewall */
#if (CONFIG_HPS_SDR_HPS_L3_0_ENABLE)
	writel((CONFIG_HPS_SDR_HPS_L3_0_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_HPS_L3_0_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_l3_base->hpsregion0addr);
	setbits_le32(&socfpga_noc_fw_ddr_l3_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_HPSREG0EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_HPS_L3_1_ENABLE)
	writel((CONFIG_HPS_SDR_HPS_L3_1_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_HPS_L3_1_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_l3_base->hpsregion1addr);
	setbits_le32(&socfpga_noc_fw_ddr_l3_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_HPSREG1EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_HPS_L3_2_ENABLE)
	writel((CONFIG_HPS_SDR_HPS_L3_2_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_HPS_L3_2_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_l3_base->hpsregion2addr);
	setbits_le32(&socfpga_noc_fw_ddr_l3_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_HPSREG2EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_HPS_L3_3_ENABLE)
	writel((CONFIG_HPS_SDR_HPS_L3_3_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_HPS_L3_3_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_l3_base->hpsregion3addr);
	setbits_le32(&socfpga_noc_fw_ddr_l3_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_HPSREG3EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_HPS_L3_4_ENABLE)
	writel((CONFIG_HPS_SDR_HPS_L3_4_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_HPS_L3_4_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_l3_base->hpsregion4addr);
	setbits_le32(&socfpga_noc_fw_ddr_l3_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_HPSREG4EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_HPS_L3_5_ENABLE)
	writel((CONFIG_HPS_SDR_HPS_L3_5_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_HPS_L3_5_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_l3_base->hpsregion5addr);
	setbits_le32(&socfpga_noc_fw_ddr_l3_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_HPSREG5EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_HPS_L3_6_ENABLE)
	writel((CONFIG_HPS_SDR_HPS_L3_6_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_HPS_L3_6_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_l3_base->hpsregion6addr);
	setbits_le32(&socfpga_noc_fw_ddr_l3_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_HPSREG6EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_HPS_L3_7_ENABLE)
	writel((CONFIG_HPS_SDR_HPS_L3_7_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_HPS_L3_7_END << ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_l3_base->hpsregion7addr);
	setbits_le32(&socfpga_noc_fw_ddr_l3_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_HPSREG7EN_SET_MSK);
#endif

	/* setup FPGA region firewall */
#if (CONFIG_HPS_SDR_FPGA2SDRAM0_0_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM0_0_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM0_0_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram0region0addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG0EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_FPGA2SDRAM0_1_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM0_1_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM0_1_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram0region1addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG1EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_FPGA2SDRAM0_2_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM0_2_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM0_2_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram0region2addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG2EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_FPGA2SDRAM0_3_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM0_3_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM0_3_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram0region3addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG3EN_SET_MSK);
#endif

#if (CONFIG_HPS_SDR_FPGA2SDRAM1_0_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM1_0_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM1_0_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram1region0addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG0EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_FPGA2SDRAM1_1_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM1_1_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM1_1_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram1region1addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG1EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_FPGA2SDRAM1_2_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM1_2_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM1_2_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram1region2addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG2EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_FPGA2SDRAM1_3_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM1_3_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM1_3_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram1region3addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG3EN_SET_MSK);
#endif

#if (CONFIG_HPS_SDR_FPGA2SDRAM2_0_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM2_0_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM2_0_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram2region0addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG0EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_FPGA2SDRAM2_1_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM2_1_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM2_1_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram2region1addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG1EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_FPGA2SDRAM2_2_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM2_2_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM2_2_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram2region2addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG2EN_SET_MSK);
#endif
#if (CONFIG_HPS_SDR_FPGA2SDRAM2_3_ENABLE)
	writel((CONFIG_HPS_SDR_FPGA2SDRAM2_3_START & ALT_NOC_FW_DDR_ADDR_MASK) |
		(CONFIG_HPS_SDR_FPGA2SDRAM2_3_END
			<< ALT_NOC_FW_DDR_END_ADDR_LSB),
		&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->
			fpga2sdram2region3addr);
	setbits_le32(&socfpga_noc_fw_ddr_mpu_fpga2sdram_base->enable,
		ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG3EN_SET_MSK);
#endif
}


/* Check whether SDRAM is CAL success */
int is_sdram_cal_success(void)
{
	return readl(&socfpga_ecc_hmc_base->ddrcalstat);
}

/* Initialise the DRAM by telling the DRAM Size */
int dram_init(void)
{
	bd_t *bd;
	unsigned long addr;
	struct sdr_cfg cfg;
	int node;

	puts("dram_init matt\n");
	WATCHDOG_RESET();
	/* assigning the SDRAM size */
	gd->ram_size = PHYS_SDRAM_1_SIZE;

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
	addr -= sizeof (bd_t);
	bd = (bd_t *) addr;
	gd->bd = bd;

	/* setup the dram info within bd */
	dram_init_banksize();

	/* enable the cache */
	enable_caches();
#endif

	WATCHDOG_RESET();
	puts("CFF   : Configuring all flip-flops\n");
	if (cff_from_fat())
		puts("CFF   : failed\n");

	WATCHDOG_RESET();

	node = of_get_sdr_cfg(gd->fdt_blob, &cfg);
#ifndef TEST_AT_ASIMOV
	/* initialize the MMR register */
	sdram_mmr_init();

	/* setup the firewall for DDR */
	sdram_firewall_setup();
#endif

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

