// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2020 Intel Corporation <www.intel.com>
 *
 */

#include <altera.h>
#include <common.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/misc.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <env.h>
#include <errno.h>
#include <init.h>
#include <log.h>
#include <mach/clock_manager.h>

#define RSU_DEFAULT_LOG_LEVEL  7

DECLARE_GLOBAL_DATA_PTR;

/* Reset type */
enum reset_type {
	por_reset,
	warm_reset,
	cold_reset,
	rsu_reset
};

/*
 * FPGA programming support for SoC FPGA Stratix 10
 */
static Altera_desc altera_fpga[] = {
	{
		/* Family */
		Intel_FPGA_SDM_Mailbox,
		/* Interface type */
		secure_device_manager_mailbox,
		/* No limitation as additional data will be ignored */
		-1,
		/* No device function table */
		NULL,
		/* Base interface address specified in driver */
		NULL,
		/* No cookie implementation */
		0
	},
};


/*
 * Print CPU information
 */
#if defined(CONFIG_DISPLAY_CPUINFO)
int print_cpuinfo(void)
{
	puts("CPU:   Intel FPGA SoCFPGA Platform (ARMv8 64bit Cortex-A53)\n");

	return 0;
}
#endif

#ifdef CONFIG_ARCH_MISC_INIT
int arch_misc_init(void)
{
	char qspi_string[13];
	char level[4];

	snprintf(level, sizeof(level), "%u", RSU_DEFAULT_LOG_LEVEL);
	sprintf(qspi_string, "<0x%08x>", cm_get_qspi_controller_clk_hz());
	env_set("qspi_clock", qspi_string);

	/* setup for RSU */
	env_set("rsu_protected_slot", "");
	env_set("rsu_log_level", level);

	/* setup for RSU sub-partition table checksum */
	env_set("rsu_spt_checksum", "");

	return 0;
}
#endif

int arch_early_init_r(void)
{
	socfpga_fpga_add(&altera_fpga[0]);

	return 0;
}

/* Return 1 if FPGA is ready otherwise return 0 */
int is_fpga_config_ready(void)
{
	return (readl(socfpga_get_sysmgr_addr() + SYSMGR_SOC64_FPGA_CONFIG) &
		SYSMGR_FPGACONFIG_READY_MASK) == SYSMGR_FPGACONFIG_READY_MASK;
}

void do_bridge_reset(int enable, unsigned int mask)
{
	/* Check FPGA status before bridge enable */
	if (!is_fpga_config_ready()) {
		puts("FPGA not ready. Bridge reset aborted!\n");
		return;
	}

	socfpga_bridges_reset(enable);
}

void arch_preboot_os(void)
{
	mbox_hps_stage_notify(HPS_EXECUTION_STATE_OS);
}

/* Only applicable to DM */
#ifdef CONFIG_TARGET_SOCFPGA_DM
static bool is_ddr_retention_enabled(u32 boot_scratch_cold0_reg)
{
	return boot_scratch_cold0_reg &
	       ALT_SYSMGR_SCRATCH_REG_0_DDR_RETENTION_MASK;
}

static bool is_ddr_bitstream_sha_matching(u32 boot_scratch_cold0_reg)
{
	return boot_scratch_cold0_reg & ALT_SYSMGR_SCRATCH_REG_0_DDR_SHA_MASK;
}

static enum reset_type get_reset_type(u32 boot_scratch_cold0_reg)
{
	return (boot_scratch_cold0_reg &
		ALT_SYSMGR_SCRATCH_REG_0_DDR_RESET_TYPE_MASK) >>
		ALT_SYSMGR_SCRATCH_REG_0_DDR_RESET_TYPE_SHIFT;
}

bool is_ddr_init_skipped(void)
{
	u32 reg = readl(socfpga_get_sysmgr_addr() +
			SYSMGR_SOC64_BOOT_SCRATCH_COLD0);

	if (get_reset_type(reg) == por_reset) {
		debug("%s: POR reset is triggered\n", __func__);
		debug("%s: DDR init is required\n", __func__);
		return false;
	}

	if (get_reset_type(reg) == warm_reset) {
		debug("%s: Warm reset is triggered\n", __func__);
		debug("%s: DDR init is skipped\n", __func__);
		return true;
	}

	if ((get_reset_type(reg) == cold_reset) ||
	    (get_reset_type(reg) == rsu_reset)) {
		debug("%s: Cold/RSU reset is triggered\n", __func__);

		if (is_ddr_retention_enabled(reg)) {
			debug("%s: DDR retention bit is set\n", __func__);

			if (is_ddr_bitstream_sha_matching(reg)) {
				debug("%s: Matching in DDR bistream\n",
				      __func__);
				debug("%s: DDR init is skipped\n", __func__);
				return true;
			}

			debug("%s: Mismatch in DDR bistream\n", __func__);
		}
	}

	debug("%s: DDR init is required\n", __func__);
	return false;
}
#endif
