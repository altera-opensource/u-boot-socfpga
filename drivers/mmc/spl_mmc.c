/*
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 *
 * Aneesh V <aneesh@ti.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <common.h>
#include <spl.h>
#include <asm/u-boot.h>
#include <asm/utils.h>
#include <mmc.h>
#include <fat.h>
#include <version.h>

DECLARE_GLOBAL_DATA_PTR;

static void mmc_load_image_raw(struct mmc *mmc)
{
	u32 image_size_sectors, err;
	const struct image_header *header;

	header = (struct image_header *)(CONFIG_SYS_TEXT_BASE -
						sizeof(struct image_header));

	/* read image header to find the image size & load address */
	err = mmc->block_dev.block_read(0,
			CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR, 1,
			(void *)header);

	if (err <= 0)
		goto end;

	spl_parse_image_header(header);

	/* convert size to sectors - round up */
	image_size_sectors = (spl_image.size + mmc->read_bl_len - 1) /
				mmc->read_bl_len;

	/* Read the header too to avoid extra memcpy */
	err = mmc->block_dev.block_read(0,
			CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR,
			image_size_sectors, (void *)spl_image.load_addr);

end:
	if (err <= 0) {
		printf("spl: mmc blk read err - %d\n", err);
		hang();
	}
}

static void mmc_load_image_mbr(struct mmc *mmc)
{
#define MBR_SIGNATURE				(0xAA55)
#define MBR_SIGNATURE_OFFSET			(0x1FE)
#define MBR_NUM_OF_PARTITIONS			(4)
#define MBR_PARTITION_OFFSET			(0x1BE)
#define MBR_PARTITION_TYPE_OFFSET		(4)
#define MBR_PARTITION_START_SECTOR_OFFSET	(8)
#define MBR_PARTITION_ENTRY_SIZE		(16)
#define CONFIG_SYS_MMCSD_MBR_MODE_U_BOOT_PARTITION_ID	(0xA2)

	u32 i;
	u32 part_type;
	u32 offset = 0;
	u16 sign;
	u8 *p_entry;
	u32 image_size_sectors, err;
	const struct image_header *header;

	header = (struct image_header *)(CONFIG_SYS_TEXT_BASE -
						sizeof(struct image_header));

	/* search the MBR to get the parition offset */
	/* Read MBR */
	err = mmc->block_dev.block_read(0, 0, 1, (void *)header);
	if (err <= 0)
		goto end;
	memcpy(&sign, (u8 *)header + MBR_SIGNATURE_OFFSET, sizeof(sign));
	if (sign != MBR_SIGNATURE) {
		puts("spl: No MBR signature is found\n");
		hang();
	}

	/* Lookup each partition entry for partition ID. */
	p_entry = (u8 *)header + MBR_PARTITION_OFFSET;
	for (i = 0; i < MBR_NUM_OF_PARTITIONS; i++) {
		part_type = p_entry[MBR_PARTITION_TYPE_OFFSET];

		if (part_type ==
			CONFIG_SYS_MMCSD_MBR_MODE_U_BOOT_PARTITION_ID){
			/* Partition found */
			memcpy(&offset,
				p_entry + MBR_PARTITION_START_SECTOR_OFFSET,
				sizeof(offset));
			debug("spl: Partition offset 0x%x sectors\n", offset);
			break;
		}
		p_entry += MBR_PARTITION_ENTRY_SIZE;
	}


	/* read image header to find the image size & load address */
	err = mmc->block_dev.block_read(0,
			(offset + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR), 1,
			(void *)header);

	if (err <= 0)
		goto end;

	spl_parse_image_header(header);

	/* convert size to sectors - round up */
	image_size_sectors = (spl_image.size + mmc->read_bl_len - 1) /
				mmc->read_bl_len;

	/* Read the header too to avoid extra memcpy */
	err = mmc->block_dev.block_read(0,
			(offset + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR),
			image_size_sectors, (void *)spl_image.load_addr);

end:
	if (err <= 0) {
		printf("spl: mmc blk read err - %d\n", err);
		hang();
	}
}

#ifdef CONFIG_SPL_FAT_SUPPORT
static void mmc_load_image_fat(struct mmc *mmc)
{
	s32 err;
	struct image_header *header;

	header = (struct image_header *)(CONFIG_SYS_TEXT_BASE -
						sizeof(struct image_header));

	err = fat_register_device(&mmc->block_dev,
				CONFIG_SYS_MMC_SD_FAT_BOOT_PARTITION);
	if (err) {
		printf("spl: fat register err - %d\n", err);
		hang();
	}

	err = file_fat_read(CONFIG_SPL_FAT_LOAD_PAYLOAD_NAME,
				(u8 *)header, sizeof(struct image_header));
	if (err <= 0)
		goto end;

	spl_parse_image_header(header);

	err = file_fat_read(CONFIG_SPL_FAT_LOAD_PAYLOAD_NAME,
				(u8 *)spl_image.load_addr, 0);

end:
	if (err <= 0) {
		printf("spl: error reading image %s, err - %d\n",
			CONFIG_SPL_FAT_LOAD_PAYLOAD_NAME, err);
		hang();
	}
}
#endif

void spl_mmc_load_image(void)
{
	struct mmc *mmc;
	int err;
	u32 boot_mode;

	mmc_initialize(gd->bd);
	/* We register only one device. So, the dev id is always 0 */
	mmc = find_mmc_device(0);
	if (!mmc) {
		puts("spl: mmc device not found!!\n");
		hang();
	}

	err = mmc_init(mmc);
	if (err) {
		printf("spl: mmc init failed: err - %d\n", err);
		hang();
	}
	boot_mode = spl_boot_mode();
	if (boot_mode == MMCSD_MODE_RAW) {
		debug("boot mode - RAW\n");
		mmc_load_image_raw(mmc);
	} else if (boot_mode == MMCSD_MODE_MBR) {
		debug("boot mode - MBR\n");
		mmc_load_image_mbr(mmc);
#ifdef CONFIG_SPL_FAT_SUPPORT
	} else if (boot_mode == MMCSD_MODE_FAT) {
		debug("boot mode - FAT\n");
		mmc_load_image_fat(mmc);
#endif
	} else {
		puts("spl: wrong MMC boot mode\n");
		hang();
	}
}
