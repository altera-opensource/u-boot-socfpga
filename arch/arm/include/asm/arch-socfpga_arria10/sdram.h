/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef	_SOCFPGA_SDRAM_H_
#define	_SOCFPGA_SDRAM_H_

extern unsigned long irq_cnt_ecc_sdram;

#ifndef __ASSEMBLY__
struct socfpga_ecc_hmc {
	volatile uint32_t ip_rev_id;
	volatile uint32_t _pad_0x4_0x7;
	volatile uint32_t ddrioctrl;
	volatile uint32_t ddrcalstat;
	volatile uint32_t mpr_0beat1;
	volatile uint32_t mpr_1beat1;
	volatile uint32_t mpr_2beat1;
	volatile uint32_t mpr_3beat1;
	volatile uint32_t mpr_4beat1;
	volatile uint32_t mpr_5beat1;
	volatile uint32_t mpr_6beat1;
	volatile uint32_t mpr_7beat1;
	volatile uint32_t mpr_8beat1;
	volatile uint32_t mpr_0beat2;
	volatile uint32_t mpr_1beat2;
	volatile uint32_t mpr_2beat2;
	volatile uint32_t mpr_3beat2;
	volatile uint32_t mpr_4beat2;
	volatile uint32_t mpr_5beat2;
	volatile uint32_t mpr_6beat2;
	volatile uint32_t mpr_7beat2;
	volatile uint32_t mpr_8beat2;
	volatile uint32_t _pad_0x58_0x5f[2];
	volatile uint32_t auto_precharge;
	volatile uint32_t _pad_0x64_0xff[39];
	volatile uint32_t eccctrl;
	volatile uint32_t eccctrl2;
	volatile uint32_t _pad_0x108_0x10f[2];
	volatile uint32_t errinten;
	volatile uint32_t errintens;
	volatile uint32_t errintenr;
	volatile uint32_t intmode;
	volatile uint32_t intstat;
	volatile uint32_t diaginttest;
	volatile uint32_t modstat;
	volatile uint32_t derraddra;
	volatile uint32_t serraddra;
	volatile uint32_t _pad_0x134_0x137;
	volatile uint32_t autowb_corraddr;
	volatile uint32_t serrcntreg;
	volatile uint32_t autowb_drop_cntreg;
	volatile uint32_t _pad_0x144_0x147;
	volatile uint32_t ecc_reg2wreccdatabus;
	volatile uint32_t ecc_rdeccdata2regbus;
	volatile uint32_t ecc_reg2rdeccdatabus;
	volatile uint32_t _pad_0x154_0x15f[3];
	volatile uint32_t ecc_diagon;
	volatile uint32_t ecc_decstat;
	volatile uint32_t _pad_0x168_0x16f[2];
	volatile uint32_t ecc_errgenaddr_0;
	volatile uint32_t ecc_errgenaddr_1;
	volatile uint32_t ecc_errgenaddr_2;
	volatile uint32_t ecc_errgenaddr_3;
};

struct socfpga_noc_ddr_scheduler {
	volatile uint32_t ddr_t_main_scheduler_id_coreid;
	volatile uint32_t ddr_t_main_scheduler_id_revisionid;
	volatile uint32_t ddr_t_main_scheduler_ddrconf;
	volatile uint32_t ddr_t_main_scheduler_ddrtiming;
	volatile uint32_t ddr_t_main_scheduler_ddrmode;
	volatile uint32_t ddr_t_main_scheduler_readlatency;
	volatile uint32_t _pad_0x20_0x34[8];
	volatile uint32_t ddr_t_main_scheduler_activate;
	volatile uint32_t ddr_t_main_scheduler_devtodev;
};

