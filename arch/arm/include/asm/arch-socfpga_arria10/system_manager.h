/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef	_SOCFPGA_SYSTEM_MANAGER_H_
#define	_SOCFPGA_SYSTEM_MANAGER_H_


/* FPGA interface group */
#define SYSMGR_FPGAINTF_MODULE		(SOCFPGA_SYSMGR_ADDRESS + 0x28)
/* EMAC interface selection */
#define CONFIG_SYSMGR_EMAC0_CTRL	(SOCFPGA_SYSMGR_ADDRESS + 0x44)
#define CONFIG_SYSMGR_EMAC1_CTRL	(SOCFPGA_SYSMGR_ADDRESS + 0x48)
#define CONFIG_SYSMGR_EMAC2_CTRL	(SOCFPGA_SYSMGR_ADDRESS + 0x4c)

/* Preloader handoff to bootloader register */
#define SYSMGR_ISWGRP_HANDOFF0		(SOCFPGA_SYSMGR_ADDRESS + 0x80)
#define SYSMGR_ISWGRP_HANDOFF1		(SOCFPGA_SYSMGR_ADDRESS + 0x84)
#define SYSMGR_ISWGRP_HANDOFF2		(SOCFPGA_SYSMGR_ADDRESS + 0x88)
#define SYSMGR_ISWGRP_HANDOFF3		(SOCFPGA_SYSMGR_ADDRESS + 0x8C)
#define SYSMGR_ISWGRP_HANDOFF4		(SOCFPGA_SYSMGR_ADDRESS + 0x90)
#define SYSMGR_ISWGRP_HANDOFF5		(SOCFPGA_SYSMGR_ADDRESS + 0x94)
#define SYSMGR_ISWGRP_HANDOFF6		(SOCFPGA_SYSMGR_ADDRESS + 0x98)
#define SYSMGR_ISWGRP_HANDOFF7		(SOCFPGA_SYSMGR_ADDRESS + 0x9C)

#define ISWGRP_HANDOFF_AXIBRIDGE	SYSMGR_ISWGRP_HANDOFF0
#define ISWGRP_HANDOFF_L3REMAP		SYSMGR_ISWGRP_HANDOFF1
#define ISWGRP_HANDOFF_FPGAINTF		SYSMGR_ISWGRP_HANDOFF2
#define ISWGRP_HANDOFF_FPGA2SDR		SYSMGR_ISWGRP_HANDOFF3

/* pin mux */
#define SYSMGR_PINMUXGRP		(SOCFPGA_SYSMGR_ADDRESS + 0x400)
#define SYSMGR_PINMUXGRP_NANDUSEFPGA	(SYSMGR_PINMUXGRP + 0x2F0)
#define SYSMGR_PINMUXGRP_EMAC1USEFPGA	(SYSMGR_PINMUXGRP + 0x2F8)
#define SYSMGR_PINMUXGRP_SDMMCUSEFPGA	(SYSMGR_PINMUXGRP + 0x308)
#define SYSMGR_PINMUXGRP_EMAC0USEFPGA	(SYSMGR_PINMUXGRP + 0x314)
#define SYSMGR_PINMUXGRP_SPIM1USEFPGA	(SYSMGR_PINMUXGRP + 0x330)
#define SYSMGR_PINMUXGRP_SPIM0USEFPGA	(SYSMGR_PINMUXGRP + 0x338)

/* bit fields */
#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGPINMUX		(1<<0)
#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGIO		(1<<1)
#define SYSMGR_ECC_OCRAM_EN		(1<<0)
#define SYSMGR_ECC_OCRAM_SERR		(1<<3)
#define SYSMGR_ECC_OCRAM_DERR		(1<<4)
#define SYSMGR_FPGAINTF_USEFPGA		0x1
#define SYSMGR_FPGAINTF_SPIM0		(1<<0)
#define SYSMGR_FPGAINTF_SPIM1		(1<<1)
#define SYSMGR_FPGAINTF_EMAC0		(1<<2)
#define SYSMGR_FPGAINTF_EMAC1		(1<<3)
#define SYSMGR_FPGAINTF_NAND		(1<<4)
#define SYSMGR_FPGAINTF_SDMMC		(1<<5)

