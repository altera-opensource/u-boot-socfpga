// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Intel Corporation <www.intel.com>
 *
 */

#include <common.h>
#include <errno.h>
#include <wait_bit.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/socfpga_smmuv3.h>
#include <asm/arch/system_manager.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <linux/bitfield.h>
#include <linux/errno.h>
#include <linux/sizes.h>

#define TIMEOUT_200MS       200

#define SMMU_S_BASE_OFFSET  0x8000
#define SMMU_NS             SOCFPGA_SMMU_ADDRESS
#define SMMU_S              SOCFPGA_SMMU_ADDRESS + SMMU_S_BASE_OFFSET

/* Offset of registers in both secure and non-secure */
#define IDR0                0x0000
#define IDR1                0x0004
#define IDR2                0x0008
#define IDR3                0x000C
#define IDR4                0x0010
#define IDR5                0x0014
#define IIDR                0x0018
#define AIDR                0x001C
#define CR0                 0x0020
#define CR0ACK              0x0024
#define CR1                 0x0028
#define CR2                 0x002C
#define INIT                0x003C
#define STATUSR             0x0040
#define GBPA                0x0044
#define AGBPA               0x0048
#define IRQ_CTRL            0x0050
#define IRQ_CTRLACK         0x0054
#define GERROR              0x0060
#define GERRORN             0x0064
#define GERROR_IRQ_CFG0     0x0068
#define GERROR_IRQ_CFG1     0x0070
#define GERROR_IRQ_CFG2     0x0074
#define STRTAB_BASE         0x0080
#define STRTAB_BASE_CFG     0x0088
#define CMDQ_BASE           0x0090
#define CMDQ_PROD           0x0098
#define CMDQ_CONS           0x009C
#define EVENTQ_BASE         0x00A0
#define S_EVENTQ_PROD       0x00A8
#define S_EVENTQ_CONS       0x00AC
#define EVENTQ_IRQ_CFG0     0x00B0
#define EVENTQ_IRQ_CFG1     0x00B8
#define EVENTQ_IRQ_CFG2     0x00BC
#define PRIQ_BASE           0x00C0
#define ASMMU_PRIQ_PROD     0x00C8
#define ASMMU_PRIQ_CONS     0x00CC
#define PRIQ_IRQ_CFG        0x00D0
#define PRIQ_IRQ_CFG1       0x00D8
#define PRIQ_IRQ_CFG2       0x00DC
#define GATOS_CTRL          0x0100
#define GATOS_SID           0x0108
#define GATOS_ADDR          0x0110
#define GATOS_PAR           0x0118
#define EVENTQ_PROD			0x100A8
#define EVENTQ_CONS			0x100AC

/* Masking */
#define SMMU_STRTAB_BASE_RA_MASK				BIT(62)
#define SMMU_STRTAB_BASE_ADDR_MASK				GENMASK(47, 6)
#define SMMU_STRTAB_BASE_CFG_STR_ID_WIDTH_MASK	GENMASK(5, 0)
#define SMMU_STRTAB_BASE_CFG_SPLIT_MASK			GENMASK(10, 6)
#define SMMU_STRTAB_BASE_CFG_FMT_MASK			GENMASK(17, 16)

#define SMMU_EVENTQ_BASE_WA_MASK				BIT(62)
#define SMMU_EVENTQ_BASE_QSIZE_MASK				GENMASK(4, 0)
#define SMMU_EVENTQ_BASE_ADDR_MASK				GENMASK(47, 5)

#define SMMU_CMDQ_BASE_RA_MASK					BIT(62)
#define SMMU_CMDQ_BASE_QSIZE_MASK				GENMASK(4, 0)
#define SMMU_CMDQ_BASE_ADDR_MASK				GENMASK(47, 5)

#define SMMU_CMDQ_CONS_ERR_MASK					GENMASK(30, 24)

#define SMMU_CR0_SMMUEN_MASK					BIT(0)
#define SMMU_CR0_EVENTQ_MASK					BIT(2)
#define SMMU_CR0_CMDQEN_MASK					BIT(3)

#define SMMU_CR0ACK_SMMUEN_MASK					BIT(0)
#define SMMU_CR0ACK_EVENTQ_MASK					BIT(2)
#define SMMU_CR0ACK_CMDQEN_MASK					BIT(3)

