/*
 * Copyright (C) 2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef	_FIREWALL_S10_
#define	_FIREWALL_S10_

struct socfpga_firwall_l4_per {
	u32	nand;		/* 0x00 */
	u32	nand_data;
	u32	_pad_0x8;
	u32	usb0;
	u32	usb1;		/* 0x10 */
	u32	_pad_0x14;;
	u32	_pad_0x18;
	u32	spim0;
	u32	spim1;		/* 0x20 */
	u32	spis0;
	u32	spis1;
	u32	emac0;
	u32	emac1;		/* 0x30 */
	u32	emac2;
	u32	_pad_0x38;
	u32	_pad_0x3c;
	u32	sdmmc;		/* 0x40 */
	u32	gpio0;
	u32	gpio1;
	u32	_pad_0x4c;
	u32	i2c0;		/* 0x50 */
	u32	i2c1;
	u32	i2c2;
	u32	i2c3;
	u32	i2c4;		/* 0x60 */
	u32	timer0;
	u32	timer1;
	u32	uart0;
	u32	uart1;		/* 0x70 */
};

struct socfpga_firwall_l4_sys {
	u32	_pad_0x00;		/* 0x00 */
	u32	_pad_0x04;
	u32	dma_ecc;
	u32	emac0rx_ecc;
	u32	emac0tx_ecc;		/* 0x10 */
	u32	emac1rx_ecc;
	u32	emac1tx_ecc;
	u32	emac2rx_ecc;
	u32	emac2tx_ecc;		/* 0x20 */
	u32	_pad_0x24;
	u32	_pad_0x28;
	u32	nand_ecc;
	u32	nand_read_ecc;		/* 0x30 */
	u32	nand_write_ecc;;
	u32	ocram_ecc;
	u32	_pad_0x3c;
	u32	sdmmc_ecc;		/* 0x40 */
	u32	usb0_ecc;
	u32	usb1_ecc;
	u32	clock_manager;
	u32	_pad_0x50;		/* 0x50 */
	u32	io_manager;
	u32	reset_manager;
	u32	system_manager;
	u32	osc0_timer;		/* 0x60 */
	u32	osc1_timer;
	u32	watchdog0;
	u32	watchdog1;
	u32	watchdog2;		/* 0x70 */
	u32	watchdog3;
};

#define FIREWALL_L4_DISABLE_ALL		(BIT(0) | BIT(24) | BIT(16))
#define FIREWALL_BRIDGE_DISABLE_ALL	(~0)

#define CCU_CPU0_MPRT_ADMASK_MEM_RAM0_ADDR	0xf7004688
#define CCU_IOM_MPRT_ADMASK_MEM_RAM0_ADDR	0xf7018628

#define CCU_ADMASK_P_MASK			(BIT(0))
#define CCU_ADMASK_NS_MASK			(BIT(1))

#endif /* _FIREWALL_S10_ */