/* Enumeration: sysmgr::emacgrp::ctrl::physel::enum                        */
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII 0x0
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII 0x1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII 0x2
#define SYSMGR_EMACGRP_CTRL_PHYSEL0_LSB 0
#define SYSMGR_EMACGRP_CTRL_PHYSEL1_LSB 2
#define SYSMGR_EMACGRP_CTRL_PHYSEL_MASK 0x00000003

/********************************************************************/
/* Baum start here, need to housekeep old one */
/********************************************************************/

#ifndef __ASSEMBLY__
#ifdef TEST_AT_ASIMOV
struct socfpga_system_manager {
	/* System Manager Module */
	u32	siliconid1;			/* 0x00 */
	u32	siliconid2;
	u32	_pad_0x8_0xf[2];
	u32	wddbg;				/* 0x10 */
	u32	bootinfo;
	u32	hpsinfo;
	u32	parityinj;
	/* FPGA Interface Group */
	u32	fpgaintfgrp_gbl;		/* 0x20 */
	u32	fpgaintfgrp_indiv;
	u32	fpgaintfgrp_module;
	u32	_pad_0x2c_0x2f;
	/* Scan Manager Group */
	u32	scanmgrgrp_ctrl;		/* 0x30 */
	u32	_pad_0x34_0x3f[3];
	/* Freeze Control Group */
	u32	frzctrl_vioctrl;		/* 0x40 */
	u32	_pad_0x44_0x4f[3];
	u32	frzctrl_hioctrl;		/* 0x50 */
	u32	frzctrl_src;
	u32	frzctrl_hwctrl;
	u32	_pad_0x5c_0x5f;
	/* EMAC Group */
	u32	emacgrp_ctrl;			/* 0x60 */
	u32	emacgrp_l3master;
	u32	_pad_0x68_0x6f[2];
	/* DMA Controller Group */
	u32	dmagrp_ctrl;			/* 0x70 */
	u32	dmagrp_persecurity;
	u32	_pad_0x78_0x7f[2];
	/* Preloader (initial software) Group */
	u32	iswgrp_handoff[8];		/* 0x80 */
	u32	_pad_0xa0_0xbf[8];		/* 0xa0 */
	/* Boot ROM Code Register Group */
	u32	romcodegrp_ctrl;		/* 0xc0 */
	u32	romcodegrp_cpu1startaddr;
	u32	romcodegrp_initswstate;
	u32	romcodegrp_initswlastld;
	u32	romcodegrp_bootromswstate;	/* 0xd0 */
	u32	__pad_0xd4_0xdf[3];
	/* Warm Boot from On-Chip RAM Group */
	u32	romcodegrp_warmramgrp_enable;	/* 0xe0 */
	u32	romcodegrp_warmramgrp_datastart;
	u32	romcodegrp_warmramgrp_length;
	u32	romcodegrp_warmramgrp_execution;
	u32	romcodegrp_warmramgrp_crc;	/* 0xf0 */
	u32	__pad_0xf4_0xff[3];
	/* Boot ROM Hardware Register Group */
	u32	romhwgrp_ctrl;			/* 0x100 */
	u32	_pad_0x104_0x107;
	/* SDMMC Controller Group */
	u32	sdmmcgrp_ctrl;
	u32	sdmmcgrp_l3master;
	/* NAND Flash Controller Register Group */
	u32	nandgrp_bootstrap;		/* 0x110 */
	u32	nandgrp_l3master;
	/* USB Controller Group */
	u32	usbgrp_l3master;
	u32	_pad_0x11c_0x13f[9];
	/* ECC Management Register Group */
	u32	eccgrp_l2;			/* 0x140 */
	u32	eccgrp_ocram;
	u32	eccgrp_usb0;
	u32	eccgrp_usb1;
	u32	eccgrp_emac0;			/* 0x150 */
	u32	eccgrp_emac1;
	u32	eccgrp_dma;
	u32	eccgrp_can0;
	u32	eccgrp_can1;			/* 0x160 */
	u32	eccgrp_nand;
	u32	eccgrp_qspi;
	u32	eccgrp_sdmmc;
	u32	_pad_0x170_0x3ff[164];
	/* Pin Mux Control Group */
	u32	emacio[20];			/* 0x400 */
	u32	flashio[12];			/* 0x450 */
	u32	generalio[28];			/* 0x480 */
	u32	_pad_0x4f0_0x4ff[4];
	u32	mixed1io[22];			/* 0x500 */
	u32	mixed2io[8];			/* 0x558 */
	u32	gplinmux[23];			/* 0x578 */
	u32	gplmux[71];			/* 0x5d4 */
	u32	nandusefpga;			/* 0x6f0 */
	u32	_pad_0x6f4;
	u32	rgmii1usefpga;			/* 0x6f8 */
	u32	_pad_0x6fc_0x700[2];
	u32	i2c0usefpga;			/* 0x704 */
	u32	sdmmcusefpga;			/* 0x708 */
	u32	_pad_0x70c_0x710[2];
	u32	rgmii0usefpga;			/* 0x714 */
	u32	_pad_0x718_0x720[3];
	u32	i2c3usefpga;			/* 0x724 */
	u32	i2c2usefpga;			/* 0x728 */
	u32	i2c1usefpga;			/* 0x72c */
	u32	spim1usefpga;			/* 0x730 */
	u32	_pad_0x734;
	u32	spim0usefpga;			/* 0x738 */
};
#else
struct socfpga_system_manager {
	volatile uint32_t  siliconid1;
	volatile uint32_t  siliconid2;
	volatile uint32_t  wddbg;
	volatile uint32_t  bootinfo;
	volatile uint32_t  mpu_ctrl_l2_ecc;
	volatile uint32_t  _pad_0x14_0x1f[3];
	volatile uint32_t  dma;
	volatile uint32_t  dma_periph;
	volatile uint32_t  sdmmcgrp_ctrl;
	volatile uint32_t  sdmmc_l3master;
	volatile uint32_t  nand_bootstrap;
	volatile uint32_t  nand_l3master;
	volatile uint32_t  usb0_l3master;
	volatile uint32_t  usb1_l3master;
	volatile uint32_t  emac_global;
	volatile uint32_t  emac0;
	volatile uint32_t  emac1;
	volatile uint32_t  emac2;
	volatile uint32_t  _pad_0x50_0x5f[4];
	volatile uint32_t  fpgaintf_en_global;
	volatile uint32_t  fpgaintf_en_0;
	volatile uint32_t  fpgaintf_en_1;
	volatile uint32_t  fpgaintf_en_2;
	volatile uint32_t  fpgaintf_en_3;
	volatile uint32_t  _pad_0x74_0x7f[3];
	volatile uint32_t  noc_addr_remap_value;
	volatile uint32_t  noc_addr_remap_set;
	volatile uint32_t  noc_addr_remap_clear;
	volatile uint32_t  _pad_0x8c_0x8f;
	volatile uint32_t  ecc_intmask_value;
	volatile uint32_t  ecc_intmask_set;
	volatile uint32_t  ecc_intmask_clr;
	volatile uint32_t  ecc_intstatus_serr;
	volatile uint32_t  ecc_intstatus_derr;
	volatile uint32_t  mpu_status_l2_ecc;
	volatile uint32_t  mpu_clear_l2_ecc;
	volatile uint32_t  mpu_status_l1_parity;
	volatile uint32_t  mpu_clear_l1_parity;
	volatile uint32_t  mpu_set_l1_parity;
	volatile uint32_t  _pad_0xb8_0xbf[2];
	volatile uint32_t  noc_timeout;
	volatile uint32_t  noc_idlereq_set;
	volatile uint32_t  noc_idlereq_clr;
	volatile uint32_t  noc_idlereq_value;
	volatile uint32_t  noc_idleack;
	volatile uint32_t  noc_idlestatus;
	volatile uint32_t  fpga2soc_ctrl;
	volatile uint32_t  _pad_0xdc_0xff[9];
	volatile uint32_t  tsmc_tsel_0;
	volatile uint32_t  tsmc_tsel_1;
	volatile uint32_t  tsmc_tsel_2;
	volatile uint32_t  tsmc_tsel_3;
	volatile uint32_t  _pad_0x110_0x200[60];
	volatile uint32_t  romhw_ctrl;
	volatile uint32_t  romcode_ctrl;
	volatile uint32_t  romcode_qspiresetcommand;
	volatile uint32_t  romcode_initswstate;
	volatile uint32_t  romcode_initswlastld;
	volatile uint32_t  _pad_0x214_0x217;
	volatile uint32_t  warmram_enable;
	volatile uint32_t  warmram_datastart;
	volatile uint32_t  warmram_length;
	volatile uint32_t  warmram_execution;
	volatile uint32_t  warmram_crc;
	volatile uint32_t  _pad_0x22c_0x22f;
	volatile uint32_t  isw_handoff[8];
	volatile uint32_t  romcode_bootromswstate[8];
};
#endif


