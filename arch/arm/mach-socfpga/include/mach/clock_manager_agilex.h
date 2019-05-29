/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation <www.intel.com>
 */

#ifndef	_CLOCK_MANAGER_AGILEX_
#define	_CLOCK_MANAGER_AGILEX_

#define CM_REG_READL(plat, reg)				\
		readl(plat->regs + (reg))

#define CM_REG_WRITEL(plat, data, reg)			\
		writel(data, plat->regs + (reg))

#define CM_REG_CLRBITS(plat, reg, clear)		\
		clrbits_le32(plat->regs + (reg), (clear))

#define CM_REG_SETBITS(plat, reg, set)			\
		setbits_le32(plat->regs + (reg), (set))

#define CM_REG_CLRSETBITS(plat, reg, clear, set)	\
		clrsetbits_le32(plat->regs + (reg), (clear), (set))

const unsigned int cm_get_osc_clk_hz(void);
const unsigned int cm_get_intosc_clk_hz(void);
const unsigned int cm_get_fpga_clk_hz(void);

unsigned long cm_get_mpu_clk_hz(void);

#define CLKMGR_EOSC1_HZ		25000000
#define CLKMGR_INTOSC_HZ	400000000
#define CLKMGR_FPGA_CLK_HZ	50000000

/* Clock configuration accessors */
const struct cm_config * const cm_get_default_config(void);

struct cm_config {
	/* main group */
	u32 main_pll_mpuclk;
	u32 main_pll_nocclk;
	u32 main_pll_nocdiv;
	u32 main_pll_pllglob;
	u32 main_pll_fdbck;
	u32 main_pll_pllc0;
	u32 main_pll_pllc1;
	u32 main_pll_pllc2;
	u32 main_pll_pllc3;
	u32 main_pll_pllm;

	/* peripheral group */
	u32 per_pll_emacctl;
	u32 per_pll_gpiodiv;
	u32 per_pll_pllglob;
	u32 per_pll_fdbck;
	u32 per_pll_pllc0;
	u32 per_pll_pllc1;
	u32 per_pll_pllc2;
	u32 per_pll_pllc3;
	u32 per_pll_pllm;

	/* altera group */
	u32 alt_emacactr;
	u32 alt_emacbctr;
	u32 alt_emacptpctr;
	u32 alt_gpiodbctr;
	u32 alt_sdmmcctr;
	u32 alt_s2fuser0ctr;
	u32 alt_s2fuser1ctr;
	u32 alt_psirefctr;

	/* incoming clock */
	u32 hps_osc_clk_hz;
	u32 fpga_clk_hz;
	u32 spare[3];
};

/* Register access with structure */
struct socfpga_clock_manager_main_pll {
	u32 en;
	u32 ens;
	u32 enr;
	u32 bypass;
	u32 bypasss;
	u32 bypassr;
	u32 mpuclk;
	u32 nocclk;
	u32 nocdiv;
	u32 pllglob;
	u32 fdbck;
	u32 mem;
	u32 memstat;
	u32 pllc0;
	u32 pllc1;
	u32 vcocalib;
	u32 pllc2;
	u32 pllc3;
	u32 pllm;
	u32 fhop;
	u32 ssc;
	u32 lostlock;
};

struct socfpga_clock_manager_per_pll {
	u32 en;
	u32 ens;
	u32 enr;
	u32 bypass;
	u32 bypasss;
	u32 bypassr;
	u32 emacctl;
	u32 gpiodiv;
	u32 pllglob;
	u32 fdbck;
	u32 mem;
	u32 memstat;
	u32 pllc0;
	u32 pllc1;
	u32 vcocalib;
	u32 pllc2;
	u32 pllc3;
	u32 pllm;
	u32 fhop;
	u32 ssc;
	u32 lostlock;
};

struct socfpga_clock_manager_alt_pll {
	u32 jtag;
	u32 emacactr;
	u32 emacbctr;
	u32 emacptpctr;
	u32 gpiodbctr;
	u32 sdmmcctr;
	u32 s2fuser0ctr;
	u32 s2fuser1ctr;
	u32 psirefctr;
	u32 extcntrst;
};

