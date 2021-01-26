// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2018 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <hang.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/secure.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/smc_api.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/timer.h>
#include <dt-bindings/reset/altr,rst-mgr-s10.h>
#include <exports.h>
#include <linux/iopoll.h>
#include <linux/intel-smc.h>
DECLARE_GLOBAL_DATA_PTR;

/* F2S manager registers */
#define F2SDRAM_SIDEBAND_FLAGINSTATUS0	0x14
#define F2SDRAM_SIDEBAND_FLAGOUTSET0	0x50
#define F2SDRAM_SIDEBAND_FLAGOUTCLR0	0x54

#ifdef CONFIG_TARGET_SOCFPGA_STRATIX10
#define FLAGINSTATUS0_MPFE_NOC_IDLE	(BIT(0) | BIT(4) | BIT(8))
#define FLAGINSTATUS0_MPFE_NOC_IDLEACK	(BIT(1) | BIT(5) | BIT(9))
#define FLAGINSTATUS0_F2S_CMD_EMPTY	(BIT(2) | BIT(6) | BIT(10))
#define FLAGINSTATUS0_F2S_RESP_EMPTY	(BIT(3) | BIT(7) | BIT(11))

#define FLGAOUTSET0_MPFE_NOC_IDLEREQ	(BIT(0) | BIT(3) | BIT(6))
#define FLGAOUTSET0_F2S_EN		(BIT(1) | BIT(4) | BIT(7))
#define FLGAOUTSET0_F2S_FORCE_DRAIN	(BIT(2) | BIT(5) | BIT(8))

#define FLGAOUTCLR0_F2S_IDLEREQ		(BIT(0) | BIT(3) | BIT(6))
#else
#define FLAGINSTATUS0_MPFE_NOC_IDLE	BIT(0)
#define FLAGINSTATUS0_MPFE_NOC_IDLEACK	BIT(1)
#define FLAGINSTATUS0_F2S_CMD_EMPTY	BIT(2)
#define FLAGINSTATUS0_F2S_RESP_EMPTY	BIT(3)

#define FLGAOUTSET0_MPFE_NOC_IDLEREQ	BIT(0)
#define FLGAOUTSET0_F2S_EN		BIT(1)
#define FLGAOUTSET0_F2S_FORCE_DRAIN	BIT(2)

#define FLGAOUTCLR0_F2S_IDLEREQ		BIT(0)
#endif

#define POLL_FOR_ZERO(expr, timeout_ms)		\
	{					\
		int timeout = (timeout_ms);	\
		while ((expr)) {		\
			if (!timeout)		\
				break;		\
			timeout--;		\
			__socfpga_udelay(1000);	\
		}				\
	}

#define POLL_FOR_SET(expr, timeout_ms)		\
	{					\
		int timeout = (timeout_ms);	\
		while (!(expr)) {		\
			if (!timeout)		\
				break;		\
			timeout--;		\
			__socfpga_udelay(1000);	\
		}				\
	}

/* Assert or de-assert SoCFPGA reset manager reset. */
void socfpga_per_reset(u32 reset, int set)
{
	unsigned long reg;

	if (RSTMGR_BANK(reset) == 0)
		reg = RSTMGR_SOC64_MPUMODRST;
	else if (RSTMGR_BANK(reset) == 1)
		reg = RSTMGR_SOC64_PER0MODRST;
	else if (RSTMGR_BANK(reset) == 2)
		reg = RSTMGR_SOC64_PER1MODRST;
	else if (RSTMGR_BANK(reset) == 3)
		reg = RSTMGR_SOC64_BRGMODRST;
	else	/* Invalid reset register, do nothing */
		return;

	if (set)
		setbits_le32(socfpga_get_rstmgr_addr() + reg,
			     1 << RSTMGR_RESET(reset));
	else
		clrbits_le32(socfpga_get_rstmgr_addr() + reg,
			     1 << RSTMGR_RESET(reset));
}

/*
 * Assert reset on every peripheral but L4WD0.
 * Watchdog must be kept intact to prevent glitches
 * and/or hangs.
 */
