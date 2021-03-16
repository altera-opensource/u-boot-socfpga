// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <clk.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <hang.h>
#include <ram.h>
#include <reset.h>
#include "sdram_soc64.h"
#include <wait_bit.h>
#include <asm/arch/firewall.h>
#include <asm/arch/handoff_soc64.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/io.h>
#include <linux/err.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

/* Memory reset manager */
#define MEM_RST_MGR_STATUS	0x8

/* Register and bit in memory reset manager */
#define MEM_RST_MGR_STATUS_RESET_COMPLETE	BIT(0)
#define MEM_RST_MGR_STATUS_PWROKIN_STATUS	BIT(1)
#define MEM_RST_MGR_STATUS_CONTROLLER_RST	BIT(2)
#define MEM_RST_MGR_STATUS_AXI_RST		BIT(3)

#define TIMEOUT_200MS     200
#define TIMEOUT_5000MS    5000

/* DDR4 umctl2 */
#define DDR4_STAT_OFFSET		0x4
#define DDR4_STAT_SELFREF_TYPE		(BIT(5) | BIT(4))
#define DDR4_STAT_SELFREF_TYPE_SHIFT	4
#define DDR4_STAT_OPERATING_MODE	(BIT(2) | BIT(1) | BIT(0))

#define DDR4_MRCTRL0_OFFSET		0x10
#define DDR4_MRCTRL0_MR_TYPE		BIT(0)
#define DDR4_MRCTRL0_MPR_EN		BIT(1)
#define DDR4_MRCTRL0_MR_RANK		(BIT(5) | BIT(4))
#define DDR4_MRCTRL0_MR_RANK_SHIFT	4
#define DDR4_MRCTRL0_MR_ADDR		(BIT(15) | BIT(14) | BIT(13) | BIT(12))
#define DDR4_MRCTRL0_MR_ADDR_SHIFT	12
#define DDR4_MRCTRL0_MR_WR		BIT(31)

#define DDR4_MRCTRL1_OFFSET		0x14
#define DDR4_MRCTRL1_MR_DATA		0x3FFFF

#define DDR4_MRSTAT_OFFSET		0x18
#define DDR4_MRSTAT_MR_WR_BUSY		BIT(0)

#define DDR4_MRCTRL2_OFFSET		0x1C

#define DDR4_PWRCTL_OFFSET			0x30
#define DDR4_PWRCTL_SELFREF_EN			BIT(0)
#define DDR4_PWRCTL_POWERDOWN_EN		BIT(1)
#define DDR4_PWRCTL_EN_DFI_DRAM_CLK_DISABLE	BIT(3)
#define DDR4_PWRCTL_SELFREF_SW			BIT(5)

#define DDR4_PWRTMG_OFFSET		0x34
#define DDR4_HWLPCTL_OFFSET		0x38
#define DDR4_RFSHCTL0_OFFSET		0x50
#define DDR4_RFSHCTL1_OFFSET		0x54

#define DDR4_RFSHCTL3_OFFSET			0x60
#define DDR4_RFSHCTL3_DIS_AUTO_REFRESH		BIT(0)
#define DDR4_RFSHCTL3_REFRESH_MODE		(BIT(6) | BIT(5) | BIT(4))
#define DDR4_RFSHCTL3_REFRESH_MODE_SHIFT	4

#define DDR4_ECCCFG0_OFFSET		0x70
#define DDR4_ECC_MODE			(BIT(2) | BIT(1) | BIT(0))
#define DDR4_DIS_SCRUB			BIT(4)

#define DDR4_CRCPARCTL0_OFFSET			0xC0
#define DDR4_CRCPARCTL0_DFI_ALERT_ERR_INIT_CLR	BIT(1)

#define DDR4_CRCPARCTL1_OFFSET			0xC4
#define DDR4_CRCPARCTL1_CRC_PARITY_RETRY_ENABLE	BIT(8)
#define DDR4_CRCPARCTL1_ALERT_WAIT_FOR_SW	BIT(9)

#define DDR4_CRCPARSTAT_OFFSET			0xCC
#define DDR4_CRCPARSTAT_DFI_ALERT_ERR_INT	BIT(16)
#define DDR4_CRCPARSTAT_DFI_ALERT_ERR_FATL_INT	BIT(17)
#define DDR4_CRCPARSTAT_DFI_ALERT_ERR_NO_SW	BIT(19)
#define DDR4_CRCPARSTAT_CMD_IN_ERR_WINDOW	BIT(29)

#define DDR4_DFIMISC_OFFSET			0x1B0
#define DDR4_DFIMISC_DFI_INIT_COMPLETE_EN	BIT(0)
#define DDR4_DFIMISC_DFI_INIT_START		BIT(5)

#define DDR4_DFISTAT_OFFSET		0x1BC
#define DDR4_DFI_INIT_COMPLETE		BIT(0)

#define DDR4_DBG0_OFFSET		0x300

#define DDR4_DBG1_OFFSET		0x304
#define DDR4_DBG1_DISDQ			BIT(0)
#define DDR4_DBG1_DIS_HIF		BIT(1)

#define DDR4_DBGCAM_OFFSET			0x308
#define DDR4_DBGCAM_DBG_RD_Q_EMPTY		BIT(25)
#define DDR4_DBGCAM_DBG_WR_Q_EMPTY		BIT(26)
#define DDR4_DBGCAM_RD_DATA_PIPELINE_EMPTY	BIT(28)
#define DDR4_DBGCAM_WR_DATA_PIPELINE_EMPTY	BIT(29)

#define DDR4_SWCTL_OFFSET		0x320
#define DDR4_SWCTL_SW_DONE		BIT(0)

#define DDR4_SWSTAT_OFFSET		0x324
#define DDR4_SWSTAT_SW_DONE_ACK		BIT(0)

#define DDR4_PSTAT_OFFSET		0x3FC
#define DDR4_PSTAT_RD_PORT_BUSY_0	BIT(0)
#define DDR4_PSTAT_WR_PORT_BUSY_0	BIT(16)

#define DDR4_PCTRL0_OFFSET		0x490
#define DDR4_PCTRL0_PORT_EN		BIT(0)