#endif /* __ASSEMBLY__ */

/* For dedicated IO configuration */
/* Voltage select enums */
#define VOLTAGE_SEL_3V		0x0
#define VOLTAGE_SEL_1P8V	0x1
#define VOLTAGE_SEL_2P5V	0x2

/* Input buffer enable */
#define INPUT_BUF_DISABLE	(0)
#define INPUT_BUF_1P8V		(1)
#define INPUT_BUF_2P5V3V	(2)

/* Weak pull up enable */
#define WK_PU_DISABLE		(0)
#define WK_PU_ENABLE		(1)

/* Pull up slew rate control */
#define PU_SLW_RT_SLOW		(0)
#define PU_SLW_RT_FAST		(1)
#define PU_SLW_RT_DEFAULT	PU_SLW_RT_SLOW

/* Pull down slew rate control */
#define PD_SLW_RT_SLOW		(0)
#define PD_SLW_RT_FAST		(1)
#define PD_SLW_RT_DEFAULT	PD_SLW_RT_SLOW

/* Drive strength control */
#define PU_DRV_STRG_DEFAULT	(0x10)
#define PD_DRV_STRG_DEFAULT	(0x10)

/* bit position */
#define PD_DRV_STRG_LSB		(0)
#define PD_SLW_RT_LSB		(5)
#define PU_DRV_STRG_LSB		(8)
#define PU_SLW_RT_LSB		(13)
#define WK_PU_LSB		(16)
#define INPUT_BUF_LSB		(17)
#define BIAS_TRIM_LSB		(19)
#define VOLTAGE_SEL_LSB		(0)