void socfpga_per_reset_all(void)
{
	const u32 l4wd0 = 1 << RSTMGR_RESET(SOCFPGA_RESET(L4WD0));

	/* disable all except OCP and l4wd0. OCP disable later */
	writel(~(l4wd0 | RSTMGR_PER0MODRST_OCP_MASK),
		      socfpga_get_rstmgr_addr() + RSTMGR_SOC64_PER0MODRST);
	writel(~l4wd0, socfpga_get_rstmgr_addr() + RSTMGR_SOC64_PER0MODRST);
	writel(0xffffffff, socfpga_get_rstmgr_addr() + RSTMGR_SOC64_PER1MODRST);
}

static __always_inline void socfpga_f2s_bridges_reset(int enable)
{
	int timeout_ms = 300;
	u32 empty;

	if (enable) {
		clrbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_BRGMODRST,
			     BRGMODRST_FPGA2SOC_BRIDGES);
		clrbits_le32(SOCFPGA_F2SDRAM_MGR_ADDRESS +
			     F2SDRAM_SIDEBAND_FLAGOUTSET0,
			     FLGAOUTSET0_MPFE_NOC_IDLEREQ);

		POLL_FOR_ZERO((readl(SOCFPGA_F2SDRAM_MGR_ADDRESS +
			      F2SDRAM_SIDEBAND_FLAGINSTATUS0) &
			      FLAGINSTATUS0_MPFE_NOC_IDLEACK), timeout_ms);
		clrbits_le32(SOCFPGA_F2SDRAM_MGR_ADDRESS +
			     F2SDRAM_SIDEBAND_FLAGOUTSET0,
			     FLGAOUTSET0_F2S_FORCE_DRAIN);
		setbits_le32(SOCFPGA_F2SDRAM_MGR_ADDRESS +
			     F2SDRAM_SIDEBAND_FLAGOUTSET0, FLGAOUTSET0_F2S_EN);

		__socfpga_udelay(1); /* wait 1us */
	} else {
		setbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_HDSKEN,
			     RSTMGR_HDSKEN_FPGAHSEN);
		setbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_HDSKREQ,
			     RSTMGR_HDSKREQ_FPGAHSREQ);
		POLL_FOR_SET(readl(socfpga_get_rstmgr_addr() +
			     RSTMGR_SOC64_HDSKACK), timeout_ms);
		clrbits_le32(SOCFPGA_F2SDRAM_MGR_ADDRESS +
			     F2SDRAM_SIDEBAND_FLAGOUTSET0, FLGAOUTSET0_F2S_EN);
		__socfpga_udelay(1);
		setbits_le32(SOCFPGA_F2SDRAM_MGR_ADDRESS +
			     F2SDRAM_SIDEBAND_FLAGOUTSET0,
			     FLGAOUTSET0_F2S_FORCE_DRAIN);
		__socfpga_udelay(1);

		do {
			/*
			 * Read response queue status twice to ensure it is
			 * empty.
			 */
			empty = readl(SOCFPGA_F2SDRAM_MGR_ADDRESS +
				      F2SDRAM_SIDEBAND_FLAGINSTATUS0) &
				      FLAGINSTATUS0_F2S_RESP_EMPTY;
			if (empty) {
				empty = readl(SOCFPGA_F2SDRAM_MGR_ADDRESS +
					      F2SDRAM_SIDEBAND_FLAGINSTATUS0) &
					      FLAGINSTATUS0_F2S_RESP_EMPTY;
				if (empty)
					break;
			}

			timeout_ms--;
			__socfpga_udelay(1000);
		} while (timeout_ms);

		setbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_BRGMODRST,
			     BRGMODRST_FPGA2SOC_BRIDGES &
			     ~RSTMGR_BRGMODRST_FPGA2SOC_MASK);
		clrbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_HDSKREQ,
			     RSTMGR_HDSKREQ_FPGAHSREQ);
		setbits_le32(SOCFPGA_F2SDRAM_MGR_ADDRESS +
			     F2SDRAM_SIDEBAND_FLAGOUTCLR0,
			     FLGAOUTCLR0_F2S_IDLEREQ);
	}
}