#define SMMU_CR1_QIC_MASK						GENMASK(1, 0)
#define SMMU_CR1_QOC_MASK						GENMASK(3, 2)
#define SMMU_CR1_QSH_MASK						GENMASK(5, 4)
#define SMMU_CR1_TIC_MASK						GENMASK(7, 6)
#define SMMU_CR1_TOC_MASK						GENMASK(9, 8)
#define SMMU_CR1_TSH_MASK						GENMASK(11, 10)

#define SMMU_S_INIT_INV_ALL						BIT(0)

#define CMD_CFGI_STE_LEAF_MASK					BIT(0)
#define CMD_CFGI_STE_SSEC_MASK					BIT(10)
#define CMD_CFGI_STE_STREAMID_MASK				GENMASK(31, 0)
#define CMD_SYNC_CS_MASK						GENMASK(13, 12)
#define CMD_SYNC_MSH_MASK						GENMASK(23, 22)
#define CMD_SYNC_MSHATTR_MASK					GENMASK(27, 24)
#define CMD_SYNC_MSIDATA_MASK					GENMASK(31, 0)
#define CMD_SYNC_MSIADDR_MASK					GENMASK(51, 2)

/* Commands opcode */
#define CMD_CFGI_STE							0x3
#define CMD_SYNC								0x46

#define CMD_SIZE_BYTE							SZ_16

int smmu_init_ste_table(struct smmuv3 *smmu, u8 ra, u8 str_id_width,
					    u8 fmt, u8 split)
{
	if (!smmu)
		return -ENODEV;

	writeq((smmu->ste_base & SMMU_STRTAB_BASE_ADDR_MASK) |
	       FIELD_PREP(SMMU_STRTAB_BASE_RA_MASK, ra),
	       smmu->curr + STRTAB_BASE);

	debug("%s: 0x%llx = 0x%llx\n", __func__, smmu->curr +
	      STRTAB_BASE, readq(smmu->curr + STRTAB_BASE));

	writel(FIELD_PREP(SMMU_STRTAB_BASE_CFG_FMT_MASK, fmt) |
	       FIELD_PREP(SMMU_STRTAB_BASE_CFG_SPLIT_MASK, split) |
		   (str_id_width & SMMU_STRTAB_BASE_CFG_STR_ID_WIDTH_MASK),
	       smmu->curr + STRTAB_BASE_CFG);

	debug("%s: 0x%llx = 0x%x\n", __func__, smmu->curr +
	      STRTAB_BASE_CFG, readl(smmu->curr + STRTAB_BASE_CFG));

	return 0;
}

int smmu_init_event_queue(struct smmuv3 *smmu, u8 wa, u8 queue_size)
{
	if (!smmu)
		return -ENODEV;

	writeq((queue_size & SMMU_EVENTQ_BASE_QSIZE_MASK) |
	       (smmu->eq_base & SMMU_EVENTQ_BASE_ADDR_MASK) |
	       FIELD_PREP(SMMU_EVENTQ_BASE_WA_MASK, wa),
		   smmu->curr + EVENTQ_BASE);

	debug("%s: 0x%llx = 0x%llx\n",  __func__, smmu->curr +
	      EVENTQ_BASE, readq(smmu->curr + EVENTQ_BASE));

	if (smmu->curr == smmu->ns_base) {
		writel(0x0, smmu->curr + EVENTQ_CONS);
		writel(0x0, smmu->curr + EVENTQ_PROD);
	} else {
		writel(0x0, smmu->curr + S_EVENTQ_CONS);
		writel(0x0, smmu->curr + S_EVENTQ_PROD);
	}

	return 0;
}

int smmu_init_command_queue(struct smmuv3 *smmu, u8 ra, u8 queue_size)
{
	if (!smmu)
		return -ENODEV;

	writeq((queue_size & SMMU_CMDQ_BASE_QSIZE_MASK) |
	       (smmu->cmdq_base & SMMU_CMDQ_BASE_ADDR_MASK) |
	       FIELD_PREP(SMMU_CMDQ_BASE_RA_MASK, ra),
		   smmu->curr + CMDQ_BASE);

	debug("%s: 0x%llx = 0x%llx\n",  __func__, smmu->curr + CMDQ_BASE,
	      readq(smmu->curr + CMDQ_BASE));

	writel(0x0, smmu->curr + CMDQ_CONS);
	writel(0x0, smmu->curr + CMDQ_PROD);

	return 0;
}

