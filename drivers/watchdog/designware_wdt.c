/*
 * Copyright (C) 2012 Altera Corporation <www.altera.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */

#include <common.h>
#include <watchdog.h>
#include <asm/io.h>
#include <asm/utils.h>

#define DW_WDT_CR	0x00
#define DW_WDT_TORR	0x04
#define DW_WDT_CRR	0x0C

#define DW_WDT_CR_EN_OFFSET	0x00
#define DW_WDT_CR_RMOD_OFFSET	0x01
#define DW_WDT_CR_RMOD_VAL	0x00
#define DW_WDT_CRR_RESTART_VAL	0x76

/*
 * Set the watchdog time interval.
 * Counter is 32 bit.
 */
int designware_wdt_settimeout(unsigned int timeout)
{
	signed int i;
	/* calculate the timeout range value */
	i = (log_2_n_round_up(timeout * CONFIG_DW_WDT_CLOCK_KHZ))\
		- 16;
	if (i > 15)
		i = 15;
	if (i < 0)
		i = 0;

	writel((i | (i<<4)),
		(CONFIG_DW_WDT_BASE + DW_WDT_TORR));
	return 0;
}

void designware_wdt_enable(void)
{
	writel(((DW_WDT_CR_RMOD_VAL << DW_WDT_CR_RMOD_OFFSET) | \
		(0x1 << DW_WDT_CR_EN_OFFSET)),
		(CONFIG_DW_WDT_BASE + DW_WDT_CR));
}

unsigned int designware_wdt_is_enabled(void)
{
	unsigned long val;
	val = readl((CONFIG_DW_WDT_BASE + DW_WDT_CR));
	return val & 0x1;
}

#if defined(CONFIG_HW_WATCHDOG)
void hw_watchdog_reset(void)
{
	if (designware_wdt_is_enabled())
		/* restart the watchdog counter */
		writel(DW_WDT_CRR_RESTART_VAL,
			(CONFIG_DW_WDT_BASE + DW_WDT_CRR));
}

void hw_watchdog_init(void)
{
	/* reset to disable the watchdog */
	hw_watchdog_reset();
	/* set timer in miliseconds */
	designware_wdt_settimeout(CONFIG_HW_WATCHDOG_TIMEOUT_MS);
	/* enable the watchdog */
	designware_wdt_enable();
	/* reset the watchdog */
	hw_watchdog_reset();
}
#endif
