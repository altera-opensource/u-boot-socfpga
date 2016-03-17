/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
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

/* GIC number */
#define IRQ_ECC_SERR				34
#define IRQ_ECC_DERR				32

/* function declaration */
int irq_register(unsigned int irqID, INTR_FXN_PTR fxn, void *arg,
	unsigned char level0edge1);
void irq_trigger(unsigned int intrID);
void ocram_ecc_masking_interrupt(unsigned mask);
void irq_handler_ecc_serr(void *arg);
void irq_handler_ecc_derr(void *arg);
void ddr0_ecc_masking_interrupt(unsigned mask);
#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_INTERRUPTS_H_ */