int smmu_init_table_attributes(struct smmuv3 *smmu, u32 inner_cache,
			       u32 outer_cache, u32 shareability)
{
	if (!smmu)
		return -ENODEV;

	writel(FIELD_PREP(SMMU_CR1_QIC_MASK, inner_cache) |
	       FIELD_PREP(SMMU_CR1_QOC_MASK, outer_cache) |
	       FIELD_PREP(SMMU_CR1_QSH_MASK, shareability) |
	       FIELD_PREP(SMMU_CR1_TIC_MASK, inner_cache) |
	       FIELD_PREP(SMMU_CR1_TOC_MASK, outer_cache) |
	       FIELD_PREP(SMMU_CR1_TSH_MASK, shareability),
		   smmu->curr + CR1);

	debug("%s: 0x%llx = 0x%llx\n", __func__, smmu->curr + CR1,
	      readq(smmu->curr + CR1));

	return 0;
}

int smmu_invalidate_all(struct smmuv3 *smmu)
{
	int ret;

	if (!smmu)
		return -ENODEV;

	if (readl(smmu->ns_base + CR0) & SMMU_CR0_SMMUEN_MASK ||
	    readl(smmu->s_base + CR0) & SMMU_CR0_SMMUEN_MASK) {
		printf("%s: ERROR - constrained unpredictable", __func__);
		printf("writing to SMMU_C_INIT when SMMU_CR0.SMMUEN==1\n");

		return -EPERM;
	}

	setbits_le32(smmu->s_base + INIT, SMMU_S_INIT_INV_ALL);

	ret = wait_for_bit_le32((const void *)(smmu->s_base + INIT),
				SMMU_S_INIT_INV_ALL, false, TIMEOUT_200MS,
				false);

	return ret;
}

int smmu_enable_cmd(struct smmuv3 *smmu)
{
	int ret;

	if (!smmu)
		return -ENODEV;

	setbits_le32(smmu->curr + CR0, SMMU_CR0_CMDQEN_MASK);

	/* Poll SMMU_CR0ACK until the CMDQEN set to "1" */
	ret = wait_for_bit_le32((const void *)(smmu->curr + CR0ACK),
				SMMU_CR0ACK_CMDQEN_MASK, true, TIMEOUT_200MS,
				false);

	return ret;
}

int smmu_enable_eventQ(struct smmuv3 *smmu)
{
	int ret;

	if (!smmu)
		return -ENODEV;

	setbits_le32(smmu->curr + CR0, SMMU_CR0_EVENTQ_MASK);

	/* Poll SMMU_CR0ACK until the EVENTQEN set to "1" */
	ret = wait_for_bit_le32((const void *)(smmu->curr + CR0ACK),
				SMMU_CR0ACK_EVENTQ_MASK, true, TIMEOUT_200MS,
				false);

	return ret;
}

int smmu_enable_translation(struct smmuv3 *smmu)
{
	int ret;

	if (!smmu)
		return -ENODEV;

	setbits_le32(smmu->curr + CR0, SMMU_CR0_SMMUEN_MASK);

	/* Poll SMMU_CR0ACK until the SMMUEN set to "1" */
	ret = wait_for_bit_le32((const void *)(smmu->curr + CR0ACK),
				SMMU_CR0ACK_SMMUEN_MASK, true, TIMEOUT_200MS,
				false);

	return ret;
}

static bool smmu_qfull(struct smmuv3 *smmu)
{
	u32 cons = readl(smmu->curr + CMDQ_CONS);
	u32 prod = readl(smmu->curr + CMDQ_PROD);

	return (((cons & smmu->cmdq_idx_mask) ==
		(prod & smmu->cmdq_idx_mask)) &&
		((cons & smmu->cmdq_wrap_mask) !=
		(prod & smmu->cmdq_wrap_mask)));
}

static u32 smmu_qidx_inc(struct smmuv3 *smmu)
{
	u32 prod = readl(smmu->curr + CMDQ_PROD);

	u32 i = (prod + 1) & smmu->cmdq_idx_mask;

	if (i <= (prod & smmu->cmdq_idx_mask))
		i = ((prod & smmu->cmdq_wrap_mask) ^
		     smmu->cmdq_wrap_mask) | i;
	else
		i = (prod & smmu->cmdq_wrap_mask) | i;

	return i;
}

