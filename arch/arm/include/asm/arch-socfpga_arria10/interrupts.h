/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef	_SOCFPGA_INTERRUPTS_H_
#define	_SOCFPGA_INTERRUPTS_H_

/* Bootloader would handle up to 10 interrupts */
#define MAX_INT_VECTORS 10
#define INVERSE_NULL ((void *)(~0))

#ifndef __ASSEMBLY__
/* typedef for interrupt function pointer */
typedef void (*INTR_FXN_PTR)(void *);

/* struct for interrupt function pointer and arguement */
typedef struct {
	INTR_FXN_PTR fxn;
	void *arg;
	unsigned intrID;
} interrupt_struct;

/* function declaration */
int irq_register (unsigned char intrID, INTR_FXN_PTR fxn, void *arg,
	unsigned char level0edge1);
void irq_trigger (unsigned char intrID);
#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_INTERRUPTS_H_ */
