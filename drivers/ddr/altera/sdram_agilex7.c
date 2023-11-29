// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Intel Corporation <www.intel.com>
 *
 */

#include <dm.h>
#include <hang.h>
#include <log.h>
#include <ram.h>
#include <reset.h>
#include <wait_bit.h>
#include <asm/arch/system_manager.h>
#include <linux/bitfield.h>
#include "iossm_mailbox.h"
#include "sdram_soc64.h"

/* NOCPLL register */
#define SYSMGR_HMC_CLK		0xB4
#define SYSMGR_HMC_CLK_NOCPLL	BIT(8)

/* MPFE NOC registers */
#define F2SDRAM_SIDEBAND_FLAGOUTSET0	0x50
#define F2SDRAM_SIDEBAND_FLAGOUTSTATUS0	0x58
#define SIDEBANDMGR_FLAGOUTSET0_REG	SOCFPGA_F2SDRAM_MGR_ADDRESS +\
					F2SDRAM_SIDEBAND_FLAGOUTSET0
#define SIDEBANDMGR_FLAGOUTSTATUS0_REG	SOCFPGA_F2SDRAM_MGR_ADDRESS +\
					F2SDRAM_SIDEBAND_FLAGOUTSTATUS0

#define SIDEBANDMGR_FLAGOUTSET0_REG_MULTICHANNEL	BIT(4)
#define SIDEBANDMGR_FLAGOUTSET0_REG_INTERLEAVING	BIT(5)

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
	return (reg & ALT_SYSMGR_SCRATCH_REG_0_DDR_RESET_TYPE_MASK) >>
		ALT_SYSMGR_SCRATCH_REG_0_DDR_RESET_TYPE_SHIFT;
}

bool is_ddr_init_hang(void)
{
	u32 reg = readl(socfpga_get_sysmgr_addr() +
			SYSMGR_SOC64_BOOT_SCRATCH_COLD8);
	debug("%s: 0x%x\n", __func__, reg);

	if (reg & ALT_SYSMGR_SCRATCH_REG_8_DDR_PROGRESS_MASK)
		return true;

	return false;
}

void ddr_init_inprogress(bool start)
{
	if (start)
		setbits_le32(socfpga_get_sysmgr_addr() +
			     SYSMGR_SOC64_BOOT_SCRATCH_COLD8,
			     ALT_SYSMGR_SCRATCH_REG_8_DDR_PROGRESS_MASK);
	else
		clrbits_le32(socfpga_get_sysmgr_addr() +
			     SYSMGR_SOC64_BOOT_SCRATCH_COLD8,
			     ALT_SYSMGR_SCRATCH_REG_8_DDR_PROGRESS_MASK);
}

void update_io96b_assigned_to_hps(u8 num_io96b_instance)
{
	u32 reg = readl(socfpga_get_sysmgr_addr() +
			SYSMGR_SOC64_BOOT_SCRATCH_COLD8);

	reg = reg & ~ALT_SYSMGR_SCRATCH_REG_8_IO96B_HPS_MASK;

	writel(reg | FIELD_PREP(ALT_SYSMGR_SCRATCH_REG_8_IO96B_HPS_MASK, num_io96b_instance),
		 socfpga_get_sysmgr_addr() + SYSMGR_SOC64_BOOT_SCRATCH_COLD8);
}

int populate_ddr_handoff(struct udevice *dev, struct io96b_info *io96b_ctrl)
{
	struct altera_sdram_plat *plat = dev_get_plat(dev);
	fdt_addr_t addr;
	int i;
	u8 count = 0;
	u32 len = SOC64_HANDOFF_DDR_LEN;
	u32 handoff_table[len];

	/* Read handoff for DDR configuration */
	socfpga_handoff_read((void *)SOC64_HANDOFF_DDR_BASE, handoff_table, len);

	/* Interleaving Mode */
	if (handoff_table[0] & SOC64_HANDOFF_DDR_INTERLEAVING_MODE_MASK)
		plat->multichannel_interleaving = true;
	else
		plat->multichannel_interleaving = false;
	debug("%s: MPFE-IO96B is in %s mode\n", __func__
			, plat->multichannel_interleaving ? "interleaving" : "multichannel");

	/* Assign IO96B CSR base address if it is valid */
	for (i = 0; i < MAX_IO96B_SUPPORTED; i++) {
		addr = dev_read_addr_index(dev, i + 1);

		if (addr == FDT_ADDR_T_NONE)
			return -EINVAL;

		switch (i) {
		case 0:
			if (handoff_table[1] & BIT(i)) {
				io96b_ctrl->io96b_0.io96b_csr_addr = addr;
				debug("%s: IO96B 0x%llx CSR enabled\n", __func__
						, io96b_ctrl->io96b_0.io96b_csr_addr);
				count++;
			}
			break;
		case 1:
			if (handoff_table[1] & BIT(i)) {
				io96b_ctrl->io96b_1.io96b_csr_addr = addr;
				debug("%s: IO96B 0x%llx CSR enabled\n", __func__
						, io96b_ctrl->io96b_1.io96b_csr_addr);
				count++;
			}
		}
	}

	io96b_ctrl->num_instance = count;
	update_io96b_assigned_to_hps(count);
	debug("%s: returned num_instance 0x%x\n", __func__, io96b_ctrl->num_instance);
	return 0;
}

