/*
 *  Copyright Altera Corporation (C) 2012-2013. All rights reserved
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms and conditions of the GNU General Public License,
 *  version 2, as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CONFIG_SOCFPGA_CYCLONE5

#ifndef __CONFIG_H
#define __CONFIG_H

#include "../../board/altera/socfpga/build.h"
#include "../../board/altera/socfpga/pinmux_config.h"
#include "../../board/altera/socfpga/pll_config.h"
#include "../../board/altera/socfpga/sdram/sdram_config.h"
#include "../../board/altera/socfpga/reset_config.h"
#include "socfpga_common.h"
#ifdef CONFIG_SPL_BUILD
#include "../../board/altera/socfpga/iocsr_config_cyclone5.h"
#endif

/*
 * Console setup
 */
/* Monitor Command Prompt */
#define CONFIG_SYS_PROMPT		"SOCFPGA_CYCLONE5 # "

/* EMAC controller and PHY used */
#define CONFIG_EMAC_BASE		CONFIG_EMAC1_BASE
#define CONFIG_EPHY_PHY_ADDR		CONFIG_EPHY1_PHY_ADDR
#define CONFIG_PHY_INTERFACE_MODE	SOCFPGA_PHYSEL_ENUM_RGMII

/* Define machine type for Cyclone 5 */
#define CONFIG_MACH_TYPE 4251


#endif	/* __CONFIG_H */
