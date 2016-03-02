/*
 * Copyright (C) 2015 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <command.h>
#include <asm/arch/sdram.h>
#include <asm/arch/reset_manager.h>

static int do_fpgabr(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int arg;

	if (argc < 2)
		return CMD_RET_USAGE;

	arg = simple_strtoul(argv[1], NULL, 10);

	switch (arg) {
	case 0:
		printf("FPGA BRIDGES: disable\n");
		/* disable all axi bridge (hps2fpga, lwhps2fpga & fpga2hps) */
		reset_assert_all_bridges();
		break;
	case 1:
		printf("FPGA BRIDGES: enable\n");
		/* enable fpga bridges */
		if (reset_deassert_bridges_handoff())
			return CMD_RET_FAILURE;
		break;
	default:
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	fpgabr,	2,	1,	do_fpgabr,
	"fpgabr [0|1]\t",
	"\n"
	"    - run as needed before/after loading fpga image"
);
