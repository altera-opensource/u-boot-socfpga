/*
 *  Copyright (C) 2012-2015 Altera Corporation <www.altera.com>
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
#include <asm/arch/debug_memory.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/system_manager.h>
#include <div64.h>
#include <pl330.h>
#include <watchdog.h>

#include "altlimits.h"

/*
 * SDRAM MMR init skip read back/verify steps
 * Define to speed up the MMR init process by just write without verifying
 */
/*#define SDRAM_MMR_SKIP_VERIFY*/

/* Determine the action when compare failed */
/*#define COMPARE_FAIL_ACTION	;*/
#define COMPARE_FAIL_ACTION	return 1;

#define ADDRORDER2_INFO \
	"INFO: Changing address order to 2 (row, chip, bank, column)\n"
#define ADDRORDER0_INFO \
	"INFO: Changing address order to 0 (chip, row, bank, column)\n"

/* define constant for 4G memory - used for SDRAM errata workaround */
#define MEMSIZE_4G (4ULL * 1024ULL * 1024ULL * 1024ULL)

DECLARE_GLOBAL_DATA_PTR;

unsigned long irq_cnt_ecc_sdram;
u8 pl330_buf[2500];

/* Initialise the DRAM by telling the DRAM Size. */
int dram_init(void)
{
	unsigned long sdram_size;

#ifdef CONFIG_SDRAM_CALCULATE_SIZE
	sdram_size = sdram_calculate_size();
#else
	sdram_size = PHYS_SDRAM_1_SIZE;
#endif
	gd->ram_size = get_ram_size(0, sdram_size);
	return 0;
}

/* Enable and disable SDRAM interrupt */
void sdram_enable_interrupt(unsigned enable)
{
	/* clear the ECC prior enable or even disable */
	setbits_le32((SOCFPGA_SDR_ADDRESS + SDR_CTRLGRP_DRAMINTR_ADDRESS),
		SDR_CTRLGRP_DRAMINTR_INTRCLR_MASK);

	if (enable)
		setbits_le32((SOCFPGA_SDR_ADDRESS +
			SDR_CTRLGRP_DRAMINTR_ADDRESS),
			SDR_CTRLGRP_DRAMINTR_INTREN_MASK);
	else
		clrbits_le32((SOCFPGA_SDR_ADDRESS +
			SDR_CTRLGRP_DRAMINTR_ADDRESS),
			SDR_CTRLGRP_DRAMINTR_INTREN_MASK);
}

/* handler for SDRAM ECC interrupt */
void irq_handler_ecc_sdram(void *arg)
{
	unsigned reg_value;

	DEBUG_MEMORY

	/* check whether SBE happen */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + SDR_CTRLGRP_DRAMSTS_ADDRESS);
	if (reg_value & SDR_CTRLGRP_DRAMSTS_SBEERR_MASK) {
		printf("Info: SDRAM ECC SBE @ 0x%08x\n",
			readl(SOCFPGA_SDR_ADDRESS +
			SDR_CTRLGRP_ERRADDR_ADDRESS));
		irq_cnt_ecc_sdram = irq_cnt_ecc_sdram +
			readl(SOCFPGA_SDR_ADDRESS +
			SDR_CTRLGRP_SBECOUNT_ADDRESS);
#ifndef CONFIG_SPL_BUILD
		setenv_ulong("sdram_ecc_sbe", irq_cnt_ecc_sdram);
#endif /* CONFIG_SPL_BUILD */
	}

	/* check whether DBE happen */
	if (reg_value & SDR_CTRLGRP_DRAMSTS_DBEERR_MASK) {
		puts("Error: SDRAM ECC DBE occurred\n");
		printf("sbecount = %lu\n", irq_cnt_ecc_sdram);
		printf("erraddr = %08x\n", readl(SOCFPGA_SDR_ADDRESS +
			SDR_CTRLGRP_ERRADDR_ADDRESS));
		printf("dropcount = %08x\n", readl(SOCFPGA_SDR_ADDRESS +
			SDR_CTRLGRP_DROPCOUNT_ADDRESS));
		printf("dropaddr = %08x\n", readl(SOCFPGA_SDR_ADDRESS +
			SDR_CTRLGRP_DROPADDR_ADDRESS));
	}

	/* clear the interrupt signal from SDRAM controller */
	setbits_le32((SOCFPGA_SDR_ADDRESS + SDR_CTRLGRP_DRAMINTR_ADDRESS),
		SDR_CTRLGRP_DRAMINTR_INTRCLR_MASK);

	/* if DBE, going into hang */
	if (reg_value & SDR_CTRLGRP_DRAMSTS_DBEERR_MASK) {
		sdram_enable_interrupt(0);
		hang();
	}
}


/* Below function only applicable for SPL */
#ifdef CONFIG_SPL_BUILD

int
compute_errata_rows(unsigned long long memsize, int cs, int width,
		    int rows, int banks, int cols)
{
	unsigned long long newrows;
	int inewrowslog2;
	int bits;

	debug("workaround rows - memsize %lld\n", memsize);
	debug("workaround rows - cs        %d\n", cs);
	debug("workaround rows - width     %d\n", width);
	debug("workaround rows - rows      %d\n", rows);
	debug("workaround rows - banks     %d\n", banks);
	debug("workaround rows - cols      %d\n", cols);

	newrows = lldiv(memsize, (cs * (width / 8)));
	debug("rows workaround - term1 %lld\n", newrows);

	newrows = lldiv(newrows, ((1 << banks) * (1 << cols)));
	debug("rows workaround - term2 %lld\n", newrows);

	/* Compute the hamming weight - same as number of bits set.
	 * Need to see if result is ordinal power of 2 before
	 * attempting log2 of result.
	 */
	bits = hweight32(newrows);

	debug("rows workaround - bits %d\n", bits);

	if (bits != 1) {
		printf("SDRAM workaround failed, bits set %d\n", bits);
		return rows;
	}

	if (newrows > UINT_MAX) {
		printf("SDRAM workaround rangecheck failed, %lld\n", newrows);
		return rows;
	}

	inewrowslog2 = __ilog2((unsigned int)newrows);

	debug("rows workaround - ilog2 %d, %d\n", inewrowslog2,
	       (int)newrows);

	if (inewrowslog2 == -1) {
		printf("SDRAM workaround failed, newrows %d\n", (int)newrows);
		return rows;
	}

	return inewrowslog2;
}

