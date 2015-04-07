/*
 * Copyright (C) 2015 Altera Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PL330_CSR_H_
#define _PL330_CSR_H_

#define PL330_DMA_BASE_SECURE		SOCFPGA_DMASECURE_ADDRESS
#define PL330_DMA_BASE_NON_SECURE	SOCFPGA_DMANONSECURE_ADDRESS
/* burst size =  2 ^ PL330_DMA_MAX_BURST_SIZE */
#define PL330_DMA_MAX_BURST_SIZE	3

/*
 * Mode used whether secure or non secure. Its tightly coupled with the
 * hardware signal lines state prior PL330 release from reset
 */
#define PL330_DMA_BASE			PL330_DMA_BASE_SECURE


#endif /* _PL330_CSR_H_ */
