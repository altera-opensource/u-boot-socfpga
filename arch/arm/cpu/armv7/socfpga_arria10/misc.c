/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/sections.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/interrupts.h>
#include <asm/arch/sdram.h>
#include <asm/arch/dwmmc.h>
#include <altera.h>
#include <dwmmc.h>
#include <fdtdec.h>
#include <fpga.h>
#include <mmc.h>
#include <netdev.h>
#include <phy.h>

DECLARE_GLOBAL_DATA_PTR;

/* FPGA programming support for SoC FPGA Cyclone V */
Altera_desc altera_fpga[CONFIG_FPGA_COUNT] = {
	{Altera_SoCFPGA, 	/* family */
	fast_passive_parallel,	/* interface type */
	-1,		/* no limitation as additional data will be ignored */
	NULL,		/* no device function table */
	NULL,		/* base interface address specified in driver */
	0}		/* no cookie implementation */
};

/* add device descriptor to FPGA device table */
void socfpga_fpga_add(void)
{
	int i;
	fpga_init();
	for (i = 0; i < CONFIG_FPGA_COUNT; i++)
		fpga_add(fpga_altera, &altera_fpga[i]);
}

/* Print CPU information */
int print_cpuinfo(void)
{
	puts("CPU   : Altera SOCFPGA Arria 10 Platform\n");
	return 0;
}

int misc_init_r(void)
{
	/* add device descriptor to FPGA device table */
	socfpga_fpga_add();
	return 0;
}

#if defined(CONFIG_SYS_CONSOLE_IS_IN_ENV) && \
defined(CONFIG_SYS_CONSOLE_OVERWRITE_ROUTINE)
int overwrite_console(void)
{
	return 0;
}
#endif

/* Initializes MMC controllers. */
int cpu_mmc_init(bd_t *bis)
{
#ifdef CONFIG_DWMMC
	return socfpga_dwmmc_init(CONFIG_SDMMC_BASE,
		CONFIG_SOCFPGA_DWMMC_BUS_WIDTH, 0);
#else
	return 0;
#endif
}

/* Enable D-cache. I-cache is already enabled in start.S */
#ifndef CONFIG_SYS_DCACHE_OFF
void enable_caches(void)
{
	dcache_enable();
}
#endif

/* Skip relocation as U-Boot cannot run on SDRAM for secure boot */
void skip_relocation(void)
{
	bd_t *bd;
	gd_t *id;
	unsigned long addr, size = 0, i;

	puts("INFO  : Skip relocation as SDRAM is non secure memory\n");

#if !(defined(CONFIG_SYS_ICACHE_OFF) && defined(CONFIG_SYS_DCACHE_OFF))
	/* reserve TLB table */
	gd->arch.tlb_size = 4096 * 4;

	/* page table is located at last 16kB of OCRAM */
	addr = CONFIG_SYS_INIT_SP_ADDR;
	gd->arch.tlb_addr = addr;
	debug("TLB table from %08lx to %08lx\n", addr, addr + gd->arch.tlb_size);
#endif

	/* After page table, it would be stack then follow by malloc */
	addr -= (CONFIG_OCRAM_STACK_SIZE + CONFIG_OCRAM_MALLOC_SIZE);

	/*
	 * (permanently) allocate a Board Info struct
	 * and a permanent copy of the "global" data
	 */
	addr -= sizeof (bd_t);
	bd = (bd_t *) addr;
	gd->bd = bd;
	debug("Reserving %zu Bytes for Board Info at: %08lx\n",
			sizeof (bd_t), addr);

#ifdef CONFIG_MACH_TYPE
	gd->bd->bi_arch_number = CONFIG_MACH_TYPE; /* board id for Linux */
#endif

	addr -= sizeof (gd_t);
	id = (gd_t *) addr;
	debug("Reserving %zu Bytes for Global Data at: %08lx\n",
			sizeof (gd_t), addr);

	/* setup stackpointer for exeptions */
	gd->irq_sp = addr;
#ifdef CONFIG_USE_IRQ
	addr -= (CONFIG_STACKSIZE_IRQ + CONFIG_STACKSIZE_FIQ);
	printf("Reserving %zu Bytes for IRQ stack at: %08lx\n",
		CONFIG_STACKSIZE_IRQ + CONFIG_STACKSIZE_FIQ, addr);
#endif
	interrupt_init();

#ifdef CONFIG_POST
	post_bootmode_init();
	post_run(NULL, POST_ROM | post_bootmode_get(0));
#endif

	/* gd->bd->bi_baudrate = gd->baudrate; */
	/* setup the dram info within bd */
	dram_init_banksize();

	/* and display it */
	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++)
		size += gd->bd->bi_dram[i].size;
	puts("DRAM  : ");
	print_size(size, "\n");

	gd->relocaddr = CONFIG_SYS_TEXT_BASE;
	gd->start_addr_sp = CONFIG_SYS_INIT_SP_ADDR;
	gd->reloc_off = 0;
	debug("relocation Offset is: %08lx\n", gd->reloc_off);

	/* copy all old global data to new allocated location */
	memcpy(id, (void *)gd, sizeof(gd_t));

	/* assign the global data to new location, no longer in stack */
	gd = id;

	/* rebase the stack pointer as we won't jump back here */
	asm volatile ("mov sp, %0" : : "r" (gd->start_addr_sp)
		: "r0", "r1", "r2", "r3", "ip", "lr", "memory", "cc");

	/* change the destination address so later malloc init will point to
	existing allocated malloc memory space */
	board_init_r(id, (CONFIG_SYS_INIT_SP_ADDR - CONFIG_OCRAM_STACK_SIZE));
}

int is_chosen_boolean_true(const void *blob, const char *name)
{
	int node;
	int rval = 0;

	node = fdt_subnode_offset(blob, 0, "chosen");

	if (node >= 0)
		rval = fdtdec_get_bool(blob, node, name);

	return rval;
}

int is_external_fpga_config(const void *blob)
{
	static const char *name = "external-fpga-config";

	return is_chosen_boolean_true(blob, name);
}
