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

#define FPGAMGRREGS_MON_GPIO_PORTA_EOI_ADDRESS	0x84c
#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_ADDRESS	0x850

#define FPGAMGRREGS_CTRL_CFGWDTH_MASK		0x200
#define FPGAMGRREGS_CTRL_AXICFGEN_MASK		0x100
#define FPGAMGRREGS_CTRL_NCONFIGPULL_MASK	0x4
#define FPGAMGRREGS_CTRL_NCE_MASK		0x2
#define FPGAMGRREGS_CTRL_EN_MASK		0x1
#define FPGAMGRREGS_CTRL_CDRATIO_LSB		6

#define FPGAMGRREGS_STAT_MODE_MASK		0x7
#define FPGAMGRREGS_STAT_MSEL_MASK		0xf8
#define FPGAMGRREGS_STAT_MSEL_LSB		3

#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_CRC_MASK	0x8
#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_ID_MASK	0x4
#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_CD_MASK	0x2
#define FPGAMGRREGS_MON_GPIO_EXT_PORTA_NS_MASK	0x1

/* Timeout counter */
#define FPGA_TIMEOUT_CNT		0x1000000

/* FPGA Mode */
#define FPGAMGRREGS_MODE_FPGAOFF	0x0
#define FPGAMGRREGS_MODE_RESETPHASE	0x1
#define FPGAMGRREGS_MODE_CFGPHASE	0x2
#define FPGAMGRREGS_MODE_INITPHASE	0x3
#define FPGAMGRREGS_MODE_USERMODE	0x4
#define FPGAMGRREGS_MODE_UNKNOWN	0x5

/* FPGA CD Ratio Value */
#define CDRATIO_x1	0x0
#define CDRATIO_x2	0x1
#define CDRATIO_x4	0x2
#define CDRATIO_x8	0x3

/* Functions */
int is_fpgamgr_fpga_ready(void);
int poll_fpgamgr_fpga_ready(void);
int fpgamgr_program_init(void);
void fpgamgr_program_write(const unsigned long *rbf_data,
	unsigned long rbf_size);
int fpgamgr_program_poll_cd(void);
int fpgamgr_program_poll_initphase(void);
int fpgamgr_program_poll_usermode(void);
int fpgamgr_program_fpga(const unsigned long *rbf_data,
	unsigned long rbf_size);
void fpgamgr_axi_write(const unsigned long *rbf_data,
	const unsigned long fpgamgr_data_addr, unsigned long rbf_size);

#endif /* _FPGA_MANAGER_H_ */
