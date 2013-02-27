/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
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

#ifndef	_ECC_RAM_H_
#define	_ECC_RAM_H_

#define IRQ_ECC_OCRAM_CORRECTED		210
#define IRQ_ECC_OCRAM_UNCORRECTED	211

extern unsigned long irq_cnt_ecc_ocram_corrected;
extern unsigned long irq_cnt_ecc_ocram_uncorrected;

void irq_handler_ecc_ocram_corrected(void *arg);
void irq_handler_ecc_ocram_uncorrected(void *arg);

#endif /* _ECC_RAM_H_ */
