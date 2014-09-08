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
#include <asm/arch/system_manager.h>
#include <linux/linkage.h>
#include <asm/arch/sdram.h>
#include <asm/system.h>
#include <asm-offsets.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/clock_manager.h>

#define PRELOADER_DEBUG_MEMMORY_MAGIC		0x444d
#define PRELOADER_DEBUG_MEMMORY_VERSION		0x1
#define PRELOADER_DEBUG_MEMMORY_HEADER	( \
	PRELOADER_DEBUG_MEMMORY_MAGIC | \
	(PRELOADER_DEBUG_MEMMORY_VERSION << 16 ))

/* Save the parameter pass in by previous boot loader */
.global save_boot_params
save_boot_params:

#ifdef CONFIG_SPL_BUILD
	/*
	 * If BSEL is zero, then the bootrom has not
         * handed any information to preloader.
         */
	ldr	r9,=SYSMGR_BOOTINFO
	ldr 	r9, [r9]
	and	r9, r9, #SYSMGR_BOOTINFO_BSEL_MASK
	cmp	r9, #0
	bne	bsel_non_zero
	ldr	r4, =rst_mgr_status
	ldr	r5, =RSTMGR_COLDRST_MASK
	str	r5, [r4]
	b	bsel_zero

bsel_non_zero:
#if (CONFIG_PRELOADER_DEBUG_MEMORY_WRITE == 1)
	/*
	 * write the debug memory header and value of R0 - R3 (which passed
	 * by BootROM) into debug memory region (which is on ocram)
	 */
	ldr	r5, =CONFIG_PRELOADER_DEBUG_MEMORY_ADDR
	ldr	r4, =PRELOADER_DEBUG_MEMMORY_HEADER
	stmia	r5!, {r4}
	stmia	r5!, {r0 -r3}
	ldr	r4, =debug_memory_write_addr
	str	r5, [r4]
#endif	/* CONFIG_PRELOADER_DEBUG_MEMORY_WRITE */
#if (CONFIG_PRELOADER_WARMRST_SKIP_CFGIO == 1)
	/* get the reset manager status register passed by BootROM */
	add	r5, r0,#0x38
	ldr	r4, =rst_mgr_status
	ldr	r5, [r5]
	str	r5, [r4]
#endif /* CONFIG_PRELOADER_WARMRST_SKIP_CFGIO */

bsel_zero:
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
	 *Initialize the early stack point on OCRAM. This is to avoid OCRAM
	 * false double bit error as OCRAM always ready in 64 bit manner
	 */
	ldr	r1, =(CONFIG_SYS_INIT_SP_ADDR)	/* start of temporary SP */
	sub	r0, r1, #GD_SIZE	/* start of mem init */
	mov	r2, #0x00000000		/* prepare zero to mem init */
clrsp:	cmp	r0, r1			/* while not at end of BSS */
	strlo	r2, [r0]		/* clear 32-bit BSS word */
	addlo	r0, r0, #4		/* move to next */
	blo	clrsp

#endif	/* CONFIG_SPL_BUILD */
	bx	lr


#ifdef CONFIG_SPL_BUILD
#ifdef CONFIG_SPL_FAT_SUPPORT
ENTRY(relocate_stack_to_sdram)
	PUSH	{r4-r11, lr}		/* save registers per AAPCS */

	mov	r0, sp			/* [r0] source address */
	ldr	r2, =__stack_start	/* [r2] source end address */
	sub	r3, r2, r0
	ldr	r4, =__sdram_stack_start
	sub	r1, r4, r3		/* [r1] target address */
	mov	r5, r1			/* [r5] new sp within SDRAM */

relocate_loop1:
	ldmia	r0!, {r3}
	stmia	r1!, {r3}
	cmp	r0, r2
	blo	relocate_loop1

	/* assign SP to new address within SDRAM now */
	mov	sp, r5

	POP	{r4-r11, pc}
ENDPROC(relocate_stack_to_sdram)
#endif /* CONFIG_SPL_FAT_SUPPORT */
#endif	/* CONFIG_SPL_BUILD */

#ifndef CONFIG_SPL_BUILD
/*
 * Configure the fpga2sdram register
 * For U-Boot only and this code need to run on OCRAM
 * No stack activity or function call to avoid access to SDRAM
 */

.global sdram_applycfg_ocram
sdram_applycfg_ocram:
	mrc	p15, 0, r0, c1, c0, 0	@ Read CP15 SCTRL Register
	bic	r0, #CR_Z		@ Disable branch predictor
	bic	r0, #CR_I		@ Disable i-cache
	mcr	p15, 0, r0, c1, c0, 0	@ Write CP15 SCTRL Register
	dsb
	isb

	ldr	r1, SDR_CTRL
	ldr	r2, SDR_STATICCFG
	add	r1, r1, r2
	ldr	r0, [r1]
	ldr	r3, SDR_APPLYCFG_MASK
	orr	r0, r0, r3
	str	r0, [r1]
	dsb
	isb

	mrc	p15, 0, r4, c1, c0, 0	@ Read CP15 SCTRL Register
	orr	r4, r4, #CR_Z		@ Enable back branch predictor
	orr	r4, r4, #CR_I		@ Enable back i-cache
	mcr	p15, 0, r4, c1, c0, 0	@ Write CP15 SCTRL Register

	mov	r0, #0
	mov	pc, lr

