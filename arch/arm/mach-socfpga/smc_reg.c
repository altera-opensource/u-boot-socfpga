// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <command.h>
#include <asm/arch/smc_api.h>
#include <linux/intel-smc.h>

static int reg_read(int argc, char * const argv[])
{
	u64 args[1];
	u64 addr;
	u32 val;
	char *endp;

	if (argc != 1)
		return CMD_RET_USAGE;

	addr =  simple_strtoul(argv[0], &endp, 16);

	args[0] = addr;
	if (invoke_smc(INTEL_SIP_SMC_REG_READ, args, 1, args, 1)) {
		printf("SMC call failed in %s\n", __func__);
		return CMD_RET_FAILURE;
	}
	val = (u32)args[0];

	printf("Read register @ %llx, value = 0x%08x.\n", addr, (int)val);

	return CMD_RET_SUCCESS;
}

static int reg_write(int argc, char * const argv[])
{
	u64 args[2];
	u64 addr;
	u32 val;
	char *endp;

	if (argc != 2)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[0], &endp, 16);
	val = simple_strtoul(argv[1], &endp, 16);

	args[0] = addr;
	args[1] = val;
	if (invoke_smc(INTEL_SIP_SMC_REG_WRITE, args, 2, NULL, 0)) {
		printf("SMC call failed in %s\n", __func__);
		return CMD_RET_FAILURE;
	}

	printf("Wrote register @ %llx, value = 0x%08x.\n", addr, (int)val);

	return CMD_RET_SUCCESS;
}

static int reg_update(int argc, char * const argv[])
{
	u64 args[3];
	u64 addr;
	u32 mask;
	u32 val;
	char *endp;

	if (argc != 3)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[0], &endp, 16);
	mask = simple_strtoul(argv[1], &endp, 16);
	val = simple_strtoul(argv[2], &endp, 16);

	args[0] = addr;
	args[1] = mask;
	args[2] = val;
	if (invoke_smc(INTEL_SIP_SMC_REG_UPDATE, args, 3, NULL, 0)) {
		printf("SMC call failed in %s\n", __func__);
		return CMD_RET_FAILURE;
	}

	printf("Updated register @ %llx, mask = 0x%08x, value = 0x%08x.\n",
	       addr, (int)mask, (int)val);

	return CMD_RET_SUCCESS;
}

int do_smc_reg(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	const char *cmd;

	if (argc < 3)
		return CMD_RET_USAGE;

	cmd = argv[1];

	argc -= 2;
	argv += 2;

	if (!strcmp("read", cmd))
		return reg_read(argc, argv);
	else if (!strcmp("write", cmd))
		return reg_write(argc, argv);
	else if (!strcmp("update", cmd))
		return reg_update(argc, argv);

	return CMD_RET_USAGE;
}

U_BOOT_CMD(smc_reg, 5, 1, do_smc_reg,
	   "access 32bit registers through SMC",
	   "read <address>\n"
	   "smc_reg write <address> <value>\n"
	   "smc_reg update <address> <mask> <value>\n"
	   ""
);