#define DDR4_SBRCTL_OFFSET		0xF24
#define DDR4_SBRCTL_SCRUB_INTERVAL	0x1FFF00
#define DDR4_SBRCTL_SCRUB_EN		BIT(0)
#define DDR4_SBRCTL_SCRUB_WRITE		BIT(2)
#define DDR_SBRCTL_SCRUB_BURST_1	BIT(4)

#define DDR4_SBRSTAT_OFFSET		0xF28
#define DDR4_SBRSTAT_SCRUB_BUSY BIT(0)
#define DDR4_SBRSTAT_SCRUB_DONE BIT(1)

#define DDR4_SBRWDATA0_OFFSET		0xF2C
#define DDR4_SBRWDATA1_OFFSET		0xF30
#define DDR4_SBRSTART0_OFFSET		0xF38
#define DDR4_SBRSTART1_OFFSET		0xF3C
#define DDR4_SBRRANGE0_OFFSET		0xF40
#define DDR4_SBRRANGE1_OFFSET		0xF44

/* DDR PHY */
#define DDR_PHY_TXODTDRVSTREN_B0_P0		0x2009A
#define DDR_PHY_RXPBDLYTG0_R0			0x200D0
#define DDR_PHY_CALRATE_OFFSET			0x40110
#define DDR_PHY_CALZAP_OFFSET			0x40112
#define DDR_PHY_SEQ0BDLY0_P0_OFFSET		0x40016
#define DDR_PHY_SEQ0BDLY1_P0_OFFSET		0x40018
#define DDR_PHY_SEQ0BDLY2_P0_OFFSET		0x4001A
#define DDR_PHY_SEQ0BDLY3_P0_OFFSET		0x4001C
#define DDR_PHY_SEQ0DISABLEFLAG0_OFFSET		0x120018
#define DDR_PHY_SEQ0DISABLEFLAG1_OFFSET		0x12001A
#define DDR_PHY_SEQ0DISABLEFLAG2_OFFSET		0x12001C
#define DDR_PHY_SEQ0DISABLEFLAG3_OFFSET		0x12001E
#define DDR_PHY_SEQ0DISABLEFLAG4_OFFSET		0x120020
#define DDR_PHY_SEQ0DISABLEFLAG5_OFFSET		0x120022
#define DDR_PHY_SEQ0DISABLEFLAG6_OFFSET		0x120024
#define DDR_PHY_SEQ0DISABLEFLAG7_OFFSET		0x120026
#define DDR_PHY_UCCLKHCLKENABLES_OFFSET		0x180100

#define DDR_PHY_APBONLY0_OFFSET			0x1A0000
#define DDR_PHY_MICROCONTMUXSEL			BIT(0)

#define DDR_PHY_MICRORESET_OFFSET		0x1A0132
#define DDR_PHY_MICRORESET_STALL		BIT(0)
#define DDR_PHY_MICRORESET_RESET		BIT(3)

#define DDR_PHY_TXODTDRVSTREN_B0_P1		0x22009A

/* Operating mode */
#define INIT_OPM			0x000
#define NORMAL_OPM			0x001
#define PWR_D0WN_OPM			0x010
#define SELF_SELFREF_OPM		0x011
#define DDR4_DEEP_PWR_DOWN_OPM		0x100

/* Refresh mode */
#define FIXED_1X		0
#define FIXED_2X		BIT(0)
#define FIXED_4X		BIT(4)

/* Address of mode register */
#define MR0	0x0000
#define MR1	0x0001
#define MR2	0x0010
#define MR3	0x0011
#define MR4	0x0100
#define MR5	0x0101
#define MR6	0x0110
#define MR7	0x0111

/* MR rank */
#define RANK0		0x1
#define RANK1		0x2
#define ALL_RANK	0x3

#define MR5_BIT4	BIT(4)

enum ddr_type {
	DDRTYPE_LPDDR4_0,
	DDRTYPE_LPDDR4_1,
	DDRTYPE_DDR4,
	DDRTYPE_UNKNOWN
};

/* DDR handoff structure */
struct ddr_handoff {
	phys_addr_t mem_reset_base;
	phys_addr_t umctl2_handoff_base;
	phys_addr_t umctl2_base;
	size_t umctl2_total_length;
	enum ddr_type umctl2_type;
	size_t umctl2_handoff_length;
	phys_addr_t phy_handoff_base;
	phys_addr_t phy_base;
	size_t phy_total_length;
	size_t phy_handoff_length;
	phys_addr_t phy_engine_handoff_base;
	size_t phy_engine_total_length;
	size_t phy_engine_handoff_length;
};

static int clr_ca_parity_error_status(phys_addr_t umctl2_base)
{
	int ret;

	debug("%s: Clear C/A parity error status in MR5[4]\n", __func__);

	/* Set mode register MRS */
	clrbits_le32(umctl2_base + DDR4_MRCTRL0_OFFSET, DDR4_MRCTRL0_MPR_EN);

	/* Set mode register to write operation */
	setbits_le32(umctl2_base + DDR4_MRCTRL0_OFFSET, DDR4_MRCTRL0_MR_TYPE);

	/* Set the address of mode rgister to 0x101(MR5) */
	setbits_le32(umctl2_base + DDR4_MRCTRL0_OFFSET,
		     (MR5 << DDR4_MRCTRL0_MR_ADDR_SHIFT) &
		     DDR4_MRCTRL0_MR_ADDR);

	/* Set MR rank to rank 1 */
	setbits_le32(umctl2_base + DDR4_MRCTRL0_OFFSET,
		     (RANK1 << DDR4_MRCTRL0_MR_RANK_SHIFT) &
		     DDR4_MRCTRL0_MR_RANK);

	/* Clear C/A parity error status in MR5[4] */
	clrbits_le32(umctl2_base + DDR4_MRCTRL1_OFFSET, MR5_BIT4);

	/* Trigger mode register read or write operation */
	setbits_le32(umctl2_base + DDR4_MRCTRL0_OFFSET, DDR4_MRCTRL0_MR_WR);

	/* Wait for retry done */
	ret = wait_for_bit_le32((const void *)(umctl2_base +
				DDR4_MRSTAT_OFFSET), DDR4_MRSTAT_MR_WR_BUSY,
				false, TIMEOUT_200MS, false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" no outstanding MR transaction\n");
		return ret;
	}

	return 0;
}

