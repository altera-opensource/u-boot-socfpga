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

#include <config.h>
#include <version.h>
#include <build.h>
#include <asm/arch/system_manager.h>

#define PRELOADER_DEBUG_MEMMORY_MAGIC		0x444d
#define PRELOADER_DEBUG_MEMMORY_VERSION		0x1
#define PRELOADER_DEBUG_MEMMORY_HEADER	( \
	PRELOADER_DEBUG_MEMMORY_MAGIC | \
	(PRELOADER_DEBUG_MEMMORY_VERSION << 16 ))

/* Save the parameter pass in by previous boot loader */
.global save_boot_params
save_boot_params:

#ifdef CONFIG_SPL_BUILD
#if (CONFIG_PRELOADER_DEBUG_MEMORY_WRITE == 1)
	/*
	 * write the debug memory header and value of R0 - R3 (which passed
	 * by BootROM) into debug memory region (which is on ocram)
	 */
	ldr	r5, =CONFIG_PRELOADER_DEBUG_MEMORY_ADDR
	ldr	r4, =PRELOADER_DEBUG_MEMMORY_HEADER
	stmia	r5!, {r4}
	stmia	r5, {r0 -r3}
	/* get the reset manager status register passed by BootROM */
	add	r5, r0,#0x34
	ldr	r4, =rst_mgr_status
	ldr	r5, [r5]
	str	r5, [r4]
#endif	/* CONFIG_PRELOADER_DEBUG_MEMORY_WRITE */

#if (CONFIG_PRELOADER_EXE_ON_FPGA == 1)
	/* relocate the .data section to supprt preloader execute on FPGA */
	ldr	r0, =__data_start
	ldr	r1, =CONFIG_FPGA_DATA_BASE
	ldr	r2, =__image_copy_end
relocate_loop:
	ldmia	r0!, {r3}		/* copy from source address [r0] */
	stmia	r1!, {r3}		/* copy to   target address [r1] */
	cmp	r1, r2			/* until source end addreee [r2] */
	ble	relocate_loop
#endif	/* CONFIG_PRELOADER_EXE_ON_FPGA */
#endif	/* CONFIG_SPL_BUILD */
	bx	lr


