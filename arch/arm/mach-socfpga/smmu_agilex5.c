// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Intel Corporation. All rights reserved
 *
 */

#include <common.h>
#include <malloc.h>
#include <memalign.h>
#include <asm/arch/firewall.h>
#include <asm/arch/smmu_agilex5.h>
#include <asm/arch/socfpga_smmuv3.h>
#include <asm/arch/system_manager.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Attributes to be used only with Table entries
 *
 * Shareability for non Memory does not apply. Those locations are
 * automatically marked outer shareable.
 */
#define PTGEN_LOWER_SHARABILITY_E_NON       0x0
#define PTGEN_LOWER_SHARABILITY_E_OUTER     0x2
#define PTGEN_LOWER_SHARABILITY_E_INNER     0x3

#define PTGEN_UPPER_TABLE_DEFAULT           ((uint64_t)0)
#define PTGEN_LOWER_MEMORY_SHARED_DEFAULT   PTGEN_LOWER_MEMORY_SHARED_INNER

#define PTGEN_LOWER_NOT_GLOBAL(bit)         (((bit) & 0x1) << 11)
#define PTGEN_LOWER_ACCESS_FLAG(flag)       (((flag) & 0x1) << 10)
#define PTGEN_LOWER_SHARABILITY(sh)         (((sh) & 0x3 << 8)

/*
 * This is for AP[2:1]. AP[0] is always 0. This is why the field overlaps
 * with NS.
 */
#define PTGEN_LOWER_ACCESS_PERM(ap)         (((ap) & 0x3) << 6)
#define PTGEN_LOWER_NON_SECURE(ns)          (((ns) & 0x1) << 5)
#define PTGEN_LOWER_ATTR_INDEX(attr)        ((attr) & 0x7) << 2)

#define PTGEN_LOWER_MEMORY	(PTGEN_LOWER_ATTR_INDEX(2) | \
	PTGEN_LOWER_ACCESS_FLAG(1) | \
	PTGEN_LOWER_ACCESS_PERM(0b00))
#define PTGEN_LOWER_MEMORY_SHARED_INNER	(PTGEN_LOWER_MEMORY | \
	PTGEN_LOWER_SHARABILITY(PTGEN_LOWER_SHARABILITY_E_INNER))

/* Stream ID configuration value for Agilex5 */
#define SDM_ID                      0x000A

#define SMMU_S_BASE_OFFSET          0x8000

#define STE_SDM_SZ                  (SZ_512 * SDM_ID)
#define CD_SZ                       SZ_1K
#define EQ_SZ                       SZ_512
#define CMDQ_SZ                     SZ_512

#define TT_3LV                      3
#define TT_SZ                       (SZ_4K * TT_3LV)

#define SMMU_READ_ALLOCATE          0x1
#define SMMU_ST_LINEAR              0x0
#define SMMU_ST_STAGE1_ONLY         0x5
#define SMMU_ST_IGNORED             0x0
#define SMMU_EQ_WRITE_ALLOC         0x1
#define SZ_5                        0x5
#define SMMU_CR1_NON_CACHEABLE      0x0
#define SMMU_CR1_WB_CACHEABLE       0x1
#define SMMU_CR1_OUTER_SHAREABLE    0x2
#define SMMU_CR1_INNER_SHAREABLE    0x3
#define SMMU_CD_MAIR_DEVICE_NGNRNE  0x0
#define SMMU_CD_MAIR_NORMAL_WB_WA   0xFF
#define SMMU_CD_MAIR_NORMAL_NOCACHE 0x44

#define SMMU_ST_ALLOW_STALLS        0
#define SMMU_ST_NO_STALLS           1
#define SMMU_SYNC_SIG_NONE          0x0

/* SMMU commands */
#define SMMU_CMD_LEAF               1
#define SMMU_CMD_BRANCH             0

/* Context descriptor */
struct cd {
	struct {
		u32 t0sz : 6;
		u32 tg0 : 2;
		u32 ir0 : 2;
		u32 or0 : 2;
		u32 sh0 : 2;
		u32 epd0 : 1;
		u32 endi : 1;
		u32 t1sz : 6;
		u32 tg1 : 2;
		u32 ir1 : 2;
		u32 or1 : 2;
		u32 sh1 : 2;
		u32 epd1 : 1;
		u32 valid : 1;
		u32 ips : 3;
		u32 affd : 1;
		u32 wxn : 1;
		u32 uwxn : 1;
		u32 tbi : 2;
		u32 pan : 1;
		u32 aa64 : 1;
		u32 hd : 1;
		u32 ha : 1;
		u32 ars : 3;
		u32 aset : 1;
		u32 asid : 16;
	} d0;

