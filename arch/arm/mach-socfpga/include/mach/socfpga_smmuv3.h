/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Intel Corporation <www.intel.com>
 *
 */

#ifndef _SOCFPGA_SMMUV3_H_
#define _SOCFPGA_SMMUV3_H_

struct smmuv3 {
	phys_addr_t curr;
	phys_addr_t ns_base;
	phys_addr_t s_base;
	phys_addr_t ste_base;
	phys_addr_t eq_base;
	phys_addr_t cmdq_base;
	phys_addr_t cd_base;
	phys_addr_t ttv0;
	phys_addr_t ttv1;
	phys_addr_t ttv2;
	phys_addr_t tt_base;
	u32 cmdq_idx_mask;
	u32 cmdq_wrap_mask;
};

int smmu_init_ste_table(struct smmuv3 *smmu, u8 ra, u8 str_id_width, u8 fmt,
						u8 split);

int smmu_init_event_queue(struct smmuv3 *smmu, u8 wa, u8 queue_size);

int smmu_init_command_queue(struct smmuv3 *smmu, u8 ra, u8 queue_size);

int smmu_init_table_attributes(struct smmuv3 *smmu, u32 inner_cache,
							   u32 outer_cache, u32 shareability);

int smmu_invalidate_all(struct smmuv3 *smmu);

int smmu_enable_cmd(struct smmuv3 *smmu);

int smmu_enable_eventQ(struct smmuv3 *smmu);

int smmu_enable_translation(struct smmuv3 *smmu);

bool smmu_invalidate_ste(struct smmuv3 *smmu, const u32 streamid,
						 const u32 leaf);

bool smmu_sync_cmdq(struct smmuv3 *smmu, const u32 cs, const u32 msh,
					const u32 msiattr, u32 msi_data, u64 msi_addr);
#endif