SDR_CTRL:		.word	SOCFPGA_SDR_ADDRESS
SDR_STATICCFG:		.word	SDR_CTRLGRP_STATICCFG_ADDRESS
SDR_APPLYCFG_MASK:	.word	SDR_CTRLGRP_STATICCFG_APPLYCFG_MASK

/*
 * Relocate the sdram_applycfg_ocram function to OCRAM and call it
 */
ENTRY(sdram_applycfg_uboot)
	PUSH	{r4-r11, lr}		/* save registers per AAPCS */

	ldr	r1, =sdram_applycfg_ocram
	ldr	r2, =CONFIG_SYS_INIT_RAM_ADDR
	mov	r3, r2
	ldmia	r1!, {r4 - r11}
	stmia	r3!, {r4 - r11}
	ldmia	r1!, {r4 - r11}		/* copy more in case code added */
	stmia	r3!, {r4 - r11}		/* in the future */
	ldmia	r1!, {r4 - r11}		/* copy more in case code added */
	stmia	r3!, {r4 - r11}		/* in the future */
	dsb
	isb
	blx	r2			/* jump to OCRAM */
	POP	{r4-r11, pc}
ENDPROC(sdram_applycfg_uboot)

#endif /* CONFIG_SPL_BUILD */

/*
 * Write RBF data in burst form to FPGA Manager
 * [r0] RBF binary source address
 * [r1] FPGA Manager data address
 * [r2] RBF data length
 */

ENTRY(fpgamgr_axi_write)
	PUSH	{r4-r11, lr}		/* save registers per AAPCS */

write_burst:
	cmp	r2,#32
	beq	write_burst_cont
	bls	write_word
write_burst_cont:
	ldmia	r0!, {r4-r11}
	stmia	r1, {r4-r11}
	subs	r2, r2, #32
	b	write_burst

write_word:
	cmp	r2,#4
	beq	write_word_cont
	bls	write_byte
write_word_cont:
	ldmia	r0!, {r4}
	stmia	r1, {r4}
	subs	r2, r2, #4
	b	write_word

write_byte:
	cmp	r2,#0
	beq	write_end
	ldr	r3, [r0]
	str	r3, [r1]
write_end:
	POP	{r4-r11, pc}
ENDPROC(fpgamgr_axi_write)

/*
 * Relocate the sdram_applycfg_ocram function to OCRAM and call it
 */
ENTRY(reset_clock_manager)
	/* Put Main PLL and Peripheral PLL in bypass */
	ldr	r0, SOCFPGA_CLKMGR
	mov	r1, #CLKMGR_BYPASS_ADDRESS
	mov	r2, #CLKMGR_BYPASS_MAIN_PER_PLL_MASK
	add	r3, r0, r1
	ldr	r4, [r3]
	orr	r5, r4, r2
	str	r5, [r3]
	dsb
	isb
	mov	r1, #CLKMGR_MAINPLLGRP_MAINQSPICLK_ADDRESS
	mov	r2, #CLKMGR_MAINQSPICLK_RESET_VALUE
	add	r3, r0, r1
	str	r2, [r3]
	mov	r1, #CLKMGR_MAINPLLGRP_MAINNANDSDMMCCLK_ADDRESS
	mov	r2, #CLKMGR_MAINNANDSDMMCCLK_RESET_VALUE
	add	r3, r0, r1
	str	r2, [r3]
	mov	r1, #CLKMGR_PERPLLGRP_PERQSPICLK_ADDRESS
	mov	r2, #CLKMGR_PERQSPICLK_RESET_VALUE
	add	r3, r0, r1
	str	r2, [r3]
	mov	r1, #CLKMGR_PERPLLGRP_PERNANDSDMMCCLK_ADDRESS
	mov	r2, #CLKMGR_PERNANDSDMMCCLK_RESET_VALUE
	add	r3, r0, r1
	str	r2, [r3]

	/* Disable the RAM boot */
	ldr	r0, SOCFPGA_RSTMGR
	ldr	r1, SYSMGR_WARMRAMGRP_ENABLE
	mov	r2, #0
	str	r2, [r1]

	/* Trigger warm reset to continue boot normally */
	mov	r1, #RSTMGR_CTRL_OFFSET
	add	r2, r0, r1
	mov	r3, #1
	mov	r3, r3, LSL #RSTMGR_CTRL_SWWARMRSTREQ_LSB
	ldr	r4, [r2]
	orr	r4, r3, r4
	str	r4, [r2]

reset_clock_manager_loop:
	dsb
	isb
	b	reset_clock_manager_loop
ENDPROC(reset_clock_manager)

SOCFPGA_CLKMGR:			.word	SOCFPGA_CLKMGR_ADDRESS
SOCFPGA_RSTMGR:			.word	SOCFPGA_RSTMGR_ADDRESS
SYSMGR_WARMRAMGRP_ENABLE:	.word	CONFIG_SYSMGR_WARMRAMGRP_ENABLE

.globl reset_clock_manager_size
reset_clock_manager_size:
	.word	. - reset_clock_manager