	struct {
		u32 nscfg0 : 1;
		u32 had0 : 1;
		u32 e0pd0 : 1;
		u32 res0 : 1;
		u64 ttb0 : 48;
		u32 padding : 12;
	} d1;

	struct {
		u32 nscfg1 : 1;
		u32 had1 : 1;
		u32 padding : 2;
		u64 ttb1 : 48;
		u32 padding1 : 12;
	} d2;

	struct {
		u32 mair0 : 8;
		u32 mair1 : 8;
		u32 mair2 : 8;
		u32 mair3 : 8;
		u32 mair4 : 8;
		u32 mair5 : 8;
		u32 mair6 : 8;
		u32 mair7 : 8;
	} d3;

	struct {
		u32 amair0 : 8;
		u32 amair1 : 8;
		u32 amair2 : 8;
		u32 amair3 : 8;
		u32 amair4 : 8;
		u32 amair5 : 8;
		u32 amair6 : 8;
		u32 amair7 : 8;
	} d4;

	struct {
		u32 impdef : 32;
		u32 padding : 32;
	} d5;

	u64 d6;
	u64 d7;
};

/* Stream Table Entry */
struct ste {
	struct {
		u32 valid : 1;
		u32 config : 3;
		u32 s1fmt : 2;
		u64 s1contextptr : 42;
		u32 padding : 11;
		u32 s1cdmax : 5;
	} d0;

	struct {
		u32 s1dss : 2;
		u32 s1cir : 2;
		u32 s1cor : 2;
		u32 s1csh : 2;
		u32 padding : 4;
		u32 dre : 1;
		u32 cont : 4;
		u32 dcp : 1;
		u32 ppar : 1;
		u32 mev : 1;
		u32 padding1 : 7;
		u32 s1stalld : 1;
		u32 eats : 2;
		u32 strw : 2;
		u32 memattr : 4;
		u32 mtcfg : 1;
		u32 alloccfg : 4;
		u32 padding2 : 3;
		u32 shcfg : 2;
		u32 nscfg : 2;
		u32 privcfg : 2;
		u32 instcfg : 2;
		u32 padding3 : 12;
	} d1;

	struct {
		u32 s2vmid : 16;
		u32 impdef : 16;
		u32 s2t0sz : 6;
		u32 s2sl0 : 2;
		u32 s2ir0 : 2;
		u32 s2or0 : 2;
		u32 s2sh0 : 2;
		u32 s2tg : 2;
		u32 s2ps : 3;
		u32 s2aa64 : 1;
		u32 s2endi : 1;
		u32 s2affd : 1;
		u32 s2ptw : 1;
		u32 s2hd : 1;
		u32 s2ha : 1;
		u32 s2s : 1;
		u32 s2r : 1;
		u32 padding : 5;
	} d2;

	u64 d3;
	u64 d4;
	u64 d5;
	u64 d6;
	u64 d7;
};

static void TTLV0_init(struct smmuv3 *smmu)
{
	writeq(((uint64_t)(((PTGEN_UPPER_TABLE_DEFAULT) & (0xffful << 52)) |
	       ((uint64_t)(smmu->ttv1) & 0xfffffffff000) | 3)), smmu->ttv0);
}

static void TTLV1_init(struct smmuv3 *smmu)
{
	writeq(((uint64_t)(((PTGEN_UPPER_TABLE_DEFAULT) & (0xffful << 52)) |
	       ((uint64_t)(smmu->ttv2) & 0xfffffffff000) | 3)), smmu->ttv1);
}

static void TTLV2_init(struct smmuv3 *smmu)
{
	int i;

	for (i = 0; i < SZ_256; i++) {
		writeq((uint64_t)(((0) & (0xffful << 52)) |
			   ((uint64_t)(0x80000000 + (i * SZ_2M)) & 0xffffffe00000) |
			   ((PTGEN_LOWER_MEMORY_SHARED_DEFAULT) & (0x3ff << 2)) | 1),
			   smmu->ttv2 + (i * 8));
	}
}

static void tt_setup_for_sdm(struct smmuv3 *smmu)
{
	TTLV0_init(smmu);
	TTLV1_init(smmu);
	TTLV2_init(smmu);
}

