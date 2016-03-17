/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <asm/arch/system_manager.h>

#ifndef	_SOCFPGA_SDRAM_H_
#define	_SOCFPGA_SDRAM_H_

extern unsigned long irq_cnt_ecc_sdram;

#ifndef __ASSEMBLY__
struct socfpga_ecc_hmc {
	uint32_t ip_rev_id;
	uint32_t _pad_0x4_0x7;
	uint32_t ddrioctrl;
	uint32_t ddrcalstat;
	uint32_t mpr_0beat1;
	uint32_t mpr_1beat1;
	uint32_t mpr_2beat1;
	uint32_t mpr_3beat1;
	uint32_t mpr_4beat1;
	uint32_t mpr_5beat1;
	uint32_t mpr_6beat1;
	uint32_t mpr_7beat1;
	uint32_t mpr_8beat1;
	uint32_t mpr_0beat2;
	uint32_t mpr_1beat2;
	uint32_t mpr_2beat2;
	uint32_t mpr_3beat2;
	uint32_t mpr_4beat2;
	uint32_t mpr_5beat2;
	uint32_t mpr_6beat2;
	uint32_t mpr_7beat2;
	uint32_t mpr_8beat2;
	uint32_t _pad_0x58_0x5f[2];
	uint32_t auto_precharge;
	uint32_t _pad_0x64_0xff[39];
	uint32_t eccctrl;
	uint32_t eccctrl2;
	uint32_t _pad_0x108_0x10f[2];
	uint32_t errinten;
	uint32_t errintens;
	uint32_t errintenr;
	uint32_t intmode;
	uint32_t intstat;
	uint32_t diaginttest;
	uint32_t modstat;
	uint32_t derraddra;
	uint32_t serraddra;
	uint32_t _pad_0x134_0x137;
	uint32_t autowb_corraddr;
	uint32_t serrcntreg;
	uint32_t autowb_drop_cntreg;
	uint32_t ecc_reg2wreccdatabus;
	uint32_t ecc_rdeccdata2regbus;
	uint32_t ecc_reg2rdeccdatabus;
	uint32_t ecc_diagon;
	uint32_t ecc_decstat;
	uint32_t _pad_0x158_0x15f[2];
	uint32_t ecc_errgenaddr_0;
	uint32_t ecc_errgenaddr_1;
	uint32_t ecc_errgenaddr_2;
	uint32_t ecc_errgenaddr_3;
	uint32_t ecc_ref2rddatabus_beat0;
	uint32_t ecc_ref2rddatabus_beat1;
	uint32_t ecc_ref2rddatabus_beat2;
	uint32_t ecc_ref2rddatabus_beat3;
};

struct socfpga_noc_ddr_scheduler {
	uint32_t ddr_t_main_scheduler_id_coreid;
	uint32_t ddr_t_main_scheduler_id_revisionid;
	uint32_t ddr_t_main_scheduler_ddrconf;
	uint32_t ddr_t_main_scheduler_ddrtiming;
	uint32_t ddr_t_main_scheduler_ddrmode;
	uint32_t ddr_t_main_scheduler_readlatency;
	uint32_t _pad_0x20_0x34[8];
	uint32_t ddr_t_main_scheduler_activate;
	uint32_t ddr_t_main_scheduler_devtodev;
};

/* for master such as MPU and FPGA */
struct socfpga_noc_fw_ddr_mpu_fpga2sdram {
	uint32_t enable;
	uint32_t enable_set;
	uint32_t enable_clear;
	uint32_t _pad_0xc_0xf;
	uint32_t mpuregion0addr;
	uint32_t mpuregion1addr;
	uint32_t mpuregion2addr;
	uint32_t mpuregion3addr;
	uint32_t fpga2sdram0region0addr;
	uint32_t fpga2sdram0region1addr;
	uint32_t fpga2sdram0region2addr;
	uint32_t fpga2sdram0region3addr;
	uint32_t fpga2sdram1region0addr;
	uint32_t fpga2sdram1region1addr;
	uint32_t fpga2sdram1region2addr;
	uint32_t fpga2sdram1region3addr;
	uint32_t fpga2sdram2region0addr;
	uint32_t fpga2sdram2region1addr;
	uint32_t fpga2sdram2region2addr;
	uint32_t fpga2sdram2region3addr;
};

/* for L3 master */
struct socfpga_noc_fw_ddr_l3 {
	uint32_t enable;
	uint32_t enable_set;
	uint32_t enable_clear;
	uint32_t hpsregion0addr;
	uint32_t hpsregion1addr;
	uint32_t hpsregion2addr;
	uint32_t hpsregion3addr;
	uint32_t hpsregion4addr;
	uint32_t hpsregion5addr;
	uint32_t hpsregion6addr;
	uint32_t hpsregion7addr;
};


