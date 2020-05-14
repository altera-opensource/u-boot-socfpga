/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation <www.intel.com>
 */

#ifndef _SYSTEM_MANAGER_SOC64_H_
#define _SYSTEM_MANAGER_SOC64_H_

void sysmgr_pinmux_init(void);
void populate_sysmgr_fpgaintf_module(void);
void populate_sysmgr_pinmux(void);
void sysmgr_pinmux_table_sel(const u32 **table, unsigned int *table_len);
void sysmgr_pinmux_table_ctrl(const u32 **table, unsigned int *table_len);
void sysmgr_pinmux_table_fpga(const u32 **table, unsigned int *table_len);
void sysmgr_pinmux_table_delay(const u32 **table, unsigned int *table_len);

#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGPINMUX	BIT(0)
#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGIO	BIT(1)
#define SYSMGR_ECC_OCRAM_EN	BIT(0)
#define SYSMGR_ECC_OCRAM_SERR	BIT(3)
#define SYSMGR_ECC_OCRAM_DERR	BIT(4)

#define SYSMGR_FPGACONFIG_FPGA_COMPLETE		BIT(0)
#define SYSMGR_FPGACONFIG_EARLY_USERMODE	BIT(1)
#define SYSMGR_FPGACONFIG_READY_MASK	(SYSMGR_FPGACONFIG_FPGA_COMPLETE | \
					 SYSMGR_FPGACONFIG_EARLY_USERMODE)

#define SYSMGR_FPGAINTF_USEFPGA	0x1
#define SYSMGR_FPGAINTF_NAND	BIT(4)
#define SYSMGR_FPGAINTF_SDMMC	BIT(8)
#define SYSMGR_FPGAINTF_SPIM0	BIT(16)
#define SYSMGR_FPGAINTF_SPIM1	BIT(24)
#define SYSMGR_FPGAINTF_EMAC0	BIT(0)
#define SYSMGR_FPGAINTF_EMAC1	BIT(8)
#define SYSMGR_FPGAINTF_EMAC2	BIT(16)

#define SYSMGR_SDMMC_SMPLSEL_SHIFT	4
#define SYSMGR_SDMMC_DRVSEL_SHIFT	0

/* EMAC Group Bit definitions */
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII	0x0
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII		0x1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII		0x2

#define SYSMGR_EMACGRP_CTRL_PHYSEL0_LSB			0
#define SYSMGR_EMACGRP_CTRL_PHYSEL1_LSB			2
#define SYSMGR_EMACGRP_CTRL_PHYSEL_MASK			0x3

#define SYSMGR_NOC_H2F_MSK		0x00000001
#define SYSMGR_NOC_LWH2F_MSK		0x00000010
#define SYSMGR_HMC_CLK_STATUS_MSK	0x00000001

#define SYSMGR_DMA_IRQ_NS		0xFF000000
#define SYSMGR_DMA_MGR_NS		0x00010000

#define SYSMGR_DMAPERIPH_ALL_NS		0xFFFFFFFF

#define SYSMGR_WDDBG_PAUSE_ALL_CPU	0x0F0F0F0F

#endif /* _SYSTEM_MANAGER_SOC64_H_ */
