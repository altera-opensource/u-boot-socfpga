/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef	_SOCFPGA_RESET_MANAGER_H_
#define	_SOCFPGA_RESET_MANAGER_H_

#ifndef __ASSEMBLY__
#ifdef TEST_AT_ASIMOV
struct socfpga_reset_manager {
	u32	status;
	u32	ctrl;
	u32	counts;
	u32	padding1;
	u32	mpu_mod_reset;
	u32	per_mod_reset;
	u32	per2_mod_reset;
	u32	brg_mod_reset;
};
#else
struct socfpga_reset_manager {
	volatile uint32_t  stat;
	volatile uint32_t  ramstat;
	volatile uint32_t  miscstat;
	volatile uint32_t  ctrl;
	volatile uint32_t  hdsken;
	volatile uint32_t  hdskreq;
	volatile uint32_t  hdskack;
	volatile uint32_t  counts;
	volatile uint32_t  mpumodrst;
	volatile uint32_t  per0modrst;
	volatile uint32_t  per1modrst;
	volatile uint32_t  brgmodrst;
	volatile uint32_t  sysmodrst;
	volatile uint32_t  coldmodrst;
	volatile uint32_t  nrstmodrst;
	volatile uint32_t  dbgmodrst;
	volatile uint32_t  mpuwarmmask;
	volatile uint32_t  per0warmmask;
	volatile uint32_t  per1warmmask;
	volatile uint32_t  brgwarmmask;
	volatile uint32_t  syswarmmask;
	volatile uint32_t  nrstwarmmask;
	volatile uint32_t  l3warmmask;
	volatile uint32_t  tststa;
	volatile uint32_t  tstscratch;
	volatile uint32_t  hdsktimeout;
	volatile uint32_t  hmcintr;
	volatile uint32_t  hmcintren;
	volatile uint32_t  hmcintrens;
	volatile uint32_t  hmcintrenr;
	volatile uint32_t  hmcgpout;
	volatile uint32_t  hmcgpin;
};
#endif
#endif /* __ASSEMBLY__ */

#define ALT_RSTMGR_CTL_SWWARMRSTREQ_SET_MSK	0x00000002
#define ALT_RSTMGR_PER0MODRST_EMAC0_SET_MSK	0x00000001
#define ALT_RSTMGR_PER0MODRST_EMAC1_SET_MSK	0x00000002
#define ALT_RSTMGR_PER0MODRST_EMAC2_SET_MSK	0x00000004
#define ALT_RSTMGR_PER0MODRST_USB0_SET_MSK	0x00000008
#define ALT_RSTMGR_PER0MODRST_USB1_SET_MSK	0x00000010
#define ALT_RSTMGR_PER0MODRST_NAND_SET_MSK	0x00000020
#define ALT_RSTMGR_PER0MODRST_QSPI_SET_MSK	0x00000040
#define ALT_RSTMGR_PER0MODRST_SDMMC_SET_MSK	0x00000080
#define ALT_RSTMGR_PER0MODRST_EMACECC0_SET_MSK	0x00000100
#define ALT_RSTMGR_PER0MODRST_EMACECC1_SET_MSK	0x00000200
#define ALT_RSTMGR_PER0MODRST_EMACECC2_SET_MSK	0x00000400
#define ALT_RSTMGR_PER0MODRST_USBECC0_SET_MSK	0x00000800
#define ALT_RSTMGR_PER0MODRST_USBECC1_SET_MSK	0x00001000
#define ALT_RSTMGR_PER0MODRST_NANDECC_SET_MSK	0x00002000
#define ALT_RSTMGR_PER0MODRST_QSPIECC_SET_MSK	0x00004000
#define ALT_RSTMGR_PER0MODRST_SDMMCECC_SET_MSK	0x00008000
#define ALT_RSTMGR_PER0MODRST_DMA_SET_MSK	0x00010000
#define ALT_RSTMGR_PER0MODRST_SPIM0_SET_MSK	0x00020000
#define ALT_RSTMGR_PER0MODRST_SPIM1_SET_MSK	0x00040000
#define ALT_RSTMGR_PER0MODRST_SPIS0_SET_MSK	0x00080000
#define ALT_RSTMGR_PER0MODRST_SPIS1_SET_MSK	0x00100000
#define ALT_RSTMGR_PER0MODRST_DMAECC_SET_MSK	0x00200000
#define ALT_RSTMGR_PER0MODRST_EMACPTP_SET_MSK	0x00400000
#define ALT_RSTMGR_PER0MODRST_DMAIF0_SET_MSK	0x01000000
#define ALT_RSTMGR_PER0MODRST_DMAIF1_SET_MSK	0x02000000
#define ALT_RSTMGR_PER0MODRST_DMAIF2_SET_MSK	0x04000000
#define ALT_RSTMGR_PER0MODRST_DMAIF3_SET_MSK	0x08000000
#define ALT_RSTMGR_PER0MODRST_DMAIF4_SET_MSK	0x10000000
#define ALT_RSTMGR_PER0MODRST_DMAIF5_SET_MSK	0x20000000
#define ALT_RSTMGR_PER0MODRST_DMAIF6_SET_MSK	0x40000000
#define ALT_RSTMGR_PER0MODRST_DMAIF7_SET_MSK	0x80000000