static int populate_smmu_ns(struct smmuv3 *smmu, u8 *smmu_mem, u8 *tt_mem)
{
	smmu->ns_base = SOCFPGA_SMMU_ADDRESS;
	smmu->s_base = SOCFPGA_SMMU_ADDRESS + SMMU_S_BASE_OFFSET;
	smmu->curr = smmu->ns_base;
	smmu->ste_base = (phys_addr_t)smmu_mem;
	smmu->eq_base = (phys_addr_t)(smmu->ste_base + STE_SDM_SZ);
	smmu->cmdq_base = (phys_addr_t)(smmu->eq_base + EQ_SZ);
	smmu->cd_base = (phys_addr_t)(smmu->cmdq_base  + CMDQ_SZ);
	smmu->ttv0 = (phys_addr_t)tt_mem;
	smmu->ttv1 = (phys_addr_t)(smmu->ttv0 + TT_SZ);
	smmu->ttv2 = (phys_addr_t)(smmu->ttv1 + TT_SZ);
	smmu->tt_base = smmu->ttv0;
	smmu->cmdq_idx_mask = 0x00001F;
	smmu->cmdq_wrap_mask = 0x20;

	debug("%s:\n", __func__);
	debug("  smmu->ns_base: 0x%08llx\n", smmu->ns_base);
	debug("  smmu->s_base: 0x%08llx\n", smmu->s_base);
	debug("  smmu->curr: 0x%08llx\n", smmu->curr);
	debug("  smmu->ste_base: 0x%08llx\n", smmu->ste_base);
	debug("  smmu->eq_base: 0x%08llx\n", smmu->eq_base);
	debug("  smmu->cmdq_base: 0x%08llx\n", smmu->cmdq_base);
	debug("  smmu->cd_base: 0x%08llx\n", smmu->cd_base);
	debug("  smmu->ttv0: 0x%08llx\n", smmu->ttv0);
	debug("  smmu->ttv1: 0x%08llx\n", smmu->ttv1);
	debug("  smmu->ttv2: 0x%08llx\n", smmu->ttv2);
	debug("  smmu->tt_base: 0x%08llx\n", smmu->tt_base);

	return 0;
}

int smmu_init_cd(struct smmuv3 *smmu)
{
	struct cd *cd_p;

	if (!smmu)
		return -ENODEV;

	cd_p = (struct cd *)smmu->cd_base;

	cd_p->d0.valid = 0;
	cd_p->d0.t0sz = 0x18;
	cd_p->d0.aa64 = 0x1;
	cd_p->d0.tg0 = 0x0;
	cd_p->d0.ir0 = 0x0;
	cd_p->d0.or0 = 0x0;
	cd_p->d0.sh0 = 0x0;
	cd_p->d0.epd0 = 0x0;
	cd_p->d0.endi = 0x0;
	cd_p->d0.t1sz = 0x20;
	cd_p->d0.tg1 = 0x2;
	cd_p->d0.ir1 = 0x0;
	cd_p->d0.or1 = 0x0;
	cd_p->d0.sh1 = 0x0;
	cd_p->d0.epd1 = 0x1;
	cd_p->d0.ips = 0x2;
	cd_p->d0.affd = 0x1;
	cd_p->d0.wxn = 0x0;
	cd_p->d0.uwxn = 0x0;
	cd_p->d0.tbi = 0x0;
	cd_p->d0.pan = 0x0;
	cd_p->d0.hd = 0x0;
	cd_p->d0.ha = 0x0;
	cd_p->d0.ars = 0x6;
	cd_p->d0.aset = 0x1;
	cd_p->d0.asid = 0x1;
	cd_p->d1.nscfg0 = 0x0;
	cd_p->d1.had0 = 0x1;
	cd_p->d1.ttb0 = (((u64)smmu->tt_base) >> 4);
	cd_p->d2.nscfg1 = 0x0;
	cd_p->d2.had1 = 0x1;
	cd_p->d2.ttb1 = 0x0;
	cd_p->d3.mair0 = SMMU_CD_MAIR_DEVICE_NGNRNE;
	cd_p->d3.mair1 = SMMU_CD_MAIR_NORMAL_NOCACHE;
	cd_p->d3.mair2 = SMMU_CD_MAIR_NORMAL_WB_WA;
	cd_p->d3.mair3 = 0x0;
	cd_p->d3.mair4 = 0x0;
	cd_p->d3.mair5 = 0x0;
	cd_p->d3.mair6 = 0x0;
	cd_p->d3.mair7 = 0x0;
	cd_p->d4.amair0 = 0x0;
	cd_p->d4.amair1 = 0x0;
	cd_p->d4.amair2 = 0x0;
	cd_p->d4.amair3 = 0x0;
	cd_p->d4.amair4 = 0x0;
	cd_p->d4.amair5 = 0x0;
	cd_p->d4.amair6 = 0x0;
	cd_p->d4.amair7 = 0x0;
	cd_p->d5.impdef = 0x0;
	cd_p->d0.valid = 0x1;

	debug("%s: Created CD:\n", __func__);
	debug("  d0: 0x%08x %08x\n", ((u32 *)cd_p)[1], ((u32 *)cd_p)[0]);
	debug("  d1: 0x%08x %08x\n", ((u32 *)cd_p)[3], ((u32 *)cd_p)[2]);
	debug("  d2: 0x%08x %08x\n", ((u32 *)cd_p)[5], ((u32 *)cd_p)[4]);
	debug("  d3: 0x%08x %08x\n", ((u32 *)cd_p)[7], ((u32 *)cd_p)[6]);
	debug("  d4: 0x%08x %08x\n", ((u32 *)cd_p)[9], ((u32 *)cd_p)[8]);
	debug("  d5: 0x%08x %08x\n", ((u32 *)cd_p)[11], ((u32 *)cd_p)[10]);
	debug("  d6: 0x%08x %08x\n", ((u32 *)cd_p)[13], ((u32 *)cd_p)[12]);
	debug("  d7: 0x%08x %08x\n", ((u32 *)cd_p)[15], ((u32 *)cd_p)[14]);

	return 0;
}

