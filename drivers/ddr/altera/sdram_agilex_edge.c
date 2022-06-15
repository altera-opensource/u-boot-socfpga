// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019-2022 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <div64.h>
#include <fdtdec.h>
#include <hang.h>
#include <log.h>
#include <ram.h>
#include <reset.h>
#include <asm/global_data.h>
#include "sdram_soc64.h"
#include <wait_bit.h>
#include <asm/arch/firewall.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/io.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

int sdram_mmr_init_full(struct udevice *dev)
{
	/* struct altera_sdram_plat *plat = dev_get_plat(dev); */
	struct altera_sdram_priv *priv = dev_get_priv(dev);
	int ret;
	phys_size_t hw_size;
	struct bd_info bd = {0};

	/* assigning the SDRAM size */
	/* hardcoded to 512MB */
	phys_size_t size = 0x20000000;

	if (size <= 0)
		hw_size = PHYS_SDRAM_1_SIZE;
	else
		hw_size = size;

	/* Get bank configuration from devicetree */
	ret = fdtdec_decode_ram_size(gd->fdt_blob, NULL, 0, NULL,
				     (phys_size_t *)&gd->ram_size, &bd);
	if (ret) {
		puts("DDR: Failed to decode memory node\n");
		return -ENXIO;
	}

	if (gd->ram_size != hw_size) {
		printf("DDR: Warning: DRAM size from device tree (%lld MiB)\n",
		       gd->ram_size >> 20);
		printf(" mismatch with hardware (%lld MiB).\n",
		       hw_size >> 20);
	}

	if (gd->ram_size > hw_size) {
		printf("DDR: Error: DRAM size from device tree is greater\n");
		printf(" than hardware size.\n");
		hang();
	}

	printf("DDR: %lld MiB\n", gd->ram_size >> 20);

	sdram_size_check(&bd);
	printf("DDR: size check success\n");

	priv->info.base = bd.bi_dram[0].start;
	priv->info.size = gd->ram_size;

	debug("DDR: init success\n");
	return 0;
}
