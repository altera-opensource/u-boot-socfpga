/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef	_SYSTEM_MANAGER_H_
#define	_SYSTEM_MANAGER_H_

#ifndef __ASSEMBLY__
/* declaration for system_manager.c */
void sysmgr_pinmux_init(void);

/* declaration for handoff table type */
typedef unsigned long sys_mgr_pinmux_entry_t;
extern unsigned long sys_mgr_init_table[CONFIG_HPS_PINMUX_NUM];
#endif

#if defined(CONFIG_SOCFPGA_VIRTUAL_TARGET)
#define CONFIG_CPU1_START_ADDR		(SOCFPGA_SYSMGR_ADDRESS + 0x10)
/* For VT, temporary use ISW valid register */
#define CONFIG_SYSMGR_ROMCODEGRP_CTRL	(SOCFPGA_SYSMGR_ADDRESS + 0x14)
#define CONFIG_SYSMGR_ECCGRP		(SOCFPGA_SYSMGR_ADDRESS + 0x0)
#define CONFIG_SYSMGR_ECC_OCRAM		(SOCFPGA_SYSMGR_ADDRESS + 0x44)
#else
#define CONFIG_SYSMGR_EMAC_CTRL		(SOCFPGA_SYSMGR_ADDRESS + 0x60)
#define CONFIG_CPU1_START_ADDR		(SOCFPGA_SYSMGR_ADDRESS + 0xc4)
#define CONFIG_SYSMGR_ROMCODEGRP_CTRL	(SOCFPGA_SYSMGR_ADDRESS + 0xc0)
#define CONFIG_SYSMGR_ECCGRP		(SOCFPGA_SYSMGR_ADDRESS + 0x140)
#define CONFIG_SYSMGR_ECC_OCRAM		(CONFIG_SYSMGR_ECCGRP + 0x4)
#endif

#define CONFIG_SYSMGR_PINMUXGRP_OFFSET	(0x400)
#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGPINMUX		(1<<0)
#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGIO		(1<<1)
#define SYSMGR_ECC_OCRAM_EN		(1<<0)
#define SYSMGR_ECC_OCRAM_SERR		(1<<3)
#define SYSMGR_ECC_OCRAM_DERR		(1<<4)

/* Enumeration: sysmgr::emacgrp::ctrl::physel::enum                        */
/* Source filename: csr/sysmgr.csr, line: 1346                             */
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII 0x0
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII 0x1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_WIDTH 2
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII 0x2

/* Field instance: sysmgr::emacgrp::ctrl.physel                            */
#define SYSMGR_EMACGRP_CTRL_PHYSEL_MSB 1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_LSB 0
#define SYSMGR_EMACGRP_CTRL_PHYSEL_WIDTH 2
#define SYSMGR_EMACGRP_CTRL_PHYSEL_READ_ACCESS 1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_WRITE_ACCESS 1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_RESET 0x2
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ARRAY_INDEX_MAX 0x1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ARRAY_INDEX_MIN 0x0
#define SYSMGR_EMACGRP_CTRL_PHYSEL_MASK 0x00000003
#define SYSMGR_EMACGRP_CTRL_PHYSEL_GET(x) \
 (((x) & 0x00000003) >> 0)
#define SYSMGR_EMACGRP_CTRL_PHYSEL_SET(x) \
 (((x) << 0) & 0x00000003)

#endif /* _SYSTEM_MANAGER_H_ */
