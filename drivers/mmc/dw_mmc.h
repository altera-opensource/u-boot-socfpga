/*
 *
 * Copyright (C) 2012
 * Altera Corporation <www.altera.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */

#include <common.h>
#include <mmc.h>

#ifndef __DW_MMC_H_
#define __DW_MMC_H_

#include <linux/dw_mmc.h>

#define DEFAULT_DEBNCE_VAL		(0x0FFFFF)

#define BIT(n)				(1UL << (n))
#define GET_FIFO_DEPTH(x)		((((x)&0x0FFF0000)>>16)+1)
#define FIFO_WIDTH			(1024)
#define FIFO_WIDTH_BYTES		(4)

#define CMD_MAX_RETRIES			(1000)

struct dw_registers {
	u32 ctrl;			/* 0x00 */
	u32 pwren;			/* 0x04 */
	u32 clkdiv;			/* 0x08 */
	u32 clksrc;			/* 0x0c */
	u32 clkena;			/* 0x10 */
	u32 tmout;			/* 0x14 */
	u32 ctype;			/* 0x18 */
	u32 blksiz;			/* 0x1c */
	u32 bytcnt;			/* 0x20 */
	u32 intmsk;			/* 0x24 */
	u32 cmdarg;			/* 0x28 */
	u32 cmd;			/* 0x2c */
	u32 resp0;			/* 0x30 */
	u32 resp1;			/* 0x34 */
	u32 resp2;			/* 0x38 */
	u32 resp3;			/* 0x3c */
	u32 mintsts;			/* 0x40 */
	u32 rintsts;			/* 0x44 */
	u32 status;			/* 0x48 */
	u32 fifoth;			/* 0x4c */
	u32 cdetect;			/* 0x50 */
	u32 wrtprt;			/* 0x54 */
	u32 gpio;			/* 0x58 */
	u32 tcbcnt;			/* 0x5c */
	u32 tbbcnt;			/* 0x60 */
	u32 debnce;			/* 0x64 */
	u32 usrid;			/* 0x68 */
	u32 verid;			/* 0x6c */
	u32 hcon;			/* 0x70 */
	u32 uhs_reg;			/* 0x74 */
	u32 rst_n;			/* 0x78 */
	u32 reserved1;			/* 0x7c */
	u32 bmod;			/* 0x80 */
	u32 pldmnd;			/* 0x84 */
	u32 dbaddr;			/* 0x88 */
	u32 idsts;			/* 0x8c */
	u32 idinten;			/* 0x90 */
	u32 dscaddr;			/* 0x94 */
	u32 bufaddr;			/* 0x98 */
	u32 reserved2[(0x100-0x9c)>>2]; /* 0x9c-0xff */
	u32 cardthrctl;			/* 0x100 */
	u32 bepwr;			/* 0x104 */
	u32 reserved3[(0x200-0x108)>>2]; /* 0x108-0x1ff */
	u32 fifodata;			/* 0x200 */
};

/* Control register defines */
#define CTRL_USE_IDMAC			BIT(25)
#define CTRL_ENABLE_OD_PULLUP		BIT(24)
#define CTRL_CEATA_INT_EN		BIT(11)
#define CTRL_SEND_AS_CCSD		BIT(10)
#define CTRL_SEND_CCSD			BIT(9)
#define CTRL_ABRT_READ_DATA		BIT(8)
#define CTRL_SEND_IRQ_RESP		BIT(7)
#define CTRL_READ_WAIT			BIT(6)
#define CTRL_DMA_ENABLE			BIT(5)
#define CTRL_INT_ENABLE			BIT(4)
#define CTRL_DMA_RESET			BIT(2)
#define CTRL_FIFO_RESET			BIT(1)
#define CTRL_RESET			BIT(0)

/* Clock Enable register defines */
#define SDMMC_CLKEN_LOW_PWR		BIT(16)
#define SDMMC_CLKEN_ENABLE		BIT(0)

/* Command register defines */
#define SDMMC_CMD_START			BIT(31)
#define SDMMC_CMD_USE_HOLD_REG		BIT(29)
#define SDMMC_CMD_CCS_EXP		BIT(23)
#define SDMMC_CMD_CEATA_RD		BIT(22)
#define SDMMC_CMD_UPD_CLK		BIT(21)
#define SDMMC_CMD_INIT			BIT(15)
#define SDMMC_CMD_STOP			BIT(14)
#define SDMMC_CMD_PRV_DAT_WAIT		BIT(13)
#define SDMMC_CMD_SEND_STOP		BIT(12)
#define SDMMC_CMD_STRM_MODE		BIT(11)
#define SDMMC_CMD_DAT_WR		BIT(10)
#define SDMMC_CMD_DAT_EXP		BIT(9)
#define SDMMC_CMD_RESP_CRC		BIT(8)
#define SDMMC_CMD_RESP_LONG		BIT(7)
#define SDMMC_CMD_RESP_EXP		BIT(6)
#define SDMMC_CMD_INDX_MASK		(0x3F)
#define SDMMC_CMD_INDX(n)		((n) & SDMMC_CMD_INDX_MASK)

/* Interrupt mask defines */

#define INTMSK_CDETECT			BIT(0)
#define INTMSK_RESP_ERR			BIT(1)
#define INTMSK_CMD_DONE			BIT(2)
#define INTMSK_DAT_OVER			BIT(3)
#define INTMSK_TXDR			BIT(4)
#define INTMSK_RXDR			BIT(5)
#define INTMSK_RCRC			BIT(6)
#define INTMSK_DCRC			BIT(7)
#define INTMSK_RTO			BIT(8)
#define INTMSK_DTO			BIT(9)
#define INTMSK_HTO			BIT(10)
#define INTMSK_FRUN			BIT(11)
#define INTMSK_HLE			BIT(12)
#define INTMSK_SBE			BIT(13)
#define INTMSK_ACD			BIT(14)
#define INTMSK_EBE			BIT(15)
#define STATUS_INT_ERR		(INTMSK_RESP_ERR | INTMSK_RCRC | INTMSK_DCRC  |\
				INTMSK_RTO | INTMSK_DTO | INTMSK_EBE)

#define INTMSK_ALL_ENABLED		0xffffffff

/* card-type register defines */
#define SDMMC_CTYPE_8BIT		BIT(16)
#define SDMMC_CTYPE_4BIT		BIT(0)
#define SDMMC_CTYPE_1BIT		0

#define GET_FIFO_COUNT(x)		(((x)&0x3ffe0000)>>17)

#endif /* __DW_MMC_H_ */