static int ddr4_retry_software_sequence(phys_addr_t umctl2_base)
{
	u32 value;
	int ret;

	/* Check software can perform MRS/MPR/PDA? */
	value = readl(umctl2_base + DDR4_CRCPARSTAT_OFFSET) &
		      DDR4_CRCPARSTAT_DFI_ALERT_ERR_NO_SW;

	if (value) {
		debug("%s: Software can't perform MRS/MPR/PDA\n", __func__);

		/* Clear interrupt bit for DFI alert error */
		setbits_le32(umctl2_base + DDR4_CRCPARCTL0_OFFSET,
			     DDR4_CRCPARCTL0_DFI_ALERT_ERR_INIT_CLR);

		/* Wait for retry done */
		ret = wait_for_bit_le32((const void *)(umctl2_base +
					DDR4_MRSTAT_OFFSET),
					DDR4_MRSTAT_MR_WR_BUSY,
					false, TIMEOUT_200MS, false);
		if (ret) {
			debug("%s: Timeout while waiting for", __func__);
			debug(" no outstanding MR transaction\n");
			return ret;
		}

		if (clr_ca_parity_error_status(umctl2_base))
			return ret;
	} else {
		debug("%s: Software can perform MRS/MPR/PDA\n", __func__);

		ret = wait_for_bit_le32((const void *)(umctl2_base +
					DDR4_MRSTAT_OFFSET),
					DDR4_MRSTAT_MR_WR_BUSY,
					false, TIMEOUT_200MS, false);
		if (ret) {
			debug("%s: Timeout while waiting for", __func__);
			debug(" no outstanding MR transaction\n");
			return ret;
		}

		if (clr_ca_parity_error_status(umctl2_base))
			return ret;

		/* Clear interrupt bit for DFI alert error */
		setbits_le32(umctl2_base + DDR4_CRCPARCTL0_OFFSET,
			     DDR4_CRCPARCTL0_DFI_ALERT_ERR_INIT_CLR);
	}

	return 0;
}

static int ensure_retry_procedure_complete(phys_addr_t umctl2_base)
{
	u32 value;
	u32 start = get_timer(0);
	int ret;

	/* Check parity/crc/error window is emptied ? */
	value = readl(umctl2_base + DDR4_CRCPARSTAT_OFFSET) &
		      DDR4_CRCPARSTAT_CMD_IN_ERR_WINDOW;

	/* Polling until parity/crc/error window is emptied */
	while (value) {
		if (get_timer(start) > TIMEOUT_200MS) {
			debug("%s: Timeout while waiting for",
			      __func__);
			debug(" parity/crc/error window empty\n");
			return -ETIMEDOUT;
		}

		/* Check software intervention is enabled? */
		value = readl(umctl2_base + DDR4_CRCPARCTL1_OFFSET) &
			      DDR4_CRCPARCTL1_ALERT_WAIT_FOR_SW;
		if (value) {
			debug("%s: Software intervention is enabled\n",
			      __func__);

			/* Check dfi alert error interrupt is set? */
			value = readl(umctl2_base + DDR4_CRCPARSTAT_OFFSET) &
				      DDR4_CRCPARSTAT_DFI_ALERT_ERR_INT;

			if (value) {
				ret =
				ddr4_retry_software_sequence(umctl2_base);
				debug("%s: DFI alert error interrupt ",
				      __func__);
				debug("is set\n");

				if (ret)
					return ret;
			}

			/*
			 * Check fatal parity error interrupt is set?
			 */
			value = readl(umctl2_base + DDR4_CRCPARSTAT_OFFSET) &
				      DDR4_CRCPARSTAT_DFI_ALERT_ERR_FATL_INT;
			if (value) {
				printf("%s: Fatal parity error  ",
				       __func__);
				printf("interrupt is set, Hang it!!\n");
				hang();
			}
		}

		value = readl(umctl2_base + DDR4_CRCPARSTAT_OFFSET) &
			      DDR4_CRCPARSTAT_CMD_IN_ERR_WINDOW;

		udelay(1);
		WATCHDOG_RESET();
	}

	return 0;
}

static int enable_quasi_dynamic_reg_grp3(phys_addr_t umctl2_base,
					 enum ddr_type umctl2_type)
{
	u32 i, value, backup;
	int ret;

	/* Disable input traffic per port */
	clrbits_le32(umctl2_base + DDR4_PCTRL0_OFFSET, DDR4_PCTRL0_PORT_EN);

	/* Polling AXI port until idle */
	ret = wait_for_bit_le32((const void *)(umctl2_base +
				DDR4_PSTAT_OFFSET),
				DDR4_PSTAT_WR_PORT_BUSY_0 |
				DDR4_PSTAT_RD_PORT_BUSY_0, false,
				TIMEOUT_200MS, false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" controller idle\n");
		return ret;
	}

	/* Backup user setting */
	backup = readl(umctl2_base + DDR4_DBG1_OFFSET);

	/* Disable input traffic to the controller */
	setbits_le32(umctl2_base + DDR4_DBG1_OFFSET, DDR4_DBG1_DIS_HIF);

	/*
	 * Ensure CAM/data pipelines are empty.
	 * Poll until CAM/data pipelines are set at least twice,
	 * timeout at 200ms
	 */
	for (i = 0; i < 2; i++) {
		ret = wait_for_bit_le32((const void *)(umctl2_base +
					DDR4_DBGCAM_OFFSET),
					DDR4_DBGCAM_WR_DATA_PIPELINE_EMPTY |
					DDR4_DBGCAM_RD_DATA_PIPELINE_EMPTY |
					DDR4_DBGCAM_DBG_WR_Q_EMPTY |
					DDR4_DBGCAM_DBG_RD_Q_EMPTY, true,
					TIMEOUT_200MS, false);
		if (ret) {
			debug("%s: loop(%u): Timeout while waiting for",
			      __func__, i + 1);
			debug(" CAM/data pipelines are empty\n");

			/* Restore user setting */
			writel(backup, umctl2_base + DDR4_DBG1_OFFSET);

			return ret;
		}
	}

	if (umctl2_type == DDRTYPE_DDR4) {
		/* Check DDR4 retry is enabled ? */
		value = readl(umctl2_base + DDR4_CRCPARCTL1_OFFSET) &
			      DDR4_CRCPARCTL1_CRC_PARITY_RETRY_ENABLE;

		if (value) {
			debug("%s: DDR4 retry is enabled\n", __func__);

			ret = ensure_retry_procedure_complete(umctl2_base);
			if (ret) {
				debug("%s: Timeout while waiting for",
				      __func__);
				debug(" retry procedure complete\n");

				/* Restore user setting */
				writel(backup, umctl2_base + DDR4_DBG1_OFFSET);

				return ret;
			}
		}
	}

	/* Restore user setting */
	writel(backup, umctl2_base + DDR4_DBG1_OFFSET);

	debug("%s: Quasi-dynamic group 3 registers are enabled\n", __func__);

	return 0;
}

