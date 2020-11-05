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
	mbox_hps_stage_notify(HPS_EXECUTION_STATE_SSBL);
}
