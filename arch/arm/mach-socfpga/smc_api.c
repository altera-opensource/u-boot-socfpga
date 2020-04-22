// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <asm/ptrace.h>
#include <asm/system.h>

int invoke_smc(u32 func_id, u64 *args, int arg_len, u64 *ret_arg, int ret_len)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));
	regs.regs[0] = func_id;

	if (args)
		memcpy(&regs.regs[1], args, arg_len * sizeof(*args));

	smc_call(&regs);

	if (ret_arg)
		memcpy(ret_arg, &regs.regs[1], ret_len * sizeof(*ret_arg));

	return regs.regs[0];
}