#define ALT_SYSMGR_NOC_H2F_SET_MSK	0x00000001
#define ALT_SYSMGR_NOC_LWH2F_SET_MSK	0x00000010
#define ALT_SYSMGR_NOC_F2H_SET_MSK	0x00000100
#define ALT_SYSMGR_NOC_F2SDR0_SET_MSK	0x00010000
#define ALT_SYSMGR_NOC_F2SDR1_SET_MSK	0x00100000
#define ALT_SYSMGR_NOC_F2SDR2_SET_MSK	0x01000000
#define ALT_SYSMGR_NOC_TMO_EN_SET_MSK	0x00000001

#define ALT_SYSMGR_ECC_INTSTAT_SERR_OCRAM_SET_MSK    0x00000002
#define ALT_SYSMGR_ECC_INTSTAT_DERR_OCRAM_SET_MSK    0x00000002
#define ALT_SYSMGR_ECC_INT_OCRAM_MSK                 0x00000002
#define ALT_SYSMGR_ECC_INT_DDR0_MSK                  0x00020000

#define SYSMGR_SDMMC_CTRL_SET(smplsel, drvsel)	\
	((drvsel << 0) & 0x7) | ((smplsel << 4) & 0x70)

#endif /* _SOCFPGA_SYSTEM_MANAGER_H_ */
