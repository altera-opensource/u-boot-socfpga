// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019-2022 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <div64.h>
#include <fdtdec.h>
#include <hang.h>
#include <log.h>
#include <ram.h>
#include <reset.h>
#include <asm/global_data.h>
#include "iossm_mailbox.h"
#include "sdram_soc64.h"
#include <wait_bit.h>
#include <asm/arch/firewall.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/io.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

/* MPFE NOC registers */
#define F2SDRAM_SIDEBAND_FLAGOUTSET0	0x50
#define F2SDRAM_SIDEBAND_FLAGOUTSTATUS0	0x58
#define SIDEBANDMGR_FLAGOUTSET0_REG	SOCFPGA_F2SDRAM_MGR_ADDRESS +\
					F2SDRAM_SIDEBAND_FLAGOUTSET0
#define SIDEBANDMGR_FLAGOUTSTATUS0_REG	SOCFPGA_F2SDRAM_MGR_ADDRESS +\
					F2SDRAM_SIDEBAND_FLAGOUTSTATUS0

/* Reset type */
enum reset_type {
	POR_RESET,
	WARM_RESET,
	COLD_RESET,
	NCONFIG,
	JTAG_CONFIG,
	RSU_RECONFIG
};

static enum reset_type get_reset_type(u32 reg)
{
	return (reg & ALT_SYSMGR_SCRATCH_REG_3_DDR_RESET_TYPE_MASK) >>
		ALT_SYSMGR_SCRATCH_REG_3_DDR_RESET_TYPE_SHIFT;
}

int set_mpfe_config(void)
{
	/* Set mpfe_lite_active */
	setbits_le32(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_MPFE_CONFIG, BIT(8));

	debug("%s: mpfe_config: 0x%x\n", __func__,
	      readl(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_MPFE_CONFIG));

	return 0;
}

bool is_ddr_init_hang(void)
{
	u32 reg = readl(socfpga_get_sysmgr_addr() +
			SYSMGR_SOC64_BOOT_SCRATCH_POR0);

	if (reg & ALT_SYSMGR_SCRATCH_REG_POR_0_DDR_PROGRESS_MASK)
		return true;

	return false;
}

void ddr_init_inprogress(bool start)
{
	if (start)
		setbits_le32(socfpga_get_sysmgr_addr() +
				SYSMGR_SOC64_BOOT_SCRATCH_POR0,
				ALT_SYSMGR_SCRATCH_REG_POR_0_DDR_PROGRESS_MASK);
	else
		clrbits_le32(socfpga_get_sysmgr_addr() +
				SYSMGR_SOC64_BOOT_SCRATCH_POR0,
				ALT_SYSMGR_SCRATCH_REG_POR_0_DDR_PROGRESS_MASK);
}

int populate_ddr_handoff(struct udevice *dev, struct io96b_info *io96b_ctrl)
{
	struct altera_sdram_plat *plat = dev_get_plat(dev);
	fdt_addr_t addr;
	int i;
	u32 len = SOC64_HANDOFF_SDRAM_LEN;
	u32 handoff_table[len];

	/* Read handoff for DDR configuration */
	socfpga_handoff_read((void *)SOC64_HANDOFF_SDRAM, handoff_table, len);

	/* Read handoff - dual port */
	plat->dualport = handoff_table[0] & BIT(0);

	/* Read handoff - dual EMIF */
	plat->dualemif = handoff_table[0] & BIT(1);
	if (plat->dualemif)
		io96b_ctrl->num_instance = 2;
	else
		io96b_ctrl->num_instance = 1;

	/* Assign IO96B CSR base address if it is valid */
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		addr = dev_read_addr_index(dev, i + 1);
		if (addr == FDT_ADDR_T_NONE)
			return -EINVAL;

		switch (i) {
		case 0:
			io96b_ctrl->io96b_0.io96b_csr_addr = addr;
			debug("%s: IO96B 0x%llx CSR enabled\n", __func__
					, io96b_ctrl->io96b_0.io96b_csr_addr);
			break;
		case 1:
			io96b_ctrl->io96b_1.io96b_csr_addr = addr;
			debug("%s: IO96B 0x%llx CSR enabled\n", __func__
					, io96b_ctrl->io96b_1.io96b_csr_addr);
			break;
		default:
			printf("%s: Invalid IO96B CSR\n", __func__);
		}
	}

	return 0;
}

int config_mpfe_sideband_mgr(struct udevice *dev)
{
	struct altera_sdram_plat *plat = dev_get_plat(dev);

	/* Dual port setting */
	if (plat->dualport)
		setbits_le32(SIDEBANDMGR_FLAGOUTSET0_REG, BIT(4));

	/* Dual EMIF setting */
	if (plat->dualemif) {
		set_mpfe_config();
		setbits_le32(SIDEBANDMGR_FLAGOUTSET0_REG, BIT(5));
	}

	debug("%s: SIDEBANDMGR_FLAGOUTSTATUS0: 0x%x\n", __func__,
	      readl(SIDEBANDMGR_FLAGOUTSTATUS0_REG));

	return 0;
}

bool hps_ocram_dbe_status(void)
{
	u32 reg = readl(socfpga_get_sysmgr_addr() +
			SYSMGR_SOC64_BOOT_SCRATCH_COLD3);

	if (reg & ALT_SYSMGR_SCRATCH_REG_3_OCRAM_DBE_MASK)
		return true;

	return false;
}

