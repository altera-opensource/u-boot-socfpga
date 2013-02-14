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
#include <asm/arch/sdram.h>
#include <sdram_config.h>
#include <asm/arch/debug_memory.h>

/*
 * SDRAM MMR init skip read back/verify steps
 * Define to speed up the MMR init process by just write without verifying
 */
/*#define SDRAM_MMR_SKIP_VERIFY*/

/* Determine the action when compare failed */
/*#define COMPARE_FAIL_ACTION	;*/
#define COMPARE_FAIL_ACTION	return 1;


DECLARE_GLOBAL_DATA_PTR;

unsigned long irq_cnt_ecc_sdram;

/* Initialise the DRAM by telling the DRAM Size */
int dram_init(void)
{
	gd->ram_size = get_ram_size((long *)PHYS_SDRAM_1, PHYS_SDRAM_1_SIZE);
	return 0;
}

void irq_handler_ecc_sdram(void *arg)
{
	DEBUG_MEMORY
	debug("IRQ triggered: SDRAM ECC\n");
	irq_cnt_ecc_sdram++;
#ifndef CONFIG_SPL_BUILD
	setenv_ulong("ECC_SDRAM",irq_cnt_ecc_sdram);
#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
	setenv_ulong("ECC_SDRAM_SBE",
		readl(SOCFPGA_SDR_ADDRESS+SDR_CTRLGRP_SBECOUNT_ADDRESS));
	setenv_ulong("ECC_SDRAM_DBE",
		readl(SOCFPGA_SDR_ADDRESS+SDR_CTRLGRP_DBECOUNT_ADDRESS));
	/* clear the interrupt signal from SDRAM controller */
	setbits_le32((SOCFPGA_SDR_ADDRESS + SDR_CTRLGRP_DRAMINTR_ADDRESS),
		SDR_CTRLGRP_DRAMINTR_INTRCLR_MASK);
#endif /* CONFIG_SOCFPGA_VIRTUAL_TARGET */
#endif /* CONFIG_SPL_BUILD */
}


#ifdef CONFIG_SPL_BUILD

/* Function to update the field within variable */
unsigned sdram_write_register_field (unsigned masked_value,
	unsigned data, unsigned shift, unsigned mask)
{
	masked_value &= ~ ( mask );
	masked_value |= ( data << shift ) & mask;
	return masked_value;
}

/* Function to write to register and verify the write */
unsigned sdram_write_verify (unsigned register_offset, unsigned reg_value)
{
#ifndef SDRAM_MMR_SKIP_VERIFY
	unsigned reg_value1;
#endif
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(SOCFPGA_SDR_ADDRESS+register_offset), reg_value);
	/* Write to register */
	writel(reg_value,(SOCFPGA_SDR_ADDRESS + register_offset));
#ifndef SDRAM_MMR_SKIP_VERIFY
	debug("   Read and verify...");
	/* Read back the wrote value */
	reg_value1 = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	/* Indicate failure if value not matched */
	if( reg_value1 != reg_value ) {
		debug("FAIL - Address 0x%08x Expected 0x%08x Data 0x%08x\n",
			(SOCFPGA_SDR_ADDRESS+register_offset),
			reg_value, reg_value1);
		return 1;
	}
	debug("correct!\n");
#endif	/* SDRAM_MMR_SKIP_VERIFY */
	return 0;
}

