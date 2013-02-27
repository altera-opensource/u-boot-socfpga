/*
 *
 * Copyright (C) 2012 Altera Corporation <www.altera.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  - Neither the name of the Altera Corporation nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ALTERA CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/debug_memory.h>
#include <asm/arch/reset_manager.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_fpga_manager *fpga_manager_base =
		(void *)SOCFPGA_FPGAMGRREGS_ADDRESS;

/* Check whether FPGA Init_Done signal is high */
static int is_fpgamgr_initdone_high(void)
{
	unsigned long val;
	DEBUG_MEMORY
	val = readl(SOCFPGA_FPGAMGRREGS_ADDRESS +
		FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS);
	if (val & FPGAMGRREGS_MON_GPIO_EXT_PORTA_ID_MASK)
		return 1;
	else
		return 0;
}

/* Check whether FPGA is ready to be accessed */
int is_fpgamgr_fpga_ready(void)
{
	unsigned long i;
	DEBUG_MEMORY
	/* If FPGA is blank, wait till WD invoke warm reset */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		/* check whether in user mode */
		if (is_fpgamgr_initdone_high() == 0)
			continue;
		/* check again whether in user mode */
		if (is_fpgamgr_initdone_high() == 0)
			continue;
		return 1;
	}
	DEBUG_MEMORY
	return 0;
}

/* set CD ratio */
static void fpgamgr_set_cd_ratio(unsigned long ratio)
{
	unsigned long reg;
	reg = readl(&fpga_manager_base->ctrl);
	reg = (reg & ~(0x3 << FPGAMGRREGS_CTRL_CDRATIO_LSB)) |
		((ratio & 0x3) << FPGAMGRREGS_CTRL_CDRATIO_LSB);
	writel(reg, &fpga_manager_base->ctrl);
}

/* Get the FPGA mode */
static int fpgamgr_get_mode(void)
{
	unsigned long val;
	DEBUG_MEMORY
	val = readl(&fpga_manager_base->stat);
	val = val & FPGAMGRREGS_STAT_MODE_MASK;
	return val;
}

static int fpgamgr_dclkcnt_set(unsigned long cnt)
{
	unsigned long i;

	/* clear any existing done status */
	if (readl(&fpga_manager_base->dclkstat))
		writel(0x1, &fpga_manager_base->dclkstat);
	/* write the dclkcnt */
	writel(cnt, &fpga_manager_base->dclkcnt);
	/*
	 * wait till the dclkcnt done
	 */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (readl(&fpga_manager_base->dclkstat)) {
			writel(0x1, &fpga_manager_base->dclkstat);
			return 0;
		}
	}
	return -1;
}

/*
 * Using FPGA Manager to program the FPGA
 * Return 0 for sucess
 */
