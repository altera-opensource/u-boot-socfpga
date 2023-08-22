// SPDX-License-Identifier:     GPL-2.0
/*
 * Copyright (C) 2023 Intel Corporation <www.intel.com> 
 */

#include <cadence-nand.h>
#include <dm.h>
#include <hang.h>
#include <malloc.h>
#include <memalign.h>
#include <nand.h>
#include <linux/bitfield.h>

int nand_spl_load_image(u32 offset, u32 len, void *dst)
{
	size_t count = len, actual = 0, page_align_overhead = 0;
	u32 page_align_offset = 0;
	u8 *page_buffer;
	int err = 0;
	struct mtd_info *mtd;

	if (!len || !dst)
		return -EINVAL;

	mtd = get_nand_dev_by_index(nand_curr_device);
	if (!mtd)
		hang();

	if ((offset & (mtd->writesize - 1)) != 0) {
		page_buffer = malloc_cache_aligned(mtd->writesize);
		if (!page_buffer) {
			debug("Error: allocating buffer\n");
			return -ENOMEM;
		}

		page_align_overhead = offset % mtd->writesize;
		page_align_offset = (offset / mtd->writesize) * mtd->writesize;
		count = mtd->writesize;

		err = nand_read_skip_bad(mtd, page_align_offset, &count,
					 &actual, mtd->size, page_buffer);

		if (err) {
			free(page_buffer);
			return err;
		}

		count -= page_align_overhead;
		count = min((size_t)len, count);
		memcpy(dst, page_buffer + page_align_overhead, count);
		free(page_buffer);

		len -= count;
		if (!len)
			return err;

		offset += count;
		dst += count;
		count = len;
	}

	return nand_read_skip_bad(mtd, offset, &count, &actual, mtd->size, dst);
}

/*
 * The offset at which the image to be loaded from NAND is located is
 * retrieved from the itb header. The presence of bad blocks in the area
 * of the NAND where the itb image is located could invalidate the offset
 * which must therefore be adjusted taking into account the state of the
 * sectors concerned
 */
u32 nand_spl_adjust_offset(u32 sector, u32 offs)
{
	u32 sector_align_offset, sector_align_end_offset;
	struct mtd_info *mtd;

	mtd = get_nand_dev_by_index(nand_curr_device);
	if (!mtd)
		hang();

	sector_align_offset = sector & (~(mtd->erasesize - 1));

	sector_align_end_offset = (sector + offs) & (~(mtd->erasesize - 1));

	while (sector_align_offset <= sector_align_end_offset) {
		if (nand_block_isbad(mtd, sector_align_offset)) {
			offs += mtd->erasesize;
			sector_align_end_offset += mtd->erasesize;
		}
		sector_align_offset += mtd->erasesize;
	}

	return offs;
}

void nand_deselect(void) {}
