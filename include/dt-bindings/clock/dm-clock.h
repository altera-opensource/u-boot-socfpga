/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020, Intel Corporation
 */

#ifndef __DM_CLOCK_H
#define __DM_CLOCK_H

/* fixed rate clocks */
#define DM_OSC1					0
#define DM_CB_INTOSC_HS_DIV2_CLK		1
#define DM_CB_INTOSC_LS_CLK			2
#define DM_L4_SYS_FREE_CLK			3
#define DM_F2S_FREE_CLK				4

/* PLL clocks */
#define DM_MAIN_PLL_CLK				5
#define DM_MAIN_PLL_C0_CLK			6
#define DM_MAIN_PLL_C1_CLK			7
#define DM_MAIN_PLL_C2_CLK			8
#define DM_MAIN_PLL_C3_CLK			9
#define DM_PERIPH_PLL_CLK			10
#define DM_PERIPH_PLL_C0_CLK			11
#define DM_PERIPH_PLL_C1_CLK			12
#define DM_PERIPH_PLL_C2_CLK			13
#define DM_PERIPH_PLL_C3_CLK			14
#define DM_MPU_FREE_CLK				15
#define DM_MPU_CCU_CLK				16
#define DM_BOOT_CLK				17

/* fixed factor clocks */
#define DM_L3_MAIN_FREE_CLK			18
#define DM_NOC_FREE_CLK				19
#define DM_S2F_USR0_CLK				20
#define DM_NOC_CLK				21
#define DM_EMAC_A_FREE_CLK			22
#define DM_EMAC_B_FREE_CLK			23
#define DM_EMAC_PTP_FREE_CLK			24
#define DM_GPIO_DB_FREE_CLK			25
#define DM_SDMMC_FREE_CLK			26
#define DM_S2F_USER0_FREE_CLK			27
#define DM_S2F_USER1_FREE_CLK			28
#define DM_PSI_REF_FREE_CLK			29

/* Gate clocks */
#define DM_MPU_CLK				30
#define DM_MPU_PERIPH_CLK			31
#define DM_L4_MAIN_CLK				32
#define DM_L4_MP_CLK				33
#define DM_L4_SP_CLK				34
#define DM_CS_AT_CLK				35
#define DM_CS_TRACE_CLK				36
#define DM_CS_PDBG_CLK				37
#define DM_CS_TIMER_CLK				38
#define DM_S2F_USER0_CLK			39
#define DM_EMAC0_CLK				40
#define DM_EMAC1_CLK				41
#define DM_EMAC2_CLK				42
#define DM_EMAC_PTP_CLK				43
#define DM_GPIO_DB_CLK				44
#define DM_NAND_CLK				45
#define DM_PSI_REF_CLK				46
#define DM_S2F_USER1_CLK			47
#define DM_SDMMC_CLK				48
#define DM_SPI_M_CLK				49
#define DM_USB_CLK				50
#define DM_NAND_X_CLK				51
#define DM_NAND_ECC_CLK				52
#define DM_NUM_CLKS				53

#endif	/* __DM_CLOCK_H */
