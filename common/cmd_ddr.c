/*
 * Copyright (C) 2015 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <command.h>
#include <asm/arch/sdram.h>

static int do_ddrcal(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;

	ret = ddr_calibration_sequence();
	if (ret)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	ddrcal,	1,	0,	do_ddrcal,
	"run ddr calibration sequence",
	"\n"
	"    - run as needed after loading fpga image"
);