static enum ddr_type get_ddr_type(phys_addr_t ddr_type_location)
{
	u32 ddr_type_magic = readl(ddr_type_location);

	if (ddr_type_magic == SOC64_HANDOFF_DDR_UMCTL2_DDR4_TYPE)
		return DDRTYPE_DDR4;

	if (ddr_type_magic == SOC64_HANDOFF_DDR_UMCTL2_LPDDR4_0_TYPE)
		return DDRTYPE_LPDDR4_0;

	if (ddr_type_magic == SOC64_HANDOFF_DDR_UMCTL2_LPDDR4_1_TYPE)
		return DDRTYPE_LPDDR4_1;

	return DDRTYPE_UNKNOWN;
}

static void use_ddr4(enum ddr_type type)
{
	if (type == DDRTYPE_DDR4) {
		printf("Starting DDR4 configuration ...\n");
		setbits_le32(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_DDR_MODE,
			     SYSMGR_SOC64_DDR_MODE_MSK);
	} else if (type == DDRTYPE_LPDDR4_0)  {
		printf("Starting LPDDR4 configuration ...\n");
		clrbits_le32(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_DDR_MODE,
			     SYSMGR_SOC64_DDR_MODE_MSK);
	}
}

static int scrubber_ddr_config(phys_addr_t umctl2_base,
			       enum ddr_type umctl2_type)
{
	u32 backup[9];
	int ret;

	/* Reset to default value, prevent scrubber stop due to lower power */
	writel(0, umctl2_base + DDR4_PWRCTL_OFFSET);

	/* Disable input traffic per port */
	clrbits_le32(umctl2_base + DDR4_PCTRL0_OFFSET,
		     DDR4_PCTRL0_PORT_EN);

	/* Backup user settings */
	backup[0] = readl(umctl2_base + DDR4_SBRCTL_OFFSET);
	backup[1] = readl(umctl2_base + DDR4_SBRWDATA0_OFFSET);
	backup[2] = readl(umctl2_base + DDR4_SBRWDATA1_OFFSET);
	backup[3] = readl(umctl2_base + DDR4_SBRSTART0_OFFSET);
	backup[4] = readl(umctl2_base + DDR4_SBRSTART1_OFFSET);
	backup[5] = readl(umctl2_base + DDR4_SBRRANGE0_OFFSET);
	backup[6] = readl(umctl2_base + DDR4_SBRRANGE1_OFFSET);

	/* Scrub_burst = 1, scrub_mode = 1(performs writes) */
	writel(DDR_SBRCTL_SCRUB_BURST_1 | DDR4_SBRCTL_SCRUB_WRITE,
	       umctl2_base + DDR4_SBRCTL_OFFSET);

	/* Zeroing whole DDR */
	writel(0, umctl2_base + DDR4_SBRWDATA0_OFFSET);
	writel(0, umctl2_base + DDR4_SBRWDATA1_OFFSET);
	writel(0, umctl2_base + DDR4_SBRSTART0_OFFSET);
	writel(0, umctl2_base + DDR4_SBRSTART1_OFFSET);
	writel(0, umctl2_base + DDR4_SBRRANGE0_OFFSET);
	writel(0, umctl2_base + DDR4_SBRRANGE1_OFFSET);

	/* Enables scrubber */
	setbits_le32(umctl2_base + DDR4_SBRCTL_OFFSET, DDR4_SBRCTL_SCRUB_EN);

	/* Polling all scrub writes commands have been sent */
	ret = wait_for_bit_le32((const void *)(umctl2_base +
				DDR4_SBRSTAT_OFFSET), DDR4_SBRSTAT_SCRUB_DONE,
				true, TIMEOUT_5000MS, false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" sending all scrub commands\n");
		return ret;
	}

	/* Polling all scrub writes data have been sent */
	ret = wait_for_bit_le32((const void *)(umctl2_base +
				DDR4_SBRSTAT_OFFSET), DDR4_SBRSTAT_SCRUB_BUSY,
				false, TIMEOUT_5000MS, false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" sending all scrub data\n");
		return ret;
	}

	/* Disables scrubber */
	clrbits_le32(umctl2_base + DDR4_SBRCTL_OFFSET, DDR4_SBRCTL_SCRUB_EN);

	/* Restore user settings */
	writel(backup[0], umctl2_base + DDR4_SBRCTL_OFFSET);
	writel(backup[1], umctl2_base + DDR4_SBRWDATA0_OFFSET);
	writel(backup[2], umctl2_base + DDR4_SBRWDATA1_OFFSET);
	writel(backup[3], umctl2_base + DDR4_SBRSTART0_OFFSET);
	writel(backup[4], umctl2_base + DDR4_SBRSTART1_OFFSET);
	writel(backup[5], umctl2_base + DDR4_SBRRANGE0_OFFSET);
	writel(backup[6], umctl2_base + DDR4_SBRRANGE1_OFFSET);

	return 0;
}

