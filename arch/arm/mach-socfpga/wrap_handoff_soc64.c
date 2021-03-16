// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/handoff_soc64.h>
#include "log.h"

int get_handoff_size(void *handoff_address, enum endianness endian)
{
	u32 handoff_size;

	if (endian == little_endian) {
		handoff_size = (readl(handoff_address +
				SOC64_HANDOFF_OFFSET_LENGTH) -
				SOC64_HANDOFF_OFFSET_DATA) /
				sizeof(u32);
	} else if (endian == big_endian) {
		handoff_size = swab32(readl(handoff_address +
					    SOC64_HANDOFF_OFFSET_LENGTH));
		handoff_size = (handoff_size - SOC64_HANDOFF_OFFSET_DATA) /
				sizeof(u32);
	} else {
		return -EINVAL;
	}

	debug("%s: handoff address = 0x%p handoff size = 0x%08x\n", __func__,
	      (u32 *)handoff_address, handoff_size);

	return handoff_size;
}

int handoff_read(void *handoff_address, void *table, u32 table_len,
		 enum endianness big_endian)
{
	u32 temp, i;
	u32 *table_x32 = table;

	debug("%s: handoff addr = 0x%p ", __func__, (u32 *)handoff_address);

	if (big_endian) {
		if (swab32(readl(SOC64_HANDOFF_BASE)) ==
			SOC64_HANDOFF_MAGIC_BOOT) {
			debug("Handoff table address = 0x%p ", table_x32);
			debug("table length = 0x%x\n", table_len);
			debug("%s: handoff data =\n{\n", __func__);

			for (i = 0; i < table_len; i++) {
				temp = readl(handoff_address +
					     SOC64_HANDOFF_OFFSET_DATA +
					     (i * sizeof(u32)));
				*table_x32 = swab32(temp);

				if (!(i % 2))
					debug(" No.%d Addr 0x%08x: ", i,
					      *table_x32);
				else
					debug(" 0x%08x\n", *table_x32);

				table_x32++;
			}
			debug("\n}\n");
		} else {
			debug("%s: Cannot find SOC64_HANDOFF_MAGIC_BOOT ",
			      __func__);
			debug("at addr  0x%p\n",
			      (u32 *)handoff_address);
			return -EPERM;
		}
	} else {
#if IS_ENABLED(CONFIG_TARGET_SOCFPGA_N5X)
		temp = readl(handoff_address);
		if (temp == SOC64_HANDOFF_DDR_UMCTL2_MAGIC) {
			debug("%s: umctl2 handoff data =\n{\n",
			      __func__);
		} else if (temp == SOC64_HANDOFF_DDR_PHY_MAGIC) {
			debug("%s: PHY handoff data =\n{\n",
			      __func__);
		} else if (temp == SOC64_HANDOFF_DDR_PHY_INIT_ENGINE_MAGIC) {
			debug("%s: PHY engine handoff data =\n{\n",
			      __func__);
		}

		debug("handoff table address = 0x%p table length = 0x%x\n",
		      table_x32, table_len);

		if (temp == SOC64_HANDOFF_DDR_UMCTL2_MAGIC ||
		    temp == SOC64_HANDOFF_DDR_PHY_MAGIC ||
		    temp == SOC64_HANDOFF_DDR_PHY_INIT_ENGINE_MAGIC) {
			/* Using handoff from Quartus tools if exists */
			for (i = 0; i < table_len; i++) {
				*table_x32 = readl(handoff_address +
						SOC64_HANDOFF_OFFSET_DATA +
						(i * 4));

				if (!(i % 2))
					debug(" No.%d Addr 0x%08x: ", i,
					      *table_x32);
				else
					debug(" 0x%08x\n", *table_x32);

				table_x32++;
			}
			debug("\n}\n");
		} else {
			debug("%s: Cannot find HANDOFF MAGIC ", __func__);
			debug("at addr 0x%p\n", (u32 *)handoff_address);
			return -EPERM;
		}
#endif
	}

	return 0;
}
