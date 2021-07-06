// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Intel Corporation <www.intel.com>
 *
 */

#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <errno.h>

static int socfpga_secreg_probe(struct udevice *dev)
{
	const fdt32_t *list;
	fdt_addr_t offset, base;
	fdt_val_t val, read_val;
	int size, i;
	u32 blk_sz, reg;
	ofnode node;
	const char *name = NULL;

	debug("%s(dev=%p)\n", __func__, dev);

	if (!dev_has_ofnode(dev))
		return 0;

	dev_for_each_subnode(node, dev) {
		name = ofnode_get_name(node);
		if (!name)
			return -EINVAL;

		if (ofnode_read_u32_index(node, "reg", 1, &blk_sz))
			return -EINVAL;

		base = ofnode_get_addr(node);
		if (base == FDT_ADDR_T_NONE)
			return -EINVAL;

		debug("%s(node_offset 0x%lx node_name %s ", __func__,
		      node.of_offset, name);
		debug("node addr 0x%llx blk sz 0x%x)\n", base, blk_sz);

		list = ofnode_read_prop(node, "intel,offset-settings", &size);
		if (!list)
			return -EINVAL;

		debug("%s(intel,offset-settings property size=%x)\n", __func__,
		      size);
		size /= sizeof(*list) * 2;
		for (i = 0; i < size; i++) {
			offset = fdt32_to_cpu(*list++);
			val = fdt32_to_cpu(*list++);
			debug("%s(intel,offset-settings 0x%llx : 0x%llx)\n",
			      __func__, offset, val);

			if (blk_sz <= offset) {
				printf("%s: Overflow as offset 0x%llx",
				       __func__, offset);
				printf(" is larger than block size 0x%x\n",
				       blk_sz);
				return -EINVAL;
			}

			reg = base + offset;
			writel(val, (uintptr_t)reg);

			read_val = readl((uintptr_t)reg);
			debug("%s(reg 0x%x = wr : 0x%llx  rd : 0x%llx)\n",
			      __func__, reg, val, read_val);
		}
	}

	return 0;
};

static const struct udevice_id socfpga_secreg_ids[] = {
	{.compatible = "intel,socfpga-secreg"},
	{ }
};

U_BOOT_DRIVER(socfpga_secreg) = {
	.name		= "socfpga-secreg",
	.id		= UCLASS_NOP,
	.of_match	= socfpga_secreg_ids,
	.probe		= socfpga_secreg_probe,
};