int smmu_init_ste(struct smmuv3 *smmu, const u32 streamid)
{
	struct ste *ste_p;

	if (!smmu)
		return -ENODEV;

	ste_p = (struct ste *)(smmu->cd_base + (SZ_512 * streamid));

	ste_p->d0.valid = 0;
	ste_p->d0.config = SMMU_ST_STAGE1_ONLY;
	ste_p->d0.s1contextptr = ((u64)smmu->cd_base >> 6);
	ste_p->d0.s1cdmax = 0x0;
	ste_p->d0.s1fmt = 0x0;
	ste_p->d1.s1dss = 0x0;
	ste_p->d1.s1cir = 0x3;
	ste_p->d1.s1cor = 0x0;
	ste_p->d1.s1csh = 0x3;
	ste_p->d1.dre = 0x0;
	ste_p->d1.cont = 0x0;
	ste_p->d1.dcp = 0x0;
	ste_p->d1.ppar = 0x0;
	ste_p->d1.mev = 0x0;
	ste_p->d1.s1stalld = SMMU_ST_NO_STALLS;
	ste_p->d1.eats = 0x1;
	ste_p->d1.strw = 0x2;
	ste_p->d1.mtcfg = 0x0;
	ste_p->d1.memattr = 0x0;
	ste_p->d1.alloccfg = 0xe;
	ste_p->d1.shcfg = 0x3;
	ste_p->d1.nscfg = 0x2;
	ste_p->d1.privcfg = 0x1;
	ste_p->d1.instcfg = 0x0;
	ste_p->d2.s2vmid = 0x0;
	ste_p->d2.s2t0sz = 0x0;
	ste_p->d2.s2sl0 = 0x0;
	ste_p->d2.s2ir0 = 0x1;
	ste_p->d2.s2or0 = 0x1;
	ste_p->d2.s2sh0 = 0x3;
	ste_p->d2.s2tg = 0x0;
	ste_p->d2.s2ps = 0x0;
	ste_p->d2.s2aa64 = 0x1;
	ste_p->d2.s2endi = 0x0;
	ste_p->d2.s2affd = 0x1;
	ste_p->d2.s2ptw = 0x0;
	ste_p->d2.impdef = 0xf;
	ste_p->d3 = 0x0;
	ste_p->d0.valid = 1;

	debug("%s: Created STE:\n", __func__);
	debug("  d0: 0x%08x %08x\n", ((u32 *)ste_p)[1],  ((u32 *)ste_p)[0]);
	debug("  d1: 0x%08x %08x\n", ((u32 *)ste_p)[3],  ((u32 *)ste_p)[2]);
	debug("  d2: 0x%08x %08x\n", ((u32 *)ste_p)[5],  ((u32 *)ste_p)[4]);
	debug("  d3: 0x%08x %08x\n", ((u32 *)ste_p)[7],  ((u32 *)ste_p)[6]);
	debug("  d4: 0x%08x %08x\n", ((u32 *)ste_p)[9],  ((u32 *)ste_p)[8]);
	debug("  d5: 0x%08x %08x\n", ((u32 *)ste_p)[11], ((u32 *)ste_p)[10]);
	debug("  d6: 0x%08x %08x\n", ((u32 *)ste_p)[13], ((u32 *)ste_p)[12]);
	debug("  d7: 0x%08x %08x\n", ((u32 *)ste_p)[15], ((u32 *)ste_p)[14]);

	return 0;
}

