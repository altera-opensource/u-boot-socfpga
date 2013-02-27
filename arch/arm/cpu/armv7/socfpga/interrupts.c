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
	unsigned long intrID;
	DEBUG_MEMORY
	intrID = gic_cpu_get_pending_intr ();

	/* possible spurious interrupt */
	if (intrID >= MAX_INT_VECTORS)
		return;

	interrupt_struct *intr_ptr = interrupt_vectors + intrID;
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
	DEBUG_MEMORY
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
	DEBUG_MEMORY
	interrupt_vectors[intrID].fxn = fxn;
	interrupt_vectors[intrID].arg = arg;
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

