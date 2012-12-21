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

#ifndef __LINUX_DW_MMC_H_
#define __LINUX_DW_MMC_H_

struct dw_host {
	struct dw_registers *reg;
	/* Need to set send_initialization bit in CMD register*/
	unsigned int need_init_cmd;
	unsigned int clock_in;

	unsigned int fifo_depth;
	unsigned int fifo_threshold;
	unsigned int num_bytes_written;		/* for write operation */

	void (*set_timing)(unsigned int clk_out);
	int (*use_hold_reg)(void);
};

int dw_mmc_init(struct dw_host *dw_host);

#endif /* __LINUX_DW_MMC_H_ */
