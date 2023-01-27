// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Intel Corporation <www.intel.com>
 */

#include <common.h>
#include <asm/system.h>
#include <cpu_func.h>
#include <linux/sizes.h>
#include "sdram_soc32.h"
#include <watchdog.h>

DECLARE_GLOBAL_DATA_PTR;

#define PGTABLE_OFF	0x4000

/* Initialize SDRAM ECC bits to avoid false DBE */
void sdram_init_ecc_bits(void)
{
	u32 start;
	phys_addr_t start_addr, saved_start;
	phys_size_t size, size_init, saved_size;
#if defined(CONFIG_SYS_ARM_CACHE_WRITETHROUGH)
	enum dcache_option option = DCACHE_WRITETHROUGH;
#elif defined(CONFIG_SYS_ARM_CACHE_WRITEALLOC)
	enum dcache_option option = DCACHE_WRITEALLOC;
#else
	enum dcache_option option = DCACHE_WRITEBACK;
#endif

	start = get_timer(0);

	start_addr = gd->bd->bi_dram[0].start;
	size = gd->bd->bi_dram[0].size;

	printf("DDRCAL: Scrubbing ECC RAM (%ld MiB).\n", size >> 20);

	memset((void *)start_addr, 0, PGTABLE_SIZE + PGTABLE_OFF);
	gd->arch.tlb_addr = start_addr + PGTABLE_OFF;
	gd->arch.tlb_size = PGTABLE_SIZE;
	start_addr += PGTABLE_SIZE + PGTABLE_OFF;
	size -= (PGTABLE_OFF + PGTABLE_SIZE);
	dcache_enable();

	saved_start = start_addr;
	saved_size = size;
	/* Set SDRAM region to writethrough to avoid false double-bit error. */
	mmu_set_region_dcache_behaviour(saved_start, saved_size,
					DCACHE_WRITETHROUGH);

	while (size > 0) {
		size_init = min((phys_addr_t)SZ_1G, (phys_addr_t)size);
		memset((void *)start_addr, 0, size_init);
		size -= size_init;
		start_addr += size_init;
		schedule();
	}

	dcache_disable();

	/* Restore back to original dcache behaviour. */
	mmu_set_region_dcache_behaviour(saved_start, saved_size,
					option);

	printf("DDRCAL: SDRAM-ECC initialized success with %d ms\n",
	       (u32)get_timer(start));
}