struct socfpga_clock_manager {
	u32 ctrl;
	u32 stat;
	u32 testioctrl;
	u32 intrgen;
	u32 intrmsk;
	u32 intrclr;
	u32 intrsts;
	u32 intrstk;
	u32 intrraw;
	struct socfpga_clock_manager_main_pll main_pll;
	struct socfpga_clock_manager_per_pll per_pll;
	struct socfpga_clock_manager_alt_pll alt_pll;
};

/* Register access with macro functions */
/* Clock Manager registers */
#define CM_REG_CTRL				0
#define CM_REG_STAT				4
#define CM_REG_TESTIOCTRL			8
#define CM_REG_INTRGEN				0x0c
#define CM_REG_INTRMSK				0x10
#define CM_REG_INTRCLR				0x14
#define CM_REG_INTRSTS				0x18
#define CM_REG_INTRSTK				0x1c
#define CM_REG_INTRRAW				0x20

/* Clock Manager Main PPL group registers */
#define CM_MAINPLL_REG_EN			0x24
#define CM_MAINPLL_REG_ENS			0x28
#define CM_MAINPLL_REG_ENR			0x2c
#define CM_MAINPLL_REG_BYPASS			0x30
#define CM_MAINPLL_REG_BYPASSS			0x34
#define CM_MAINPLL_REG_BYPASSR			0x38
#define CM_MAINPLL_REG_MPUCLK			0x3c
#define CM_MAINPLL_REG_NOCCLK			0x40
#define CM_MAINPLL_REG_NOCDIV			0x44
#define CM_MAINPLL_REG_PLLGLOB			0x48
#define CM_MAINPLL_REG_FDBCK			0x4c
#define CM_MAINPLL_REG_MEM			0x50
#define CM_MAINPLL_REG_MEMSTAT			0x54
#define CM_MAINPLL_REG_PLLC0			0x58
#define CM_MAINPLL_REG_PLLC1			0x5c
#define CM_MAINPLL_REG_VCOCALIB			0x60
#define CM_MAINPLL_REG_PLLC2			0x64
#define CM_MAINPLL_REG_PLLC3			0x68
#define CM_MAINPLL_REG_PLLM			0x6c
#define CM_MAINPLL_REG_FHOP			0x70
#define CM_MAINPLL_REG_SSC			0x74
#define CM_MAINPLL_REG_LOSTLOCK			0x78

/* Clock Manager Peripheral PPL group registers */
#define CM_PERPLL_REG_EN			0x7c
#define CM_PERPLL_REG_ENS			0x80
#define CM_PERPLL_REG_ENR			0x84
#define CM_PERPLL_REG_BYPASS			0x88
#define CM_PERPLL_REG_BYPASSS			0x8c
#define CM_PERPLL_REG_BYPASSR			0x90
#define CM_PERPLL_REG_EMACCTL			0x94
#define CM_PERPLL_REG_GPIODIV			0x98
#define CM_PERPLL_REG_PLLGLOB			0x9c
#define CM_PERPLL_REG_FDBCK			0xa0
#define CM_PERPLL_REG_MEM			0xa4
#define CM_PERPLL_REG_MEMSTAT			0xa8
#define CM_PERPLL_REG_PLLC0			0xac
#define CM_PERPLL_REG_PLLC1			0xb0
#define CM_PERPLL_REG_VCOCALIB			0xb4
#define CM_PERPLL_REG_PLLC2			0xb8
#define CM_PERPLL_REG_PLLC3			0xbc
#define CM_PERPLL_REG_PLLM			0xc0
#define CM_PERPLL_REG_FHOP			0xc4
#define CM_PERPLL_REG_SSC			0xc8
#define CM_PERPLL_REG_LOSTLOCK			0xcc