static int init_umctl2(phys_addr_t umctl2_handoff_base,
		       phys_addr_t umctl2_base, enum ddr_type umctl2_type,
		       size_t umctl2_handoff_length,
		       u32 *user_backup)
{
	u32 handoff_table[umctl2_handoff_length];
	u32 i;
	int ret;

	printf("Initializing DDR controller ...\n");

	/* Prevent controller from issuing read/write to SDRAM */
	setbits_le32(umctl2_base + DDR4_DBG1_OFFSET, DDR4_DBG1_DISDQ);

	/* Put SDRAM into self-refresh */
	setbits_le32(umctl2_base + DDR4_PWRCTL_OFFSET, DDR4_PWRCTL_SELFREF_EN);

	/* Enable quasi-dynamic programing of the controller registers */
	clrbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	/* Ensure the controller is in initialization mode */
	ret = wait_for_bit_le32((const void *)(umctl2_base + DDR4_STAT_OFFSET),
				DDR4_STAT_OPERATING_MODE, false, TIMEOUT_200MS,
				false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" init operating mode\n");
		return ret;
	}

	debug("%s: UMCTL2 handoff base address = 0x%p table length = 0x%08x\n",
	      __func__, (u32 *)umctl2_handoff_base,
	      (u32)umctl2_handoff_length);

	handoff_read((void *)umctl2_handoff_base, handoff_table,
		     umctl2_handoff_length, little_endian);

	for (i = 0; i < umctl2_handoff_length; i = i + 2) {
		debug("%s: Absolute addr: 0x%08llx APB offset: 0x%08x",
		      __func__, handoff_table[i] + umctl2_base,
		      handoff_table[i]);
		debug(" wr = 0x%08x ", handoff_table[i + 1]);

		writel(handoff_table[i + 1], (uintptr_t)(handoff_table[i] +
		       umctl2_base));

		debug("rd = 0x%08x\n", readl((uintptr_t)(handoff_table[i] +
		      umctl2_base)));
	}

	/* Backup user settings, restore after DDR up running */
	*user_backup = readl(umctl2_base + DDR4_PWRCTL_OFFSET);

	/* Disable self resfresh */
	clrbits_le32(umctl2_base + DDR4_PWRCTL_OFFSET, DDR4_PWRCTL_SELFREF_EN);

	/* Complete quasi-dynamic register programming */
	setbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	/* Enable controller from issuing read/write to SDRAM */
	clrbits_le32(umctl2_base + DDR4_DBG1_OFFSET, DDR4_DBG1_DISDQ);

	return 0;
}

static int phy_pre_handoff_config(phys_addr_t umctl2_base,
				  enum ddr_type umctl2_type)
{
	int ret;
	u32 value;

	if (umctl2_type == DDRTYPE_DDR4) {
		/* Check DDR4 retry is enabled ? */
		value = readl(umctl2_base + DDR4_CRCPARCTL1_OFFSET) &
			      DDR4_CRCPARCTL1_CRC_PARITY_RETRY_ENABLE;

		if (value) {
			debug("%s: DDR4 retry is enabled\n", __func__);
			debug("%s: Disable auto refresh is not supported\n",
			      __func__);
		} else {
			/* Disable auto refresh */
			setbits_le32(umctl2_base + DDR4_RFSHCTL3_OFFSET,
				     DDR4_RFSHCTL3_DIS_AUTO_REFRESH);
		}
	}

	/* Disable selfref_en & powerdown_en, nvr disable dfi dram clk */
	clrbits_le32(umctl2_base + DDR4_PWRCTL_OFFSET,
		     DDR4_PWRCTL_EN_DFI_DRAM_CLK_DISABLE |
		     DDR4_PWRCTL_POWERDOWN_EN | DDR4_PWRCTL_SELFREF_EN);

	/* Enable quasi-dynamic programing of the controller registers */
	clrbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	ret = enable_quasi_dynamic_reg_grp3(umctl2_base, umctl2_type);
	if (ret)
		return ret;

	/* Masking dfi init complete */
	clrbits_le32(umctl2_base + DDR4_DFIMISC_OFFSET,
		     DDR4_DFIMISC_DFI_INIT_COMPLETE_EN);

	/* Complete quasi-dynamic register programming */
	setbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	/* Polling programming done */
	ret = wait_for_bit_le32((const void *)(umctl2_base +
				DDR4_SWSTAT_OFFSET), DDR4_SWSTAT_SW_DONE_ACK,
				true, TIMEOUT_200MS, false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" programming done\n");
	}

	return ret;
}

static int init_phy(struct ddr_handoff *ddr_handoff_info)
{
	u32 handoff_table[ddr_handoff_info->phy_handoff_length];
	u32 i, value;
	int ret;

	printf("Initializing DDR PHY ...\n");

	ret = phy_pre_handoff_config(ddr_handoff_info->umctl2_base,
				     ddr_handoff_info->umctl2_type);
	if (ret)
		return ret;

	/* Execute PHY configuration handoff */
	handoff_read((void *)ddr_handoff_info->phy_handoff_base, handoff_table,
		     (u32)ddr_handoff_info->phy_handoff_length, little_endian);

	for (i = 0; i < ddr_handoff_info->phy_handoff_length; i = i + 2) {
		/*
		 * Convert PHY odd offset to even offset that supported by
		 * ARM processor.
		 */
		value = handoff_table[i] << 1;
		debug("%s: Absolute addr: 0x%08llx, APB offset: 0x%08x ",
		      __func__, value + ddr_handoff_info->phy_base, value);
		debug("PHY offset: 0x%08x", handoff_table[i]);
		debug(" wr = 0x%08x ", handoff_table[i + 1]);
		writew(handoff_table[i + 1], (uintptr_t)(value +
		       ddr_handoff_info->phy_base));
		debug("rd = 0x%08x\n", readw((uintptr_t)(value +
		      ddr_handoff_info->phy_base)));
	}

	printf("DDR PHY configuration is completed\n");

	return 0;
}

