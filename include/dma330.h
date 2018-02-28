/*
 * Copyright (C) 2018 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <asm/arch/dma330_csr.h>
#include <dma.h>

#ifndef __DMA330_CORE_H
#define __DMA330_CORE_H

#define DMA330_MAX_CHAN		8
#define DMA330_MAX_IRQS		32
#define DMA330_MAX_PERI		32

#define DMA330_STATE_STOPPED		(1 << 0)
#define DMA330_STATE_EXECUTING		(1 << 1)
#define DMA330_STATE_WFE		(1 << 2)
#define DMA330_STATE_FAULTING		(1 << 3)
#define DMA330_STATE_COMPLETING		(1 << 4)
#define DMA330_STATE_WFP		(1 << 5)
#define DMA330_STATE_KILLING		(1 << 6)
#define DMA330_STATE_FAULT_COMPLETING	(1 << 7)
#define DMA330_STATE_CACHEMISS		(1 << 8)
#define DMA330_STATE_UPDTPC		(1 << 9)
#define DMA330_STATE_ATBARRIER		(1 << 10)
#define DMA330_STATE_QUEUEBUSY		(1 << 11)
#define DMA330_STATE_INVALID		(1 << 15)

enum dma330_srccachectrl {
	SCCTRL0 = 0,	/* Noncacheable and nonbufferable */
	SCCTRL1,	/* Bufferable only */
	SCCTRL2,	/* Cacheable, but do not allocate */
	SCCTRL3,	/* Cacheable and bufferable, but do not allocate */
	SINVALID1,
	SINVALID2,
	SCCTRL6,	/* Cacheable write-through, allocate on reads only */
	SCCTRL7,	/* Cacheable write-back, allocate on reads only */
};

enum dma330_dstcachectrl {
	DCCTRL0 = 0,	/* Noncacheable and nonbufferable */
	DCCTRL1,	/* Bufferable only */
	DCCTRL2,	/* Cacheable, but do not allocate */
	DCCTRL3,	/* Cacheable and bufferable, but do not allocate */
	DINVALID1 = 8,
	DINVALID2,
	DCCTRL6,	/* Cacheable write-through, allocate on writes only */
	DCCTRL7,	/* Cacheable write-back, allocate on writes only */
};

enum dma330_byteswap {
	SWAP_NO = 0,
	SWAP_2,
	SWAP_4,
	SWAP_8,
	SWAP_16,
};

/**
 * dma330_reqcfg - Request Configuration.
 *
 * The DMA330 core does not modify this and uses the last
 * working configuration if the request doesn't provide any.
 *
 * The Client may want to provide this info only for the
 * first request and a request with new settings.
 */
struct dma330_reqcfg {
	/* Address Incrementing */
	u8 dst_inc;
	u8 src_inc;

	/*
	 * For now, the SRC & DST protection levels
	 * and burst size/length are assumed same.
	 */
	u8 nonsecure;
	u8 privileged;
	u8 insnaccess;
	u8 brst_len;
	u8 brst_size; /* in power of 2 */

	enum dma330_dstcachectrl dcctl;
	enum dma330_srccachectrl scctl;
	enum dma330_byteswap swap;
};

/**
 * dma330_transfer_struct - Structure to be passed in for dma330_transfer_x.
 *
 * @channel_num: Channel number assigned, valid from 0 to 7.
 * @src_addr: Address to transfer from / source.
 * @dst_addr: Address to transfer to / destination.
 * @size_byte: Number of bytes to be transferred.
 * @brst_size: Valid from 0 - 3,
 *	       where 0 = 1 (2 ^ 0) bytes and 3 = 8 bytes (2 ^ 3).
 * @single_brst_size: Single transfer size (from 0 - 3).
 * @brst_len: Valid from 1 - 16 where each burst can trasfer 1 - 16
 *	      Data chunk (each chunk size equivalent to brst_size).
 * @peripheral_id: Assigned peripheral_id, valid from 0 to 31.
 * @transfer_type: MEM2MEM, MEM2PERIPH or PERIPH2MEM.
 * @enable_cache1: 1 for cache enabled for memory
 *		   (cacheable and bufferable, but do not allocate).
 * @buf_size: sizeof(buf).
 * @reg_base: DMA base address(nonsecure base or secure base)
 * @buf: Buffer handler which will point to the memory
 *	 allocated for dma microcode
 *
 * Description of the structure.
 */
struct dma330_transfer_struct {
	u32 channel0_manager1;
	u32 channel_num;
	u32 src_addr;
	u32 dst_addr;
	u32 size_byte;
	u32 brst_size;
	u32 single_brst_size;
	u32 brst_len;
	u32 peripheral_id;
	u32 transfer_type;
	u32 enable_cache1;
	u32 buf_size;
	u32 reg_base;
	u32 *buf;
};

/* Functions declaration */
void dma330_stop(struct dma330_transfer_struct *dma330, u32 timeout_ms);
int dma330_transfer_setup(struct dma330_transfer_struct *dma330);
int dma330_transfer_start(struct dma330_transfer_struct *dma330);
int dma330_transfer_finish(struct dma330_transfer_struct *dma330);
int dma330_transfer_zeroes(struct dma330_transfer_struct *dma330);

#endif	/* __DMA330_CORE_H */
