/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation <www.intel.com>
 */

#ifndef	_SYSTEM_MANAGER_AGILEX_
#define	_SYSTEM_MANAGER_AGILEX_

struct socfpga_system_manager {
	/* System Manager Module */
	u32	siliconid1;			/* 0x00 */
	u32	siliconid2;
	u32	wddbg;
	u32	_pad_0xc;
	u32	mpu_status;			/* 0x10 */
	u32	mpu_ace;
	u32	_pad_0x18_0x1c[2];
	u32	dma;				/* 0x20 */
	u32	dma_periph;
	/* SDMMC Controller Group */
	u32	sdmmcgrp_ctrl;
	u32	sdmmcgrp_l3master;
	/* NAND Flash Controller Register Group */
	u32	nandgrp_bootstrap;		/* 0x30 */
	u32	nandgrp_l3master;
	/* USB Controller Group */
	u32	usb0_l3master;
	u32	usb1_l3master;
	/* EMAC Group */
	u32	emac_gbl;			/* 0x40 */
	u32	emac0;
	u32	emac1;
	u32	emac2;
	u32	emac0_ace;			/* 0x50 */
	u32	emac1_ace;
	u32	emac2_ace;
	u32	nand_axuser;
	u32	_pad_0x60_0x64[2];		/* 0x60 */
	/* FPGA interface Group */
	u32	fpgaintf_en_1;
	u32	fpgaintf_en_2;
	u32	fpgaintf_en_3;			/* 0x70 */
	u32	dma_l3master;
	u32	etr_l3master;
	u32	_pad_0x7c;
	u32	sec_ctrl_slt;			/* 0x80 */
	u32	osc_trim;
	u32	_pad_0x88_0x8c[2];
	/* ECC Group */
	u32	ecc_intmask_value;		/* 0x90 */
	u32	ecc_intmask_set;
	u32	ecc_intmask_clr;
	u32	ecc_intstatus_serr;
	u32	ecc_intstatus_derr;		/* 0xa0 */
	u32	_pad_0xa4_0xac[3];
	u32	noc_addr_remap;			/* 0xb0 */
	u32	hmc_clk;
	u32	io_pa_ctrl;
	u32	_pad_0xbc;
	/* NOC Group */
	u32	noc_timeout;			/* 0xc0 */
	u32	noc_idlereq_set;
	u32	noc_idlereq_clr;
	u32	noc_idlereq_value;
	u32	noc_idleack;			/* 0xd0 */
	u32	noc_idlestatus;
	u32	fpga2soc_ctrl;
	u32	fpga_config;
	u32	iocsrclk_gate;			/* 0xe0 */
	u32	gpo;
	u32	gpi;
	u32	_pad_0xec;
	u32	mpu;				/* 0xf0 */
	u32	sdm_hps_spare;
	u32	hps_sdm_spare;
	u32	_pad_0xfc_0x1fc[65];
	/* Boot scratch register group */
	u32	boot_scratch_cold0;		/* store qspi ref clock */
	u32	boot_scratch_cold1;		/* store osc1 clock freq */
	u32	boot_scratch_cold2;		/* store fpga clock freq */
	u32	boot_scratch_cold3;		/* reserved for customer use */
	u32	boot_scratch_cold4;		/* store PSCI_CPU_ON value */
	u32	boot_scratch_cold5;		/* store PSCI_CPU_ON value */
	u32	boot_scratch_cold6;		/* store VBAR_EL3 value */
	u32	boot_scratch_cold7;		/* store VBAR_EL3 value */
	u32	boot_scratch_cold8;		/* store ECC DBE register */
	u32	boot_scratch_cold9;		/* store ECC DBE address */
	u32	mpfe_config;
	u32	mpfe_status;
	u32	_pad_0x230_0xffc[884];
	/* Pin select and pin control group */
	u32	pinsel0[40];			/* 0x1000 */
	u32	_pad_0x10a0_0x10fc[24];
	u32	pinsel40[8];
	u32	_pad_0x1120_0x112c[4];
	u32	ioctrl0[28];
	u32	_pad_0x11a0_0x11fc[24];
	u32	ioctrl28[20];
	u32	_pad_0x1250_0x12fc[44];
	/* Use FPGA mux */
	u32	rgmii0usefpga;			/* 0x1300 */
	u32	rgmii1usefpga;
	u32	rgmii2usefpga;
	u32	i2c0usefpga;
	u32	i2c1usefpga;
	u32	i2c_emac0_usefpga;
	u32	i2c_emac1_usefpga;
	u32	i2c_emac2_usefpga;
	u32	nandusefpga;
	u32	_pad_0x1324;
	u32	spim0usefpga;
	u32	spim1usefpga;
	u32	spis0usefpga;
	u32	spis1usefpga;
	u32	uart0usefpga;
	u32	uart1usefpga;
	u32	mdio0usefpga;
	u32	mdio1usefpga;
	u32	mdio2usefpga;
	u32	_pad_0x134c;
	u32	jtagusefpga;
	u32	sdmmcusefpga;
	u32	_pad_0x1358_0x13fc[42];
	u32	iodelay0[48];
};

#include <asm/arch/system_manager_soc64.h>

#endif /* _SYSTEM_MANAGER_AGILEX_ */
