/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef	_RESET_MANAGER_H_
#define	_RESET_MANAGER_H_

#ifndef __ASSEMBLY__
void reset_cpu(ulong addr);
void reset_deassert_peripherals_handoff(void);

void socfpga_bridges_reset(int enable);

void socfpga_emac_reset(int enable);
void socfpga_watchdog_reset(void);

struct socfpga_reset_manager {
	u32	status;
	u32	ctrl;
	u32	counts;
	u32	padding1;
	u32	mpu_mod_reset;
	u32	per_mod_reset;
	u32	per2_mod_reset;
	u32	brg_mod_reset;
};
#endif

#if defined(CONFIG_SOCFPGA_VIRTUAL_TARGET)
#define RSTMGR_CTRL_SWWARMRSTREQ_LSB 2
#else
#define RSTMGR_CTRL_SWWARMRSTREQ_LSB 1
#endif
#define RSTMGR_STAT_SWCOLDRST_MASK		0x00000010
#define RSTMGR_STAT_CONFIGIOCOLDRST_MASK	0x00000008
#define RSTMGR_STAT_FPGACOLDRST_MASK		0x00000004
#define RSTMGR_STAT_NPORPINRST_MASK		0x00000002
#define RSTMGR_STAT_PORVOLTRST_MASK		0x00000001

#define RSTMGR_COLDRST_MASK     (\
	RSTMGR_STAT_SWCOLDRST_MASK | \
	RSTMGR_STAT_CONFIGIOCOLDRST_MASK | \
	RSTMGR_STAT_FPGACOLDRST_MASK | \
	RSTMGR_STAT_NPORPINRST_MASK | \
	RSTMGR_STAT_PORVOLTRST_MASK)

#define RSTMGR_PERMODRST_EMAC0_LSB	0
#define RSTMGR_PERMODRST_EMAC1_LSB	1
#define RSTMGR_PERMODRST_L4WD0_LSB	6

#endif /* _RESET_MANAGER_H_ */
