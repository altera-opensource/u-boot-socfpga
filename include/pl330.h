/*
 * Copyright (C) 2013 Altera Corporation <www.altera.com>
 *
 * Copyright (C) 2010 Samsung Electronics Co. Ltd.
 *	Jaswinder Singh <jassi.brar@samsung.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <asm/arch/pl330_csr.h>

#ifndef __PL330_CORE_H
#define __PL330_CORE_H

#define PL330_MAX_CHAN		8
#define PL330_MAX_IRQS		32
#define PL330_MAX_PERI		32

#define PL330_STATE_STOPPED		(1 << 0)
#define PL330_STATE_EXECUTING		(1 << 1)
#define PL330_STATE_WFE			(1 << 2)
#define PL330_STATE_FAULTING		(1 << 3)
#define PL330_STATE_COMPLETING		(1 << 4)
#define PL330_STATE_WFP			(1 << 5)
#define PL330_STATE_KILLING		(1 << 6)
#define PL330_STATE_FAULT_COMPLETING	(1 << 7)
#define PL330_STATE_CACHEMISS		(1 << 8)
#define PL330_STATE_UPDTPC		(1 << 9)
#define PL330_STATE_ATBARRIER		(1 << 10)
#define PL330_STATE_QUEUEBUSY		(1 << 11)
#define PL330_STATE_INVALID		(1 << 15)


enum pl330_srccachectrl {
	SCCTRL0 = 0,	/* Noncacheable and nonbufferable */
	SCCTRL1,	/* Bufferable only */
	SCCTRL2,	/* Cacheable, but do not allocate */
	SCCTRL3,	/* Cacheable and bufferable, but do not allocate */
	SINVALID1,
	SINVALID2,
	SCCTRL6,	/* Cacheable write-through, allocate on reads only */
	SCCTRL7,	/* Cacheable write-back, allocate on reads only */
};

enum pl330_dstcachectrl {
	DCCTRL0 = 0,	/* Noncacheable and nonbufferable */
	DCCTRL1,	/* Bufferable only */
	DCCTRL2,	/* Cacheable, but do not allocate */
	DCCTRL3,	/* Cacheable and bufferable, but do not allocate */
	DINVALID1 = 8,
	DINVALID2,
	DCCTRL6,	/* Cacheable write-through, allocate on writes only */
	DCCTRL7,	/* Cacheable write-back, allocate on writes only */
};

/* Populated by the PL330 core driver for DMA API driver's info */
struct pl330_config {
	u32	periph_id;
	u32	pcell_id;
#define DMAC_MODE_NS	(1 << 0)
	unsigned int	mode;
	unsigned int	data_bus_width:10; /* In number of bits */
	unsigned int	data_buf_dep:10;
	unsigned int	num_chan:4;
	unsigned int	num_peri:6;
	u32		peri_ns;
	unsigned int	num_events:6;
	u32		irq_ns;
	unsigned int	cache_line_width;
	unsigned int	cache_line_num;
	unsigned int	read_issuing;
	unsigned int	read_queue_depth;
	unsigned int	write_issuing;
	unsigned int	write_queue_depth;
};

enum pl330_byteswap {
	SWAP_NO = 0,
	SWAP_2,
	SWAP_4,
	SWAP_8,
	SWAP_16,
};

enum pl330_transfer_type {
	MEM2MEM = 0,
	MEM2PERIPH,
	PERIPH2MEM,
};

/*
 * Request Configuration.
 * The PL330 core does not modify this and uses the last
 * working configuration if the request doesn't provide any.
 *
 * The Client may want to provide this info only for the
 * first request and a request with new settings.
 */
struct pl330_reqcfg {
	/* Address Incrementing */
	unsigned dst_inc:1;
	unsigned src_inc:1;

	/*
	 * For now, the SRC & DST protection levels
	 * and burst size/length are assumed same.
	 */
	int nonsecure;
	int privileged;
	int insnaccess;
	unsigned brst_len:5;
	unsigned brst_size:3; /* in power of 2 */

	enum pl330_dstcachectrl dcctl;
	enum pl330_srccachectrl scctl;
	enum pl330_byteswap swap;
};

/* structure to be passed in for pl330_transfer_x */
struct pl330_transfer_struct {
	unsigned long channel_num;
	unsigned long src_addr;
	unsigned long dst_addr;
	unsigned long size_byte;
	unsigned long brst_size;
	unsigned long single_brst_size;
	unsigned long brst_len;
	unsigned long peripheral_id;
	unsigned long transfer_type;
	unsigned long enable_cache1;
	unsigned long buf_size;
	u8 *buf;
};

/* function declaration */
void pl330_stop(int channel0_manager1, int channel_num, int timeout_loops);
int pl330_transfer_setup(struct pl330_transfer_struct *pl330);
int pl330_transfer_start(struct pl330_transfer_struct *pl330);
int pl330_transfer_finish(struct pl330_transfer_struct *pl330);
int pl330_transfer_zeroes(struct pl330_transfer_struct *pl330);

#endif	/* __PL330_CORE_H */
