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

DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_fpga_manager *fpga_manager_base =
		(void *)SOCFPGA_FPGAMGRREGS_ADDRESS;

/* Check whether FPGA in user mode */
int is_fpgamgr_in_usermode(void)
{
	unsigned long val;
	DEBUG_MEMORY
	val = readl(&fpga_manager_base->stat);
	val = val & FPGAMGRREGS_STAT_MODE_MASK;
	if (val == FPGAMGRREGS_STAT_MODE_ENUM_USERMODE)
		return 1;
	else
		return 0;
}

/* Check whether FPGA bootFPGArdy signal is high */
int is_fpgamgr_bootfpgardy_high(void)
{
	unsigned long val;
	DEBUG_MEMORY
	val = readl(&fpga_manager_base->misci);
	if (val & FPGAMGRREGS_MISCI_BOOTFPGARDY_MASK)
		return 1;
	else
		return 0;
}

/* Check whether FPGA Init_Done signal is high */
int is_fpgamgr_initdone_high(void)
{
	unsigned long val;
	DEBUG_MEMORY
	val = readl(SOCFPGA_FPGAMGRREGS_ADDRESS +
		FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS);
	if (val & MON_GPIO_EXT_PORTA_ID_MASK)
		return 1;
	else
		return 0;
}

/* Check whether FPGA is ready to be accessed */
int is_fpgamgr_fpga_ready(void)
{
	unsigned long start, timeout;
	DEBUG_MEMORY
	reset_timer();
	timeout = FPGA_READY_TIMEOUT_US;
	start = get_timer(0);
	for ( ; get_timer(start) < timeout ; ) {
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