int smmu_sdm_init(void)
{
	struct smmuv3 smmu;
	int ret;
	size_t total_size = STE_SDM_SZ + EQ_SZ + CMDQ_SZ + CD_SZ;
	u8 *smmu_mem = NULL;
	u8 *tt_mem = NULL;

	printf("Starting to setup HPS SMMU...\n");

	smmu_mem = memalign(SZ_512, total_size);
	if (!smmu_mem) {
		debug("%s: Failed to allocate memory for SMMU init\n", __func__);
		ret = -ENOMEM;
		goto err_smmu;
	}

	memset(smmu_mem, 0, total_size);

	tt_mem = memalign(SZ_4K, TT_SZ);
	if (!tt_mem) {
		debug("%s: Failed to allocate memory for translation table\n",
			  __func__);
		ret = -ENOMEM;
		goto err_tt;
	}

	memset(tt_mem, 0, TT_SZ);

	ret = populate_smmu_ns(&smmu, smmu_mem, tt_mem);
	if (ret) {
		debug("%s: Failed to populate smmu\n", __func__);
		goto err_tt;
	}

	printf("start to set up translation table\n");

	tt_setup_for_sdm(&smmu);

	ret = smmu_init_ste_table(&smmu, SMMU_READ_ALLOCATE, SZ_4,
				  SMMU_ST_LINEAR, SMMU_ST_IGNORED);
	if (ret) {
		debug("%s: Failed to init stream table\n", __func__);
		goto err_tt;
	}

	ret = smmu_init_event_queue(&smmu, SMMU_EQ_WRITE_ALLOC, SZ_4);
	if (ret) {
		debug("%s: Failed to init event queue\n", __func__);
		goto err_tt;
	}

	ret = smmu_init_command_queue(&smmu, SMMU_READ_ALLOCATE, SZ_5);
	if (ret) {
		debug("%s: Failed to init command queue\n", __func__);
		goto err_tt;
	}

	ret = smmu_init_table_attributes(&smmu, SMMU_CR1_WB_CACHEABLE,
									 SMMU_CR1_WB_CACHEABLE,
									 SMMU_CR1_INNER_SHAREABLE);
	if (ret) {
		debug("%s: Failed to init table attribute\n", __func__);
		goto err_tt;
	}

	ret = smmu_invalidate_all(&smmu);
	if (ret) {
		debug("%s: Failed to invalidate all smmu\n", __func__);
		goto err_tt;
	}

	ret = smmu_enable_cmd(&smmu);
	if (ret) {
		debug("%s: Failed to enable command processing\n", __func__);
		goto err_tt;
	}

	ret = smmu_enable_eventQ(&smmu);
	if (ret) {
		debug("%s: Failed to enable event queue\n", __func__);
		goto err_tt;
	}

	ret = smmu_enable_translation(&smmu);
	if (ret) {
		debug("%s: Failed to enable translation\n", __func__);
		goto err_tt;
	}

	smmu_init_cd(&smmu);
	if (ret) {
		debug("%s: Failed to initialize CD => ret: 0x%x\n",
		      __func__, ret);
		goto err_tt;
	}

	ret = smmu_init_ste(&smmu, SDM_ID);
	if (ret) {
		debug("%s: Failed to init\n", __func__);
		goto err_tt;
	}

	ret = smmu_invalidate_ste(&smmu, SDM_ID, SMMU_CMD_BRANCH);
	if (ret) {
		debug("%s: Failed to invalidate STE => ret: 0x%x\n",
		      __func__, ret);
		goto err_tt;
	}

	ret = smmu_sync_cmdq(&smmu, SMMU_SYNC_SIG_NONE, 0, 0, 0, 0);
	if (ret) {
		debug("%s: Failed to invalidate STE => ret: 0x%x\n",
		      __func__, ret);
		goto err_tt;
	}

	printf("SMMU is up running...\n");

	return 0;

err_tt:
	free(tt_mem);

err_smmu:
	free(smmu_mem);

	return ret;
}

static inline void setup_smmu_firewall(void)
{
	u32 i;

	/* Off the DDR secure transaction for all TBU supported peripherals */
	for (i = SYSMGR_DMA0_SID_ADDR; i < SYSMGR_TSN2_SID_ADDR; i +=
	     SOCFPGA_NEXT_TBU_PERIPHERAL) {
		/* skip this, future use register */
		if (i == SYSMGR_USB3_SID_ADDR)
			continue;

		writel(SECURE_TRANS_RESET, (uintptr_t)i);
	}
}

void socfpga_init_smmu(void)
{
	setup_smmu_firewall();
}
