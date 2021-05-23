// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2020 Intel Corporation. All rights reserved
 *
 */

#include <asm/io.h>
#include <asm/u-boot.h>
#include <asm/utils.h>
#include <common.h>
#include <debug_uart.h>
#include <dm.h>
#include <dm/ofnode.h>
#include <image.h>
#include <log.h>
#include <spl.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/firewall.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/smmu_s10.h>
#include <watchdog.h>
#include <dm/uclass.h>

DECLARE_GLOBAL_DATA_PTR;

u32 spl_boot_device(void)
{
	int ret, size;
	ofnode node;
	const fdt32_t *phandle_p;
	u32 phandle;
	struct udevice *dev;

	node = ofnode_path("/chosen");
	if (!ofnode_valid(node)) {
		debug("%s: /chosen node was not found.\n", __func__);
		goto fallback;
	}

	phandle_p = ofnode_get_property(node, "u-boot,boot0", &size);
	if (!phandle_p) {
		debug("%s: u-boot,boot0 property was not found.\n",
		      __func__);
		goto fallback;
	}

	phandle = fdt32_to_cpu(*phandle_p);

	node = ofnode_get_by_phandle(phandle);

	ret = device_get_global_by_ofnode(node, &dev);
	if (ret) {
		debug("%s: Boot device at not found, error: %d\n", __func__,
		      ret);
		goto fallback;
	}

	debug("%s: Found boot device %s\n", __func__, dev->name);

	switch (device_get_uclass_id(dev)) {
	case UCLASS_SPI_FLASH:
		return BOOT_DEVICE_SPI;
	case UCLASS_MTD:
		return BOOT_DEVICE_NAND;
	case UCLASS_MMC:
		return BOOT_DEVICE_MMC1;
	default:
		debug("%s: Booting from device uclass '%s' is not supported\n",
		      __func__, dev_get_uclass_name(dev));
	}

fallback:
	/* Return default boot device */
	return BOOT_DEVICE_MMC1;
}

#if IS_ENABLED(CONFIG_SPL_MMC)
u32 spl_boot_mode(const u32 boot_device)
{
#if defined(CONFIG_SPL_FS_FAT) || defined(CONFIG_SPL_FS_EXT4)
	return MMCSD_MODE_FS;
#else
	return MMCSD_MODE_RAW;
#endif
}
#endif

/* board specific function prior loading SSBL / U-Boot */
void spl_perform_fixups(struct spl_image_info *spl_image)
{
	/* Setup and Initialize SMMU */
	socfpga_init_smmu();

	mbox_hps_stage_notify(HPS_EXECUTION_STATE_SSBL);
}

/* This function is to map specified node onto SPL boot devices */
static int spl_node_to_boot_device(int node)
{
	struct udevice *parent;

	if (!uclass_get_device_by_of_offset(UCLASS_MMC, node, &parent))
		return BOOT_DEVICE_MMC1;
	else if (!uclass_get_device_by_of_offset(UCLASS_SPI_FLASH, node, &parent))
		return BOOT_DEVICE_SPI;
	else if (!uclass_get_device_by_of_offset(UCLASS_MTD, node, &parent))
		return BOOT_DEVICE_NAND;
	else
		return -1;
}

static void default_spl_boot_list(u32 *spl_boot_list, int length)
{
	spl_boot_list[0] = BOOT_DEVICE_MMC1;

	if (length > 1)
		spl_boot_list[1] = BOOT_DEVICE_SPI;

	if (length > 2)
		spl_boot_list[2] = BOOT_DEVICE_NAND;
}

void board_boot_order(u32 *spl_boot_list)
{
	int idx = 0;
	const void *blob = gd->fdt_blob;
	int chosen_node = fdt_path_offset(blob, "/chosen");
	const char *conf;
	int elem;
	int boot_device;
	int node;
	int length;

	/* expect valid initialized spl_boot_list */
	if (!spl_boot_list)
		return;

	length = 1;
	while (spl_boot_list[length] == spl_boot_list[length - 1])
		length++;

	debug("%s: chosen_node is %d\n", __func__, chosen_node);
	if (chosen_node < 0) {
		printf("%s: /chosen not found, using default\n", __func__);
		default_spl_boot_list(spl_boot_list, length);
		return;
	}

	for (elem = 0;
	    (conf = fdt_stringlist_get(blob, chosen_node,
			"u-boot,spl-boot-order", elem, NULL));
	    elem++) {
		if (idx >= length) {
			printf("%s: limit %d to spl_boot_list exceeded\n", __func__,
			       length);
			break;
		}

		/* Resolve conf item as a path in device tree */
		node = fdt_path_offset(blob, conf);
		if (node < 0) {
			debug("%s: could not find %s in FDT\n", __func__, conf);
			continue;
		}

		/* Try to map spl node back onto SPL boot devices */
		boot_device = spl_node_to_boot_device(node);
		if (boot_device < 0) {
			debug("%s: could not map node @%x to a boot-device\n",
			      __func__, node);
			continue;
		}

		spl_boot_list[idx] = boot_device;
		debug("%s: spl_boot_list[%d] = %u\n", __func__, idx,
		      spl_boot_list[idx]);
		idx++;
	}

	if (idx == 0) {
		if (!conf && !elem) {
			printf("%s: spl-boot-order invalid, using default\n", __func__);
			default_spl_boot_list(spl_boot_list, length);
		} else {
			printf("%s: no valid element spl-boot-order list\n", __func__);
		}
	}
}
