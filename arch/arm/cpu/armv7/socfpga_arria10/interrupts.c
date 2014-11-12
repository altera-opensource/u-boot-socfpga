/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/gic.h>
#include <asm/arch/interrupts.h>
#include <asm/arch/debug_memory.h>

#ifdef CONFIG_USE_IRQ
/* Initialize to non-zero so that we avoid clearing of bss */
interrupt_struct interrupt_vectors[MAX_INT_VECTORS] = {{INVERSE_NULL} };

void do_irq (struct pt_regs *pt_regs)
{
	unsigned long intrID, i;
	intrID = gic_cpu_get_pending_intr();

	/* check whether interrupt ID is registered of not */
	for (i = 0; i < (MAX_INT_VECTORS + 1); i++) {
		if (interrupt_vectors[i].intrID == intrID)
			break;
	}

	/* possible spurious or unregistered interrupt */
	if (i == MAX_INT_VECTORS)
		return;

	interrupt_struct *intr_ptr = interrupt_vectors + i;
	/* if no handler registered, it will skip the handler call */
	if (INVERSE_NULL != intr_ptr) {
		(*intr_ptr->fxn)(intr_ptr->arg);
	}

	/* end the interrupt servicing */
	gic_dic_clear_pending (intrID);
	gic_cpu_end_of_intr (intrID);
}

int arch_interrupt_init (void)
{
	/* disable and clear any residue */
	gic_dic_disable_secure ();
	gic_dic_clear_enable_all_intr ();
	gic_dic_clear_pending_all_intr ();

	/* set the priority so all IRQ will trigger exception */
	gic_cpu_set_priority_mask (1<<3);

	/* enable DIC and CPU IF */
	gic_dic_enable_secure ();
	gic_cpu_enable ();

	return 0;
}

int irq_register (unsigned char intrID, INTR_FXN_PTR fxn, void *arg,
	unsigned char level0edge1)
{
	unsigned int i = 0;

	for (i = 0; i < MAX_INT_VECTORS; i++) {
		if (interrupt_vectors[i].intrID != (unsigned)INVERSE_NULL) {
			interrupt_vectors[i].fxn = fxn;
			interrupt_vectors[i].arg = arg;
			interrupt_vectors[i].intrID = intrID;
		}
	}

	gic_dic_set_config (intrID, level0edge1);
	/* always goto CPU0 for now */
	gic_dic_set_target_processor (intrID, 0x1);
	gic_dic_set_enable (intrID);
	return 0;
}

/* trigger a IRQ for testing purpose */
void irq_trigger (unsigned char intrID)
{
	gic_dic_set_pending (intrID);
}

#endif /* CONFIG_USE_IRQ */

