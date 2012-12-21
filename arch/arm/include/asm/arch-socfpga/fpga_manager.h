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

#ifndef	_FPGA_MANAGER_H_
#define	_FPGA_MANAGER_H_

struct socfpga_fpga_manager {
	u32	stat;
	u32	ctrl;
	u32	dclkcnt;
	u32	dclkstat;
	u32	gpo;
	u32	gpi;
	u32	misci;
};

#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS	0x850

#define FPGAMGRREGS_STAT_MODE_ENUM_USERMODE 0x4
#define FPGAMGRREGS_STAT_MODE_MASK 0x00000007
#define FPGAMGRREGS_MISCI_BOOTFPGARDY_MASK 0x00000002
#define MON_GPIO_EXT_PORTA_ID_MASK 0x00000004

/* timeout in unit of us as CONFIG_SYS_HZ = 1000*1000 */
#define FPGA_READY_TIMEOUT_US	5000000

int is_fpgamgr_in_usermode(void);
int is_fpgamgr_bootfpgardy_high(void);
int is_fpgamgr_initdone_high(void);
int is_fpgamgr_fpga_ready(void);

#endif /* _FPGA_MANAGER_H_ */
