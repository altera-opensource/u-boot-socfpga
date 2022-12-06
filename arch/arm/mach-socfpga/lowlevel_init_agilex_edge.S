/*
 * Copyright (C) 2022 Intel Corporation. All rights reserved
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <asm-offsets.h>
#include <config.h>
#include <linux/linkage.h>
#include <asm/macro.h>
#include <asm/arch/reset_manager_soc64.h>

#if !defined(CONFIG_SPL_BUILD) && defined(CONFIG_ARMV8_PSCI)
.align 3
_el3_exception_vectors:
	.quad el3_exception_vectors;
#endif

ENTRY(lowlevel_init)
	mov	x29, lr			/* Save LR */

#ifdef CONFIG_SPL_BUILD
	branch_if_slave x0, 3f

	/* Check rstmgr.stat for warm reset status */
	ldr	x1, =SOCFPGA_RSTMGR_ADDRESS
	ldr	x0, [x1]
	/* Check whether any L4 watchdogs or SDM had triggered warm reset */
	ldr	x2, =RSTMGR_L4WD_MPU_WARMRESET_MASK
	ands	x0, x0, x2
	/*
	 * If current Reset Manager's status is warm reset just reload the
	 * .data section by copying the data from data preserve section.
	 * Otherwise, copy the .data section to the data preserve section to
	 * keep an original copy of .data section. This ensure SPL is
	 * reentrant after warm reset.
	 */
	b.ne	reload_data_section
	/* Copy from .data to preserved .data to backup the SPL state */
	ldr	x0, =__data_start
	ldr	x1, =__preserve_data_start
	ldr	x2, =__preserve_data_end
	b	copy_loop
reload_data_section:
	/* Copy from preserved .data to .data to restore the SPL state */
	ldr	x0, =__preserve_data_start
	ldr	x1, =__data_start
	ldr	x2, =__data_end
copy_loop:
	ldr	w3, [x0]
	add	x0, x0, #4
	str	w3, [x1]
	add	x1, x1, #4
	cmp	x1, x2
	b.ne	copy_loop
3:
#endif

	mov	lr, x29			/* Restore LR */
	ret
ENDPROC(lowlevel_init)
