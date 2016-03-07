/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/ecc_ram.h>
#include <asm/arch/system_manager.h>

DECLARE_GLOBAL_DATA_PTR;

unsigned long irq_cnt_ecc_ocram_corrected = 0;

static const struct socfpga_ecc *ecc_ocram_base =
		(void *)SOCFPGA_ECC_OCRAM_ADDRESS;
#ifndef TEST_AT_ASIMOV
static const struct socfpga_system_manager *system_manager_base =
		(void *)SOCFPGA_SYSMGR_ADDRESS;


static void irq_handler_ecc_ocram_corrected(void)
{
	printf("IRQ triggered: Corrected OCRAM ECC @ 0x%08x\n",
		readl(&ecc_ocram_base->serraddra));
	irq_cnt_ecc_ocram_corrected++;
#ifndef CONFIG_ENV_IS_NOWHERE
	setenv_ulong("ocram_ecc_sbe", irq_cnt_ecc_ocram_corrected);
#endif
	/* write 1 to clear the ECC */
	writel(ALT_ECC_INTSTAT_SERRPENA_SET_MSK, &ecc_ocram_base->intstat);
}

static void irq_handler_ecc_ocram_uncorrected(void)
{
	printf("IRQ triggered: Un-corrected OCRAM ECC @ 0x%08x\n",
		readl(&ecc_ocram_base->derraddra));

	/* write 1 to clear the ECC */
	writel(ALT_ECC_INTSTAT_DERRPENA_SET_MSK, &ecc_ocram_base->intstat);
	hang();
}

int is_ocram_ecc_enabled(void)
{
	static const struct socfpga_ecc *ecc_ocram_base =
		(void *)SOCFPGA_ECC_OCRAM_ADDRESS;

	return (readl(&ecc_ocram_base->ctrl) &
			ALT_ECC_CTRL_ECCEN_SET_MSK) != 0;
}
#endif
void irq_handler_ecc_ram_serr(void)
{
#ifndef TEST_AT_ASIMOV
	unsigned reg;
	reg = readl(&system_manager_base->ecc_intstatus_serr);

	/* is it OCRAM SERR? */
	if (reg & ALT_SYSMGR_ECC_INTSTAT_SERR_OCRAM_SET_MSK)
		irq_handler_ecc_ocram_corrected();
#endif
}

void irq_handler_ecc_ram_derr(void)
{

#ifndef TEST_AT_ASIMOV
	unsigned reg;
	reg = readl(&system_manager_base->ecc_intstatus_derr);

	/* is it OCRAM SERR? */
	if (reg & ALT_SYSMGR_ECC_INTSTAT_DERR_OCRAM_SET_MSK)
		irq_handler_ecc_ocram_uncorrected();
#endif
}

void enable_ecc_ram_serr_int(void)
{
	writel(ALT_ECC_ERRINTEN_SERRINTEN_SET_MSK,
			&ecc_ocram_base->errinten);
}

void clear_ecc_ocram_ecc_status(void)
{
	/* check whether single bit error pending */
	if (readl(&ecc_ocram_base->intstat) &
		ALT_ECC_INTSTAT_SERRPENA_SET_MSK) {
		writel(ALT_ECC_INTSTAT_SERRPENA_SET_MSK,
			&ecc_ocram_base->intstat);
		irq_cnt_ecc_ocram_corrected = 0;
	}

	/* check whether double bit error pending */
	if (readl(&ecc_ocram_base->intstat) &
		ALT_ECC_INTSTAT_DERRPENA_SET_MSK) {
		writel(ALT_ECC_INTSTAT_DERRPENA_SET_MSK,
			&ecc_ocram_base->intstat);
		irq_cnt_ecc_ocram_corrected = 0;
	}
}