#define ALT_RSTMGR_PER1MODRST_WD0_SET_MSK	0x00000001
#define ALT_RSTMGR_PER1MODRST_WD1_SET_MSK	0x00000002
#define ALT_RSTMGR_PER1MODRST_L4SYSTMR0_SET_MSK	0x00000004
#define ALT_RSTMGR_PER1MODRST_L4SYSTMR1_SET_MSK	0x00000008
#define ALT_RSTMGR_PER1MODRST_SPTMR0_SET_MSK	0x00000010
#define ALT_RSTMGR_PER1MODRST_SPTMR1_SET_MSK	0x00000020
#define ALT_RSTMGR_PER1MODRST_I2C0_SET_MSK	0x00000100
#define ALT_RSTMGR_PER1MODRST_I2C1_SET_MSK	0x00000200
#define ALT_RSTMGR_PER1MODRST_I2C2_SET_MSK	0x00000400
#define ALT_RSTMGR_PER1MODRST_I2C3_SET_MSK	0x00000800
#define ALT_RSTMGR_PER1MODRST_I2C4_SET_MSK	0x00001000
#define ALT_RSTMGR_PER1MODRST_UART0_SET_MSK	0x00010000
#define ALT_RSTMGR_PER1MODRST_UART1_SET_MSK	0x00020000
#define ALT_RSTMGR_PER1MODRST_GPIO0_SET_MSK	0x01000000
#define ALT_RSTMGR_PER1MODRST_GPIO1_SET_MSK	0x02000000
#define ALT_RSTMGR_PER1MODRST_GPIO2_SET_MSK	0x04000000

#define ALT_RSTMGR_BRGMODRST_H2F_SET_MSK	0x00000001
#define ALT_RSTMGR_BRGMODRST_LWH2F_SET_MSK	0x00000002
#define ALT_RSTMGR_BRGMODRST_F2H_SET_MSK	0x00000004
#define ALT_RSTMGR_BRGMODRST_F2SSDRAM0_SET_MSK	0x00000008
#define ALT_RSTMGR_BRGMODRST_F2SSDRAM1_SET_MSK	0x00000010
#define ALT_RSTMGR_BRGMODRST_F2SSDRAM2_SET_MSK	0x00000020
#define ALT_RSTMGR_BRGMODRST_DDRSCH_SET_MSK	0x00000040

#define ALT_RSTMGR_HDSKEN_SDRSELFREFEN_SET_MSK	0x00000001
#define ALT_RSTMGR_HDSKEN_FPGAMGRHSEN_SET_MSK	0x00000002
#define ALT_RSTMGR_HDSKEN_FPGAHSEN_SET_MSK	0x00000004
#define ALT_RSTMGR_HDSKEN_ETRSTALLEN_SET_MSK	0x00000008


#ifndef __ASSEMBLY__
void reset_deassert_noc_ddr_scheduler(void);
void watchdog_disable(void);
int is_wdt_in_reset(void);
void emac_manage_reset(ulong emacbase, uint state);
void reset_assert_all_bridges(void);
int reset_deassert_bridges_handoff(void);
void reset_assert_all_peripherals_except_l4wd0_l4timer0(void);
void reset_assert_uart(void);
void reset_deassert_uart(void);
void reset_deassert_dedicated_peripherals(void);
void reset_deassert_shared_connected_peripherals(void);
void reset_deassert_fpga_connected_peripherals(void);
void reset_assert_shared_connected_peripherals(void);
void reset_assert_fpga_connected_peripherals(void);
void reset_deassert_osc1wd0(void);
#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_RESET_MANAGER_H_ */