typedef struct _sdram_prot_rule {
	uint32_t	rule;		/* SDRAM protection rule number: 0-19 */
	uint64_t	sdram_start;	/* SDRAM start address */
	uint64_t	sdram_end;	/* SDRAM end address */
	int		valid;		/* Rule valid or not? 1 - valid, 0 not*/

	uint32_t	security;
	uint32_t	portmask;
	uint32_t	result;
	uint32_t	lo_prot_id;
	uint32_t	hi_prot_id;
} sdram_prot_rule, *psdram_prot_rule;

/* SDRAM protection rules vary from 0-19, a total of 20 rules. */

void sdram_set_rule(psdram_prot_rule prule)
{
	int regoffs;
	uint32_t lo_addr_bits;
	uint32_t hi_addr_bits;
	int ruleno = prule->rule;

	/* Select the rule */
	regoffs = SDR_CTRLGRP_PROTRULERDWR_ADDRESS;
	writel(ruleno, (SOCFPGA_SDR_ADDRESS + regoffs));

	/* Obtain the address bits */
	lo_addr_bits = (uint32_t)(((prule->sdram_start) >> 20ULL) & 0xFFF);
	hi_addr_bits = (uint32_t)((((prule->sdram_end-1) >> 20ULL)) & 0xFFF);

	debug("sdram set rule start %x, %lld\n", lo_addr_bits,
	      prule->sdram_start);
	debug("sdram set rule end   %x, %lld\n", hi_addr_bits,
	      prule->sdram_end);

	/* Set rule addresses */
	regoffs = SDR_CTRLGRP_PROTRULEADDR_ADDRESS;
	writel(lo_addr_bits | (hi_addr_bits << 12),
	       (SOCFPGA_SDR_ADDRESS + regoffs));

	/* Set rule protection ids */
	regoffs = SDR_CTRLGRP_PROTRULEID_ADDRESS;
	writel(prule->lo_prot_id | (prule->hi_prot_id << 12),
	       (SOCFPGA_SDR_ADDRESS + regoffs));

	/* Set the rule data */
	regoffs = SDR_CTRLGRP_PROTRULEDATA_ADDRESS;
	writel(prule->security | (prule->valid << 2) |
	       (prule->portmask << 3) | (prule->result << 13),
	       (SOCFPGA_SDR_ADDRESS + regoffs));

	/* write the rule */
	regoffs = SDR_CTRLGRP_PROTRULERDWR_ADDRESS;
	writel(ruleno | (1L << 5),
	       (SOCFPGA_SDR_ADDRESS + regoffs));

	/* Set rule number to 0 by default */
	writel(0, (SOCFPGA_SDR_ADDRESS + regoffs));
}

void sdram_get_rule(psdram_prot_rule prule)
{
	int regoffs;
	uint32_t protruleaddr;
	uint32_t protruleid;
	uint32_t protruledata;
	int ruleno = prule->rule;

	/* Read the rule */
	regoffs = SDR_CTRLGRP_PROTRULERDWR_ADDRESS;
	writel(ruleno, (SOCFPGA_SDR_ADDRESS + regoffs));
	writel(ruleno | (1L << 6),
	       (SOCFPGA_SDR_ADDRESS + regoffs));

	/* Get the addresses */
	regoffs = SDR_CTRLGRP_PROTRULEADDR_ADDRESS;
	protruleaddr = readl(SOCFPGA_SDR_ADDRESS + regoffs);
	prule->sdram_start = (protruleaddr & 0xFFF) << 20;
	prule->sdram_end = ((protruleaddr >> 12) & 0xFFF) << 20;

	/* Get the configured protection IDs */
	regoffs = SDR_CTRLGRP_PROTRULEID_ADDRESS;
	protruleid = readl(SOCFPGA_SDR_ADDRESS + regoffs);
	prule->lo_prot_id = protruleid & 0xFFF;
	prule->hi_prot_id = (protruleid >> 12) & 0xFFF;

	/* Get protection data */
	regoffs = SDR_CTRLGRP_PROTRULEDATA_ADDRESS;
	protruledata = readl(SOCFPGA_SDR_ADDRESS + regoffs);

	prule->security = protruledata & 0x3;
	prule->valid = (protruledata >> 2) & 0x1;
	prule->portmask = (protruledata >> 3) & 0x3FF;
	prule->result = (protruledata >> 13) & 0x1;
}


void sdram_set_protection_config(uint64_t sdram_start, uint64_t sdram_end)
{
	sdram_prot_rule rule;
	int rules;
	int regoffs = SDR_CTRLGRP_PROTPORTDEFAULT_ADDRESS;

	/* Start with accepting all SDRAM transaction */
	writel(0x0, (SOCFPGA_SDR_ADDRESS + regoffs));

	/* Clear all protection rules for warm boot case */

	rule.sdram_start = 0;
	rule.sdram_end = 0;
	rule.lo_prot_id = 0;
	rule.hi_prot_id = 0;
	rule.portmask = 0;
	rule.security = 0;
	rule.result = 0;
	rule.valid = 0;
	rule.rule = 0;

	for (rules = 0; rules < 20; rules++) {
		rule.rule = rules;
		sdram_set_rule(&rule);
	}

	/* new rule: accept SDRAM */
	rule.sdram_start = sdram_start;
	rule.sdram_end = sdram_end;
	rule.lo_prot_id = 0x0;
	rule.hi_prot_id = 0xFFF;
	rule.portmask = 0x3FF;
	rule.security = 0x3;
	rule.result = 0;
	rule.valid = 1;
	rule.rule = 0;

	/* set new rule */
	sdram_set_rule(&rule);

	/* default rule: reject everything */
	writel(0x3ff, (SOCFPGA_SDR_ADDRESS + regoffs));
}

void sdram_dump_protection_config(void)
{
	sdram_prot_rule rule;
	int rules;
	int regoffs = SDR_CTRLGRP_PROTPORTDEFAULT_ADDRESS;

	debug("SDRAM Prot rule, default %x\n",
	      readl(SOCFPGA_SDR_ADDRESS + regoffs));

	for (rules = 0; rules < 20; rules++) {
		sdram_get_rule(&rule);
		debug("Rule %d, rules ...\n", rules);
		debug("    sdram start %llx\n", rule.sdram_start);
		debug("    sdram end   %llx\n", rule.sdram_end);
		debug("    low prot id %d, hi prot id %d\n",
		      rule.lo_prot_id,
		      rule.hi_prot_id);
		debug("    portmask %x\n", rule.portmask);
		debug("    security %d\n", rule.security);
		debug("    result %d\n", rule.result);
		debug("    valid %d\n", rule.valid);
	}
}




