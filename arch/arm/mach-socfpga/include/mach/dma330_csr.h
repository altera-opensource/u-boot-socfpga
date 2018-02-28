/*
 * Copyright (C) 2018 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef _DMA330_CSR_H_
#define _DMA330_CSR_H_

#define DMA330_BASE_SECURE	SOCFPGA_DMASECURE_ADDRESS
#define DMA330_BASE_NON_SECURE	SOCFPGA_DMANONSECURE_ADDRESS
/* burst size =  2 ^ DMA330_DMA_MAX_BURST_SIZE */
#define DMA330_MAX_BURST_SIZE	2

#endif /* _DMA330_CSR_H_ */
