/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2016-2019 Intel Corporation <www.intel.com>
 *
 */

#ifndef	_RESET_MANAGER_S10_
#define	_RESET_MANAGER_S10_

struct socfpga_reset_manager {
	u32	status;
	u32	mpu_rst_stat;
	u32	misc_stat;
	u32	padding1;
	u32	hdsk_en;
	u32	hdsk_req;
	u32	hdsk_ack;
	u32	hdsk_stall;
	u32	mpumodrst;
	u32	per0modrst;
	u32	per1modrst;
	u32	brgmodrst;
	u32	padding2;
	u32     cold_mod_reset;
	u32	padding3;
	u32     dbg_mod_reset;
	u32     tap_mod_reset;
	u32	padding4;
	u32	padding5;
	u32     brg_warm_mask;
	u32	padding6[3];
	u32     tst_stat;
	u32	padding7;
	u32     hdsk_timeout;
	u32     mpul2flushtimeout;
	u32     dbghdsktimeout;
};

#include <asm/arch/reset_manager_soc64.h>

#endif /* _RESET_MANAGER_S10_ */