/* Function to update the field within variable */
unsigned sdram_write_register_field (unsigned masked_value,
	unsigned data, unsigned shift, unsigned mask)
{
	masked_value &= ~(mask);
	masked_value |= (data << shift) & mask;
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
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
#ifndef SDRAM_MMR_SKIP_VERIFY
	debug("   Read and verify...");
	/* Read back the wrote value */
	reg_value1 = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	/* Indicate failure if value not matched */
	if (reg_value1 != reg_value) {
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
unsigned sdram_mmr_init_full(unsigned int sdr_phy_reg)
{
	unsigned long register_offset, reg_value;
	unsigned long status = 0;
	int addrorder;
	char *paddrorderinfo = NULL;

#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS) && \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS) && \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_BANKBITS) && \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_COLBITS) && \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS)

	int cs = CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS;
	int width = 8;
	int rows = CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS;
	int banks = CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_BANKBITS;
	int cols = CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_COLBITS;
	unsigned long long workaround_memsize = MEMSIZE_4G;

	writel(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS,
	       ISWGRP_HANDOFF_ROWBITS);
#endif

	DEBUG_MEMORY
	/***** CTRLCFG *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMTYPE) || \
defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMBL) || \
defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ADDRORDER) || \
defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN) || \
defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCCORREN) || \
defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_REORDEREN) || \
defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_STARVELIMIT) || \
defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_DQSTRKEN) || \
defined(CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_NODMPINS)
	debug("\nConfiguring CTRLCFG\n");
	register_offset = SDR_CTRLGRP_CTRLCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMTYPE
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMTYPE,
			SDR_CTRLGRP_CTRLCFG_MEMTYPE_LSB,
			SDR_CTRLGRP_CTRLCFG_MEMTYPE_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMBL
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_MEMBL,
			SDR_CTRLGRP_CTRLCFG_MEMBL_LSB,
			SDR_CTRLGRP_CTRLCFG_MEMBL_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ADDRORDER

	/* SDRAM Failure When Accessing Non-Existent Memory
	 * Set the addrorder field of the SDRAM control register
	 * based on the CSBITs setting.
	 */

	switch (CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS) {
	case 1:
		addrorder = 0; /* chip, row, bank, column */
		if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ADDRORDER != 0)
			paddrorderinfo = ADDRORDER0_INFO;
		break;
	case 2:
		addrorder = 2; /* row, chip, bank, column */
		if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ADDRORDER != 2)
			paddrorderinfo = ADDRORDER2_INFO;
		break;
	default:
		addrorder = CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ADDRORDER;
		break;
	}
	if (paddrorderinfo)
		printf(paddrorderinfo);

	reg_value = sdram_write_register_field(reg_value,
			addrorder,
			SDR_CTRLGRP_CTRLCFG_ADDRORDER_LSB,
			SDR_CTRLGRP_CTRLCFG_ADDRORDER_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN,
			SDR_CTRLGRP_CTRLCFG_ECCEN_LSB,
			SDR_CTRLGRP_CTRLCFG_ECCEN_MASK);
#endif

	/* Always set ECCCORREN to disabled */
	reg_value = sdram_write_register_field(reg_value,
			0,
			SDR_CTRLGRP_CTRLCFG_ECCCORREN_LSB,
			SDR_CTRLGRP_CTRLCFG_ECCCORREN_MASK);

#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_REORDEREN
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_REORDEREN,
			SDR_CTRLGRP_CTRLCFG_REORDEREN_LSB,
			SDR_CTRLGRP_CTRLCFG_REORDEREN_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_STARVELIMIT
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_STARVELIMIT,
			SDR_CTRLGRP_CTRLCFG_STARVELIMIT_LSB,
			SDR_CTRLGRP_CTRLCFG_STARVELIMIT_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_DQSTRKEN
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_DQSTRKEN,
			SDR_CTRLGRP_CTRLCFG_DQSTRKEN_LSB,
			SDR_CTRLGRP_CTRLCFG_DQSTRKEN_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_NODMPINS
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_NODMPINS,
			SDR_CTRLGRP_CTRLCFG_NODMPINS_LSB,
			SDR_CTRLGRP_CTRLCFG_NODMPINS_MASK);
