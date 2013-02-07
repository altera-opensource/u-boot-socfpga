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
#include <asm/arch/ecc_ram.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/debug_memory.h>

DECLARE_GLOBAL_DATA_PTR;

unsigned long irq_cnt_ecc_ocram_corrected;
unsigned long irq_cnt_ecc_ocram_uncorrected;

void irq_handler_ecc_ocram_corrected(void *arg)
{
	DEBUG_MEMORY
	puts("IRQ triggered: Corrected OCRAM ECC\n");
	irq_cnt_ecc_ocram_corrected++;
	/* write 1 to clear the ECC */
	writel(SYSMGR_ECC_OCRAM_SERR | SYSMGR_ECC_OCRAM_EN,
		CONFIG_SYSMGR_ECC_OCRAM);
}

void irq_handler_ecc_ocram_uncorrected(void *arg)
{
	DEBUG_MEMORY
	irq_cnt_ecc_ocram_uncorrected++;
	puts("IRQ triggered: Un-corrected OCRAM ECC\n");
	/* write 1 to clear the ECC */
	writel(SYSMGR_ECC_OCRAM_DERR  | SYSMGR_ECC_OCRAM_EN,
		CONFIG_SYSMGR_ECC_OCRAM);
	hang();
}
