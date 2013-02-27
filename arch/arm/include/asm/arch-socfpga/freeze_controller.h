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

#ifndef _FREEZE_CONTROLLER_H_
#define _FREEZE_CONTROLLER_H_


/* Freeze controller register mapping */
/*
Register        Offset  RegisterField  Corresponding Freeze Controller port
vioctrl[0]      0x20    cfg		iochannel0_csrdone
			bushold		iochannel0_bhniotri_n
			tristate	iochannel0_niotri_n
			wkpullup	iochannel0_plniotri_n
			slew		iochannel0_enrnsl_n
vioctrl[1]      0x24    cfg		iochannel1_csrdone
			bushold		iochannel1_bhniotri_n
			tristate	iochannel1_niotri_n
			wkpullup	iochannel1_plniotri_n
			slew		iochannel1_enrnsl_n
vioctrl[2]      0x28    cfg		iochannel2_csrdone
			bushold		iochannel2_bhniotri_n
			tristate	iochannel2_niotri_n
			wkpullup	iochannel2_plniotri_n
			slew		iochannel2_enrnsl_n
hioctrl         0x30    cfg		iochannel3_csrdone
			bushold		iochannel3_bhniotri_n
			tristate	iochannel3_niotri_n
			wkpullup	iochannel3_plniotri_n
			slew		iochannel3_enrnsl_n
			dllrst		iochannel3_reinit
			octrst		iochannel3_nfrzdrv_n
			regrst		iochannel3_frzreg
			oct_cfgen_calstart	iochannel3_pllbiasen
src             0x34    vio1		iochannel1_src_sel
hwctrl          0x38    vio1req		iochannel1_frz_thaw_request
			vio1state	{iochannel1_frz_thaw_state_1,
					iochannel1_frz_thaw_state_0}
*/

#define SYSMGR_FRZCTRL_ADDRESS 0x40
#define SYSMGR_FRZCTRL_VIOCTRL_ADDRESS 0x40
#define SYSMGR_FRZCTRL_HIOCTRL_ADDRESS 0x50
#define SYSMGR_FRZCTRL_SRC_ADDRESS 0x54
#define SYSMGR_FRZCTRL_HWCTRL_ADDRESS 0x58
#define SYSMGR_FRZCTRL_SRC_VIO1_ENUM_SW 0x0
#define SYSMGR_FRZCTRL_SRC_VIO1_ENUM_HW 0x1
#define SYSMGR_FRZCTRL_VIOCTRL_SLEW_MASK 0x00000010
#define SYSMGR_FRZCTRL_VIOCTRL_WKPULLUP_MASK 0x00000008
#define SYSMGR_FRZCTRL_VIOCTRL_TRISTATE_MASK 0x00000004
#define SYSMGR_FRZCTRL_VIOCTRL_BUSHOLD_MASK 0x00000002
#define SYSMGR_FRZCTRL_VIOCTRL_CFG_MASK 0x00000001
#define SYSMGR_FRZCTRL_HIOCTRL_SLEW_MASK 0x00000010
#define SYSMGR_FRZCTRL_HIOCTRL_WKPULLUP_MASK 0x00000008
#define SYSMGR_FRZCTRL_HIOCTRL_TRISTATE_MASK 0x00000004
#define SYSMGR_FRZCTRL_HIOCTRL_BUSHOLD_MASK 0x00000002
#define SYSMGR_FRZCTRL_HIOCTRL_CFG_MASK 0x00000001
#define SYSMGR_FRZCTRL_HIOCTRL_REGRST_MASK 0x00000080
#define SYSMGR_FRZCTRL_HIOCTRL_OCTRST_MASK 0x00000040
#define SYSMGR_FRZCTRL_HIOCTRL_OCT_CFGEN_CALSTART_MASK 0x00000100
#define SYSMGR_FRZCTRL_HIOCTRL_DLLRST_MASK 0x00000020
#define SYSMGR_FRZCTRL_HWCTRL_VIO1REQ_MASK 0x00000001
#define SYSMGR_FRZCTRL_HWCTRL_VIO1STATE_ENUM_FROZEN 0x2
#define SYSMGR_FRZCTRL_HWCTRL_VIO1STATE_ENUM_THAWED 0x1

#define SYSMGR_FRZCTRL_HWCTRL_VIO1STATE_GET(x) \
(((x) & 0x00000006) >> 1)



/*
 * FreezeChannelSelect
 * Definition of enum for freeze channel
 */
typedef enum  {
	FREEZE_CHANNEL_0 = 0,   /* EMAC_IO & MIXED2_IO */
	FREEZE_CHANNEL_1,   /* MIXED1_IO and FLASH_IO */
	FREEZE_CHANNEL_2,   /* General IO */
	FREEZE_CHANNEL_3,   /* DDR IO */
	FREEZE_CHANNEL_UNDEFINED
} FreezeChannelSelect;

/* Maximum number of freeze channel */
#define FREEZE_CHANNEL_NUM  (4)

/*
 * FreezeControllerFSMSelect
 * Definition of enum for freeze controller state machine
 */
typedef enum {
	FREEZE_CONTROLLER_FSM_SW = 0,
	FREEZE_CONTROLLER_FSM_HW,
	FREEZE_CONTROLLER_FSM_UNDEFINED
} FreezeControllerFSMSelect;

/* Shift count needed to calculte for FRZCTRL VIO control register offset */
#define SYSMGR_FRZCTRL_VIOCTRL_SHIFT    (2)

/*
 * Freeze HPS IOs
 *
 * FreezeChannelSelect [in] - Freeze channel ID
 * FreezeControllerFSMSelect [in] - To use hardware or software state machine
 * If FREEZE_CONTROLLER_FSM_HW is selected for FSM select then the
 *       the freeze channel id is input is ignored. It is default to channel 1
 */
extern uint32_t
sys_mgr_frzctrl_freeze_req(
	FreezeChannelSelect channel_id,
	FreezeControllerFSMSelect fsm_select);

/*
 * Unfreeze/Thaw HPS IOs
 *
 * FreezeChannelSelect [in] - Freeze channel ID
 * FreezeControllerFSMSelect [in] - To use hardware or software state machine
 * If FREEZE_CONTROLLER_FSM_HW is selected for FSM select then the
 *       the freeze channel id is input is ignored. It is default to channel 1
 */
extern uint32_t
sys_mgr_frzctrl_thaw_req(
	FreezeChannelSelect channel_id,
	FreezeControllerFSMSelect fsm_select);

/*
 * Get current state of freeze channel
 *
 * FreezeChannelSelect [in] - Freeze channel ID
 * return uint32_t [out] - Current freeze channel status, TRUE if frozen,
 *         0 otherwise
 */
extern uint32_t
sys_mgr_frzctrl_frzchn_is_frozen(FreezeChannelSelect channel_id);


/*
 * Get current state of freeze channel 1 (VIO)
 * return uint32_t Current freeze channel 1 status
 */
static inline uint32_t sys_mgr_frzctrl_frzchn1_state_get(void)
{
	uint32_t frzchn1_state;

	frzchn1_state = readl(SOCFPGA_SYSMGR_ADDRESS +
		SYSMGR_FRZCTRL_HWCTRL_ADDRESS);

	frzchn1_state = SYSMGR_FRZCTRL_HWCTRL_VIO1STATE_GET(frzchn1_state);

	return frzchn1_state;
}

#endif	/* _FREEZE_CONTROLLER_H_ */