/* Clock Manager Altera group registers */
#define CM_ALTERA_REG_JTAG			0xd0
#define CM_ALTERA_REG_EMACACTR			0xd4
#define CM_ALTERA_REG_EMACBCTR			0xd8
#define CM_ALTERA_REG_EMACPTPCTR		0xdc
#define CM_ALTERA_REG_GPIODBCTR			0xe0
#define CM_ALTERA_REG_SDMMCCTR			0xe4
#define CM_ALTERA_REG_S2FUSER0CTR		0xe8
#define CM_ALTERA_REG_S2FUSER1CTR		0xec
#define CM_ALTERA_REG_PSIREFCTR			0xf0
#define CM_ALTERA_REG_EXTCNTRST			0xf4

#define CLKMGR_CTRL_BOOTMODE			BIT(0)

#define CLKMGR_STAT_BUSY			BIT(0)
#define CLKMGR_STAT_MAINPLL_LOCKED		BIT(8)
#define CLKMGR_STAT_MAIN_TRANS			BIT(9)
#define CLKMGR_STAT_PERPLL_LOCKED		BIT(16)
#define CLKMGR_STAT_PERF_TRANS			BIT(17)
#define CLKMGR_STAT_BOOTMODE			BIT(24)
#define CLKMGR_STAT_BOOTCLKSRC			BIT(25)

#define CLKMGR_STAT_ALLPLL_LOCKED_MASK		\
		(CLKMGR_STAT_MAINPLL_LOCKED | CLKMGR_STAT_PERPLL_LOCKED)

#define CLKMGR_INTER_MAINPLLLOCKED_MASK		0x00000001
#define CLKMGR_INTER_PERPLLLOCKED_MASK		0x00000002
#define CLKMGR_INTER_MAINPLLLOST_MASK		0x00000004
#define CLKMGR_INTER_PERPLLLOST_MASK		0x00000008

#define CLKMGR_CLKSRC_MASK			GENMASK(18, 16)
#define CLKMGR_CLKSRC_OFFSET			16
#define CLKMGR_CLKSRC_MAIN			0
#define CLKMGR_CLKSRC_PER			1
#define CLKMGR_CLKSRC_OSC1			2
#define CLKMGR_CLKSRC_INTOSC			3
#define CLKMGR_CLKSRC_FPGA			4
#define CLKMGR_CLKCNT_MSK			GENMASK(10, 0)

#define CLKMGR_BYPASS_MAINPLL_ALL		0x7
#define CLKMGR_BYPASS_PERPLL_ALL		0x7f

#define CLKMGR_NOCDIV_L4MAIN_OFFSET		0
#define CLKMGR_NOCDIV_L4MPCLK_OFFSET		8
#define CLKMGR_NOCDIV_L4SPCLK_OFFSET		16
#define CLKMGR_NOCDIV_CSATCLK_OFFSET		24
#define CLKMGR_NOCDIV_CSTRACECLK_OFFSET		26
#define CLKMGR_NOCDIV_CSPDBGCLK_OFFSET		28
#define CLKMGR_NOCDIV_DIVIDER_MASK		0x3

#define CLKMGR_PLLGLOB_PD_MASK				BIT(0)
#define CLKMGR_PLLGLOB_RST_MASK				BIT(1)
#define CLKMGR_PLLGLOB_AREFCLKDIV_MASK			GENMASK(11, 8)
#define CLKMGR_PLLGLOB_DREFCLKDIV_MASK			GENMASK(13, 12)
#define CLKMGR_PLLGLOB_REFCLKDIV_MASK			GENMASK(13, 8)
#define CLKMGR_PLLGLOB_MODCLKDIV_MASK			GENMASK(24, 27)
#define CLKMGR_PLLGLOB_AREFCLKDIV_OFFSET		8
#define CLKMGR_PLLGLOB_DREFCLKDIV_OFFSET		12
#define CLKMGR_PLLGLOB_REFCLKDIV_OFFSET			8
#define CLKMGR_PLLGLOB_MODCLKDIV_OFFSET			24
#define CLKMGR_PLLGLOB_VCO_PSRC_MASK			GENMASK(17, 16)
#define CLKMGR_PLLGLOB_VCO_PSRC_OFFSET			16
#define CLKMGR_PLLGLOB_CLR_LOSTLOCK_BYPASS_MASK		BIT(29)

