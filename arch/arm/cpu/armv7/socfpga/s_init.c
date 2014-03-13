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
#include <asm/arch/debug_memory.h>
#include <watchdog.h>
#include <asm/arch/reset_manager.h>
#include <spl.h>
#include <asm/spl.h>
#include <asm/arch/system_manager.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * First C function to initialize the critical hardware early
 */
void s_init(void)
{
#ifdef CONFIG_SPL_BUILD
	unsigned long reg;
	/*
	 * First C code to run. Clear fake OCRAM ECC first as SBE
	 * and DBE might triggered during power on
	 */
	reg = readl(CONFIG_SYSMGR_ECC_OCRAM);
	if (reg & SYSMGR_ECC_OCRAM_SERR)
		writel(SYSMGR_ECC_OCRAM_SERR | SYSMGR_ECC_OCRAM_EN,
			CONFIG_SYSMGR_ECC_OCRAM);
	if (reg & SYSMGR_ECC_OCRAM_DERR)
		writel(SYSMGR_ECC_OCRAM_DERR  | SYSMGR_ECC_OCRAM_EN,
			CONFIG_SYSMGR_ECC_OCRAM);

	/* re-setup watchdog */
	DEBUG_MEMORY
	if (!(is_wdt_in_reset())) {
		/*
		 * only disabled if wdt not in reset state
		 * disable the watchdog prior PLL reconfiguration
		 */
		DEBUG_MEMORY
		watchdog_disable();
	}

#ifdef CONFIG_HW_WATCHDOG
	/* release osc1 watchdog timer 0 from reset */
	DEBUG_MEMORY
	reset_deassert_osc1wd0();

	/* reconfigure and enable the watchdog */
	DEBUG_MEMORY
	hw_watchdog_init();
	WATCHDOG_RESET();
#endif /* CONFIG_HW_WATCHDOG */

	DEBUG_MEMORY
	/* Memory initialization for ECC padding*/
	if (&__ecc_padding_start < &__ecc_padding_end) {
		memset(&__ecc_padding_start, 0,
			&__ecc_padding_end - &__ecc_padding_start);
	}

#else
	/*
	 * Private components security
	 * U-Boot : configure private timer, global timer and cpu
	 * component access as non secure for kernel stage (as required
	 * by kernel)
	 */
	setbits_le32(SOCFPGA_SCU_SNSAC, 0xfff);

#endif	/* CONFIG_SPL_BUILD */

	/* Configure the L2 controller to make SDRAM start at 0	*/
#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
	writel(0x2, SOCFPGA_L3REGS_ADDRESS);
#else
	writel(0x1, (SOCFPGA_MPUL2_ADDRESS + SOCFPGA_MPUL2_ADRFLTR_START));
#endif
}
