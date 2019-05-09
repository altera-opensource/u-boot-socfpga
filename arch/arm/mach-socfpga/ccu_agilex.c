// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Intel Corporation <www.intel.com>
 *
 */
#include <asm/io.h>
#include <wait_bit.h>
#include <asm/arch/ccu_agilex.h>

static void ccu_init_dirs(void)
{
	ulong i, f;
	int ret;
	u32 num_of_dirs;
	u32 num_of_snoop_filters;
	u32 reg;

	num_of_dirs = CSUIDR_NUMDIRUS_GET(readl(CCU_REG_ADDR(CSUIDR)));
	num_of_snoop_filters =
		CSIDR_NUMSFS_GET(readl(CCU_REG_ADDR(CSIDR))) + 1;

	/* Initialize each snoop filter in each directory */
	for (f = 0; f < num_of_snoop_filters; f++) {
		reg = f << DIRUSFMCR_SFID_SHIFT;
		for (i = 0; i < num_of_dirs; i++) {
			/* Initialize all entries */
			writel(reg, CCU_DIR_REG_ADDR(i, DIRUSFMCR));

			/* Poll snoop filter maintenance operation active
			 * bit become 0.
			 */
			ret = wait_for_bit_le32((const void *)
						CCU_DIR_REG_ADDR(i, DIRUSFMAR),
						BIT(0), false, 1000, false);
			if (ret) {
				puts("CCU: Directory initialization failed!\n");
				hang();
			}

			/* Enable snoop filter, a bit per snoop filter */
			setbits_le32((ulong)CCU_DIR_REG_ADDR(i, DIRUSFER),
				     BIT(f));
		}
	}
}

void ccu_init_coh_agent_intf(void)
{
	u32 num_of_coh_agent_intf;
	u32 num_of_dirs;
	u32 reg;
	u32 type;
	u32 i, dir;

	num_of_coh_agent_intf =
		CSUIDR_NUMCAIUS_GET(readl(CCU_REG_ADDR(CSUIDR)));
	num_of_dirs = CSUIDR_NUMDIRUS_GET(readl(CCU_REG_ADDR(CSUIDR)));

	for (i = 0; i < num_of_coh_agent_intf; i++) {
		reg = readl((ulong)CCU_CAIU_REG_ADDR(i, CAIUIDR));
		if (CAIUIDR_CA_GET(reg)) {
			/* Caching agent bit is enabled, enable caching agent
			 * snoop in each directory
			 */
			for (dir = 0; dir < num_of_dirs; dir++) {
				setbits_le32((ulong)
					     CCU_DIR_REG_ADDR(dir, DIRUCASER0),
					     BIT(i));
			}
		}

		type = CAIUIDR_TYPE_GET(reg);
		if (type == CAIUIDR_TYPE_ACE_CAI_DVM_SUPPORT ||
		    type == CAIUIDR_TYPE_ACELITE_CAI_DVM_SUPPORT) {
			/* DVM support is enabled, enable ACE DVM snoop*/
			setbits_le32((ulong)(CCU_REG_ADDR(CSADSER0)),
				     BIT(i));
		}
	}
}

static void ocram_bypass_firewall(void)
{
	clrbits_le32((ulong)(COH_CPU0_BYPASS_REG_ADDR(OCRAM_BLK_CGF_01_REG)),
		     OCRAM_PRIVILEGED_MASK | OCRAM_SECURE_MASK);
	clrbits_le32((ulong)(COH_CPU0_BYPASS_REG_ADDR(OCRAM_BLK_CGF_02_REG)),
		     OCRAM_PRIVILEGED_MASK | OCRAM_SECURE_MASK);
	clrbits_le32((ulong)(COH_CPU0_BYPASS_REG_ADDR(OCRAM_BLK_CGF_03_REG)),
		     OCRAM_PRIVILEGED_MASK | OCRAM_SECURE_MASK);
	clrbits_le32((ulong)(COH_CPU0_BYPASS_REG_ADDR(OCRAM_BLK_CGF_04_REG)),
		     OCRAM_PRIVILEGED_MASK | OCRAM_SECURE_MASK);
}

void ccu_init(void)
{
	ccu_init_dirs();
	ccu_init_coh_agent_intf();
	ocram_bypass_firewall();
}
