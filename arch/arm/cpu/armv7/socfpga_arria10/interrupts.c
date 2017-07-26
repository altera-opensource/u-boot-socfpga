/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/gic.h>
#include <asm/arch/interrupts.h>
#include <asm/arch/debug_memory.h>
#include <asm/arch/ecc_ram.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/sdram.h>

#ifdef CONFIG_USE_IRQ
interrupt_struct interrupt_vectors[MAX_INT_VECTORS];

void do_irq (struct pt_regs *pt_regs)
{
	unsigned long intrID, i;
	intrID = gic_cpu_get_pending_intr();

	/* check whether interrupt ID is registered of not */
	for (i = 0; i < MAX_INT_VECTORS; i++) {
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

	/* Initialize to non-zero so that we avoid clearing of bss */
	memset(interrupt_vectors, (int) INVERSE_NULL,
		sizeof(interrupt_vectors));

	if (is_ocram_ecc_enabled() || is_sdram_ecc_enabled()) {
		irq_register(IRQ_ECC_SERR,
			irq_handler_ecc_serr,
			0, 0);

		irq_register(IRQ_ECC_DERR,
			irq_handler_ecc_derr,
			0, 0);
	}

	if (is_ocram_ecc_enabled())
		ocram_ecc_masking_interrupt(0);

	if (is_sdram_ecc_enabled()) {
		ddr0_ecc_masking_interrupt(0);

		sb_err_enable_interrupt(1);

		ext_addrparity_err_enable_interrupt(1);

		db_err_enable_interrupt(1);
	}

	return 0;
}

int irq_register(unsigned int irqID, INTR_FXN_PTR fxn, void *arg,
	unsigned char level0edge1)
{
	unsigned int i = 0;
	unsigned int intrID = irqID;

	for (i = 0; i < MAX_INT_VECTORS; i++) {
		if (interrupt_vectors[i].intrID == (unsigned)INVERSE_NULL) {
			interrupt_vectors[i].fxn = fxn;
			interrupt_vectors[i].arg = arg;
			interrupt_vectors[i].intrID = intrID;
			break;
		}
	}

	gic_dic_set_config (intrID, level0edge1);
	/* always goto CPU0 for now */
	gic_dic_set_target_processor (intrID, 0x1);
	gic_dic_set_enable (intrID);
	return 0;
}

/* trigger a IRQ for testing purpose */
void irq_trigger(unsigned int intrID)
{
	gic_dic_set_pending (intrID);
}

/* Masking DDR0 ecc interrupt in system manager */
void ddr0_ecc_masking_interrupt(unsigned mask)
{
	const struct socfpga_system_manager *socfpga_system_mgr =
		(void *)SOCFPGA_SYSMGR_ADDRESS;

	if (mask)
		writel(ALT_SYSMGR_ECC_INT_DDR0_MSK,
			&socfpga_system_mgr->ecc_intmask_set);
	else
		writel(ALT_SYSMGR_ECC_INT_DDR0_MSK,
			&socfpga_system_mgr->ecc_intmask_clr);

	return;
}

/* Masking OCRAM ecc interrupt in system manager */
void ocram_ecc_masking_interrupt(unsigned mask)
{
	const struct socfpga_system_manager *socfpga_system_mgr =
		(void *)SOCFPGA_SYSMGR_ADDRESS;

	if (mask)
		writel(ALT_SYSMGR_ECC_INT_OCRAM_MSK,
			&socfpga_system_mgr->ecc_intmask_set);
	else
		writel(ALT_SYSMGR_ECC_INT_OCRAM_MSK,
			&socfpga_system_mgr->ecc_intmask_clr);

	return;
}

/* Handler for sb interrupt */
void irq_handler_ecc_serr(void *arg)
{
	unsigned int source_reg_value = 0;
	unsigned source_num = 0;
	unsigned source = 0;
	const struct socfpga_system_manager *socfpga_system_mgr =
		(void *)SOCFPGA_SYSMGR_ADDRESS;

	/* Checking interrupt triggered by which ECC sources */
	source_reg_value = readl(&socfpga_system_mgr->ecc_intstatus_serr);

	for (source_num = 0; source_num < 19; source_num++) {
		source = (source_reg_value >> source_num) & 1;

		if (source) {
			switch (source_num) {
			case 1:
				irq_handler_ecc_ram_serr();
				break;
			case 17:
				irq_handler_DDR_ecc_serr();
				break;
			case 0:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
			case 16:
			case 18:
				printf("Can't find serr irq handler ");
				printf("for source %d\n", source_num);
				break;
			default:
				printf("invalid SBE interrupt ");
				printf("source\n");
			}
		}
	}

	return;
}

/* Handler for db interrupt */
void irq_handler_ecc_derr(void *arg)
{
	unsigned int source_reg_value = 0;
	unsigned source_num = 0;
	unsigned source = 0;
	const struct socfpga_system_manager *socfpga_system_mgr =
		(void *)SOCFPGA_SYSMGR_ADDRESS;

	/* Checking interrupt triggered by which ECC sources */
	source_reg_value = readl(&socfpga_system_mgr->ecc_intstatus_derr);

	for (source_num = 0; source_num < 19; source_num++) {
		source = (source_reg_value >> source_num) & 1;

		if (source) {
			switch (source_num) {
			case 1:
				irq_handler_ecc_ram_derr();
				break;
			case 17:
				irq_handler_DDR_ecc_derr();
				break;
			case 0:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
			case 16:
			case 18:
				printf("Can't find derr irq handler ");
				printf("for source %d\n", source_num);
				break;
			default:
				printf("invalid DBE interrupt ");
				printf("source\n");
			}
		}
	}

	return;
}
#endif /* CONFIG_USE_IRQ */

