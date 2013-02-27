/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/freeze_controller.h>
#include <asm/arch/debug_memory.h>

#define SYSMGR_FRZCTRL_LOOP_PARAM       (1000)
#define SYSMGR_FRZCTRL_DELAY_LOOP_PARAM (10)
#define SYSMGR_FRZCTRL_INTOSC_33	(33)
#define SYSMGR_FRZCTRL_INTOSC_1000	(1000)

typedef enum {
	FREEZE_CTRL_FROZEN = 0,
	FREEZE_CTRL_THAWED = 1
} FREEZE_CTRL_CHAN_STATE;

/*
 * Default state from cold reset is FREEZE_ALL; the global
 * flag is set to TRUE to indicate the IO banks are frozen
 */
static uint32_t frzctrl_channel_freeze[FREEZE_CHANNEL_NUM]
	= { FREEZE_CTRL_FROZEN, FREEZE_CTRL_FROZEN,
	FREEZE_CTRL_FROZEN, FREEZE_CTRL_FROZEN};

/*
 * sys_mgr_frzctrl_freeze_req
 * Freeze HPS IOs
 */
uint32_t
sys_mgr_frzctrl_freeze_req(
	FreezeChannelSelect channel_id,
	FreezeControllerFSMSelect fsm_select)
{
	uint32_t frzctrl_ioctrl_reg_offset;
	uint32_t frzctrl_reg_value;
	uint32_t frzctrl_reg_cfg_mask;
	uint32_t i;

	if (FREEZE_CONTROLLER_FSM_SW == fsm_select) {

		DEBUG_MEMORY
		/* select software FSM */
		writel(SYSMGR_FRZCTRL_SRC_VIO1_ENUM_SW,
			(SOCFPGA_SYSMGR_ADDRESS + SYSMGR_FRZCTRL_SRC_ADDRESS));

		/* Freeze channel ID checking and base address */
		switch (channel_id) {
		case FREEZE_CHANNEL_0:
		case FREEZE_CHANNEL_1:
		case FREEZE_CHANNEL_2:
			DEBUG_MEMORY
			frzctrl_ioctrl_reg_offset =
				SYSMGR_FRZCTRL_VIOCTRL_ADDRESS +
				(channel_id << SYSMGR_FRZCTRL_VIOCTRL_SHIFT);

			/*
			 * Assert active low enrnsl, plniotri
			 * and niotri signals
			 */
			frzctrl_reg_cfg_mask =
				SYSMGR_FRZCTRL_VIOCTRL_SLEW_MASK
				| SYSMGR_FRZCTRL_VIOCTRL_WKPULLUP_MASK
				| SYSMGR_FRZCTRL_VIOCTRL_TRISTATE_MASK;

			clrbits_le32((SOCFPGA_SYSMGR_ADDRESS+
				frzctrl_ioctrl_reg_offset),
				frzctrl_reg_cfg_mask);

			/*
			 * Note: Delay for 20ns at min
			 * Assert active low bhniotri signal and de-assert
			 * active high csrdone
			 */
			frzctrl_reg_cfg_mask
				= SYSMGR_FRZCTRL_VIOCTRL_BUSHOLD_MASK
				| SYSMGR_FRZCTRL_VIOCTRL_CFG_MASK;
			clrbits_le32((SOCFPGA_SYSMGR_ADDRESS+
				frzctrl_ioctrl_reg_offset),
				frzctrl_reg_cfg_mask);

			/* Set global flag to indicate channel is frozen */
			frzctrl_channel_freeze[channel_id] =
				FREEZE_CTRL_FROZEN;

			break;

		case FREEZE_CHANNEL_3:
			DEBUG_MEMORY
			/*
			 * Assert active low enrnsl, plniotri and
			 * niotri signals
			 */
			frzctrl_reg_cfg_mask
				= SYSMGR_FRZCTRL_HIOCTRL_SLEW_MASK
				| SYSMGR_FRZCTRL_HIOCTRL_WKPULLUP_MASK
				| SYSMGR_FRZCTRL_HIOCTRL_TRISTATE_MASK;
			clrbits_le32((SOCFPGA_SYSMGR_ADDRESS+
				SYSMGR_FRZCTRL_HIOCTRL_ADDRESS),
				frzctrl_reg_cfg_mask);

			/*
			 * Note: Delay for 40ns at min
			 * assert active low bhniotri & nfrzdrv signals,
			 * de-assert active high csrdone and assert
			 * active high frzreg and nfrzdrv signals
			 */
			frzctrl_reg_value = readl(SOCFPGA_SYSMGR_ADDRESS +
				SYSMGR_FRZCTRL_HIOCTRL_ADDRESS);
			frzctrl_reg_cfg_mask
				= SYSMGR_FRZCTRL_HIOCTRL_BUSHOLD_MASK
				| SYSMGR_FRZCTRL_HIOCTRL_CFG_MASK;
			frzctrl_reg_value
				= (frzctrl_reg_value & ~frzctrl_reg_cfg_mask)
				| SYSMGR_FRZCTRL_HIOCTRL_REGRST_MASK
				| SYSMGR_FRZCTRL_HIOCTRL_OCTRST_MASK;
			writel(frzctrl_reg_value,
				(SOCFPGA_SYSMGR_ADDRESS +
				SYSMGR_FRZCTRL_HIOCTRL_ADDRESS));

			/*
			 * Note: Delay for 40ns at min
			 * assert active high reinit signal and de-assert
			 * active high pllbiasen signals
			 */
			frzctrl_reg_value = readl(SOCFPGA_SYSMGR_ADDRESS+
				SYSMGR_FRZCTRL_HIOCTRL_ADDRESS);
			frzctrl_reg_value
				= (frzctrl_reg_value &
				~SYSMGR_FRZCTRL_HIOCTRL_OCT_CFGEN_CALSTART_MASK)
				| SYSMGR_FRZCTRL_HIOCTRL_DLLRST_MASK;
			writel(frzctrl_reg_value,
				(SOCFPGA_SYSMGR_ADDRESS +
				SYSMGR_FRZCTRL_HIOCTRL_ADDRESS));

			/* Set global flag to indicate channel is frozen */
			frzctrl_channel_freeze[channel_id] =
				FREEZE_CTRL_FROZEN;

			break;

		default:
			return 1;
		}
	} else if (FREEZE_CONTROLLER_FSM_HW == fsm_select) {

		DEBUG_MEMORY
		/* select hardware FSM */
		writel(SYSMGR_FRZCTRL_SRC_VIO1_ENUM_HW,
			(SOCFPGA_SYSMGR_ADDRESS + SYSMGR_FRZCTRL_SRC_ADDRESS));

		/* write to hwctrl reg to enable freeze req */
		setbits_le32((SOCFPGA_SYSMGR_ADDRESS+
			SYSMGR_FRZCTRL_HWCTRL_ADDRESS),
			SYSMGR_FRZCTRL_HWCTRL_VIO1REQ_MASK);

		i = 0;
		while (SYSMGR_FRZCTRL_HWCTRL_VIO1STATE_ENUM_FROZEN
			!= sys_mgr_frzctrl_frzchn1_state_get()) {

			i++;

			if (SYSMGR_FRZCTRL_LOOP_PARAM < i)
				return 1;
		} /* while ( not frozen) */

		/* Set global flag to indicate channel is frozen */
		DEBUG_MEMORY
		frzctrl_channel_freeze[channel_id] = FREEZE_CTRL_FROZEN;
	} else {
		return 1;
	} /* if-else (fsm_select) */

	return 0;
}