static void phy_init_engine(struct ddr_handoff *ddr_handoff_info)
{
	u32 i, value;
	u32 handoff_table[ddr_handoff_info->phy_engine_handoff_length];

	printf("Load PHY Init Engine ...\n");

	/* Execute PIE production code handoff */
	handoff_read((void *)ddr_handoff_info->phy_engine_handoff_base,
		     handoff_table,
		     (u32)ddr_handoff_info->phy_engine_handoff_length,
		     little_endian);

	for (i = 0; i < ddr_handoff_info->phy_engine_handoff_length;
	    i = i + 2) {
		debug("Handoff addr: 0x%8llx ", handoff_table[i] +
		      ddr_handoff_info->phy_base);

		/*
		 * Convert PHY odd offset to even offset that supported by
		 * ARM processor.
		 */
		value = handoff_table[i] << 1;
		debug("%s: Absolute addr: 0x%08llx, APB offset: 0x%08x ",
		      __func__, value + ddr_handoff_info->phy_base, value);
		debug("PHY offset: 0x%08x", handoff_table[i]);
		debug(" wr = 0x%08x ", handoff_table[i + 1]);

		writew(handoff_table[i + 1], (uintptr_t)(value +
		       ddr_handoff_info->phy_base));

		debug("rd = 0x%08x\n", readw((uintptr_t)(value +
		      ddr_handoff_info->phy_base)));
	}

	printf("End of loading PHY Init Engine\n");
}

int populate_ddr_handoff(struct ddr_handoff *ddr_handoff_info)
{
	phys_addr_t next_section_header;

	/* DDR handoff */
	ddr_handoff_info->mem_reset_base = SOC64_HANDOFF_DDR_MEMRESET_BASE;
	debug("%s: DDR memory reset base = 0x%x\n", __func__,
	      (u32)ddr_handoff_info->mem_reset_base);
	debug("%s: DDR memory reset address = 0x%x\n", __func__,
	      readl(ddr_handoff_info->mem_reset_base));

	/* Beginning of DDR controller handoff */
	ddr_handoff_info->umctl2_handoff_base =
		SOC64_HANDOFF_DDR_UMCTL2_SECTION;
	debug("%s: umctl2 handoff base = 0x%x\n", __func__,
	      (u32)ddr_handoff_info->umctl2_handoff_base);

	/* Get 1st DDR type */
	ddr_handoff_info->umctl2_type =
		get_ddr_type(ddr_handoff_info->umctl2_handoff_base +
			     SOC64_HANDOFF_DDR_UMCTL2_TYPE_OFFSET);
	if (ddr_handoff_info->umctl2_type == DDRTYPE_LPDDR4_1 ||
	    ddr_handoff_info->umctl2_type == DDRTYPE_UNKNOWN) {
		debug("%s: Wrong DDR handoff format, the 1st DDR ", __func__);
		debug("type must be DDR4 or LPDDR4_0\n");
		return -ENOEXEC;
	}

	/* 1st umctl2 base physical address */
	ddr_handoff_info->umctl2_base =
		readl(ddr_handoff_info->umctl2_handoff_base +
		      SOC64_HANDOFF_DDR_UMCTL2_BASE_ADDR_OFFSET);
	debug("%s: umctl2 base = 0x%x\n", __func__,
	      (u32)ddr_handoff_info->umctl2_base);

	/* Get the total length of DDR umctl2 handoff section */
	ddr_handoff_info->umctl2_total_length =
			readl(ddr_handoff_info->umctl2_handoff_base +
			      SOC64_HANDOFF_OFFSET_LENGTH);
	debug("%s: Umctl2 total length in byte = 0x%x\n", __func__,
	      (u32)ddr_handoff_info->umctl2_total_length);

	/* Get the length of user setting data in DDR umctl2 handoff section */
	ddr_handoff_info->umctl2_handoff_length =
		get_handoff_size((void *)ddr_handoff_info->umctl2_handoff_base,
				 little_endian);
	debug("%s: Umctl2 handoff length in word(32-bit) = 0x%x\n", __func__,
	      (u32)ddr_handoff_info->umctl2_handoff_length);

	/* Wrong format on user setting data */
	if (ddr_handoff_info->umctl2_handoff_length < 0) {
		debug("%s: Wrong format on user setting data\n", __func__);
		return -ENOEXEC;
	}

	/* Get the next handoff section address */
	next_section_header = ddr_handoff_info->umctl2_handoff_base +
				ddr_handoff_info->umctl2_total_length;
	debug("%s: Next handoff section header location = 0x%llx\n", __func__,
	      next_section_header);

	/* Checking next section handoff is PHY ? */
	if (readl(next_section_header) == SOC64_HANDOFF_DDR_PHY_MAGIC) {
		/* DDR PHY handoff */
		ddr_handoff_info->phy_handoff_base = next_section_header;
		debug("%s: PHY handoff base = 0x%x\n", __func__,
		      (u32)ddr_handoff_info->phy_handoff_base);

		/* PHY base physical address */
		ddr_handoff_info->phy_base =
			readl(ddr_handoff_info->phy_handoff_base +
			      SOC64_HANDOFF_DDR_PHY_BASE_OFFSET);
		debug("%s: PHY base = 0x%x\n", __func__,
		      (u32)ddr_handoff_info->phy_base);

		/* Get the total length of PHY handoff section */
		ddr_handoff_info->phy_total_length =
				readl(ddr_handoff_info->phy_handoff_base +
				      SOC64_HANDOFF_OFFSET_LENGTH);
		debug("%s: PHY total length in byte = 0x%x\n", __func__,
		      (u32)ddr_handoff_info->phy_total_length);

		/*
		 * Get the length of user setting data in DDR PHY handoff
		 * section
		 */
		ddr_handoff_info->phy_handoff_length =
		get_handoff_size((void *)ddr_handoff_info->phy_handoff_base,
				 little_endian);
		debug("%s: PHY handoff length in word(32-bit) = 0x%x\n",
		      __func__, (u32)ddr_handoff_info->phy_handoff_length);

		/* Wrong format on PHY user setting data */
		if (ddr_handoff_info->phy_handoff_length < 0) {
			debug("%s: Wrong format on PHY user setting data\n",
			      __func__);
			return -ENOEXEC;
		}

		/* Get the next handoff section address */
		next_section_header =
			ddr_handoff_info->phy_handoff_base +
			ddr_handoff_info->phy_total_length;
		debug("%s: Next handoff section header location = 0x%llx\n",
		      __func__, next_section_header);
	} else {
		debug("%s: Wrong format for DDR handoff, expect PHY",
		      __func__);
		debug(" handoff section after umctl2 handoff section\n");
		return -ENOEXEC;
	}

	/* Checking next section handoff is PHY init Engine ? */
	if (readl(next_section_header) ==
		SOC64_HANDOFF_DDR_PHY_INIT_ENGINE_MAGIC) {
		/* DDR PHY Engine handoff */
		ddr_handoff_info->phy_engine_handoff_base =
			next_section_header;
		debug("%s: PHY init engine handoff base = 0x%x\n", __func__,
		      (u32)ddr_handoff_info->phy_engine_handoff_base);

		/* Get the total length of PHY init engine handoff section */
		ddr_handoff_info->phy_engine_total_length =
			readl(ddr_handoff_info->phy_engine_handoff_base +
			      SOC64_HANDOFF_OFFSET_LENGTH);
		debug("%s: PHY engine total length in byte = 0x%x\n", __func__,
		      (u32)ddr_handoff_info->phy_engine_total_length);

		/*
		 * Get the length of user setting data in DDR PHY init engine
		 * handoff section
		 */
		ddr_handoff_info->phy_engine_handoff_length =
		get_handoff_size((void *)
				 ddr_handoff_info->phy_engine_handoff_base,
				 little_endian);
		debug("%s: PHY engine handoff length in word(32-bit) = 0x%x\n",
		      __func__,
		      (u32)ddr_handoff_info->phy_engine_handoff_length);

		/* Wrong format on PHY init engine setting data */
		if (ddr_handoff_info->phy_engine_handoff_length < 0) {
			debug("%s: Wrong format on PHY init engine ",
			      __func__);
			debug("user setting data\n");
			return -ENOEXEC;
		}
	} else {
		debug("%s: Wrong format for DDR handoff, expect PHY",
		      __func__);
		debug(" init engine handoff section after PHY handoff\n");
		debug(" section\n");
		return -ENOEXEC;
	}

	return 0;
}