struct socfpga_io48_mmr {
	uint32_t dbgcfg0;
	uint32_t dbgcfg1;
	uint32_t dbgcfg2;
	uint32_t dbgcfg3;
	uint32_t dbgcfg4;
	uint32_t dbgcfg5;
	uint32_t dbgcfg6;
	uint32_t reserve0;
	uint32_t reserve1;
	uint32_t reserve2;
	uint32_t ctrlcfg0;
	uint32_t ctrlcfg1;
	uint32_t ctrlcfg2;
	uint32_t ctrlcfg3;
	uint32_t ctrlcfg4;
	uint32_t ctrlcfg5;
	uint32_t ctrlcfg6;
	uint32_t ctrlcfg7;
	uint32_t ctrlcfg8;
	uint32_t ctrlcfg9;
	uint32_t dramtiming0;
	uint32_t dramodt0;
	uint32_t dramodt1;
	uint32_t sbcfg0;
	uint32_t sbcfg1;
	uint32_t sbcfg2;
	uint32_t sbcfg3;
	uint32_t sbcfg4;
	uint32_t sbcfg5;
	uint32_t sbcfg6;
	uint32_t sbcfg7;
	uint32_t caltiming0;
	uint32_t caltiming1;
	uint32_t caltiming2;
	uint32_t caltiming3;
	uint32_t caltiming4;
	uint32_t caltiming5;
	uint32_t caltiming6;
	uint32_t caltiming7;
	uint32_t caltiming8;
	uint32_t caltiming9;
	uint32_t caltiming10;
	uint32_t dramaddrw;
	uint32_t sideband0;
	uint32_t sideband1;
	uint32_t sideband2;
	uint32_t sideband3;
	uint32_t sideband4;
	uint32_t sideband5;
	uint32_t sideband6;
	uint32_t sideband7;
	uint32_t sideband8;
	uint32_t sideband9;
	uint32_t sideband10;
	uint32_t sideband11;
	uint32_t sideband12;
	uint32_t sideband13;
	uint32_t sideband14;
	uint32_t sideband15;
	uint32_t dramsts;
	uint32_t dbgdone;
	uint32_t dbgsignals;
	uint32_t dbgreset;
	uint32_t dbgmatch;
	uint32_t counter0mask;
	uint32_t counter1mask;
	uint32_t counter0match;
	uint32_t counter1match;
	uint32_t niosreserve0;
	uint32_t niosreserve1;
	uint32_t niosreserve2;
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

#endif /* __ASSEMBLY__ */
#define IRQ_ECC_SERR				34
#define IRQ_ECC_DERR				32

#define ALT_ECC_HMC_OCP_DDRIOCTRL_IO_SIZE_MSK			0x00000003

#define ALT_ECC_HMC_OCP_INTSTAT_SERRPENA_SET_MSK		0x00000001
#define ALT_ECC_HMC_OCP_INTSTAT_DERRPENA_SET_MSK		0x00000002
#define ALT_ECC_HMC_OCP_ERRINTEN_SERRINTEN_SET_MSK		0x00000001
#define ALT_ECC_HMC_OCP_ERRINTEN_DERRINTEN_SET_MSK		0x00000002
#define ALT_ECC_HMC_OCP_INTMOD_INTONCMP_SET_MSK			0x00010000
#define ALT_ECC_HMC_OCP_INTMOD_SERR_SET_MSK			0x00000001
#define ALT_ECC_HMC_OCP_INTMOD_EXT_ADDRPARITY_SET_MSK		0x00000100
#define ALT_ECC_HMC_OCP_ECCCTL_AWB_CNT_RST_SET_MSK		0x00010000
#define ALT_ECC_HMC_OCP_ECCCTL_CNT_RST_SET_MSK			0x00000100
#define ALT_ECC_HMC_OCP_ECCCTL_ECC_EN_SET_MSK			0x00000001
#define ALT_ECC_HMC_OCP_ECCCTL2_RMW_EN_SET_MSK			0x00000100
#define ALT_ECC_HMC_OCP_ECCCTL2_AWB_EN_SET_MSK			0x00000001
#define ALT_ECC_HMC_OCP_ERRINTEN_SERR_SET_MSK			0x00000001
#define ALT_ECC_HMC_OCP_ERRINTEN_DERR_SET_MSK			0x00000002
#define ALT_ECC_HMC_OCP_ERRINTEN_HMI_SET_MSK			0x00000004
#define ALT_ECC_HMC_OCP_INTSTAT_SERR_MSK			0x00000001
#define ALT_ECC_HMC_OCP_INTSTAT_DERR_MSK			0x00000002
#define ALT_ECC_HMC_OCP_INTSTAT_HMI_MSK				0x00000004
#define ALT_ECC_HMC_OCP_INTSTAT_ADDRMTCFLG_MSK			0x00010000
#define ALT_ECC_HMC_OCP_INTSTAT_ADDRPARFLG_MSK			0x00020000
#define ALT_ECC_HMC_OCP_INTSTAT_DERRBUSFLG_MSK			0x00040000

#define ALT_ECC_HMC_OCP_SERRCNTREG_VALUE				8

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
void sdram_mmr_init(void);
void sdram_firewall_setup(void);
int is_sdram_cal_success(void);
int ddr_calibration_sequence(void);
int is_sdram_ecc_enabled(void);
void irq_handler_DDR_ecc_serr(void);
void irq_handler_DDR_ecc_derr(void);
void db_err_enable_interrupt(unsigned enable);
void sb_err_enable_interrupt(unsigned enable);
void ext_addrparity_err_enable_interrupt(unsigned enable);
#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_SDRAM_H_ */

























































