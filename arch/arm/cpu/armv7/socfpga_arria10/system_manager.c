/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/system_manager.h>

/*
 * Configure the pin mux registers
 */
void sysmgr_pinmux_init(unsigned long base_address,
	unsigned long *handoff_data, unsigned long length)
{
	unsigned long i;

	for (i = 0; i < length;	i++)
		writel(*handoff_data++, (base_address + (i * 4)));
}
