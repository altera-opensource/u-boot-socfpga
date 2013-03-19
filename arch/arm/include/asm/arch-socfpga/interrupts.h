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

#ifndef	_INTERRUPTS_H_
#define	_INTERRUPTS_H_

#ifdef CONFIG_USE_IRQ

/* typedef for interrupt function pointer */
typedef void (*INTR_FXN_PTR)(void *);

/* struct for interrupt function pointer and arguement */
typedef struct {
	INTR_FXN_PTR fxn;
	void *arg;
} interrupt_struct;

#define MAX_INT_VECTORS 212
#define INVERSE_NULL ((void *)(~0))

/* function declaration */
int irq_register (unsigned char intrID, INTR_FXN_PTR fxn, void *arg,
	unsigned char level0edge1);
void irq_trigger (unsigned char intrID);

#endif /* CONFIG_USE_IRQ */

#endif /* _INTERRUPTS_H_ */