int enable_ddr_clock(struct udevice *dev)
{
	struct clk *ddr_clk;
	int ret;

	/* Enable clock before init DDR */
	ddr_clk = devm_clk_get(dev, "mem_clk");
	if (!IS_ERR(ddr_clk)) {
		ret = clk_enable(ddr_clk);
		if (ret) {
			printf("%s: Failed to enable DDR clock\n", __func__);
			return ret;
		}
	} else {
		ret = PTR_ERR(ddr_clk);
		debug("%s: Failed to get DDR clock from dts\n", __func__);
		return ret;
	}

	printf("%s: DDR clock is enabled\n", __func__);

	return 0;
}

static int ddr_start_dfi_init(phys_addr_t umctl2_base,
			      enum ddr_type umctl2_type)
{
	int ret;

	debug("%s: Start DFI init\n", __func__);

	/* Enable quasi-dynamic programing of controller registers */
	clrbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	ret = enable_quasi_dynamic_reg_grp3(umctl2_base, umctl2_type);
	if (ret)
		return ret;

	/* Start DFI init sequence */
	setbits_le32(umctl2_base + DDR4_DFIMISC_OFFSET,
		     DDR4_DFIMISC_DFI_INIT_START);

	/* Complete quasi-dynamic register programming */
	setbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	/* Polling programming done */
	ret = wait_for_bit_le32((const void *)(umctl2_base +
				DDR4_SWSTAT_OFFSET),
				DDR4_SWSTAT_SW_DONE_ACK, true,
				TIMEOUT_200MS, false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" programming done\n");
	}

	return ret;
}

static int ddr_check_dfi_init_complete(phys_addr_t umctl2_base,
				       enum ddr_type umctl2_type)
{
	int ret;

	/* Polling DFI init complete */
	ret = wait_for_bit_le32((const void *)(umctl2_base +
				DDR4_DFISTAT_OFFSET),
				DDR4_DFI_INIT_COMPLETE, true,
				TIMEOUT_200MS, false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" DFI init done\n");
		return ret;
	}

	debug("%s: DFI init completed.\n", __func__);

	/* Enable quasi-dynamic programing of controller registers */
	clrbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	ret = enable_quasi_dynamic_reg_grp3(umctl2_base, umctl2_type);
	if (ret)
		return ret;

	/* Stop DFI init sequence */
	clrbits_le32(umctl2_base + DDR4_DFIMISC_OFFSET,
		     DDR4_DFIMISC_DFI_INIT_START);

	/* Complete quasi-dynamic register programming */
	setbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	/* Polling programming done */
	ret = wait_for_bit_le32((const void *)(umctl2_base +
				DDR4_SWSTAT_OFFSET),
				DDR4_SWSTAT_SW_DONE_ACK, true,
				TIMEOUT_200MS, false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" programming done\n");
		return ret;
	}

	debug("%s:DDR programming done\n", __func__);

	return ret;
}

static int ddr_trigger_sdram_init(phys_addr_t umctl2_base,
				  enum ddr_type umctl2_type)
{
	int ret;

	/* Enable quasi-dynamic programing of controller registers */
	clrbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	ret = enable_quasi_dynamic_reg_grp3(umctl2_base, umctl2_type);
	if (ret)
		return ret;

	/* Unmasking dfi init complete */
	setbits_le32(umctl2_base + DDR4_DFIMISC_OFFSET,
		     DDR4_DFIMISC_DFI_INIT_COMPLETE_EN);

	/* Software exit from self-refresh */
	clrbits_le32(umctl2_base + DDR4_PWRCTL_OFFSET, DDR4_PWRCTL_SELFREF_SW);

	/* Complete quasi-dynamic register programming */
	setbits_le32(umctl2_base + DDR4_SWCTL_OFFSET, DDR4_SWCTL_SW_DONE);

	/* Polling programming done */
	ret = wait_for_bit_le32((const void *)(umctl2_base +
				DDR4_SWSTAT_OFFSET),
				DDR4_SWSTAT_SW_DONE_ACK, true,
				TIMEOUT_200MS, false);
	if (ret) {
		debug("%s: Timeout while waiting for", __func__);
		debug(" programming done\n");
		return ret;
	}