/*
 * sys_mgr_frzctrl_thaw_req
 * Unfreeze/Thaw HPS IOs
 */
uint32_t
sys_mgr_frzctrl_thaw_req(
	FreezeChannelSelect channel_id,
	FreezeControllerFSMSelect fsm_select)
{
	uint32_t frzctrl_ioctrl_reg_offset;
	uint32_t frzctrl_reg_cfg_mask;
	uint32_t frzctrl_reg_value;
	uint32_t i;
	uint32_t start_count, clk_cycles_count;

	if (FREEZE_CONTROLLER_FSM_SW == fsm_select) {
		DEBUG_MEMORY
		/* select software FSM */
		writel(SYSMGR_FRZCTRL_SRC_VIO1_ENUM_SW,
			(SOCFPGA_SYSMGR_ADDRESS +
			SYSMGR_FRZCTRL_SRC_ADDRESS));

	/* Freeze channel ID checking and base address */
	switch (channel_id) {
	case FREEZE_CHANNEL_0:
	case FREEZE_CHANNEL_1:
	case FREEZE_CHANNEL_2:
		DEBUG_MEMORY
		frzctrl_ioctrl_reg_offset
			= SYSMGR_FRZCTRL_VIOCTRL_ADDRESS
			+ (channel_id << SYSMGR_FRZCTRL_VIOCTRL_SHIFT);

		/*
		 * Assert active low bhniotri signal and
		 * de-assert active high csrdone
		 */
		frzctrl_reg_cfg_mask
			= SYSMGR_FRZCTRL_VIOCTRL_BUSHOLD_MASK
			| SYSMGR_FRZCTRL_VIOCTRL_CFG_MASK;
		setbits_le32((SOCFPGA_SYSMGR_ADDRESS+
			frzctrl_ioctrl_reg_offset),
			frzctrl_reg_cfg_mask);

		/*
		 * Note: Delay for 20ns at min
		 * de-assert active low plniotri and niotri signals
		 */
		frzctrl_reg_cfg_mask
			= SYSMGR_FRZCTRL_VIOCTRL_WKPULLUP_MASK
			| SYSMGR_FRZCTRL_VIOCTRL_TRISTATE_MASK;
		setbits_le32((SOCFPGA_SYSMGR_ADDRESS+
			frzctrl_ioctrl_reg_offset),
			frzctrl_reg_cfg_mask);

		/*
		 * Note: Delay for 20ns at min
		 * de-assert active low enrnsl signal
		 */
		setbits_le32((SOCFPGA_SYSMGR_ADDRESS+
			frzctrl_ioctrl_reg_offset),
			SYSMGR_FRZCTRL_VIOCTRL_SLEW_MASK);

		/* Set global flag to indicate channel is thawed */
		frzctrl_channel_freeze[channel_id] = FREEZE_CTRL_THAWED;

		break;

	case FREEZE_CHANNEL_3:
		DEBUG_MEMORY
		/* de-assert active high reinit signal */
		clrbits_le32((SOCFPGA_SYSMGR_ADDRESS+
			SYSMGR_FRZCTRL_HIOCTRL_ADDRESS),
			SYSMGR_FRZCTRL_HIOCTRL_DLLRST_MASK);

		/*
		 * Note: Delay for 40ns at min
		 * assert active high pllbiasen signals
		 */
		setbits_le32((SOCFPGA_SYSMGR_ADDRESS+
			SYSMGR_FRZCTRL_HIOCTRL_ADDRESS),
			SYSMGR_FRZCTRL_HIOCTRL_OCT_CFGEN_CALSTART_MASK);

		/* Delay 1000 intosc. intosc is based on eosc1 */
		reset_timer_count();
		start_count = get_timer_count(0);

		do {
			clk_cycles_count =
				get_timer_count(start_count);
		} while (clk_cycles_count <
			SYSMGR_FRZCTRL_INTOSC_1000);

		/*
		 * de-assert active low bhniotri signals,
		 * assert active high csrdone and nfrzdrv signal
		 */
		frzctrl_reg_value = readl (SOCFPGA_SYSMGR_ADDRESS +
					SYSMGR_FRZCTRL_HIOCTRL_ADDRESS);
		frzctrl_reg_value = (frzctrl_reg_value
			| SYSMGR_FRZCTRL_HIOCTRL_BUSHOLD_MASK
			| SYSMGR_FRZCTRL_HIOCTRL_CFG_MASK)
			& ~SYSMGR_FRZCTRL_HIOCTRL_OCTRST_MASK;
		writel(frzctrl_reg_value,
			(SOCFPGA_SYSMGR_ADDRESS +
			SYSMGR_FRZCTRL_HIOCTRL_ADDRESS));

		/* Delay 33 intosc */
		reset_timer_count();
		start_count = get_timer_count(0);

		do {
			clk_cycles_count =
				get_timer_count(start_count);
		} while (clk_cycles_count <
			SYSMGR_FRZCTRL_INTOSC_33);

		/* de-assert active low plniotri and niotri signals */
		frzctrl_reg_cfg_mask
			= SYSMGR_FRZCTRL_HIOCTRL_WKPULLUP_MASK
			| SYSMGR_FRZCTRL_HIOCTRL_TRISTATE_MASK;

		setbits_le32((SOCFPGA_SYSMGR_ADDRESS+
			SYSMGR_FRZCTRL_HIOCTRL_ADDRESS),
			frzctrl_reg_cfg_mask);

		/*
		 * Note: Delay for 40ns at min
		 * de-assert active high frzreg signal
		 */
		clrbits_le32((SOCFPGA_SYSMGR_ADDRESS+
			SYSMGR_FRZCTRL_HIOCTRL_ADDRESS),
			SYSMGR_FRZCTRL_HIOCTRL_REGRST_MASK);

		/*
		 * Note: Delay for 40ns at min
		 * de-assert active low enrnsl signal
		 */
		setbits_le32((SOCFPGA_SYSMGR_ADDRESS +
			SYSMGR_FRZCTRL_HIOCTRL_ADDRESS),
			SYSMGR_FRZCTRL_HIOCTRL_SLEW_MASK);

		/* Set global flag to indicate channel is thawed */
		frzctrl_channel_freeze[channel_id] = FREEZE_CTRL_THAWED;

		break;

	default:
		return 1;
	}

	} else if (FREEZE_CONTROLLER_FSM_HW == fsm_select) {

		DEBUG_MEMORY
		/* select hardware FSM */
		writel(SYSMGR_FRZCTRL_SRC_VIO1_ENUM_HW,
			(SOCFPGA_SYSMGR_ADDRESS + SYSMGR_FRZCTRL_SRC_ADDRESS));

		/* write to hwctrl reg to enable thaw req; 0: thaw */
		clrbits_le32((SOCFPGA_SYSMGR_ADDRESS +
			SYSMGR_FRZCTRL_HWCTRL_ADDRESS),
			SYSMGR_FRZCTRL_HWCTRL_VIO1REQ_MASK);

		i = 0;
		while (SYSMGR_FRZCTRL_HWCTRL_VIO1STATE_ENUM_THAWED
			!= sys_mgr_frzctrl_frzchn1_state_get()) {

			i++;

			if (SYSMGR_FRZCTRL_LOOP_PARAM < i)
				return 1;
		} /* while (not thaw) */

		/* Set global flag to indicate channel is thawed */
		frzctrl_channel_freeze[channel_id] = FREEZE_CTRL_THAWED;
	} else {

		return 1;
	} /* if-else (fsm_select) */

	return 0;
}

/*
 * Get current state of freeze channel
 * channel_id Freeze channel ID with range of enum
 *            FreezeChannelEnum
 * return bool Current freeze channel status, TRUE if frozen, 0 otherwise
 */
uint32_t
sys_mgr_frzctrl_frzchn_is_frozen(FreezeChannelSelect channel_id)
{
	if (FREEZE_CTRL_FROZEN == frzctrl_channel_freeze[channel_id])
		return 1;
	else
		return 0;
}
