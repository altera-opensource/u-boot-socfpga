/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef	_SOCFPGA_GIC_H_
#define	_SOCFPGA_GIC_H_

#ifndef __ASSEMBLY__
static const struct gic_cpu_if *gic_cpu_if_base = (void *)SOCFPGA_GIC_CPU_IF;
struct gic_cpu_if {
	u32	ICCICR;
	u32	ICCPMR;
	u32	ICCBPR;
	u32	ICCIAR;
	u32	ICCEOIR;
	u32	ICCRPR;
	u32	ICCHPIR;
};
#endif /* __ASSEMBLY__ */

#define CONFIG_GIC_DIC_CONTROL		(SOCFPGA_GIC_DIC + 0)
#define CONFIG_GIC_DIC_SECURITY		(SOCFPGA_GIC_DIC + 0x80)
#define CONFIG_GIC_DIC_SET_ENABLE	(SOCFPGA_GIC_DIC + 0x100)
#define CONFIG_GIC_DIC_CLR_ENABLE	(SOCFPGA_GIC_DIC + 0x180)
#define CONFIG_GIC_DIC_SET_PENDING	(SOCFPGA_GIC_DIC + 0x200)
#define CONFIG_GIC_DIC_CLR_PENDING	(SOCFPGA_GIC_DIC + 0x280)
#define CONFIG_GIC_DIC_PRIORITY		(SOCFPGA_GIC_DIC + 0x400)
#define CONFIG_GIC_DIC_TARGET		(SOCFPGA_GIC_DIC + 0x800)
#define CONFIG_GIC_DIC_CONFIG		(SOCFPGA_GIC_DIC + 0xc00)
#define CONFIG_GIC_DIC_PPI_STATUS	(SOCFPGA_GIC_DIC + 0xd00)
#define CONFIG_GIC_DIC_SPI_STATUS	(SOCFPGA_GIC_DIC + 0xd04)

#ifndef __ASSEMBLY__
void gic_dic_clear_enable_all_intr (void);
void gic_dic_clear_pending_all_intr (void);
void gic_dic_set_enable (unsigned char intrID);
void gic_dic_set_target_processor (unsigned char intrID,
	unsigned char target);
void gic_dic_set_config (unsigned char intrID,
	unsigned char level0edge1);
void gic_dic_set_pending (unsigned char intrID);
void gic_dic_clear_pending (unsigned char intrID);

static inline void gic_cpu_disable(void)
{
	writel(0x0, &gic_cpu_if_base->ICCICR);
}

static inline void gic_cpu_enable(void)
{
	writel(0x1, &gic_cpu_if_base->ICCICR);
}

static inline void gic_cpu_set_priority_mask(unsigned char priority_mask)
{
	writel(priority_mask, &gic_cpu_if_base->ICCPMR);
}

static inline unsigned long gic_cpu_get_pending_intr(void)
{
	return readl(&gic_cpu_if_base->ICCHPIR);
}

static inline void gic_cpu_end_of_intr(unsigned char intrID)
{
	writel(intrID, &gic_cpu_if_base->ICCEOIR);
}

static inline void gic_dic_disable_secure(void)
{
	clrbits_le32(CONFIG_GIC_DIC_CONTROL, 0x1);
}

static inline void gic_dic_disable_nonsecure(void)
{
	clrbits_le32(CONFIG_GIC_DIC_CONTROL, 0x2);
}

static inline void gic_dic_enable_secure(void)
{
	setbits_le32(CONFIG_GIC_DIC_CONTROL, 0x1);
}

static inline void gic_dic_enable_nonsecure(void)
{
	setbits_le32(CONFIG_GIC_DIC_CONTROL, 0x2);
}
#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_GIC_H_ */
