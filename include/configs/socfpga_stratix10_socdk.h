/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2017-2019 Intel Corporation <www.intel.com>
 *
 */

#ifndef __CONFIG_SOCFGPA_STRATIX10_H__
#define __CONFIG_SOCFGPA_STRATIX10_H__

#include <configs/socfpga_soc64_common.h>

#ifndef __ASSEMBLY__
unsigned int cm_get_l4_sp_clk_hz(void);
#endif

#undef CONFIG_SYS_NS16550_CLK
#define CONFIG_SYS_NS16550_CLK		cm_get_l4_sp_clk_hz()

#endif	/* __CONFIG_SOCFGPA_STRATIX10_H__ */