int fpgamgr_program_fpga(const unsigned long *rbf_data,
	unsigned long rbf_size)
{
	unsigned long reg, i;

	/* get the MSEL value */
	reg = readl(&fpga_manager_base->stat);
	reg = ((reg & FPGAMGRREGS_STAT_MSEL_MASK) >> FPGAMGRREGS_STAT_MSEL_LSB);

	/*
	 * Set the cfg width
	 * If MSEL[3] = 1, cfg width = 32 bit
	 */
	if (reg & 0x8)
		setbits_le32(&fpga_manager_base->ctrl,
			FPGAMGRREGS_CTRL_CFGWDTH_MASK);
	else
		clrbits_le32(&fpga_manager_base->ctrl,
			FPGAMGRREGS_CTRL_CFGWDTH_MASK);

	/* To determine the CD ratio */
	/* MSEL[3] = 1 & MSEL[1:0] = 0, CD Ratio = 1 */
	if ((reg & 0xb) == 0x8)
		fpgamgr_set_cd_ratio(CDRATIO_x1);
	/* MSEL[3] = 1 & MSEL[1:0] = 1, CD Ratio = 4 */
	else if ((reg & 0xb) == 0x9)
		fpgamgr_set_cd_ratio(CDRATIO_x4);
	/* MSEL[3] = 1 & MSEL[1:0] = 2, CD Ratio = 8 */
	else if ((reg & 0xb) == 0xa)
		fpgamgr_set_cd_ratio(CDRATIO_x8);
	/* MSEL[3] = 0 & MSEL[1:0] = 0, CD Ratio = 1 */
	else if ((reg & 0xb) == 0x0)
		fpgamgr_set_cd_ratio(CDRATIO_x1);
	/* MSEL[3] = 0 & MSEL[1:0] = 1, CD Ratio = 2 */
	else if ((reg & 0xb) == 0x1)
		fpgamgr_set_cd_ratio(CDRATIO_x2);
	/* MSEL[3] = 0 & MSEL[1:0] = 2, CD Ratio = 4 */
	else if ((reg & 0xb) == 0x2)
		fpgamgr_set_cd_ratio(CDRATIO_x4);

	/* to enable FPGA Manager configuration */
	clrbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_NCE_MASK);

	/* to enable FPGA Manager drive over configuration line */
	setbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_EN_MASK);

	/* put FPGA into reset phase */
	setbits_le32(&fpga_manager_base->ctrl,
		FPGAMGRREGS_CTRL_NCONFIGPULL_MASK);

	/* (1) wait until FPGA enter reset phase */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_RESETPHASE)
			break;
	}
	/* if not in reset state, return error */
	if (fpgamgr_get_mode() != FPGAMGRREGS_MODE_RESETPHASE)
		return -1;

	/* release FPGA from reset phase */
	clrbits_le32(&fpga_manager_base->ctrl,
		FPGAMGRREGS_CTRL_NCONFIGPULL_MASK);

	/* (2) wait until FPGA enter configuration phase */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_CFGPHASE)
			break;
	}
	/* if not in configuration state, return error */
	if (fpgamgr_get_mode() != FPGAMGRREGS_MODE_CFGPHASE)
		return -2;

	/* clear all interrupt in CB Monitor */
	writel(0xFFF, (SOCFPGA_FPGAMGRREGS_ADDRESS +
		FPGAMGRREGS_MON_GPIO_PORTA_EOI_ADDRESS));

	/* enable AXI configuration */
	setbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_AXICFGEN_MASK);

	/* write to FPGA Manager AXI data */
	for (i = 0; i < rbf_size; i = i + 4) {
		reg = rbf_data[i/4];
		writel(reg, SOCFPGA_FPGAMGRDATA_ADDRESS);
		reg = readl(SOCFPGA_FPGAMGRREGS_ADDRESS +
			FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS);
	}

	/* (3) wait until full config done */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		reg = readl(SOCFPGA_FPGAMGRREGS_ADDRESS +
			FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS);
		/* config error */
		if (!(reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_NS_MASK) &&
			!(reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_CD_MASK))
			return -3;
		/* config done without error */
		if ((reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_NS_MASK) &&
			(reg & FPGAMGRREGS_MON_GPIO_EXT_PORTA_CD_MASK))
			break;
	}
	/* tiemout happen, return error */
	if (i == FPGA_TIMEOUT_CNT)
		return -4;

	/* disable AXI configuration */
	clrbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_AXICFGEN_MASK);

	/* additional clocks for the CB to enter initialization phase */
	if (fpgamgr_dclkcnt_set(0x4) != 0)
		return -5;

	/* (4) wait until FPGA enter init phase */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_INITPHASE)
			break;
	}
	/* if not in configuration state, return error */
	if (i == FPGA_TIMEOUT_CNT)
		return -6;

	/* additional clocks for the CB to exit initialization phase */
	if (fpgamgr_dclkcnt_set(0x5000) != 0)
		return -7;

	/* (5) wait until FPGA enter user mode */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_USERMODE)
			break;
	}
	/* if not in configuration state, return error */
	if (i == FPGA_TIMEOUT_CNT)
		return -8;

	/* to release FPGA Manager drive over configuration line */
	clrbits_le32(&fpga_manager_base->ctrl, FPGAMGRREGS_CTRL_EN_MASK);

	/* release bridge from reset in case Preloader skip it */
	reset_deassert_all_bridges();

	return 0;
}
