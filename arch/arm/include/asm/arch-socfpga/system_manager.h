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

#ifdef CONFIG_SPL_BUILD
#ifndef __ASSEMBLY__
/* declaration for system_manager.c */
void sysmgr_pinmux_init(void);
void sysmgr_sdmmc_pweren_mux_check(void);
void ram_boot_setup(void);

/* declaration for handoff table type */
typedef unsigned long sys_mgr_pinmux_entry_t;
extern unsigned long sys_mgr_init_table[CONFIG_HPS_PINMUX_NUM];
#endif /* __ASSEMBLY__ */
#endif /* CONFIG_SPL_BUILD */

/* address */
#if defined(CONFIG_SOCFPGA_VIRTUAL_TARGET)
#define CONFIG_CPU1_START_ADDR		(SOCFPGA_SYSMGR_ADDRESS + 0x10)
/* For VT, temporary use ISW valid register */
#define CONFIG_SYSMGR_ROMCODEGRP_CTRL	(SOCFPGA_SYSMGR_ADDRESS + 0x14)
#define CONFIG_SYSMGR_ECCGRP		(SOCFPGA_SYSMGR_ADDRESS + 0x0)
#define CONFIG_SYSMGR_ECC_OCRAM		(SOCFPGA_SYSMGR_ADDRESS + 0x44)
#else
#define CONFIG_CPU1_START_ADDR		(SOCFPGA_SYSMGR_ADDRESS + 0xc4)
#define CONFIG_SYSMGR_ROMCODEGRP_CTRL	(SOCFPGA_SYSMGR_ADDRESS + 0xc0)
#define CONFIG_SYSMGR_ECCGRP		(SOCFPGA_SYSMGR_ADDRESS + 0x140)
#define CONFIG_SYSMGR_ECC_OCRAM		(CONFIG_SYSMGR_ECCGRP + 0x4)
#endif

/* FPGA interface group */
#define SYSMGR_FPGAINTF_MODULE		(SOCFPGA_SYSMGR_ADDRESS + 0x28)
/* EMAC interface selection */
#define CONFIG_SYSMGR_EMAC_CTRL		(SOCFPGA_SYSMGR_ADDRESS + 0x60)

/* Preloader handoff to bootloader register */
#define SYSMGR_ISWGRP_HANDOFF0		(SOCFPGA_SYSMGR_ADDRESS + 0x80)
#define SYSMGR_ISWGRP_HANDOFF1		(SOCFPGA_SYSMGR_ADDRESS + 0x84)
#define SYSMGR_ISWGRP_HANDOFF2		(SOCFPGA_SYSMGR_ADDRESS + 0x88)
#define SYSMGR_ISWGRP_HANDOFF3		(SOCFPGA_SYSMGR_ADDRESS + 0x8C)
#define SYSMGR_ISWGRP_HANDOFF4		(SOCFPGA_SYSMGR_ADDRESS + 0x90)
#define SYSMGR_ISWGRP_HANDOFF5		(SOCFPGA_SYSMGR_ADDRESS + 0x94)
#define SYSMGR_ISWGRP_HANDOFF6		(SOCFPGA_SYSMGR_ADDRESS + 0x98)
#define SYSMGR_ISWGRP_HANDOFF7		(SOCFPGA_SYSMGR_ADDRESS + 0x9C)

#define ISWGRP_HANDOFF_AXIBRIDGE	SYSMGR_ISWGRP_HANDOFF0
#define ISWGRP_HANDOFF_L3REMAP		SYSMGR_ISWGRP_HANDOFF1
#define ISWGRP_HANDOFF_FPGAINTF		SYSMGR_ISWGRP_HANDOFF2
#define ISWGRP_HANDOFF_FPGA2SDR		SYSMGR_ISWGRP_HANDOFF3
#define ISWGRP_HANDOFF_ROWBITS		SYSMGR_ISWGRP_HANDOFF4

/* pin mux */
#define SYSMGR_PINMUXGRP		(SOCFPGA_SYSMGR_ADDRESS + 0x400)
#define SYSMGR_PINMUXGRP_FLASHIO1	(SYSMGR_PINMUXGRP + 0x54)
#define SYSMGR_PINMUXGRP_NANDUSEFPGA	(SYSMGR_PINMUXGRP + 0x2F0)
#define SYSMGR_PINMUXGRP_EMAC1USEFPGA	(SYSMGR_PINMUXGRP + 0x2F8)
#define SYSMGR_PINMUXGRP_SDMMCUSEFPGA	(SYSMGR_PINMUXGRP + 0x308)
#define SYSMGR_PINMUXGRP_EMAC0USEFPGA	(SYSMGR_PINMUXGRP + 0x314)
#define SYSMGR_PINMUXGRP_SPIM1USEFPGA	(SYSMGR_PINMUXGRP + 0x330)
#define SYSMGR_PINMUXGRP_SPIM0USEFPGA	(SYSMGR_PINMUXGRP + 0x338)

/* bit fields */
#define CONFIG_SYSMGR_PINMUXGRP_OFFSET	(0x400)
#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGPINMUX		(1<<0)
#define SYSMGR_ROMCODEGRP_CTRL_WARMRSTCFGIO		(1<<1)
#define SYSMGR_ECC_OCRAM_EN		(1<<0)
#define SYSMGR_ECC_OCRAM_SERR		(1<<3)
#define SYSMGR_ECC_OCRAM_DERR		(1<<4)
#define SYSMGR_FPGAINTF_USEFPGA		0x1
#define SYSMGR_FPGAINTF_SPIM0		(1<<0)
#define SYSMGR_FPGAINTF_SPIM1		(1<<1)
#define SYSMGR_FPGAINTF_EMAC0		(1<<2)
#define SYSMGR_FPGAINTF_EMAC1		(1<<3)
#define SYSMGR_FPGAINTF_NAND		(1<<4)
#define SYSMGR_FPGAINTF_SDMMC		(1<<5)

/* Enumeration: sysmgr::emacgrp::ctrl::physel::enum                        */
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII 0x0
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII 0x1
#define SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII 0x2
#define SYSMGR_EMACGRP_CTRL_PHYSEL0_LSB 0
#define SYSMGR_EMACGRP_CTRL_PHYSEL1_LSB 2
#define SYSMGR_EMACGRP_CTRL_PHYSEL_MASK 0x00000003

/* CSEL */
#define SYSMGR_BOOTINFO			(SOCFPGA_SYSMGR_ADDRESS + 0x14)
#define SYSMGR_BOOTINFO_CSEL_LSB 	3
#define SYSMGR_BOOTINFO_CSEL_MASK	0x18
#define SYSMGR_BOOTINFO_BSEL_MASK	0x7

/* Warm RAM group */
#define CONFIG_SYSMGR_WARMRAMGRP_ENABLE		(SOCFPGA_SYSMGR_ADDRESS + 0xe0)
#define CONFIG_SYSMGR_WARMRAMGRP_EXECUTION	(SOCFPGA_SYSMGR_ADDRESS + 0xec)
#define CONFIG_SYSMGR_WARMRAMGRP_ENABLE_MAGIC		0xae9efebc
#define CONFIG_SYSMGR_WARMRAMGRP_EXECUTION_OFFSET_MASK	0xffff

#endif /* _SYSTEM_MANAGER_H_ */
