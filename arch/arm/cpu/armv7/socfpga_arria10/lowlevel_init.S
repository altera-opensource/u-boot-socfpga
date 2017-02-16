/*
 * Copyright (C) 2014-2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <asm-offsets.h>
#include <config.h>
#include <linux/linkage.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/misc.h>

#define	ISW_HANDOFF7	0xFFD0624C

/* Save the parameter pass in by previous boot loader */
.global save_boot_params
save_boot_params:

#ifndef TEST_AT_ASIMOV
	/* get the reset manager status register passed by BootROM */
	ldr	r5, =0x438
	add	r5, r0, r5

	/* for debugging scenario as r0 would be random value. Ensure
	it always point back to OCRAM (0xffe0_0000 to 0xffe3_ffff) */
	ldr	r6, =0xfffc0003
	bic	r5, r5, r6
	ldr	r6, =0xffe00000
	orr	r5, r5, r6

	ldr	r4, =rst_mgr_status
	ldr	r5, [r5]
	str	r5, [r4]
	
	ldr r5, =0
	mcr	p15, 0, r5, c1, c0, 1	@ Clear CP15 ACTLR Register
	mcr	p15, 0, r5, c1, c0, 2	@ Clear CP15 CPACR Register
#endif

#if (CONFIG_UBOOT_EXE_ON_FPGA == 1)
	/* relocate the .data section to support U-boot execute on FPGA */
	ldr	r0, =__data_start
	ldr	r1, =CONFIG_FPGA_DATA_BASE
	ldr	r2, =__data_reloc_end
relocate_loop:
	ldmia	r0!, {r3}		/* copy from source address [r0] */
	stmia	r1!, {r3}		/* copy to   target address [r1] */
	cmp	r1, r2			/* until target end address [r2] */
	blo	relocate_loop

/*
 * Clearing the OCRAM memory that wasn't initialized during the write
 * of the data section
 */
	ldr	r0, =__data_reloc_end	/* start of remaining region out of
						U-boot data section */
	ldr	r1, = CONFIG_FPGA_DATA_BASE	/* start of OCRAM */
	ldr r2, = CONFIG_SYS_INIT_RAM_SIZE 	/* size of OCRAM */
	add r1, r1, r2			/* end of OCRAM */
	sub r1, r1, #1
	mov	r2, #0x00000000		/* prepare zero to clear BSS */
clrrmn:
	cmp	r0, r1			/* while not at end of OCRAM */
	strlo	r2, [r0]		/* clear 32-bit word length */
	addlo	r0, r0, #4		/* move to next */
	blo	clrrmn

/*
 * Clearing the bss section
 */
	ldr	r0, =__bss_start	/* start of remaining region out of
						U-boot binary image */
	ldr	r1, = __bss_end	/* end of bss */
	mov	r2, #0x00000000		/* prepare zero to clear bss */
clrbss:
	cmp	r0, r1			/* while not at end of bss */
	strlo	r2, [r0]		/* clear 32-bit word length */
	addlo	r0, r0, #4		/* move to next */
	blo	clrbss

	bx	lr
#else
	/* Loading isw_handoff[7] reg value */
	ldr	r0, = ISW_HANDOFF7
	ldr	r1, [r0]
	ldr	r2, = REGULAR_BOOT_MAGIC

	cmp	r1, r2
	blo	preserve_datasect

	/* restore .data section */
	ldr	r0, =__preserve_data_start
	ldr	r1, =__data_start
	ldr	r2, =__data_end
	b	relocate_loop

preserve_datasect:
	/* preserve .data section */
	ldr	r0, =__data_start
	ldr	r1, =__preserve_data_start
	ldr	r2, =__preserve_data_end

relocate_loop:
	ldmia	r0!, {r3}		/* copy from source address [r0] */
	stmia	r1!, {r3}		/* copy to   target address [r1] */
	cmp	r1, r2			/* until target end address [r2] */
	blo	relocate_loop

	/*
	 * Clearing the OCRAM memory that wasn't initialized during the write
	 * of the U-Boot image
	 */
	ldr	r0, =__bss_start	/* start of remaining region out of
						U-boot binary image */
	ldr	r1, = CONFIG_SYS_TEXT_BASE	/* start of OCRAM */
	ldr r2, = CONFIG_SYS_INIT_RAM_SIZE 	/* size of OCRAM */
	add r1, r1, r2			/* end of OCRAM */
	sub r1, r1, #1
	mov	r2, #0x00000000		/* prepare zero to clear BSS */
clrrmn:
	cmp	r0, r1			/* while not at end of OCRAM */
	strlo	r2, [r0]		/* clear 32-bit word length */
	addlo	r0, r0, #4		/* move to next */
	blo	clrrmn

	bx	lr
#endif	/* CONFIG_UBOOT_EXE_ON_FPGA */

#if (CONFIG_PRELOADER_EXE_ON_FPGA == 1)
	/* relocate the .data section to supprt preloader execute on FPGA */
	ldr	r0, =__data_start
	ldr	r1, =CONFIG_FPGA_DATA_BASE
	ldr	r2, =__image_copy_end
relocate_loop:
	ldmia	r0!, {r3}		/* copy from source address [r0] */
	stmia	r1!, {r3}		/* copy to   target address [r1] */
	cmp	r1, r2			/* until target end addreee [r2] */
	blo	relocate_loop
#endif	/* CONFIG_PRELOADER_EXE_ON_FPGA */

/*
 * Write RBF data in burst form to FPGA Manager
 * [r0] RBF binary source address
 * [r1] FPGA Manager data address
 * [r2] RBF data length
 */

ENTRY(fpgamgr_axi_write)
	PUSH    {r4-r11, lr}            /* save registers per AAPCS */

write_burst:
	cmp     r2,#32
	beq     write_burst_cont
	bls     write_word
write_burst_cont:
	ldmia   r0!, {r4-r11}
	stmia   r1, {r4-r11}
	subs    r2, r2, #32
	b       write_burst

write_word:
	cmp     r2,#4
	beq     write_word_cont
	bls     write_byte
write_word_cont:
	ldmia   r0!, {r4}
	stmia   r1, {r4}
	subs    r2, r2, #4
	b       write_word

write_byte:
	cmp     r2,#0
	beq     write_end
	ldr     r3, [r0]
	str     r3, [r1]
write_end:
	POP     {r4-r11, pc}
ENDPROC(fpgamgr_axi_write)