#endif
#if (CONFIG_HPS_SDR_CTRLCFG_CTRLCFG_ECCEN == 1)
	/* Enable ECC overwrites if ECC is enabled */
	reg_value = sdram_write_register_field(reg_value, 0x1,
		SDR_CTRLGRP_CTRLCFG_CFG_ENABLE_ECC_CODE_OVERWRITES_LSB,
		SDR_CTRLGRP_CTRLCFG_CFG_ENABLE_ECC_CODE_OVERWRITES_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMTIMING1 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCWL) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_AL) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCL) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRRD) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TFAW) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRFC)
	debug("Configuring DRAMTIMING1\n");
	register_offset = SDR_CTRLGRP_DRAMTIMING1_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCWL
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCWL,
			SDR_CTRLGRP_DRAMTIMING1_TCWL_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TCWL_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_AL
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_AL,
			SDR_CTRLGRP_DRAMTIMING1_TAL_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TAL_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCL
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TCL,
			SDR_CTRLGRP_DRAMTIMING1_TCL_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TCL_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRRD
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRRD,
			SDR_CTRLGRP_DRAMTIMING1_TRRD_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TRRD_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TFAW
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TFAW,
			SDR_CTRLGRP_DRAMTIMING1_TFAW_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TFAW_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRFC
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING1_TRFC,
			SDR_CTRLGRP_DRAMTIMING1_TRFC_LSB,
			SDR_CTRLGRP_DRAMTIMING1_TRFC_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMTIMING2 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TREFI) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRCD) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRP) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWR) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWTR)
	debug("Configuring DRAMTIMING2\n");
	register_offset = SDR_CTRLGRP_DRAMTIMING2_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TREFI
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TREFI,
			SDR_CTRLGRP_DRAMTIMING2_TREFI_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TREFI_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRCD
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRCD,
			SDR_CTRLGRP_DRAMTIMING2_TRCD_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TRCD_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRP
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TRP,
			SDR_CTRLGRP_DRAMTIMING2_TRP_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TRP_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWR
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWR,
			SDR_CTRLGRP_DRAMTIMING2_TWR_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TWR_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWTR
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING2_IF_TWTR,
			SDR_CTRLGRP_DRAMTIMING2_TWTR_LSB,
			SDR_CTRLGRP_DRAMTIMING2_TWTR_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMTIMING3 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRTP) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRAS) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRC) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TMRD) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TCCD)
	debug("Configuring DRAMTIMING3\n");
	register_offset = SDR_CTRLGRP_DRAMTIMING3_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRTP
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRTP,
			SDR_CTRLGRP_DRAMTIMING3_TRTP_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TRTP_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRAS
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRAS,
			SDR_CTRLGRP_DRAMTIMING3_TRAS_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TRAS_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRC
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TRC,
			SDR_CTRLGRP_DRAMTIMING3_TRC_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TRC_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TMRD
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TMRD,
			SDR_CTRLGRP_DRAMTIMING3_TMRD_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TMRD_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TCCD
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING3_TCCD,
			SDR_CTRLGRP_DRAMTIMING3_TCCD_LSB,
			SDR_CTRLGRP_DRAMTIMING3_TCCD_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMTIMING4 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_SELFRFSHEXIT) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_PWRDOWNEXIT)
	debug("Configuring DRAMTIMING4\n");
	register_offset = SDR_CTRLGRP_DRAMTIMING4_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_SELFRFSHEXIT
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_SELFRFSHEXIT,
			SDR_CTRLGRP_DRAMTIMING4_SELFRFSHEXIT_LSB,
			SDR_CTRLGRP_DRAMTIMING4_SELFRFSHEXIT_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_PWRDOWNEXIT
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMTIMING4_PWRDOWNEXIT,
			SDR_CTRLGRP_DRAMTIMING4_PWRDOWNEXIT_LSB,
			SDR_CTRLGRP_DRAMTIMING4_PWRDOWNEXIT_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** LOWPWRTIMING *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_LOWPWRTIMING_AUTOPDCYCLES) || \
defined(CONFIG_HPS_SDR_CTRLCFG_LOWPWRTIMING_CLKDISABLECYCLES)
	debug("Configuring LOWPWRTIMING\n");
	register_offset = SDR_CTRLGRP_LOWPWRTIMING_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_LOWPWRTIMING_AUTOPDCYCLES
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_LOWPWRTIMING_AUTOPDCYCLES,
			SDR_CTRLGRP_LOWPWRTIMING_AUTOPDCYCLES_LSB,
			SDR_CTRLGRP_LOWPWRTIMING_AUTOPDCYCLES_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_LOWPWRTIMING_CLKDISABLECYCLES
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_LOWPWRTIMING_CLKDISABLECYCLES,
			SDR_CTRLGRP_LOWPWRTIMING_CLKDISABLECYCLES_LSB,
			SDR_CTRLGRP_LOWPWRTIMING_CLKDISABLECYCLES_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** DRAMADDRW *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_COLBITS) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_BANKBITS) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS)
	debug("Configuring DRAMADDRW\n");
	register_offset = SDR_CTRLGRP_DRAMADDRW_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_COLBITS
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_COLBITS,
			SDR_CTRLGRP_DRAMADDRW_COLBITS_LSB,
			SDR_CTRLGRP_DRAMADDRW_COLBITS_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS
	/* SDRAM Failure When Accessing Non-Existent Memory
	 * Update Preloader to artificially increase the number of rows so
	 * that the memory thinks it has 4GB of RAM.
	 */
	rows = compute_errata_rows(workaround_memsize, cs, width, rows, banks,
				   cols);

	reg_value = sdram_write_register_field(reg_value,
			rows,
			SDR_CTRLGRP_DRAMADDRW_ROWBITS_LSB,
			SDR_CTRLGRP_DRAMADDRW_ROWBITS_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_BANKBITS
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_BANKBITS,
			SDR_CTRLGRP_DRAMADDRW_BANKBITS_LSB,
			SDR_CTRLGRP_DRAMADDRW_BANKBITS_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS
	/* SDRAM Failure When Accessing Non-Existent Memory
	 * Set SDR_CTRLGRP_DRAMADDRW_CSBITS_LSB to
	 * log2(number of chip select bits). Since there's only
	 * 1 or 2 chip selects, log2(1) => 0, and log2(2) => 1,
	 * which is the same as "chip selects" - 1.
	 */
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS - 1,
			SDR_CTRLGRP_DRAMADDRW_CSBITS_LSB,
			SDR_CTRLGRP_DRAMADDRW_CSBITS_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** DRAMIFWIDTH *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMIFWIDTH_IFWIDTH
	debug("Configuring DRAMIFWIDTH\n");
	register_offset = SDR_CTRLGRP_DRAMIFWIDTH_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMIFWIDTH_IFWIDTH,
			SDR_CTRLGRP_DRAMIFWIDTH_IFWIDTH_LSB,
			SDR_CTRLGRP_DRAMIFWIDTH_IFWIDTH_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMDEVWIDTH *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMDEVWIDTH_DEVWIDTH
	debug("Configuring DRAMDEVWIDTH\n");
	register_offset = SDR_CTRLGRP_DRAMDEVWIDTH_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMDEVWIDTH_DEVWIDTH,
			SDR_CTRLGRP_DRAMDEVWIDTH_DEVWIDTH_LSB,
			SDR_CTRLGRP_DRAMDEVWIDTH_DEVWIDTH_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** LOWPWREQ *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_LOWPWREQ_SELFRFSHMASK
	debug("Configuring LOWPWREQ\n");
	register_offset = SDR_CTRLGRP_LOWPWREQ_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_LOWPWREQ_SELFRFSHMASK,
			SDR_CTRLGRP_LOWPWREQ_SELFRFSHMASK_LSB,
			SDR_CTRLGRP_LOWPWREQ_SELFRFSHMASK_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** DRAMINTR *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMINTR_INTREN
	debug("Configuring DRAMINTR\n");
	register_offset = SDR_CTRLGRP_DRAMINTR_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMINTR_INTREN,
			SDR_CTRLGRP_DRAMINTR_INTREN_LSB,
			SDR_CTRLGRP_DRAMINTR_INTREN_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** STATICCFG *****/
	debug("Configuring STATICCFG\n");
	register_offset = SDR_CTRLGRP_STATICCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_STATICCFG_MEMBL
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_STATICCFG_MEMBL,
			SDR_CTRLGRP_STATICCFG_MEMBL_LSB,
			SDR_CTRLGRP_STATICCFG_MEMBL_MASK);
