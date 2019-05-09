/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Intel Corporation <www.intel.com>
 */

#ifndef	_CCU_AGILEX_H_
#define	_CCU_AGILEX_H_

/* Directory */
#define DIRUSFER		0x80010
#define DIRUCASER0		0x80040
#define DIRUMRHER		0x80070
#define DIRUSFMCR		0x80080
#define DIRUSFMAR		0x80084

#define DIRUSFMCR_SFID_SHIFT	16

/* Coherent cache agent interface */
#define CAIUIDR			0x00ffc

#define CAIUIDR_CA_GET(v)	(((v) & 0x00008000) >> 15)
#define CAIUIDR_TYPE_GET(v)	(((v) & 0x000f0000) >> 16)
#define CAIUIDR_TYPE_ACE_CAI_DVM_SUPPORT	0
#define CAIUIDR_TYPE_ACELITE_CAI_DVM_SUPPORT	1

/* Coherent subsystem */
#define CSADSER0		0xff040
#define CSUIDR			0xffff8
#define CSIDR			0xffffc

#define CSUIDR_NUMCAIUS_GET(v)	(((v) & 0x0000007f) >> 0)
#define CSUIDR_NUMDIRUS_GET(v)	(((v) & 0x003f0000) >> 16)
#define CSUIDR_NUMCMIUS_GET(v)	(((v) & 0x3f000000) >> 24)

#define CSIDR_NUMSFS_GET(v)	(((v) & 0x007c0000) >> 18)

#define DIR_REG_OFF		0x1000
#define CAIU_REG_OFF		0x1000
#define COHMEM_REG_OFF		0x1000

#define CCU_REG_ADDR(reg)		\
		(SOCFPGA_CCU_ADDRESS + (reg))

#define CCU_DIR_REG_ADDR(dir, reg)	\
		(CCU_REG_ADDR(reg) + ((dir) * DIR_REG_OFF))

#define CCU_CAIU_REG_ADDR(caiu, reg)	\
		(CCU_REG_ADDR(reg) + ((caiu) * CAIU_REG_OFF))

/* Coherent CPU BYPASS OCRAM */
#define COH_CPU0_BYPASS_OC_BASE		(SOCFPGA_CCU_ADDRESS + 0x100200)

#define OCRAM_BLK_CGF_01_REG		0x4
#define OCRAM_BLK_CGF_02_REG		0x8
#define OCRAM_BLK_CGF_03_REG		0xc
#define OCRAM_BLK_CGF_04_REG		0x10
#define OCRAM_SECURE_REGIONS		4

#define OCRAM_PRIVILEGED_MASK		BIT(29)
#define OCRAM_SECURE_MASK		BIT(30)

#define COH_CPU0_BYPASS_REG_ADDR(reg)		\
		(COH_CPU0_BYPASS_OC_BASE + (reg))

void ccu_init(void);

#endif /* _CCU_AGILEX_H_ */
