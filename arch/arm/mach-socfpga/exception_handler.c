/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <common.h>
#include <asm/io.h>
#include <asm/secure.h>
#include <asm/system.h>

void __secure plat_error_handler(void)
{
	asm volatile(
			"ldr	x1, =wfi_loop\n"
			"msr	elr_el3, x1\n"
			"mrs	x1, spsr_el3\n"
			/* Make sure eret return to EL3 */
			"orr	x1, x1, 0x08\n"
			"msr	spsr_el3, x1\n"
			/* Determine current CPU is master or slave */
			"mrs	x1, mpidr_el1\n"
			"ands	x1, x1, #0xff\n"
			"isb\n"
			"dsb	sy\n"
			/* Let slave CPUs go to WFI mode */
			"b.ne	1f\n"
			/* Let master CPU (CPU0) handle the L2 reset */
			"ldr	x1, =reset_handler\n"
			"msr	elr_el3, x1\n"
			/*
			 * Read sysmgr.core.ecc_intstatus_derr to
			 * get the ECC double bit error status for
			 * individual modules
			 */
			"ldr	w2, [%2, #0xA0]\n"
			/* Skip if no ECC double bit errors */
			"cbz	w2, 1f\n"
			/*
			 * Save sysmgr.core.ecc_intstatus_derr
			 * into sysmgr.boot_scratch_cold8 for error
			 * reporting.
			 */
			"str	w2, [%2, #0x220]\n"
			/*
			 * Read double-bit error address from
			 * hmc_adp.derraddra
			 */
			"ldr	w2, [%3, #0x12C]\n"
			/*
			 * Save the double-bit error address into
			 * sysmgr.boot_scratch_cold9 for error
			 * reporting.
			 */
			"str	w2, [%2, #0x224]\n"
			/*
			 * Read Error Interrupt Reset from
			 * hmc_adp.errintenr
			 */
			"ldr	w2, [%3, #0x118]\n"
			/* Disable all error interrupts */
			"orr	x2, x2, #0x0F\n"
			"str	w2, [%3, #0x118]\n"
			/*
			 * Read Error Interrupt Status from
			 * hmc_adp.intstat
			 */
			"ldr	w2, [%3, #0x120]\n"
			/* Clear all pending error interrupts */
			"str	w2, [%3, #0x120]\n"
			/*
			 * Store a magic word into
			 * sysmgr.boot_scratch_cold6.
			 * SPL will check for this magic word
			 * in next reboot and trigger warm reset.
			 */
			"str	%0, [%1]\n"
			/*
			 * Master CPU jump to reset_handler.
			 * Slaves CPUs jump to wfi_loop.
			 */
			"1:	eret\n"
			/* L2 reset handler */
			"reset_handler:\n"
			/* Increase timeout in rstmgr.hdsktimeout */
			"ldr	x2, =0xFFFFFF\n"
			"str	w2, [%4, #0x64]\n"
			"ldr	w2, [%4, #0x10]\n"
			/*
			 * Set l2flushen = 1, etrstallen = 1,
			 * fpgahsen = 1 and sdrselfrefen = 1
			 * in rstmgr.hdsken to perform handshake
			 * in certain peripherals before trigger
			 * L2 reset.
			 */
			"ldr	x3, =0x10D\n"
			"orr	x2, x2, x3\n"
			"str	w2, [%4, #0x10]\n"
			/* Trigger L2 reset in rstmgr.coldmodrst */
			"ldr	w2, [%4, #0x34]\n"
			"orr	x2, x2, #0x100\n"
			"isb\n"
			"dsb	sy\n"
			"str	w2, [%4, #0x34]\n"
			/* Put all cores into WFI mode */
			"wfi_loop:\n"
			"	wfi\n"
			"	b	wfi_loop\n"
			: : "r" (L2_RESET_DONE_STATUS),
			    "r" (L2_RESET_DONE_REG),
			    "r" (SOCFPGA_SYSMGR_ADDRESS),
			    "r" (SOCFPGA_SDR_ADDRESS),
			    "r" (SOCFPGA_RSTMGR_ADDRESS)
			: "x1", "x2", "x3", "cc");
}
