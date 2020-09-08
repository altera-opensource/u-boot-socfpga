/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Intel Corporation <www.intel.com>
 */

#ifndef _CLOCK_MANAGER_DM_
#define _CLOCK_MANAGER_DM_

unsigned long cm_get_mpu_clk_hz(void);

#include <asm/arch/clock_manager_soc64.h>
#include "../../../../../drivers/clk/altera/clk-dm.h"

#endif /* _CLOCK_MANAGER_DM_ */