int config_mpfe_sideband_mgr(struct udevice *dev)
{
	struct altera_sdram_plat *plat = dev_get_plat(dev);
	u32 reg;
	u32 mask;

	if (plat->multichannel_interleaving) {
		debug("%s: Set interleaving bit\n", __func__);
		mask = SIDEBANDMGR_FLAGOUTSET0_REG_INTERLEAVING;
		setbits_le32(SIDEBANDMGR_FLAGOUTSET0_REG, mask);
	} else {
		debug("%s: Set multichannel bit\n", __func__);
		mask = SIDEBANDMGR_FLAGOUTSET0_REG_MULTICHANNEL;
		setbits_le32(SIDEBANDMGR_FLAGOUTSET0_REG, mask);
	}

	reg = readl(SIDEBANDMGR_FLAGOUTSTATUS0_REG);
	debug("%s: F2SDRAM_SIDEBAND_FLAGOUTSTATUS0: 0x%x\n", __func__, reg);

	if ((reg & mask) == SIDEBANDMGR_FLAGOUTSET0_REG_INTERLEAVING) {
		debug("%s: Interleaving bit is set\n", __func__);
		return 0;
	} else if ((reg & mask) == SIDEBANDMGR_FLAGOUTSET0_REG_MULTICHANNEL) {
		debug("%s: Multichannel bit is set\n", __func__);
		return 0;
	} else {
		return -1;
	}
}

bool hps_ocram_dbe_status(void)
{
	u32 reg = readl(socfpga_get_sysmgr_addr() +
			SYSMGR_SOC64_BOOT_SCRATCH_COLD8);

	if (reg & ALT_SYSMGR_SCRATCH_REG_8_OCRAM_DBE_MASK)
		return true;

	return false;
}

bool ddr_ecc_dbe_status(void)
{
	u32 reg = readl(socfpga_get_sysmgr_addr() +
			SYSMGR_SOC64_BOOT_SCRATCH_COLD8);

	if (reg & ALT_SYSMGR_SCRATCH_REG_8_DDR_DBE_MASK)
		return true;

	return false;
}

int sdram_mmr_init_full(struct udevice *dev)
{
	struct altera_sdram_plat *plat = dev_get_plat(dev);
	struct altera_sdram_priv *priv = dev_get_priv(dev);
	struct io96b_info *io96b_ctrl = malloc(sizeof(*io96b_ctrl));
	struct bd_info bd = {0};
	bool full_mem_init = false;
	phys_size_t hw_size;
	int ret;
	u32 reg = readl(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_BOOT_SCRATCH_COLD0);
	enum reset_type reset_t = get_reset_type(reg);

	debug("DDR: Address MPFE 0x%llx\n", plat->mpfe_base_addr);

	/* DDR initialization progress status tracking */
	bool is_ddr_hang_be4_rst = is_ddr_init_hang();

	printf("DDR: IO96B SDRAM init in progress ...\n");
	ddr_init_inprogress(true);

	/* Populating DDR handoff data */
	debug("DDR: MPFE configuration in progress ...\n");
	ret = populate_ddr_handoff(dev, io96b_ctrl);
	if (ret) {
		printf("DDR: Failed to populate DDR handoff\n");
		return ret;
	}

	/* Configuring MPFE sideband manager registers - multichannel or interleaving*/
	ret = config_mpfe_sideband_mgr(dev);
	if (ret) {
		printf("DDR: Failed to configure multichannel/interleaving mode\n");
		return ret;
	}

	debug("DDR: MPFE configuration completed\n");

	printf("DDR: Waiting for NOCPLL locked ...\n");
	/* Ensure NOCPLL locked */
	ret = wait_for_bit_le32((const void *)socfpga_get_sysmgr_addr() + SYSMGR_HMC_CLK
				, SYSMGR_HMC_CLK_NOCPLL, true, TIMEOUT, false);
	if (ret) {
		printf("DDR: NOCPLL is not locked\n");
		return ret;
	}

	printf("DDR: NOCPLL locked\n");

	printf("DDR: Checking calibration...\n");

	/* Configure if polling is needed for IO96B GEN PLL locked */
	io96b_ctrl->ckgen_lock = false;

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
		init_mem_cal(io96b_ctrl);
	}

	if (!(io96b_ctrl->overall_cal_status)) {
		printf("DDR: Retry calibration failed & not able to re-calibrate\n");
		return -1;
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

	/* Get bank configuration from devicetree */
	ret = fdtdec_decode_ram_size(gd->fdt_blob, NULL, 0, NULL,
				     (phys_size_t *)&gd->ram_size, &bd);
	if (ret) {
		printf("DDR: Failed to decode memory node\n");
		return -ENXIO;
	}

	hw_size = (phys_size_t)io96b_ctrl->overall_size * SZ_1G / SZ_8;

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
			debug("%s: Needed to fully initialize DDR memory\n", io96b_ctrl->ddr_type);
			ret = bist_mem_init_start(io96b_ctrl);
			if (ret) {
				printf("%s: Failed to fully initialize DDR memory\n"
					, io96b_ctrl->ddr_type);
				return ret;
			}
		}
	}

	sdram_size_check(&bd);
	printf("%s: size check success\n", io96b_ctrl->ddr_type);

	sdram_set_firewall(&bd);
	printf("%s: firewall init success\n", io96b_ctrl->ddr_type);

	priv->info.base = bd.bi_dram[0].start;
	priv->info.size = gd->ram_size;

	/* Ending DDR driver initialization success tracking */
	ddr_init_inprogress(false);

	printf("%s: IO96B SDRAM init success\n", io96b_ctrl->ddr_type);

	return 0;
}