/* for master such as MPU and FPGA */
struct socfpga_noc_fw_ddr_mpu_fpga2sdram {
	volatile uint32_t enable;
	volatile uint32_t enable_set;
	volatile uint32_t enable_clear;
	volatile uint32_t _pad_0xc_0xf;
	volatile uint32_t mpuregion0addr;
	volatile uint32_t mpuregion1addr;
	volatile uint32_t mpuregion2addr;
	volatile uint32_t mpuregion3addr;
	volatile uint32_t fpga2sdram0region0addr;
	volatile uint32_t fpga2sdram0region1addr;
	volatile uint32_t fpga2sdram0region2addr;
	volatile uint32_t fpga2sdram0region3addr;
	volatile uint32_t fpga2sdram1region0addr;
	volatile uint32_t fpga2sdram1region1addr;
	volatile uint32_t fpga2sdram1region2addr;
	volatile uint32_t fpga2sdram1region3addr;
	volatile uint32_t fpga2sdram2region0addr;
	volatile uint32_t fpga2sdram2region1addr;
	volatile uint32_t fpga2sdram2region2addr;
	volatile uint32_t fpga2sdram2region3addr;
};

/* for L3 master */
struct socfpga_noc_fw_ddr_l3 {
	volatile uint32_t enable;
	volatile uint32_t enable_set;
	volatile uint32_t enable_clear;
	volatile uint32_t hpsregion0addr;
	volatile uint32_t hpsregion1addr;
	volatile uint32_t hpsregion2addr;
	volatile uint32_t hpsregion3addr;
	volatile uint32_t hpsregion4addr;
	volatile uint32_t hpsregion5addr;
	volatile uint32_t hpsregion6addr;
	volatile uint32_t hpsregion7addr;
};


struct socfpga_io48_mmr {
	volatile uint32_t dbgcfg0;
	volatile uint32_t dbgcfg1;
	volatile uint32_t dbgcfg2;
	volatile uint32_t dbgcfg3;
	volatile uint32_t dbgcfg4;
	volatile uint32_t dbgcfg5;
	volatile uint32_t dbgcfg6;
	volatile uint32_t reserve0;
	volatile uint32_t reserve1;
	volatile uint32_t reserve2;
	volatile uint32_t ctrlcfg0;
	volatile uint32_t ctrlcfg1;
	volatile uint32_t ctrlcfg2;
	volatile uint32_t ctrlcfg3;
	volatile uint32_t ctrlcfg4;
	volatile uint32_t ctrlcfg5;
	volatile uint32_t ctrlcfg6;
	volatile uint32_t ctrlcfg7;
	volatile uint32_t ctrlcfg8;
	volatile uint32_t ctrlcfg9;
	volatile uint32_t dramtiming0;
	volatile uint32_t dramodt0;
	volatile uint32_t dramodt1;
	volatile uint32_t sbcfg0;
	volatile uint32_t sbcfg1;
	volatile uint32_t sbcfg2;
	volatile uint32_t sbcfg3;
	volatile uint32_t sbcfg4;
	volatile uint32_t sbcfg5;
	volatile uint32_t sbcfg6;
	volatile uint32_t sbcfg7;
	volatile uint32_t caltiming0;
	volatile uint32_t caltiming1;
	volatile uint32_t caltiming2;
	volatile uint32_t caltiming3;
	volatile uint32_t caltiming4;
	volatile uint32_t caltiming5;
	volatile uint32_t caltiming6;
	volatile uint32_t caltiming7;
	volatile uint32_t caltiming8;
	volatile uint32_t caltiming9;
	volatile uint32_t caltiming10;
	volatile uint32_t dramaddrw;
	volatile uint32_t sideband0;
	volatile uint32_t sideband1;
	volatile uint32_t sideband2;
	volatile uint32_t sideband3;
	volatile uint32_t sideband4;
	volatile uint32_t sideband5;
	volatile uint32_t sideband6;
	volatile uint32_t sideband7;
	volatile uint32_t sideband8;
	volatile uint32_t sideband9;
	volatile uint32_t sideband10;
	volatile uint32_t sideband11;
	volatile uint32_t sideband12;
	volatile uint32_t sideband13;
	volatile uint32_t sideband14;
	volatile uint32_t sideband15;
	volatile uint32_t dramsts;
	volatile uint32_t dbgdone;
	volatile uint32_t dbgsignals;
	volatile uint32_t dbgreset;
	volatile uint32_t dbgmatch;
	volatile uint32_t counter0mask;
	volatile uint32_t counter1mask;
	volatile uint32_t counter0match;
	volatile uint32_t counter1match;
	volatile uint32_t niosreserve0;
	volatile uint32_t niosreserve1;
	volatile uint32_t niosreserve2;
};

