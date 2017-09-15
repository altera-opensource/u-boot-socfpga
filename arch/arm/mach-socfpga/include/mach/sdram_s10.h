/*
 * Copyright (C) 2016-2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef	_SDRAM_S10_H_
#define	_SDRAM_S10_H_

unsigned long sdram_calculate_size(void);
int sdram_mmr_init_full(unsigned int sdr_phy_reg);
int sdram_calibration_full(void);

#define DDR_TWR				15
#define DDR_READ_LATENCY_DELAY		40
#define DDR_ACTIVATE_FAWBANK		0x1


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
	uint32_t _pad_0x64_0xdf[31];
	uint32_t dramaddrwidth;
	uint32_t _pad_0xe4_0xff[7];
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
	uint32_t ecc_errgenhaddr_0;
	uint32_t ecc_errgenhaddr_1;
	uint32_t ecc_errgenhaddr_2;
	uint32_t ecc_errgenhaddr_3;
	uint32_t ecc_rdeccdata2regbus_beat0;
	uint32_t ecc_rdeccdata2regbus_beat1;
	uint32_t ecc_rdeccdata2regbus_beat2;
	uint32_t ecc_rdeccdata2regbus_beat3;
	uint32_t _pad_0x1a0_0x1af[4];
	uint32_t derrhaddr;
	uint32_t serrhaddr;
	uint32_t _pad_0x1b8_0x1bb;
	uint32_t autowb_corrhaddr;
	uint32_t _pad_0x1c0_0x20f[20];
	uint32_t hpsintfcsel;
	uint32_t rsthandshakectrl;
	uint32_t rsthandshakestat;
};

struct socfpga_noc_ddr_scheduler {
	uint32_t main_scheduler_id_coreid;
	uint32_t main_scheduler_id_revisionid;
	uint32_t main_scheduler_ddrconf;
	uint32_t main_scheduler_ddrtiming;
	uint32_t main_scheduler_ddrmode;
	uint32_t main_scheduler_readlatency;
	uint32_t _pad_0x18_0x37[8];
	uint32_t main_scheduler_activate;
	uint32_t main_scheduler_devtodev;
	uint32_t main_scheduler_ddr4timing;
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

union dramtiming0_reg {
	struct {
		u32 cfg_tcl:6;
		u32 reserved:8;  /* Other fields unused */
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

#define DDR_SCHED_DDRTIMING_ACTTOACT_OFFSET	0
#define DDR_SCHED_DDRTIMING_RDTOMISS_OFFSET	6
#define DDR_SCHED_DDRTIMING_WRTOMISS_OFFSET	12
#define DDR_SCHED_DDRTIMING_BURSTLEN_OFFSET	18
#define DDR_SCHED_DDRTIMING_RDTOWR_OFFSET	21
#define DDR_SCHED_DDRTIMING_WRTORD_OFFSET	26
#define DDR_SCHED_DDRTIMING_BWRATIO_OFFSET	31
#define DDR_SCHED_DDRMOD_BWRATIOEXTENDED_OFFSET	1
#define DDR_SCHED_ACTIVATE_RRD_OFFSET		0
#define DDR_SCHED_ACTIVATE_FAW_OFFSET		4
#define DDR_SCHED_ACTIVATE_FAWBANK_OFFSET	10
#define DDR_SCHED_DEVTODEV_BUSRDTORD_OFFSET	0
#define DDR_SCHED_DEVTODEV_BUSRDTOWR_OFFSET	2
#define DDR_SCHED_DEVTODEV_BUSWRTORD_OFFSET	4
#define DDR_HMC_DDRIOCTRL_IOSIZE_MSK		0x00000003
#define DDR_HMC_DDRCALSTAT_CAL_MSK		0x00000001
#define DDR_HMC_ECCCTL_AWB_CNT_RST_SET_MSK	0x00010000
#define DDR_HMC_ECCCTL_CNT_RST_SET_MSK		0x00000100
#define DDR_HMC_ECCCTL_ECC_EN_SET_MSK		0x00000001
#define DDR_HMC_ECCCTL2_RMW_EN_SET_MSK		0x00000100
#define DDR_HMC_ECCCTL2_AWB_EN_SET_MSK		0x00000001
#define DDR_HMC_RSTHANDSHAKE_MASK		0x000000ff
#define DDR_HMC_CORE2SEQ_INT_REQ		0xF
#define DDR_HMC_SEQ2CORE_INT_RESP_MASK		0x8
#define DDR_HMC_HPSINTFCSEL_ENABLE_MASK		0x001f1f1f

#define CCU_CPU0_MPRT_ADBASE_DDRREG_ADDR	0xf7004400
#define CCU_CPU0_MPRT_ADBASE_MEMSPACE0_ADDR	0xf70045c0
#define CCU_CPU0_MPRT_ADBASE_MEMSPACE1A_ADDR	0xf70045e0
#define CCU_CPU0_MPRT_ADBASE_MEMSPACE1B_ADDR	0xf7004600
#define CCU_CPU0_MPRT_ADBASE_MEMSPACE1C_ADDR	0xf7004620
#define CCU_CPU0_MPRT_ADBASE_MEMSPACE1D_ADDR	0xf7004640
#define CCU_CPU0_MPRT_ADBASE_MEMSPACE1E_ADDR	0xf7004660

#define CCU_IOM_MPRT_ADBASE_MEMSPACE0_ADDR	0xf7018560
#define CCU_IOM_MPRT_ADBASE_MEMSPACE1A_ADDR	0xf7018580
#define CCU_IOM_MPRT_ADBASE_MEMSPACE1B_ADDR	0xf70185a0
#define CCU_IOM_MPRT_ADBASE_MEMSPACE1C_ADDR	0xf70185c0
#define CCU_IOM_MPRT_ADBASE_MEMSPACE1D_ADDR	0xf70185e0
#define CCU_IOM_MPRT_ADBASE_MEMSPACE1E_ADDR	0xf7018600

#define CCU_ADBASE_DI_MASK			0x00000010

#endif /* _SDRAM_S10_H_ */