#endif
	/* Always set USEECCASDATA to 0 */
	reg_value = sdram_write_register_field(reg_value,
			0,
			SDR_CTRLGRP_STATICCFG_USEECCASDATA_LSB,
			SDR_CTRLGRP_STATICCFG_USEECCASDATA_MASK);

	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}


	/***** CTRLWIDTH *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CTRLWIDTH_CTRLWIDTH
	debug("Configuring CTRLWIDTH\n");
	register_offset = SDR_CTRLGRP_CTRLWIDTH_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CTRLWIDTH_CTRLWIDTH,
			SDR_CTRLGRP_CTRLWIDTH_CTRLWIDTH_LSB,
			SDR_CTRLGRP_CTRLWIDTH_CTRLWIDTH_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** PORTCFG *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_PORTCFG_AUTOPCHEN
	debug("Configuring PORTCFG\n");
	register_offset = SDR_CTRLGRP_PORTCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_PORTCFG_AUTOPCHEN,
			SDR_CTRLGRP_PORTCFG_AUTOPCHEN_LSB,
			SDR_CTRLGRP_PORTCFG_AUTOPCHEN_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** FIFOCFG *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_SYNCMODE) || \
defined(CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_INCSYNC)
	debug("Configuring FIFOCFG\n");
	register_offset = SDR_CTRLGRP_FIFOCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_SYNCMODE
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_SYNCMODE,
			SDR_CTRLGRP_FIFOCFG_SYNCMODE_LSB,
			SDR_CTRLGRP_FIFOCFG_SYNCMODE_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_INCSYNC
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_FIFOCFG_INCSYNC,
			SDR_CTRLGRP_FIFOCFG_INCSYNC_LSB,
			SDR_CTRLGRP_FIFOCFG_INCSYNC_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPPRIORITY *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPRIORITY_USERPRIORITY
	debug("Configuring MPPRIORITY\n");
	register_offset = SDR_CTRLGRP_MPPRIORITY_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPRIORITY_USERPRIORITY,
			SDR_CTRLGRP_MPPRIORITY_USERPRIORITY_LSB,
			SDR_CTRLGRP_MPPRIORITY_USERPRIORITY_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPWEIGHT_MPWEIGHT_0 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_0_STATICWEIGHT_31_0
	debug("Configuring MPWEIGHT_MPWEIGHT_0\n");
	register_offset = SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_0_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_0_STATICWEIGHT_31_0,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_0_STATICWEIGHT_31_0_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_0_STATICWEIGHT_31_0_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPWEIGHT_MPWEIGHT_1 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_STATICWEIGHT_49_32) || \
defined(CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_SUMOFWEIGHT_13_0)
	debug("Configuring MPWEIGHT_MPWEIGHT_1\n");
	register_offset = SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_STATICWEIGHT_49_32
	reg_value = sdram_write_register_field(reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_STATICWEIGHT_49_32,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_STATICWEIGHT_49_32_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_STATICWEIGHT_49_32_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_SUMOFWEIGHT_13_0
	reg_value = sdram_write_register_field(reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_1_SUMOFWEIGHT_13_0,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_SUMOFWEIGHTS_13_0_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_1_SUMOFWEIGHTS_13_0_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPWEIGHT_MPWEIGHT_2 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_2_SUMOFWEIGHT_45_14
	debug("Configuring MPWEIGHT_MPWEIGHT_2\n");
	register_offset = SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_2_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_2_SUMOFWEIGHT_45_14,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_2_SUMOFWEIGHTS_45_14_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_2_SUMOFWEIGHTS_45_14_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPWEIGHT_MPWEIGHT_3 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_3_SUMOFWEIGHT_63_46
	debug("Configuring MPWEIGHT_MPWEIGHT_3\n");
	register_offset = SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_3_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
		CONFIG_HPS_SDR_CTRLCFG_MPWIEIGHT_3_SUMOFWEIGHT_63_46,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_3_SUMOFWEIGHTS_63_46_LSB,
		SDR_CTRLGRP_MPWEIGHT_MPWEIGHT_3_SUMOFWEIGHTS_63_46_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPPACING_MPPACING_0 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_0_THRESHOLD1_31_0
	debug("Configuring MPPACING_MPPACING_0\n");
	register_offset = SDR_CTRLGRP_MPPACING_MPPACING_0_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_0_THRESHOLD1_31_0,
			SDR_CTRLGRP_MPPACING_MPPACING_0_THRESHOLD1_31_0_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_0_THRESHOLD1_31_0_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** MPPACING_MPPACING_1 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD1_59_32) || \
defined(CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD2_3_0)
	debug("Configuring MPPACING_MPPACING_1\n");
	register_offset = SDR_CTRLGRP_MPPACING_MPPACING_1_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD1_59_32
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD1_59_32,
			SDR_CTRLGRP_MPPACING_MPPACING_1_THRESHOLD1_59_32_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_1_THRESHOLD1_59_32_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD2_3_0
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_1_THRESHOLD2_3_0,
			SDR_CTRLGRP_MPPACING_MPPACING_1_THRESHOLD2_3_0_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_1_THRESHOLD2_3_0_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** MPPACING_MPPACING_2 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_2_THRESHOLD2_35_4
	debug("Configuring MPPACING_MPPACING_2\n");
	register_offset = SDR_CTRLGRP_MPPACING_MPPACING_2_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_2_THRESHOLD2_35_4,
			SDR_CTRLGRP_MPPACING_MPPACING_2_THRESHOLD2_35_4_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_2_THRESHOLD2_35_4_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPPACING_MPPACING_3 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPPACING_3_THRESHOLD2_59_36
	debug("Configuring MPPACING_MPPACING_3\n");
	register_offset = SDR_CTRLGRP_MPPACING_MPPACING_3_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_MPPACING_3_THRESHOLD2_59_36,
			SDR_CTRLGRP_MPPACING_MPPACING_3_THRESHOLD2_59_36_LSB,
			SDR_CTRLGRP_MPPACING_MPPACING_3_THRESHOLD2_59_36_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPTHRESHOLDRST_MPTHRESHOLDRST_0 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_0_THRESHOLDRSTCYCLES_31_0
	debug("Configuring MPTHRESHOLDRST_MPTHRESHOLDRST_0\n");
	register_offset = SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_0_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
	CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_0_THRESHOLDRSTCYCLES_31_0,
	SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_0_THRESHOLDRSTCYCLES_31_0_LSB,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_0_THRESHOLDRSTCYCLES_31_0_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPTHRESHOLDRST_MPTHRESHOLDRST_1 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_1_THRESHOLDRSTCYCLES_63_32
	debug("Configuring MPTHRESHOLDRST_MPTHRESHOLDRST_1\n");
	register_offset = SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_1_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
	CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_1_THRESHOLDRSTCYCLES_63_32,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_1_THRESHOLDRSTCYCLES_63_32_LSB,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_1_THRESHOLDRSTCYCLES_63_32_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif


	/***** MPTHRESHOLDRST_MPTHRESHOLDRST_2 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_2_THRESHOLDRSTCYCLES_79_64
	debug("Configuring MPTHRESHOLDRST_MPTHRESHOLDRST_2\n");
	register_offset = SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_2_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
	CONFIG_HPS_SDR_CTRLCFG_MPTHRESHOLDRST_2_THRESHOLDRSTCYCLES_79_64,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_2_THRESHOLDRSTCYCLES_79_64_LSB,
SDR_CTRLGRP_MPTHRESHOLDRST_MPTHRESHOLDRST_2_THRESHOLDRSTCYCLES_79_64_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** PHYCTRL_PHYCTRL_0 *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_PHYCTRL_PHYCTRL_0
	debug("Configuring PHYCTRL_PHYCTRL_0\n");
	register_offset = SDR_CTRLGRP_PHYCTRL_PHYCTRL_0_ADDRESS;
	/* Read original register value */
	reg_value = CONFIG_HPS_SDR_CTRLCFG_PHYCTRL_PHYCTRL_0;
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