union dramaddrw_reg {
	struct {
		u32 cfg_col_addr_width:5;
		u32 cfg_row_addr_width:5;
		u32 cfg_bank_addr_width:4;
		u32 cfg_bank_group_addr_width:2;
		u32 cfg_cs_addr_width:3;
		u32 reserved:13;
	};
	u32 word;
};

union ctrlcfg0_reg {
	struct {
		u32 cfg_mem_type:4;
		u32 cfg_dimm_type:3;
		u32 cfg_ac_pos:2;
		u32 cfg_ctrl_burst_len:5;
		u32 reserved:18;  /* Other fields unused */
	};
	u32 word;
};

union ctrlcfg1_reg {
	struct {
		u32 cfg_dbc3_burst_len:5;
		u32 cfg_addr_order:2;
		u32 cfg_ctrl_enable_ecc:1;
		u32 reserved:24;  /* Other fields unused */
	};
	u32 word;
};

union caltiming0_reg {
	struct {
		u32 cfg_act_to_rdwr:6;
		u32 cfg_act_to_pch:6;
		u32 cfg_act_to_act:6;
		u32 cfg_act_to_act_db:6;
		u32 reserved:8;  /* Other fields unused */
	};
	u32 word;
};

union caltiming1_reg {
	struct {
		u32 cfg_rd_to_rd:6;
		u32 cfg_rd_to_rd_dc:6;
		u32 cfg_rd_to_rd_db:6;
		u32 cfg_rd_to_wr:6;
		u32 cfg_rd_to_wr_dc:6;
		u32 reserved:2;
	};
	u32 word;
};

union caltiming2_reg {
	struct {
		u32 cfg_rd_to_wr_db:6;
		u32 cfg_rd_to_pch:6;
		u32 cfg_rd_ap_to_valid:6;
		u32 cfg_wr_to_wr:6;
		u32 cfg_wr_to_wr_dc:6;
		u32 reserved:2;
	};
	u32 word;
};

union caltiming3_reg {
	struct {
		u32 cfg_wr_to_wr_db:6;
		u32 cfg_wr_to_rd:6;
		u32 cfg_wr_to_rd_dc:6;
		u32 cfg_wr_to_rd_db:6;
		u32 cfg_wr_to_pch:6;
		u32 reserved:2;
	};
	u32 word;
};

union caltiming4_reg {
	struct {
		u32 cfg_wr_ap_to_valid:6;
		u32 cfg_pch_to_valid:6;
		u32 cfg_pch_all_to_valid:6;
		u32 cfg_arf_to_valid:8;
		u32 cfg_pdn_to_valid:6;
	};
	u32 word;
};

union caltiming9_reg {
	struct {
		u32 cfg_4_act_to_act:8;
		u32 reserved:24;
	};
	u32 word;
};

struct sdr_cfg {
	u32 ecc_en;
	u32 serrcnt;
	u32 io_size;
	u32 ddrconf;
	u32 ddrtiming;
	u32 ddrmode;
	u32 readlatency;
	u32 activate;
	u32 devtodev;
};
#endif /* __ASSEMBLY__ */

#define ALT_ECC_HMC_OCP_DDRIOCTRL_IO_SIZE_MSK		0x00000003

#define ALT_ECC_HMC_OCP_INTSTAT_SERRPENA_SET_MSK	0x00000001
#define ALT_ECC_HMC_OCP_INTSTAT_DERRPENA_SET_MSK	0x00000002
#define ALT_ECC_HMC_OCP_ERRINTEN_SERRINTEN_SET_MSK	0x00000001
#define ALT_ECC_HMC_OCP_ERRINTEN_DERRINTEN_SET_MSK	0x00000002
#define ALT_ECC_HMC_OCP_INTMOD_INTONCMP_SET_MSK		0x00010000
#define ALT_ECC_HMC_OCP_ECCCTL_AWB_CNT_RST_SET_MSK	0x00010000
#define ALT_ECC_HMC_OCP_ECCCTL_CNT_RST_SET_MSK		0x00000100
#define ALT_ECC_HMC_OCP_ECCCTL_ECC_EN_SET_MSK		0x00000001
#define ALT_ECC_HMC_OCP_ECCCTL2_RMW_EN_SET_MSK		0x00000100
#define ALT_ECC_HMC_OCP_ECCCTL2_AWB_EN_SET_MSK		0x00000001

