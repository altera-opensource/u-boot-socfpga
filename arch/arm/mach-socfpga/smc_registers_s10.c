/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/sdram_s10.h>
#include <asm/arch/smc_s10.h>
#include <asm/arch/system_manager.h>
#include <linux/intel-smc.h>

static struct socfpga_system_manager *sysmgr_regs =
	(struct socfpga_system_manager *)SOCFPGA_SYSMGR_ADDRESS;
static const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;

static void __secure smc_socfpga_sysman_read(unsigned long function_id,
					     unsigned long offset)
{
	const u32 *pdata = (&sysmgr_regs->siliconid1 + (offset / sizeof(u32)));

	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	switch (offset) {
	case(0x90):	/* ECC_INT_MASK_VALUE */
	case(0x94):	/* ECC_INT_MASK_SET */
	case(0x98):	/* ECC_INT_MASK_CLEAR */
	case(0x9C):	/* ECC_INTSTATUS_SERR */
	case(0xA0):	/* ECC_INTSTATUS_DERR */
		/* Value at offset */
		SMC_ASSIGN_REG_MEM(r, SMC_ARG1,
				   readl(pdata));
		break;
	default:
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_SYSMAN_ERROR);
		break;
	}

	SMC_ASSIGN_REG_MEM(r, SMC_ARG2, offset);

	SMC_RET_REG_MEM(r);
}

static void __secure smc_socfpga_sysman_write(unsigned long function_id,
					      unsigned long offset,
					      unsigned long mask,
					      unsigned long value)
{
	const u32 *pdata = (&sysmgr_regs->siliconid1 + (offset / sizeof(u32)));

	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	switch (offset) {
	case(0x90):	/* ECC_INT_MASK_VALUE */
	case(0x94):	/* ECC_INT_MASK_SET */
	case(0x98):	/* ECC_INT_MASK_CLEAR */
	case(0x9C):	/* ECC_INTSTATUS_SERR */
	case(0xA0):	/* ECC_INTSTATUS_DERR */
		clrsetbits_le32(pdata, mask, value);
		/* Value at offset */
		SMC_ASSIGN_REG_MEM(r, SMC_ARG1, readl(pdata));
		break;
	default:
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_SYSMAN_ERROR);
		break;
	}

	SMC_ASSIGN_REG_MEM(r, SMC_ARG2, offset);

	SMC_RET_REG_MEM(r);
}
static void __secure smc_socfpga_hmcecc_read(unsigned long function_id,
					     unsigned long offset)
{
	const u32 *pdata = (&socfpga_ecc_hmc_base->eccctrl +
			    (offset / sizeof(u32)));

	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	switch (offset) {
	case(0x0):	/* ECCCTRL1 */
	case(0x4):	/* ECCCTRL2 */
	case(0x10):	/* ERRINTEN */
	case(0x14):	/* ERRINTENS */
	case(0x18):	/* ERRINTENR */
	case(0x1C):	/* INTMODE */
	case(0x20):	/* INTSTAT */
		/* Value at offset */
		SMC_ASSIGN_REG_MEM(r, SMC_ARG1,
				   readl(pdata));
		break;
	default:
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_ECC_ERROR);
		break;
	}

	SMC_ASSIGN_REG_MEM(r, SMC_ARG2, offset);

	SMC_RET_REG_MEM(r);
}

static void __secure smc_socfpga_hmcecc_write(unsigned long function_id,
					      unsigned long offset,
					      unsigned long mask,
					      unsigned long value)
{
	const u32 *pdata = (&socfpga_ecc_hmc_base->eccctrl +
			    (offset / sizeof(u32)));

	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	switch (offset) {
	case(0x0):	/* ECCCTRL1 */
	case(0x4):	/* ECCCTRL2 */
	case(0x10):	/* ERRINTEN */
	case(0x14):	/* ERRINTENS */
	case(0x18):	/* ERRINTENR */
	case(0x1C):	/* INTMODE */
	case(0x20):	/* INTSTAT */
		clrsetbits_le32(pdata, mask, value);
		/* Value at offset */
		SMC_ASSIGN_REG_MEM(r, SMC_ARG1, readl(pdata));
		break;
	default:
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_ECC_ERROR);
		break;
	}

	SMC_ASSIGN_REG_MEM(r, SMC_ARG2, offset);

	SMC_RET_REG_MEM(r);
}

DECLARE_SECURE_SVC(sysman_read, INTEL_SIP_SMC_SYSMAN_READ,
		   smc_socfpga_sysman_read);
DECLARE_SECURE_SVC(sysman_write, INTEL_SIP_SMC_SYSMAN_WRITE,
		   smc_socfpga_sysman_write);
DECLARE_SECURE_SVC(hmc_read, INTEL_SIP_SMC_ECC_HMC_READ,
		   smc_socfpga_hmcecc_read);
DECLARE_SECURE_SVC(hmc_write, INTEL_SIP_SMC_ECC_HMC_WRITE,
		   smc_socfpga_hmcecc_write);