/* Function to initialize SDRAM MMR */
unsigned sdram_mmr_init_full(void)
{
	unsigned long register_offset, reg_value;
	unsigned long status=0;

	DEBUG_MEMORY
	/***** CTRLCFG *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMTYPE)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMBL)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ADDRORDER)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCORREN)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_REORDEREN)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_STARVELIMIT)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_DQSTRKEN)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_NODMPINS)
#ifdef DEBUG
	debug("\nConfiguring CTRLCFG\n");
#endif
	register_offset = SDR_CTRLGRP_CTRLCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMTYPE
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMTYPE,
			SDR_CTRLGRP_CTRLCFG_MEMTYPE_LSB,
			SDR_CTRLGRP_CTRLCFG_MEMTYPE_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMBL
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMBL,
			SDR_CTRLGRP_CTRLCFG_MEMBL_LSB,
			SDR_CTRLGRP_CTRLCFG_MEMBL_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ADDRORDER
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ADDRORDER,
			SDR_CTRLGRP_CTRLCFG_ADDRORDER_LSB,
			SDR_CTRLGRP_CTRLCFG_ADDRORDER_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN,
			SDR_CTRLGRP_CTRLCFG_ECCEN_LSB,
			SDR_CTRLGRP_CTRLCFG_ECCEN_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCORREN
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCORREN,
			SDR_CTRLGRP_CTRLCFG_ECCCORREN_LSB,
			SDR_CTRLGRP_CTRLCFG_ECCCORREN_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_REORDEREN
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_REORDEREN,
			SDR_CTRLGRP_CTRLCFG_REORDEREN_LSB,
			SDR_CTRLGRP_CTRLCFG_REORDEREN_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_STARVELIMIT
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_STARVELIMIT,
			SDR_CTRLGRP_CTRLCFG_STARVELIMIT_LSB,
			SDR_CTRLGRP_CTRLCFG_STARVELIMIT_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_DQSTRKEN
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_DQSTRKEN,
			SDR_CTRLGRP_CTRLCFG_DQSTRKEN_LSB,
			SDR_CTRLGRP_CTRLCFG_DQSTRKEN_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_NODMPINS
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_NODMPINS,
			SDR_CTRLGRP_CTRLCFG_NODMPINS_LSB,
			SDR_CTRLGRP_CTRLCFG_NODMPINS_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMTIMING1 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCWL)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_AL)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCL)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRRD)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TFAW)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRFC)
#ifdef DEBUG
	debug("Configuring DRAMTIMING1\n");
#endif
	register_offset = SDR_CTRLGRP_DRAMTIMING1_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCWL
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCWL,
			SDR_CTRLGRP_DRAMTIMING1_TCWL_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TCWL_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_AL
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_AL,
			SDR_CTRLGRP_DRAMTIMING1_TAL_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TAL_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCL
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCL,
			SDR_CTRLGRP_DRAMTIMING1_TCL_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TCL_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRRD
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRRD,
			SDR_CTRLGRP_DRAMTIMING1_TRRD_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TRRD_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TFAW
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TFAW,
			SDR_CTRLGRP_DRAMTIMING1_TFAW_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TFAW_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRFC
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRFC,
			SDR_CTRLGRP_DRAMTIMING1_TRFC_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TRFC_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMTIMING2 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TREFI)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRCD)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRP)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWR)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWTR)
#ifdef DEBUG
	debug("Configuring DRAMTIMING2\n");
#endif
	register_offset = SDR_CTRLGRP_DRAMTIMING2_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TREFI
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TREFI,
			SDR_CTRLGRP_DRAMTIMING2_TREFI_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TREFI_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRCD
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRCD,
			SDR_CTRLGRP_DRAMTIMING2_TRCD_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TRCD_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRP
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRP,
			SDR_CTRLGRP_DRAMTIMING2_TRP_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TRP_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWR
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWR,
			SDR_CTRLGRP_DRAMTIMING2_TWR_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TWR_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWTR
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWTR,
			SDR_CTRLGRP_DRAMTIMING2_TWTR_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TWTR_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMTIMING3 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRTP)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRAS)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRC)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TMRD)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TCCD)
#ifdef DEBUG
	debug("Configuring DRAMTIMING3\n");
#endif
	register_offset = SDR_CTRLGRP_DRAMTIMING3_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRTP
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRTP,
			SDR_CTRLGRP_DRAMTIMING3_TRTP_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TRTP_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRAS
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRAS,
			SDR_CTRLGRP_DRAMTIMING3_TRAS_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TRAS_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRC
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRC,
			SDR_CTRLGRP_DRAMTIMING3_TRC_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TRC_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TMRD
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TMRD,
			SDR_CTRLGRP_DRAMTIMING3_TMRD_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TMRD_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TCCD
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TCCD,
			SDR_CTRLGRP_DRAMTIMING3_TCCD_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TCCD_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMTIMING4 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_SELFRFSHEXIT)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_PWRDOWNEXIT)
#ifdef DEBUG
	debug("Configuring DRAMTIMING4\n");
#endif
	register_offset = SDR_CTRLGRP_DRAMTIMING4_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_SELFRFSHEXIT
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_SELFRFSHEXIT,
			SDR_CTRLGRP_DRAMTIMING4_SELFRFSHEXIT_LSB,
			SDR_CTRLGRP_DRAMTIMING4_SELFRFSHEXIT_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_PWRDOWNEXIT
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_PWRDOWNEXIT,
			SDR_CTRLGRP_DRAMTIMING4_PWRDOWNEXIT_LSB,
			SDR_CTRLGRP_DRAMTIMING4_PWRDOWNEXIT_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** LOWPWRTIMING *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_LOWPWRTIMING_AUTOPDCYCLES
#ifdef DEBUG
	debug("Configuring LOWPWRTIMING\n");
#endif
	register_offset = SDR_CTRLGRP_LOWPWRTIMING_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_LOWPWRTIMING_AUTOPDCYCLES,
			SDR_CTRLGRP_LOWPWRTIMING_AUTOPDCYCLES_LSB,
			SDR_CTRLGRP_LOWPWRTIMING_AUTOPDCYCLES_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** DRAMADDRW *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_COLBITS)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_BANKBITS)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS)
#ifdef DEBUG
	debug("Configuring DRAMADDRW\n");
#endif
	register_offset = SDR_CTRLGRP_DRAMADDRW_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_COLBITS
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_COLBITS,
			SDR_CTRLGRP_DRAMADDRW_COLBITS_LSB,
			SDR_CTRLGRP_DRAMADDRW_COLBITS_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS,
			SDR_CTRLGRP_DRAMADDRW_ROWBITS_LSB,
			SDR_CTRLGRP_DRAMADDRW_ROWBITS_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_BANKBITS
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_BANKBITS,
			SDR_CTRLGRP_DRAMADDRW_BANKBITS_LSB,
			SDR_CTRLGRP_DRAMADDRW_BANKBITS_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS,
			SDR_CTRLGRP_DRAMADDRW_CSBITS_LSB,
			SDR_CTRLGRP_DRAMADDRW_CSBITS_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** DRAMIFWIDTH *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMIFWIDTH_IFWIDTH
#ifdef DEBUG
	debug("Configuring DRAMIFWIDTH\n");
#endif
	register_offset = SDR_CTRLGRP_DRAMIFWIDTH_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMIFWIDTH_IFWIDTH,
			SDR_CTRLGRP_DRAMIFWIDTH_IFWIDTH_LSB,
			SDR_CTRLGRP_DRAMIFWIDTH_IFWIDTH_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMDEVWIDTH *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMDEVWIDTH_DEVWIDTH
#ifdef DEBUG
	debug("Configuring DRAMDEVWIDTH\n");
#endif
	register_offset = SDR_CTRLGRP_DRAMDEVWIDTH_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMDEVWIDTH_DEVWIDTH,
			SDR_CTRLGRP_DRAMDEVWIDTH_DEVWIDTH_LSB,
			SDR_CTRLGRP_DRAMDEVWIDTH_DEVWIDTH_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMINTR *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMINTR_INTREN
#ifdef DEBUG
	debug("Configuring DRAMINTR\n");
#endif
	register_offset = SDR_CTRLGRP_DRAMINTR_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMINTR_INTREN,
			SDR_CTRLGRP_DRAMINTR_INTREN_LSB,
			SDR_CTRLGRP_DRAMINTR_INTREN_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** STATICCFG *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_STATICCFG_MEMBL)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_STATICCFG_USEECCASDATA)
#ifdef DEBUG
	debug("Configuring STATICCFG\n");
#endif
	register_offset = SDR_CTRLGRP_STATICCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_STATICCFG_MEMBL
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_STATICCFG_MEMBL,
			SDR_CTRLGRP_STATICCFG_MEMBL_LSB,
			SDR_CTRLGRP_STATICCFG_MEMBL_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_STATICCFG_USEECCASDATA
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_STATICCFG_USEECCASDATA,
			SDR_CTRLGRP_STATICCFG_USEECCASDATA_LSB,
			SDR_CTRLGRP_STATICCFG_USEECCASDATA_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** CTRLWIDTH *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLWIDTH_CTRLWIDTH
#ifdef DEBUG
	debug("Configuring CTRLWIDTH\n");
#endif
	register_offset = SDR_CTRLGRP_CTRLWIDTH_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLWIDTH_CTRLWIDTH,
			SDR_CTRLGRP_CTRLWIDTH_CTRLWIDTH_LSB,
			SDR_CTRLGRP_CTRLWIDTH_CTRLWIDTH_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** PORTCFG *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_PORTCFG_AUTOPCHEN
#ifdef DEBUG
	debug("Configuring PORTCFG\n");
#endif
	register_offset = SDR_CTRLGRP_PORTCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_PORTCFG_AUTOPCHEN,
			SDR_CTRLGRP_PORTCFG_AUTOPCHEN_LSB,
			SDR_CTRLGRP_PORTCFG_AUTOPCHEN_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** FIFOCFG *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_SYNCMODE)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_INCSYNC)
#ifdef DEBUG
	debug("Configuring FIFOCFG\n");
#endif
	register_offset = SDR_CTRLGRP_FIFOCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_SYNCMODE
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_SYNCMODE,
			SDR_CTRLGRP_FIFOCFG_SYNCMODE_LSB,
			SDR_CTRLGRP_FIFOCFG_SYNCMODE_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_INCSYNC
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_INCSYNC,
			SDR_CTRLGRP_FIFOCFG_INCSYNC_LSB,
			SDR_CTRLGRP_FIFOCFG_INCSYNC_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPPRIORITY *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPRIORITY_USERPRIORITY
#ifdef DEBUG
	debug("Configuring MPPRIORITY\n");
#endif
	register_offset = SDR_CTRLGRP_MPPRIORITY_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPRIORITY_USERPRIORITY,
			SDR_CTRLGRP_MPPRIORITY_USERPRIORITY_LSB,
			SDR_CTRLGRP_MPPRIORITY_USERPRIORITY_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPWEIGHT_MPWEIGHT_0 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_0_STATICWEIGHT_31_0
#ifdef DEBUG
	debug("Configuring MPWEIGHT_MPWEIGHT_0\n");
#endif
	register_offset = SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_0_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_0_STATICWEIGHT_31_0,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_0_STATICWEIGHT_31_0_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_0_STATICWEIGHT_31_0_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPWEIGHT_MPWEIGHT_1 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_STATICWEIGHT_49_32)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_SUMOFWEIGHT_13_0)
#ifdef DEBUG
	debug("Configuring MPWEIGHT_MPWEIGHT_1\n");
#endif
	register_offset = SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_STATICWEIGHT_49_32
	reg_value = sdram_write_register_field( reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_STATICWEIGHT_49_32,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_STATICWEIGHT_49_32_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_STATICWEIGHT_49_32_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_SUMOFWEIGHT_13_0
	reg_value = sdram_write_register_field( reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_SUMOFWEIGHT_13_0,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_SUMOFWEIGHTS_13_0_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_SUMOFWEIGHTS_13_0_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPWEIGHT_MPWEIGHT_2 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_2_SUMOFWEIGHT_45_14
#ifdef DEBUG
	debug("Configuring MPWEIGHT_MPWEIGHT_2\n");
#endif
	register_offset = SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_2_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_2_SUMOFWEIGHT_45_14,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_2_SUMOFWEIGHTS_45_14_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_2_SUMOFWEIGHTS_45_14_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPWEIGHT_MPWEIGHT_3 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_3_SUMOFWEIGHT_63_46
#ifdef DEBUG
	debug("Configuring MPWEIGHT_MPWEIGHT_3\n");
#endif
	register_offset = SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_3_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_3_SUMOFWEIGHT_63_46,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_3_SUMOFWEIGHTS_63_46_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_3_SUMOFWEIGHTS_63_46_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPPACING_MPPACING_0 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_0_THRESHOLD1_31_0
#ifdef DEBUG
	debug("Configuring MPPACING_MPPACING_0\n");
#endif
	register_offset = SDR_CTRLGRP_MPPACING_MPPACING_0_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_0_THRESHOLD1_31_0,
			SDR_CTRLGRP_MPPACING_MPPACING_0_THRESHOLD1_31_0_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_0_THRESHOLD1_31_0_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** MPPACING_MPPACING_1 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD1_59_32)|\
    defined(CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD2_3_0)
#ifdef DEBUG
	debug("Configuring MPPACING_MPPACING_1\n");
#endif
	register_offset = SDR_CTRLGRP_MPPACING_MPPACING_1_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD1_59_32
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD1_59_32,
			SDR_CTRLGRP_MPPACING_MPPACING_1_THRESHOLD1_59_32_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_1_THRESHOLD1_59_32_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD2_3_0
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD2_3_0,
			SDR_CTRLGRP_MPPACING_MPPACING_1_THRESHOLD2_3_0_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_1_THRESHOLD2_3_0_MASK);
#endif
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** MPPACING_MPPACING_2 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_2_THRESHOLD2_35_4
#ifdef DEBUG
	debug("Configuring MPPACING_MPPACING_2\n");
#endif
	register_offset = SDR_CTRLGRP_MPPACING_MPPACING_2_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_2_THRESHOLD2_35_4,
			SDR_CTRLGRP_MPPACING_MPPACING_2_THRESHOLD2_35_4_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_2_THRESHOLD2_35_4_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPPACING_MPPACING_3 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_3_THRESHOLD2_59_36
#ifdef DEBUG
	debug("Configuring MPPACING_MPPACING_3\n");
#endif
	register_offset = SDR_CTRLGRP_MPPACING_MPPACING_3_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_3_THRESHOLD2_59_36,
			SDR_CTRLGRP_MPPACING_MPPACING_3_THRESHOLD2_59_36_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_3_THRESHOLD2_59_36_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPTHRESHOLDRST_MPTHRESHOLDRST_0 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_0_THRESHOLDRSTCYCLES_31_0
#ifdef DEBUG
	debug("Configuring MPTHRESHOLDRST_MPTHRESHOLDRST_0\n");
#endif
	register_offset = SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_0_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
	CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_0_THRESHOLDRSTCYCLES_31_0,
	SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_0_THRESHOLDRSTCYCLES_31_0_LSB,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_0_THRESHOLDRSTCYCLES_31_0_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPTHRESHOLDRST_MPTHRESHOLDRST_1 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_1_THRESHOLDRSTCYCLES_63_32
#ifdef DEBUG
	debug("Configuring MPTHRESHOLDRST_MPTHRESHOLDRST_1\n");
#endif
	register_offset = SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_1_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
	CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_1_THRESHOLDRSTCYCLES_63_32,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_1_THRESHOLDRSTCYCLES_63_32_LSB,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_1_THRESHOLDRSTCYCLES_63_32_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPTHRESHOLDRST_MPTHRESHOLDRST_2 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_2_THRESHOLDRSTCYCLES_79_64
#ifdef DEBUG
	debug("Configuring MPTHRESHOLDRST_MPTHRESHOLDRST_2\n");
#endif
	register_offset = SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_2_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
	CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_2_THRESHOLDRSTCYCLES_79_64,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_2_THRESHOLDRSTCYCLES_79_64_LSB,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_2_THRESHOLDRSTCYCLES_79_64_MASK);
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** PHYCTRL_PHYCTRL_0 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_PHYCTRL_PHYCTRL_0
#ifdef DEBUG
	debug("Configuring PHYCTRL_PHYCTRL_0\n");
#endif
	register_offset = SDR_CTRLGRP_PHYCTRL_PHYCTRL_0_ADDRESS;
	/* Read original register value */
	reg_value = CONFIG_HPS_SDR_CTRLCFG_PHYCTRL_PHYCTRL_0;
	if( sdram_write_verify(register_offset,	reg_value) == 1 ) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

/* newly added registers */
	/***** CPORTWIDTH_CPORTWIDTH *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CPORTWIDTH_CPORTWIDTH
#ifdef DEBUG
	debug("Configuring CPORTWIDTH\n");
#endif
	register_offset = SDR_CTRLGRP_CPORTWIDTH_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CPORTWIDTH_CPORTWIDTH,
			SDR_CTRLGRP_CPORTWIDTH_CMDPORTWIDTH_LSB,
			SDR_CTRLGRP_CPORTWIDTH_CMDPORTWIDTH_MASK);
#ifdef DEBUG
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
#endif
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
#ifdef DEBUG
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif
#endif

	/***** CPORTWMAP_CPORTWMAP *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CPORTWMAP_CPORTWMAP
#ifdef DEBUG
	debug("Configuring CPORTWMAP\n");
#endif
	register_offset = SDR_CTRLGRP_CPORTWMAP_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CPORTWMAP_CPORTWMAP,
			SDR_CTRLGRP_CPORTWMAP_CPORTWFIFOMAP_LSB,
			SDR_CTRLGRP_CPORTWMAP_CPORTWFIFOMAP_MASK);
#ifdef DEBUG
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
#endif
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
#ifdef DEBUG
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif
#endif

	/***** CPORTRMAP_CPORTRMAP *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CPORTRMAP_CPORTRMAP
#ifdef DEBUG
	debug("Configuring CPORTRMAP\n");
#endif
	register_offset = SDR_CTRLGRP_CPORTRMAP_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CPORTRMAP_CPORTRMAP,
			SDR_CTRLGRP_CPORTRMAP_CPORTRFIFOMAP_LSB,
			SDR_CTRLGRP_CPORTRMAP_CPORTRFIFOMAP_MASK);
#ifdef DEBUG
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
#endif
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
#ifdef DEBUG
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif
#endif

	/***** RFIFOCMAP_RFIFOCMAP *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_RFIFOCMAP_RFIFOCMAP
#ifdef DEBUG
	debug("Configuring RFIFOCMAP\n");
#endif
	register_offset = SDR_CTRLGRP_RFIFOCMAP_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_RFIFOCMAP_RFIFOCMAP,
			SDR_CTRLGRP_RFIFOCMAP_RFIFOCPORTMAP_LSB,
			SDR_CTRLGRP_RFIFOCMAP_RFIFOCPORTMAP_MASK);
#ifdef DEBUG
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
#endif
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
#ifdef DEBUG
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif
#endif

	/***** WFIFOCMAP_WFIFOCMAP *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_WFIFOCMAP_WFIFOCMAP
#ifdef DEBUG
	debug("Configuring WFIFOCMAP\n");
#endif
	register_offset = SDR_CTRLGRP_WFIFOCMAP_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_WFIFOCMAP_WFIFOCMAP,
			SDR_CTRLGRP_WFIFOCMAP_WFIFOCPORTMAP_LSB,
			SDR_CTRLGRP_WFIFOCMAP_WFIFOCPORTMAP_MASK);
#ifdef DEBUG
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
#endif
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
#ifdef DEBUG
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif
#endif

	/***** CPORTRDWR_CPORTRDWR *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CPORTRDWR_CPORTRDWR
#ifdef DEBUG
	debug("Configuring CPORTRDWR\n");
#endif
	register_offset = SDR_CTRLGRP_CPORTRDWR_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CPORTRDWR_CPORTRDWR,
			SDR_CTRLGRP_CPORTRDWR_CPORTRDWR_LSB,
			SDR_CTRLGRP_CPORTRDWR_CPORTRDWR_MASK);
#ifdef DEBUG
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
#endif
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
#ifdef DEBUG
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif
#endif
/* end of newly added registers */

	DEBUG_MEMORY
/***** Final step - apply configuration changes *****/
#ifdef DEBUG
	debug("Configuring STATICCFG_\n");
#endif
	register_offset = SDR_CTRLGRP_STATICCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field( reg_value, 1,
			SDR_CTRLGRP_STATICCFG_APPLYCFG_LSB,
			SDR_CTRLGRP_STATICCFG_APPLYCFG_MASK);
#ifdef DEBUG
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
#endif
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
#ifdef DEBUG
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif

	DEBUG_MEMORY
	return status;
}

unsigned sdram_calibration_full(void)
{
	return sdram_calibration();
}

#endif	/* CONFIG_SPL_BUILD */