/* newly added registers */
	/***** CPORTWIDTH_CPORTWIDTH *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CPORTWIDTH_CPORTWIDTH
	debug("Configuring CPORTWIDTH\n");
	register_offset = SDR_CTRLGRP_CPORTWIDTH_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CPORTWIDTH_CPORTWIDTH,
			SDR_CTRLGRP_CPORTWIDTH_CMDPORTWIDTH_LSB,
			SDR_CTRLGRP_CPORTWIDTH_CMDPORTWIDTH_MASK);
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif

	/***** CPORTWMAP_CPORTWMAP *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CPORTWMAP_CPORTWMAP
	debug("Configuring CPORTWMAP\n");
	register_offset = SDR_CTRLGRP_CPORTWMAP_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CPORTWMAP_CPORTWMAP,
			SDR_CTRLGRP_CPORTWMAP_CPORTWFIFOMAP_LSB,
			SDR_CTRLGRP_CPORTWMAP_CPORTWFIFOMAP_MASK);
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif

	/***** CPORTRMAP_CPORTRMAP *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CPORTRMAP_CPORTRMAP
	debug("Configuring CPORTRMAP\n");
	register_offset = SDR_CTRLGRP_CPORTRMAP_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CPORTRMAP_CPORTRMAP,
			SDR_CTRLGRP_CPORTRMAP_CPORTRFIFOMAP_LSB,
			SDR_CTRLGRP_CPORTRMAP_CPORTRFIFOMAP_MASK);
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif

	/***** RFIFOCMAP_RFIFOCMAP *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_RFIFOCMAP_RFIFOCMAP
	debug("Configuring RFIFOCMAP\n");
	register_offset = SDR_CTRLGRP_RFIFOCMAP_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_RFIFOCMAP_RFIFOCMAP,
			SDR_CTRLGRP_RFIFOCMAP_RFIFOCPORTMAP_LSB,
			SDR_CTRLGRP_RFIFOCMAP_RFIFOCPORTMAP_MASK);
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif

	/***** WFIFOCMAP_WFIFOCMAP *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_WFIFOCMAP_WFIFOCMAP
	debug("Configuring WFIFOCMAP\n");
	register_offset = SDR_CTRLGRP_WFIFOCMAP_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_WFIFOCMAP_WFIFOCMAP,
			SDR_CTRLGRP_WFIFOCMAP_WFIFOCPORTMAP_LSB,
			SDR_CTRLGRP_WFIFOCMAP_WFIFOCPORTMAP_MASK);
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif

	/***** CPORTRDWR_CPORTRDWR *****/