bool ddr_ecc_dbe_status(void)
{
	u32 reg = readl(socfpga_get_sysmgr_addr() +
			SYSMGR_SOC64_BOOT_SCRATCH_COLD3);

	if (reg & ALT_SYSMGR_SCRATCH_REG_3_DDR_DBE_MASK)
		return true;

	return false;
}

int sdram_mmr_init_full(struct udevice *dev)
{
	int ret;
	phys_size_t hw_size;
	struct bd_info bd = {0};
	struct altera_sdram_plat *plat = dev_get_plat(dev);
	struct altera_sdram_priv *priv = dev_get_priv(dev);
	struct io96b_info *io96b_ctrl = malloc(sizeof(*io96b_ctrl));

	u32 reg = readl(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_BOOT_SCRATCH_COLD3);
	enum reset_type reset_t = get_reset_type(reg);
	bool full_mem_init = false;

	/* DDR initialization progress status tracking */
	bool is_ddr_hang_be4_rst = is_ddr_init_hang();

	debug("DDR: SDRAM init in progress ...\n");
	ddr_init_inprogress(true);

	debug("DDR: Address MPFE 0x%llx\n", plat->mpfe_base_addr);

	/* Populating DDR handoff data */
	debug("DDR: Checking SDRAM configuration in progress ...\n");
	ret = populate_ddr_handoff(dev, io96b_ctrl);
	if (ret) {
		printf("DDR: Failed to populate DDR handoff\n");
		return ret;
	}

	/* Configuring MPFE sideband manager registers - dual port & dual emif*/
	ret = config_mpfe_sideband_mgr(dev);
	if (ret) {
		printf("DDR: Failed to configure dual port dual emif\n");
		return ret;
	}

	/* Ensure calibration status passing */
	init_mem_cal(io96b_ctrl);

	/* Initiate IOSSM mailbox */
	io96b_mb_init(io96b_ctrl);

	/* Need to trigger re-calibration for DDR DBE */
	if (ddr_ecc_dbe_status()) {
		io96b_ctrl->io96b_0.cal_status = false;
		io96b_ctrl->io96b_1.cal_status = false;
		io96b_ctrl->overall_cal_status = io96b_ctrl->io96b_0.cal_status ||
						 io96b_ctrl->io96b_1.cal_status;
	}

	/* Trigger re-calibration if calibration failed */
	if (!(io96b_ctrl->overall_cal_status)) {
		printf("DDR: Re-calibration in progress...\n");
		trig_mem_cal(io96b_ctrl);
	}

	printf("DDR: Calibration success\n");

	/* DDR type, DDR size and ECC status) */
	ret = get_mem_technology(io96b_ctrl);
	if (ret) {
		printf("DDR: Failed to get DDR type\n");
		return ret;
	}

	ret = get_mem_width_info(io96b_ctrl);
	if (ret) {
		printf("DDR: Failed to get DDR size\n");
		return ret;
	}

	hw_size = (phys_size_t)io96b_ctrl->overall_size * SZ_1G / SZ_8;

	/* Get bank configuration from devicetree */
	ret = fdtdec_decode_ram_size(gd->fdt_blob, NULL, 0, NULL,
				     (phys_size_t *)&gd->ram_size, &bd);
	if (ret) {
		puts("DDR: Failed to decode memory node\n");
		return -ENXIO;
	}

	if (gd->ram_size != hw_size) {
		printf("DDR: Warning: DRAM size from device tree (%lld MiB)\n",
		       gd->ram_size >> 20);
		printf(" mismatch with hardware (%lld MiB).\n",
		       hw_size >> 20);
	}

	if (gd->ram_size > hw_size) {
		printf("DDR: Error: DRAM size from device tree is greater\n");
		printf(" than hardware size.\n");
		hang();
	}

	printf("%s: %lld MiB\n", io96b_ctrl->ddr_type, gd->ram_size >> 20);

	ret = ecc_enable_status(io96b_ctrl);
	if (ret) {
		printf("DDR: Failed to get DDR ECC status\n");
		return ret;
	}

	/* Is HPS cold or warm reset? If yes, Skip full memory initialization if ECC
	 *  enabled to preserve memory content
	 */
	if (io96b_ctrl->ecc_status) {
		full_mem_init = hps_ocram_dbe_status() | ddr_ecc_dbe_status() |
				is_ddr_hang_be4_rst;
		if (full_mem_init || !(reset_t == WARM_RESET || reset_t == COLD_RESET)) {
			ret = bist_mem_init_start(io96b_ctrl);
			if (ret) {
				printf("DDR: Failed to fully initialize DDR memory\n");
				return ret;
			}
		}

		printf("SDRAM-ECC: Initialized success\n");
	}

	sdram_size_check(&bd);
	printf("DDR: size check success\n");

	sdram_set_firewall(&bd);

	/* Firewall setting for MPFE CSR */
	/* IO96B0_reg */
	writel(0x1, 0x18000d00);
	/* IO96B1_reg */
	writel(0x1, 0x18000d04);
	/* noc_csr */
	writel(0x1, 0x18000d08);

	printf("DDR: firewall init success\n");

	priv->info.base = bd.bi_dram[0].start;
	priv->info.size = gd->ram_size;

	/* Ending DDR driver initialization success tracking */
	ddr_init_inprogress(false);

	printf("DDR: init success\n");

	return 0;
}