#define ALT_ECC_HMC_OCP_SERRCNTREG_VALUE		8

#define ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_ACTTOACT_LSB	0
#define ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_RDTOMISS_LSB	6
#define ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_WRTOMISS_LSB	12
#define ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_BURSTLEN_LSB	18
#define ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_RDTOWR_LSB	21
#define ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_WRTORD_LSB	26
#define ALT_NOC_MPU_DDR_T_SCHED_DDRTIMING_BWRATIO_LSB	31

#define ALT_NOC_MPU_DDR_T_SCHED_DDRMOD_AUTOPRECHARGE_LSB	0
#define ALT_NOC_MPU_DDR_T_SCHED_DDRMOD_BWRATIOEXTENDED_LSB	1

#define ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_RRD_LSB	0
#define ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_FAW_LSB	4
#define ALT_NOC_MPU_DDR_T_SCHED_ACTIVATE_FAWBANK_LSB	10

#define ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSRDTORD_LSB	0
#define ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSRDTOWR_LSB	2
#define ALT_NOC_MPU_DDR_T_SCHED_DEVTODEV_BUSWRTORD_LSB	4

#define ALT_NOC_FW_DDR_END_ADDR_LSB	16
#define ALT_NOC_FW_DDR_ADDR_MASK	0xFFFF
#define ALT_NOC_FW_DDR_SCR_EN_HPSREG0EN_SET_MSK		0x00000001
#define ALT_NOC_FW_DDR_SCR_EN_HPSREG1EN_SET_MSK		0x00000002
#define ALT_NOC_FW_DDR_SCR_EN_HPSREG2EN_SET_MSK		0x00000004
#define ALT_NOC_FW_DDR_SCR_EN_HPSREG3EN_SET_MSK		0x00000008
#define ALT_NOC_FW_DDR_SCR_EN_HPSREG4EN_SET_MSK		0x00000010
#define ALT_NOC_FW_DDR_SCR_EN_HPSREG5EN_SET_MSK		0x00000020
#define ALT_NOC_FW_DDR_SCR_EN_HPSREG6EN_SET_MSK		0x00000040
#define ALT_NOC_FW_DDR_SCR_EN_HPSREG7EN_SET_MSK		0x00000080
#define ALT_NOC_FW_DDR_SCR_EN_MPUREG0EN_SET_MSK		0x00000001
#define ALT_NOC_FW_DDR_SCR_EN_MPUREG1EN_SET_MSK		0x00000002
#define ALT_NOC_FW_DDR_SCR_EN_MPUREG2EN_SET_MSK		0x00000004
#define ALT_NOC_FW_DDR_SCR_EN_MPUREG3EN_SET_MSK		0x00000008
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG0EN_SET_MSK	0x00000010
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG1EN_SET_MSK	0x00000020
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG2EN_SET_MSK	0x00000040
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR0REG3EN_SET_MSK	0x00000080
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG0EN_SET_MSK	0x00000100
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG1EN_SET_MSK	0x00000200
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG2EN_SET_MSK	0x00000400
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR1REG3EN_SET_MSK	0x00000800
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG0EN_SET_MSK	0x00001000
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG1EN_SET_MSK	0x00002000
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG2EN_SET_MSK	0x00004000
#define ALT_NOC_FW_DDR_SCR_EN_F2SDR2REG3EN_SET_MSK	0x00008000

#define ALT_IO48_DRAMTIME_MEM_READ_LATENCY_MASK		0x0000003F

#ifndef __ASSEMBLY__
/* function declaration */
void irq_handler_ecc_sdram(void *arg);
void sdram_enable_interrupt(unsigned enable);
void sdram_mmr_init(struct sdr_cfg *pcfg);
void sdram_firewall_setup(void);
int is_sdram_cal_success(void);
int ddr_calibration_sequence(void);
#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_SDRAM_H_ */

























