	debug("%s:DDR programming done\n", __func__);
	return ret;
}

static int ddr_post_handoff_config(phys_addr_t umctl2_base,
				   enum ddr_type umctl2_type)
{
	int ret = 0;
	u32 value;
	u32 start = get_timer(0);

	/* Polling until SDRAM entered normal operating mode */
	value = readl(umctl2_base + DDR4_STAT_OFFSET) &
		      DDR4_STAT_OPERATING_MODE;
	while (value != NORMAL_OPM) {
		if (get_timer(start) > TIMEOUT_200MS) {
			debug("%s: Timeout while waiting for",
			      __func__);
			debug(" DDR enters normal operating mode\n");
			return -ETIMEDOUT;
		}

		value = readl(umctl2_base + DDR4_STAT_OFFSET) &
			      DDR4_STAT_OPERATING_MODE;

		udelay(1);
		WATCHDOG_RESET();
	}

	printf("DDR entered normal operating mode\n");

	/* Enabling auto refresh */
	clrbits_le32(umctl2_base + DDR4_RFSHCTL3_OFFSET,
		     DDR4_RFSHCTL3_DIS_AUTO_REFRESH);

	/* Checking ECC is enabled? */
	value = readl(umctl2_base + DDR4_ECCCFG0_OFFSET) & DDR4_ECC_MODE;
	if (value) {
		printf("ECC is enabled\n");
		ret = scrubber_ddr_config(umctl2_base, umctl2_type);
		if (ret)
			printf("Failed to enable ECC\n");
	}

	return ret;
}

int sdram_mmr_init_full(struct udevice *dev)
{
	u32 user_backup;
	int ret;
	struct bd_info bd;
	struct ddr_handoff ddr_handoff_info;
	struct altera_sdram_priv *priv = dev_get_priv(dev);

	printf("Checking SDRAM configuration in progress ...\n");
	ret = populate_ddr_handoff(&ddr_handoff_info);
		if (ret) {
			debug("%s: Failed to populate DDR handoff\n",
			      __func__);
			return ret;
		}

	/* Set the MPFE NoC mux to correct DDR controller type */
	use_ddr4(ddr_handoff_info.umctl2_type);

	if (!is_ddr_init_skipped()) {
		printf("%s: SDRAM init in progress ...\n", __func__);

		/*
		 * Polling reset complete, must be high to ensure DDR subsystem
		 * in complete reset state before init DDR clock and DDR
		 * controller
		 */
		ret = wait_for_bit_le32((const void *)((uintptr_t)(readl
					(ddr_handoff_info.mem_reset_base) +
					MEM_RST_MGR_STATUS)),
					MEM_RST_MGR_STATUS_RESET_COMPLETE,
					true, TIMEOUT_200MS, false);
		if (ret) {
			debug("%s: Timeout while waiting for", __func__);
			debug(" reset complete done\n");
			return ret;
		}

		ret = enable_ddr_clock(dev);
		if (ret)
			return ret;

		if (ddr_handoff_info.umctl2_type == DDRTYPE_DDR4) {
			/* Initialize 1st DDR controller */
			ret =
			init_umctl2(ddr_handoff_info.umctl2_handoff_base,
				    ddr_handoff_info.umctl2_base,
				    ddr_handoff_info.umctl2_type,
				    ddr_handoff_info.umctl2_handoff_length,
				    &user_backup);
			if (ret) {
				debug("%s: Failed to inilialize DDR ",
				      __func__);
				debug("controller\n");
				return ret;
			}
		}

		/* Release the controller from reset */
		setbits_le32((uintptr_t)
			     (readl(ddr_handoff_info.mem_reset_base) +
			     MEM_RST_MGR_STATUS), MEM_RST_MGR_STATUS_AXI_RST |
			     MEM_RST_MGR_STATUS_CONTROLLER_RST |
			     MEM_RST_MGR_STATUS_RESET_COMPLETE);

		printf("DDR controller configuration is completed\n");

		/* Initialize DDR PHY */
		ret = init_phy(&ddr_handoff_info);
		if (ret) {
			debug("%s: Failed to inilialize DDR PHY\n", __func__);
			return ret;
		}

		/* Reset ARC processor when no using for security purpose */
		setbits_le16(ddr_handoff_info.phy_base +
			     DDR_PHY_MICRORESET_OFFSET,
			     DDR_PHY_MICRORESET_RESET);

		/* DDR freq set to support DDR4-3200 */
		phy_init_engine(&ddr_handoff_info);

		ret = ddr_start_dfi_init(ddr_handoff_info.umctl2_base,
					 ddr_handoff_info.umctl2_type);
		if (ret)
			return ret;

		ret = ddr_check_dfi_init_complete
			(ddr_handoff_info.umctl2_base,
			 ddr_handoff_info.umctl2_type);
		if (ret)
			return ret;

		ret = ddr_trigger_sdram_init(ddr_handoff_info.umctl2_base,
					     ddr_handoff_info.umctl2_type);
		if (ret)
			return ret;

		ret = ddr_post_handoff_config(ddr_handoff_info.umctl2_base,
					      ddr_handoff_info.umctl2_type);
		if (ret)
			return ret;

		/* Restore user settings */
		writel(user_backup, ddr_handoff_info.umctl2_base +
		       DDR4_PWRCTL_OFFSET);

		/* Enable input traffic per port */
		setbits_le32(ddr_handoff_info.umctl2_base + DDR4_PCTRL0_OFFSET,
			     DDR4_PCTRL0_PORT_EN);

		printf("%s: DDR init success\n", __func__);
	}

	/* Get bank configuration from devicetree */
	ret = fdtdec_decode_ram_size(gd->fdt_blob, NULL, 0, NULL,
				     (phys_size_t *)&gd->ram_size, &bd);
	if (ret) {
		debug("%s: Failed to decode memory node\n",  __func__);
		return -1;
	}

	printf("DDR: %lld MiB\n", gd->ram_size >> 20);

	priv->info.base = bd.bi_dram[0].start;
	priv->info.size = gd->ram_size;

	sdram_size_check(&bd);

	sdram_set_firewall(&bd);

	return 0;
}