static __always_inline void socfpga_s2f_bridges_reset(int enable)
{
	if (enable) {
		/* clear idle request to all bridges */
		setbits_le32(socfpga_get_sysmgr_addr() +
			     SYSMGR_SOC64_NOC_IDLEREQ_CLR, ~0);

		/* Release SOC2FPGA bridges from reset state */
		clrbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_BRGMODRST,
			     BRGMODRST_SOC2FPGA_BRIDGES);

		/* Poll until all idleack to 0 */
		POLL_FOR_ZERO(readl(socfpga_get_sysmgr_addr() +
			      SYSMGR_SOC64_NOC_IDLEACK), 300);
	} else {
		/* set idle request to all bridges */
		writel(~0,
		       socfpga_get_sysmgr_addr() +
		       SYSMGR_SOC64_NOC_IDLEREQ_SET);

		/* Enable the NOC timeout */
		writel(1, socfpga_get_sysmgr_addr() + SYSMGR_SOC64_NOC_TIMEOUT);

		/* Poll until all idleack to 1 */
		POLL_FOR_ZERO(readl(socfpga_get_sysmgr_addr() +
				    SYSMGR_SOC64_NOC_IDLEACK) ^
			      (SYSMGR_NOC_H2F_MSK | SYSMGR_NOC_LWH2F_MSK),
			      300);

		/* Poll until all idlestatus to 1 */
		POLL_FOR_ZERO(readl(socfpga_get_sysmgr_addr() +
				    SYSMGR_SOC64_NOC_IDLESTATUS) ^
			      (SYSMGR_NOC_H2F_MSK | SYSMGR_NOC_LWH2F_MSK),
			      300);

		/* Reset all SOC2FPGA bridges (except NOR DDR scheduler & F2S) */
		setbits_le32(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_BRGMODRST,
			     BRGMODRST_SOC2FPGA_BRIDGES);

		/* Disable NOC timeout */
		writel(0, socfpga_get_sysmgr_addr() + SYSMGR_SOC64_NOC_TIMEOUT);
	}
}

void socfpga_bridges_reset(int enable)
{
	if (!IS_ENABLED(CONFIG_SPL_BUILD) && IS_ENABLED(CONFIG_SPL_ATF)) {
		u64 arg = enable;
		int ret;

		ret = invoke_smc(INTEL_SIP_SMC_HPS_SET_BRIDGES, &arg, 1, NULL,
				 0);
		if (ret)
			printf("Failed to %s the HPS bridges, error %d\n",
			       enable ? "enable" : "disable", ret);
	} else {
		socfpga_s2f_bridges_reset(enable);
		socfpga_f2s_bridges_reset(enable);
	}
}

void __secure socfpga_bridges_reset_psci(int enable)
{
	socfpga_s2f_bridges_reset(enable);
	socfpga_f2s_bridges_reset(enable);
}

/*
 * Return non-zero if the CPU has been warm reset
 */
int cpu_has_been_warmreset(void)
{
	return readl(socfpga_get_rstmgr_addr() + RSTMGR_SOC64_STATUS) &
			RSTMGR_L4WD_MPU_WARMRESET_MASK;
}

void print_reset_info(void)
{
	bool iswd;
	int n;
	u32 stat = cpu_has_been_warmreset();

	printf("Reset state: %s%s", stat ? "Warm " : "Cold",
	       (stat & RSTMGR_STAT_SDMWARMRST) ? "[from SDM] " : "");

	stat &= ~RSTMGR_STAT_SDMWARMRST;
	if (!stat) {
		puts("\n");
		return;
	}

	n = generic_ffs(stat) - 1;
	iswd = (n >= RSTMGR_STAT_L4WD0RST_BITPOS);
	printf("(Triggered by %s %d)\n", iswd ? "Watchdog" : "MPU",
	       iswd ? (n - RSTMGR_STAT_L4WD0RST_BITPOS) :
	       (n - RSTMGR_STAT_MPU0RST_BITPOS));
}