#ifdef CONFIG_HPS_SDR_CTRLCFG_CPORTRDWR_CPORTRDWR
	debug("Configuring CPORTRDWR\n");
	register_offset = SDR_CTRLGRP_CPORTRDWR_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_CPORTRDWR_CPORTRDWR,
			SDR_CTRLGRP_CPORTRDWR_CPORTRDWR_LSB,
			SDR_CTRLGRP_CPORTRDWR_CPORTRDWR_MASK);
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);
#endif
/* end of newly added registers */


	/***** DRAMODT *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_DRAMODT_READ) || \
defined(CONFIG_HPS_SDR_CTRLCFG_DRAMODT_WRITE)
	debug("Configuring DRAMODT\n");
	register_offset = SDR_CTRLGRP_DRAMODT_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMODT_READ
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMODT_READ,
			SDR_CTRLGRP_DRAMODT_READ_LSB,
			SDR_CTRLGRP_DRAMODT_READ_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_DRAMODT_WRITE
	reg_value = sdram_write_register_field(reg_value,
			CONFIG_HPS_SDR_CTRLCFG_DRAMODT_WRITE,
			SDR_CTRLGRP_DRAMODT_WRITE_LSB,
			SDR_CTRLGRP_DRAMODT_WRITE_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

	/***** EXTRATIME1 *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_EXTRATIME1_CFG_EXTRA_CTL_CLK_RD_TO_WR) || \
defined(CONFIG_HPS_SDR_CTRLCFG_EXTRATIME1_CFG_EXTRA_CTL_CLK_RD_TO_WR_BC) || \
defined(CONFIG_HPS_SDR_CTRLCFG_EXTRATIME1_CFG_EXTRA_CTL_CLK_RD_TO_WR_DIFF_CHIP)
	debug("Configuring EXTRATIME1\n");
	register_offset = SDR_CTRLGRP_EXTRATIME1_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
#ifdef CONFIG_HPS_SDR_CTRLCFG_EXTRATIME1_CFG_EXTRA_CTL_CLK_RD_TO_WR
	reg_value = sdram_write_register_field(reg_value,
		CONFIG_HPS_SDR_CTRLCFG_EXTRATIME1_CFG_EXTRA_CTL_CLK_RD_TO_WR,
			SDR_CTRLGRP_EXTRATIME1_RD_TO_WR_LSB,
			SDR_CTRLGRP_EXTRATIME1_RD_TO_WR_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_EXTRATIME1_CFG_EXTRA_CTL_CLK_RD_TO_WR_BC
	reg_value = sdram_write_register_field(reg_value,
		CONFIG_HPS_SDR_CTRLCFG_EXTRATIME1_CFG_EXTRA_CTL_CLK_RD_TO_WR_BC,
			SDR_CTRLGRP_EXTRATIME1_RD_TO_WR_BC_LSB,
			SDR_CTRLGRP_EXTRATIME1_RD_TO_WR_BC_MASK);
#endif
#ifdef CONFIG_HPS_SDR_CTRLCFG_EXTRATIME1_CFG_EXTRA_CTL_CLK_RD_TO_WR_DIFF_CHIP
	reg_value = sdram_write_register_field(reg_value,
	CONFIG_HPS_SDR_CTRLCFG_EXTRATIME1_CFG_EXTRA_CTL_CLK_RD_TO_WR_DIFF_CHIP,
			SDR_CTRLGRP_EXTRATIME1_RD_TO_WR_DIFF_LSB,
			SDR_CTRLGRP_EXTRATIME1_RD_TO_WR_DIFF_MASK);
#endif
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		status = 1;
		COMPARE_FAIL_ACTION
	}
#endif

/***** FPGAPORTRST *****/
#if defined(CONFIG_HPS_SDR_CTRLCFG_FPGAPORTRST_READ_PORT_USED)
#ifdef DEBUG
	debug("Configuring FPGAPORTRST\n");
#endif
	register_offset = SDR_CTRLGRP_FPGAPORTRST_ADDRESS;
	/* All value will be provided */
	reg_value = CONFIG_HPS_SDR_CTRLCFG_FPGAPORTRST;

	/* saving this value to SYSMGR.ISWGRP.HANDOFF.FPGA2SDR */
	writel(reg_value, ISWGRP_HANDOFF_FPGA2SDR);

	/* only enable if the FPGA is programmed */
	if (is_fpgamgr_fpga_ready()) {
		if (sdram_write_verify(register_offset,	reg_value) == 1) {
			/* Set status to 1 to ensure we return failed status
			if user wish the COMPARE_FAIL_ACTION not to do anything.
			This is to cater scenario where user wish to
			continue initlization even verify failed. */
			status = 1;
			COMPARE_FAIL_ACTION
		}
	}
#endif

	/* Restore the SDR PHY Register if valid */
	if (sdr_phy_reg != 0xffffffff)
		writel(sdr_phy_reg, SOCFPGA_SDR_ADDRESS +
			SDR_CTRLGRP_PHYCTRL_PHYCTRL_0_ADDRESS);

	DEBUG_MEMORY
/***** Final step - apply configuration changes *****/
	debug("Configuring STATICCFG_\n");
	register_offset = SDR_CTRLGRP_STATICCFG_ADDRESS;
	/* Read original register value */
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value, 1,
			SDR_CTRLGRP_STATICCFG_APPLYCFG_LSB,
			SDR_CTRLGRP_STATICCFG_APPLYCFG_MASK);
	debug("   Write - Address ");
	debug("0x%08x Data 0x%08x\n",
		(unsigned)(SOCFPGA_SDR_ADDRESS+register_offset),
		(unsigned)reg_value);
	writel(reg_value, (SOCFPGA_SDR_ADDRESS + register_offset));
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	debug("   Read value without verify 0x%08x\n", (unsigned)reg_value);

	sdram_set_protection_config(0, sdram_calculate_size());

	sdram_dump_protection_config();

	DEBUG_MEMORY
	return status;
}

unsigned sdram_calibration_full(void)
{
	return sdram_calibration();
}

/* Checks if the controller can enter then exit self-refresh correctly */
unsigned sdram_check_self_refresh_seq(void)
{
#define MAX_POLLS 10
	unsigned int counter;
	unsigned register_offset;
	unsigned reg_value;

	/*****************************/
	/* Try to go to self-refresh */
	/*****************************/
	debug("Checks if the controller can enter then exit self-refresh correctly\n");

	/* Set sdr.ctrlgrp.lowpwreq.selfrfshmask = 3 */
	register_offset = SDR_CTRLGRP_LOWPWREQ_ADDRESS;
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			SDR_CTRLGRP_LOWPWREQ_SELFRFSHMASK_BOTH_CHIPS,
			SDR_CTRLGRP_LOWPWREQ_SELFRFSHMASK_LSB,
			SDR_CTRLGRP_LOWPWREQ_SELFRFSHMASK_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		COMPARE_FAIL_ACTION
	}

	/* Set sdr.ctrlgrp.lowpwreq.selfrshreq = 1 */
	register_offset = SDR_CTRLGRP_LOWPWREQ_ADDRESS;
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			SDR_CTRLGRP_LOWPWREQ_SELFRSHREQ_ENABLED,
			SDR_CTRLGRP_LOWPWREQ_SELFRSHREQ_LSB,
			SDR_CTRLGRP_LOWPWREQ_SELFRSHREQ_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		COMPARE_FAIL_ACTION
	}

	/* Poll until sdr.ctrlgrp.lowpwrack.selfrfshack = 1, with timeout */
	for (counter = 0; counter < MAX_POLLS; counter++)
		if (readl(SOCFPGA_SDR_ADDRESS + SDR_CTRLGRP_LOWPWRACK_ADDRESS)
			& SDR_CTRLGRP_LOWPWRACK_SELFRFSHACK_MASK)
			break;

	/* Check if succeeded getting sdr.ctrlgrp.lowpwrack.selfrfshack = 1*/
	if (!(readl(SOCFPGA_SDR_ADDRESS + SDR_CTRLGRP_LOWPWRACK_ADDRESS)
		& SDR_CTRLGRP_LOWPWRACK_SELFRFSHACK_MASK)) {
		/* Set back sdr.ctrlgrp.lowpwreq.selfrshreq = 0 */
		register_offset = SDR_CTRLGRP_LOWPWREQ_ADDRESS;
		reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
		reg_value = sdram_write_register_field(reg_value,
				SDR_CTRLGRP_LOWPWREQ_SELFRSHREQ_DISABLED,
				SDR_CTRLGRP_LOWPWREQ_SELFRSHREQ_LSB,
				SDR_CTRLGRP_LOWPWREQ_SELFRSHREQ_MASK);
		if (sdram_write_verify(register_offset,	reg_value) == 1) {
			COMPARE_FAIL_ACTION
		}

		/* Failure */
		return 1;
	}

	/*****************************/
	/* Try to exit self-refresh */
	/*****************************/

	/* Set sdr.ctrlgrp.lowpwreq.selfrshreq = 0 */
	register_offset = SDR_CTRLGRP_LOWPWREQ_ADDRESS;
	reg_value = readl(SOCFPGA_SDR_ADDRESS + register_offset);
	reg_value = sdram_write_register_field(reg_value,
			SDR_CTRLGRP_LOWPWREQ_SELFRSHREQ_DISABLED,
			SDR_CTRLGRP_LOWPWREQ_SELFRSHREQ_LSB,
			SDR_CTRLGRP_LOWPWREQ_SELFRSHREQ_MASK);
	if (sdram_write_verify(register_offset,	reg_value) == 1) {
		COMPARE_FAIL_ACTION
	}

	/* Poll until sdr.ctrlgrp.lowpwrack.selfrfshack = 0, with timeout */
	for (counter = 0; counter < MAX_POLLS; counter++)
		if (!(readl(SOCFPGA_SDR_ADDRESS + SDR_CTRLGRP_LOWPWRACK_ADDRESS)
			& SDR_CTRLGRP_LOWPWRACK_SELFRFSHACK_MASK))
			break;

	/* Check if we succeeded */
	if (readl(SOCFPGA_SDR_ADDRESS + SDR_CTRLGRP_LOWPWRACK_ADDRESS)
		& SDR_CTRLGRP_LOWPWRACK_SELFRFSHACK_MASK) {
		/* Failure */
		return 1;
	}

	/* Success */
	return 0;
}

