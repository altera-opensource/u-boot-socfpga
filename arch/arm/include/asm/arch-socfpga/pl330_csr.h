/*
 *  Copyright (C) 2013 Altera Corporation <www.altera.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PL330_CSR_H_
#define _PL330_CSR_H_

#define PL330_DMA_BASE_SECURE		SOCFPGA_DMA_SECURE_ADDRESS
#define PL330_DMA_BASE_NON_SECURE	SOCFPGA_DMA_NON_SECURE_ADDRESS
/* burst size =  2 ^ PL330_DMA_MAX_BURST_SIZE */
#define PL330_DMA_MAX_BURST_SIZE	3

/*
 * Mode used whether secure or non secure. Its tightly coupled with the
 * hardware signal lines state prior PL330 release from reset
 */
#define PL330_DMA_BASE			PL330_DMA_BASE_SECURE


#endif /* _PL330_CSR_H_ */