#define CLKMGR_VCO_PSRC_EOSC1			0
#define CLKMGR_VCO_PSRC_INTOSC			1
#define CLKMGR_VCO_PSRC_F2S			2

#define CLKMGR_MEM_REQ_SET_MSK			BIT(24)
#define CLKMGR_MEM_WR_SET_MSK			BIT(25)
#define CLKMGR_MEM_ERR_MSK			BIT(26)
#define CLKMGR_MEM_WDAT_LSB_OFFSET		16
#define CLKMGR_MEM_ADDR_MASK			GENMASK(15, 0)
#define CLKMGR_MEM_ADDR_START			0x00004000

#define CLKMGR_PLLCX_EN_SET_MSK			BIT(27)
#define CLKMGR_PLLCX_MUTE_SET_MSK		BIT(28)

#define CLKMGR_VCOCALIB_MSCNT_MASK		GENMASK(23, 16)
#define CLKMGR_VCOCALIB_MSCNT_OFFSET		16
#define CLKMGR_VCOCALIB_HSCNT_MASK		GENMASK(9, 0)
#define CLKMGR_VCOCALIB_MSCNT_CONST		100
#define CLKMGR_VCOCALIB_HSCNT_CONST		4

#define CLKMGR_PLLM_MDIV_MASK			GENMASK(9, 0)

#define CLKMGR_LOSTLOCK_SET_MASK		BIT(0)

#define CLKMGR_PERPLLGRP_EN_SDMMCCLK_MASK		BIT(5)
#define CLKMGR_PERPLLGRP_EMACCTL_EMAC0SELB_OFFSET	26
#define CLKMGR_PERPLLGRP_EMACCTL_EMAC0SELB_MASK		BIT(26)
#define CLKMGR_PERPLLGRP_EMACCTL_EMAC1SELB_OFFSET	27
#define CLKMGR_PERPLLGRP_EMACCTL_EMAC1SELB_MASK		BIT(27)
#define CLKMGR_PERPLLGRP_EMACCTL_EMAC2SELB_OFFSET	28
#define CLKMGR_PERPLLGRP_EMACCTL_EMAC2SELB_MASK		BIT(28)

#define CLKMGR_ALT_EMACCTR_SRC_OFFSET		16
#define CLKMGR_ALT_EMACCTR_SRC_MASK		GENMASK(18, 16)
#define CLKMGR_ALT_EMACCTR_CNT_OFFSET		0
#define CLKMGR_ALT_EMACCTR_CNT_MASK		GENMASK(10, 0)

#define CLKMGR_ALT_EXTCNTRST_EMACACNTRST	BIT(0)
#define CLKMGR_ALT_EXTCNTRST_EMACBCNTRST	BIT(1)
#define CLKMGR_ALT_EXTCNTRST_EMACPTPCNTRST	BIT(2)
#define CLKMGR_ALT_EXTCNTRST_GPIODBCNTRST	BIT(3)
#define CLKMGR_ALT_EXTCNTRST_SDMMCCNTRST	BIT(4)
#define CLKMGR_ALT_EXTCNTRST_S2FUSER0CNTRST	BIT(5)
#define CLKMGR_ALT_EXTCNTRST_S2FUSER1CNTRST	BIT(6)
#define CLKMGR_ALT_EXTCNTRST_PSIREFCNTRST	BIT(7)
#define CLKMGR_ALT_EXTCNTRST_ALLCNTRST		\
		(CLKMGR_ALT_EXTCNTRST_EMACACNTRST | \
		 CLKMGR_ALT_EXTCNTRST_EMACBCNTRST | \
		 CLKMGR_ALT_EXTCNTRST_EMACPTPCNTRST | \
		 CLKMGR_ALT_EXTCNTRST_GPIODBCNTRST | \
		 CLKMGR_ALT_EXTCNTRST_SDMMCCNTRST | \
		 CLKMGR_ALT_EXTCNTRST_S2FUSER0CNTRST | \
		 CLKMGR_ALT_EXTCNTRST_S2FUSER1CNTRST | \
		 CLKMGR_ALT_EXTCNTRST_PSIREFCNTRST)
#endif /* _CLOCK_MANAGER_AGILEX_ */
