/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
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
#include <asm/arch/gic.h>

DECLARE_GLOBAL_DATA_PTR;

void gic_dic_clear_enable_all_intr (void)
{
	/* disable SGI and PPI */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_ENABLE + 0x00));
	/* disable SPI interrupt sources ID32 - ID63 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_ENABLE + 0x04));
	/* disable SPI interrupt sources ID64 - ID95 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_ENABLE + 0x08));
	/* disable SPI interrupt sources ID96 - ID127 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_ENABLE + 0x0C));
	/* disable SPI interrupt sources ID128 - ID159 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_ENABLE + 0x10));
	/* disable SPI interrupt sources ID160 - ID191 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_ENABLE + 0x14));
	/* disable SPI interrupt sources ID192 - ID223 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_ENABLE + 0x18));
	/* disable SPI interrupt sources ID224 - ID255 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_ENABLE + 0x1C));
}

void gic_dic_clear_pending_all_intr (void)
{
	/* disable SGI and PPI pending interrupt */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_PENDING + 0x00));
	/* disable SPI pending interrupt ID32 - ID63 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_PENDING + 0x04));
	/* disable SPI pending interrupt ID64 - ID95 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_PENDING + 0x08));
	/* disable SPI pending interrupt ID96 - ID127 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_PENDING + 0x0C));
	/* disable SPI pending interrupt ID128 - ID159 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_PENDING + 0x10));
	/* disable SPI pending interrupt ID160 - ID191 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_PENDING + 0x14));
	/* disable SPI pending interrupt ID192 - ID223 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_PENDING + 0x18));
	/* disable SPI pending interrupt ID224 - ID255 */
	writel(0xFFFFFFFF, (CONFIG_GIC_DIC_CLR_PENDING + 0x1C));
}

void gic_dic_set_enable (unsigned char intrID)
{
	int div = intrID / 32;
	int mod = intrID % 32;
	setbits_le32((CONFIG_GIC_DIC_SET_ENABLE + (div<<2)), (1 << mod));
}

void gic_dic_set_target_processor (unsigned char intrID,
	unsigned char target)
{
	if (intrID < 32)
		return;
	int div = intrID / 4;
	int mod = intrID % 4;
	clrbits_le32((CONFIG_GIC_DIC_TARGET + (div<<2)),
		(0xFF << (8 * mod)));
	setbits_le32((CONFIG_GIC_DIC_TARGET + (div<<2)),
		(target << (8 * mod)));
}

void gic_dic_set_config (unsigned char intrID,
	unsigned char level0edge1)
{
	int div = intrID / 16;
	int mod = intrID % 16;
	clrbits_le32((CONFIG_GIC_DIC_CONFIG + (div<<2)),
		(0x3 << (2 * mod)));
	if (level0edge1 == 1)
		setbits_le32((CONFIG_GIC_DIC_CONFIG + (div<<2)),
			(0x2 << (2 * mod)));
}

void gic_dic_set_pending (unsigned char intrID)
{
	int div = intrID / 32;
	int mod = intrID % 32;
	setbits_le32((CONFIG_GIC_DIC_SET_PENDING + (div<<2)),
		(1 << mod));
}

void gic_dic_clear_pending (unsigned char intrID)
{
	int div = intrID / 32;
	int mod = intrID % 32;
	setbits_le32((CONFIG_GIC_DIC_CLR_PENDING + (div<<2)),
		(1 << mod));
}