static bool smmu_qempty(struct smmuv3 *smmu)
{
	u32 cons = readl(smmu->curr + CMDQ_CONS);
	u32 prod = readl(smmu->curr + CMDQ_PROD);

	return ((cons & smmu->cmdq_idx_mask) ==
		(prod & smmu->cmdq_idx_mask)) &&
		((cons & smmu->cmdq_wrap_mask) ==
		(prod & smmu->cmdq_wrap_mask));
}

static u32 smmu_cmd_qerr(struct smmuv3 *smmu)
{
	return (readl(smmu->curr + CMDQ_CONS) & SMMU_CMDQ_CONS_ERR_MASK);
}

static int smmu_add_cmd(struct smmuv3 *smmu, const void *cmd)
{
	phys_addr_t *entry;
	u32 start = get_timer(0);

	while (smmu_qfull(smmu)) {
		if (get_timer(start) > TIMEOUT_200MS) {
			debug("%s: Timeout while waiting for",
			      __func__);
			debug(" available queue to process\n");
			return -ETIMEDOUT;
		}

		schedule();
	}

	/* Get empty entry in queue */
	entry = (phys_addr_t *)(smmu->cmdq_base +
			       ((readl(smmu->curr + CMDQ_PROD) &
				smmu->cmdq_idx_mask) * CMD_SIZE_BYTE));

	debug("%s: Command at %p\n", __func__, entry);

	/* Copy command into queue */
	memcpy((void *)entry, cmd, CMD_SIZE_BYTE);

	debug("%s: Command is:\n 0x%llx\n 0x%llx\n", __func__, entry[0],
	      entry[1]);

	flush_dcache_range((unsigned long)entry,
					   (unsigned long)(entry + CMD_SIZE_BYTE));

	/* Increment queue pointer */
	writel(smmu_qidx_inc(smmu), smmu->curr + CMDQ_PROD);

	start = get_timer(0);

	while (!smmu_qempty(smmu) && !smmu_cmd_qerr(smmu)) {
		if (get_timer(start) > TIMEOUT_200MS) {
			debug("%s: Timeout while waiting for",
			      __func__);
			debug(" command to be processed\n");
			return -ETIMEDOUT;
		}

		schedule();
	}

	debug("%s: PROD = 0x%x COND = 0x%x ERROR = 0x%lx\n",  __func__,
	      (readl(smmu->curr + CMDQ_PROD) & smmu->cmdq_idx_mask),
	      (readl(smmu->curr + CMDQ_CONS) & smmu->cmdq_idx_mask),
	      (readl(smmu->curr + CMDQ_CONS) & SMMU_CMDQ_CONS_ERR_MASK));

	return smmu_cmd_qerr(smmu);
}

bool smmu_invalidate_ste(struct smmuv3 *smmu, const u32 streamid,
			 const u32 leaf)
{
	u32 cmd[4] = {0};

	if (!smmu)
		return -ENODEV;

	cmd[0] = CMD_CFGI_STE;

	if (smmu->curr == smmu->s_base)
		setbits_le32(cmd, CMD_CFGI_STE_SSEC_MASK);
	else
		clrbits_le32(cmd, CMD_CFGI_STE_SSEC_MASK);

	cmd[1] = streamid & CMD_CFGI_STE_STREAMID_MASK;

	cmd[2]  = leaf & CMD_CFGI_STE_LEAF_MASK;

	return smmu_add_cmd(smmu, cmd);
}

// Issues a CMD_SYNC command to the specified queue
bool smmu_sync_cmdq(struct smmuv3 *smmu, const u32 cs, const u32 msh,
		    const u32 msiattr, u32 msi_data, u64 msi_addr)
{
	u32 cmd[4] = {0};

	cmd[0] = CMD_SYNC;

	setbits_le32(cmd, FIELD_PREP(CMD_SYNC_CS_MASK, cs));

	setbits_le32(cmd, FIELD_PREP(CMD_SYNC_MSH_MASK, msh));

	setbits_le32(cmd, FIELD_PREP(CMD_SYNC_MSHATTR_MASK, msiattr));

	writel(msi_data & CMD_SYNC_MSIDATA_MASK, &cmd[1]);

	setbits_le64(&cmd[2], FIELD_PREP(CMD_SYNC_MSIADDR_MASK, msi_addr));

	return smmu_add_cmd(smmu, cmd);
}