#endif	/* CONFIG_SPL_BUILD */

/* To calculate SDRAM device size based on SDRAM controller parameters.
 * Size is specified in bytes.
 *
 * NOTE!!!!
 * This function is compiled and linked into the preloader and
 * Uboot (there may be others). So if this function changes, the Preloader
 * and UBoot must be updated simultaneously.
 */
unsigned long sdram_calculate_size(void)
{
	unsigned long temp;
	unsigned long row, bank, col, cs, width;

	temp = readl(SOCFPGA_SDR_ADDRESS +
		SDR_CTRLGRP_DRAMADDRW_ADDRESS);
	col = (temp & SDR_CTRLGRP_DRAMADDRW_COLBITS_MASK) >>
		SDR_CTRLGRP_DRAMADDRW_COLBITS_LSB;

	/* SDRAM Failure When Accessing Non-Existent Memory
	 * Use ROWBITS from Quartus/QSys to calculate SDRAM size
	 * since the FB specifies we modify ROWBITs to work around SDRAM
	 * controller issue.
	 *
	 * If the stored handoff value for rows is 0, it probably means
	 * the preloader is older than UBoot. Use the
	 * #define from the SOCEDS Tools per Crucible review
	 * uboot-socfpga-204. Note that this is not a supported
	 * configuration and is not tested. The customer
	 * should be using preloader and uboot built from the
	 * same tag.
	 */
	row = readl(ISWGRP_HANDOFF_ROWBITS);
	if (row == 0)
		row = CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS;
	/* If the stored handoff value for rows is greater than
	 * the field width in the sdr.dramaddrw register then
	 * something is very wrong. Revert to using the the #define
	 * value handed off by the SOCEDS tool chain instead of
	 * using a broken value.
	 */
	if (row > 31)
		row = CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_ROWBITS;

	bank = (temp & SDR_CTRLGRP_DRAMADDRW_BANKBITS_MASK) >>
		SDR_CTRLGRP_DRAMADDRW_BANKBITS_LSB;

	/* SDRAM Failure When Accessing Non-Existent Memory
	 * Use CSBITs from Quartus/QSys to calculate SDRAM size
	 * since the FB specifies we modify CSBITs to work around SDRAM
	 * controller issue.
	 */
	cs = (temp & SDR_CTRLGRP_DRAMADDRW_CSBITS_MASK) >>
	      SDR_CTRLGRP_DRAMADDRW_CSBITS_LSB;
	cs += 1;

	cs = CONFIG_HPS_SDR_CTRLCFG_DRAMADDRW_CSBITS;

	width = readl(SOCFPGA_SDR_ADDRESS +
		SDR_CTRLGRP_DRAMIFWIDTH_ADDRESS);
	/* ECC would not be calculated as its not addressible */
	if (width == SDRAM_WIDTH_32BIT_WITH_ECC)
		width = 32;
	if (width == SDRAM_WIDTH_16BIT_WITH_ECC)
		width = 16;

	/* calculate the SDRAM size base on this info */
	temp = 1 << (row + bank + col);
	temp = temp * cs * (width  / 8);

	debug("sdram_calculate_memory returns %ld\n", temp);

	return temp;
}

/* init the whole SDRAM ECC bit */
void sdram_ecc_init(void)
{
	struct pl330_transfer_struct pl330;
	unsigned int start;

	pl330.dst_addr = CONFIG_SYS_SDRAM_BASE;
	pl330.size_byte = sdram_calculate_size();
	pl330.channel_num = 0;
	pl330.buf_size = sizeof(pl330_buf);
	pl330.buf = pl330_buf;

	puts("SDRAM: Initializing SDRAM ECC\n");
	reset_timer();
	start = get_timer(0);

	if (pl330_transfer_zeroes(&pl330)) {
		puts("ERROR - DMA setup failed\n");
		hang();
	}

	if (pl330_transfer_start(&pl330)) {
		puts("ERROR - DMA start failed\n");
		hang();
	}

	if (pl330_transfer_finish(&pl330)) {
		puts("ERROR - DMA finish failed\n");
		hang();
	}

	printf("SDRAM: ECC initialized successfully with %d ms\n",
		(unsigned)get_timer(start));
}
