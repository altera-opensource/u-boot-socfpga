// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022-2023 Intel Corporation <www.intel.com>
 *
 */

#define DEBUG
#include <common.h>
#include <hang.h>
#include <wait_bit.h>
#include <asm/io.h>
#include "iossm_mailbox.h"

#define ECC_INTSTATUS_SERR SOCFPGA_SYSMGR_ADDRESS + 0x9C
#define ECC_INISTATUS_DERR SOCFPGA_SYSMGR_ADDRESS + 0xA0
#define DDR_CSR_CLKGEN_LOCKED_IO96B0_MASK BIT(16)
#define DDR_CSR_CLKGEN_LOCKED_IO96B1_MASK BIT(17)

/* supported DDR type list */
static const char *ddr_type_list[7] = {
		"DDR4", "DDR5", "DDR5_RDIMM", "LPDDR4", "LPDDR5", "QDRIV", "UNKNOWN"
};

static int is_ddr_csr_clkgen_locked(u32 clkgen_mask)
{
	int ret;

	ret = wait_for_bit_le32((const void *)(ECC_INTSTATUS_SERR)
				, clkgen_mask, true, TIMEOUT, false);

	if (ret) {
		debug("%s: ddr csr clkgena locked is timeout\n", __func__);
		return ret;
	}

	ret = wait_for_bit_le32((const void *)(ECC_INISTATUS_DERR)
				, clkgen_mask, true, TIMEOUT, false);

	if (ret) {
		debug("%s: ddr csr clkgenb locked is timeout\n", __func__);
		return ret;
	}

	return 0;
}

/* Mailbox request function
 * This function will send the request to IOSSM mailbox and wait for response return
 *
 * @io96b_csr_addr: CSR address for the target IO96B
 * @ip_type:	    IP type for the specified memory interface
 * @instance_id:    IP instance ID for the specified memory interface
 * @usr_cmd_type:   User desire IOSSM mailbox command type
 * @usr_cmd_opcode: User desire IOSSM mailbox command opcode
 * @cmd_param_*:    Parameters (if applicable) for the requested IOSSM mailbox command
 * @resp_data_len:  User desire extra response data fields other than
 *					CMD_RESPONSE_DATA_SHORT field on CMD_RESPONSE_STATUS
 * @resp:			Structure contain responses returned from the requested IOSSM
 *					mailbox command
 */
int io96b_mb_req(phys_addr_t io96b_csr_addr, u32 ip_type, u32 instance_id
					, u32 usr_cmd_type, u32 usr_cmd_opcode, u32 cmd_param_0
					, u32 cmd_param_1, u32 cmd_param_2, u32 cmd_param_3
					, u32 cmd_param_4, u32 cmd_param_5, u32 cmd_param_6
					, u32 resp_data_len, struct io96b_mb_resp *resp)
{
	int i;
	int ret;
	u32 cmd_req, cmd_resp;

	/* Initialized zeros for responses*/
	resp->cmd_resp_status = 0;
	resp->cmd_resp_data_0 = 0;
	resp->cmd_resp_data_1 = 0;
	resp->cmd_resp_data_2 = 0;

	/* Ensure CMD_REQ is cleared before write any command request */
	ret = wait_for_bit_le32((const void *)(io96b_csr_addr + IOSSM_CMD_REQ_OFFSET)
				, GENMASK(31, 0), 0, TIMEOUT, false);

	if (ret) {
		printf("%s: CMD_REQ not ready\n", __func__);
		return -1;
	}

	/* Write CMD_PARAM_* */
	for (i = 0; i < 6 ; i++) {
		switch (i) {
		case 0:
			if (cmd_param_0)
				writel(cmd_param_0, io96b_csr_addr + IOSSM_CMD_PARAM_0_OFFSET);
			break;
		case 1:
			if (cmd_param_1)
				writel(cmd_param_1, io96b_csr_addr + IOSSM_CMD_PARAM_1_OFFSET);
			break;
		case 2:
			if (cmd_param_2)
				writel(cmd_param_2, io96b_csr_addr + IOSSM_CMD_PARAM_2_OFFSET);
			break;
		case 3:
			if (cmd_param_3)
				writel(cmd_param_3, io96b_csr_addr + IOSSM_CMD_PARAM_3_OFFSET);
			break;
		case 4:
			if (cmd_param_4)
				writel(cmd_param_4, io96b_csr_addr + IOSSM_CMD_PARAM_4_OFFSET);
			break;
		case 5:
			if (cmd_param_5)
				writel(cmd_param_5, io96b_csr_addr + IOSSM_CMD_PARAM_5_OFFSET);
			break;
		case 6:
			if (cmd_param_6)
				writel(cmd_param_6, io96b_csr_addr + IOSSM_CMD_PARAM_6_OFFSET);
			break;
		default:
			printf("%s: Invalid command parameter\n", __func__);
		}
	}

	/* Write CMD_REQ (IP_TYPE, IP_INSTANCE_ID, CMD_TYPE and CMD_OPCODE) */
	cmd_req = (usr_cmd_opcode << 0) | (usr_cmd_type << 16) | (instance_id << 24) |
				(ip_type << 29);
	writel(cmd_req, io96b_csr_addr + IOSSM_CMD_REQ_OFFSET);
	debug("%s: Write 0x%x to IOSSM_CMD_REQ_OFFSET 0x%llx\n", __func__, cmd_req
		, io96b_csr_addr + IOSSM_CMD_REQ_OFFSET);

	/* Read CMD_RESPONSE_READY in CMD_RESPONSE_STATUS*/
	ret = wait_for_bit_le32((const void *)(io96b_csr_addr +
			IOSSM_CMD_RESPONSE_STATUS_OFFSET), IOSSM_STATUS_COMMAND_RESPONSE_READY, 1,
			TIMEOUT, false);

	if (ret) {
		printf("%s: CMD_RESPONSE ERROR:\n", __func__);
		cmd_resp = readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET);
		printf("%s: STATUS_GENERAL_ERROR: 0x%x\n", __func__, (cmd_resp >> 1) & 0xF);
		printf("%s: STATUS_CMD_RESPONSE_ERROR: 0x%x\n", __func__, (cmd_resp >> 5) & 0x7);
	}

	/* read CMD_RESPONSE_STATUS*/
	resp->cmd_resp_status = readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET);
	debug("%s: CMD_RESPONSE_STATUS 0x%llx: 0x%x\n", __func__, io96b_csr_addr +
		IOSSM_CMD_RESPONSE_STATUS_OFFSET, resp->cmd_resp_status);

	/* read CMD_RESPONSE_DATA_* */
	for (i = 0; i < resp_data_len; i++) {
		switch (i) {
		case 0:
			resp->cmd_resp_data_0 =
					readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_0_OFFSET);
			debug("%s: IOSSM_CMD_RESPONSE_DATA_0_OFFSET 0x%llx: 0x%x\n", __func__
				, io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_0_OFFSET,
				resp->cmd_resp_data_0);
			break;
		case 1:
			resp->cmd_resp_data_1 =
					readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_1_OFFSET);
			debug("%s: IOSSM_CMD_RESPONSE_DATA_1_OFFSET 0x%llx: 0x%x\n", __func__
				, io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_1_OFFSET,
				resp->cmd_resp_data_1);
			break;
		case 2:
			resp->cmd_resp_data_2 =
					readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_2_OFFSET);
			debug("%s: IOSSM_CMD_RESPONSE_DATA_2_OFFSET 0x%llx: 0x%x\n", __func__
				, io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_2_OFFSET,
				resp->cmd_resp_data_2);
			break;
		default:
			printf("%s: Invalid response data\n", __func__);
		}
	}

	resp->cmd_resp_status = readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET);
	debug("%s: CMD_RESPONSE_STATUS 0x%llx: 0x%x\n", __func__, io96b_csr_addr +
		IOSSM_CMD_RESPONSE_STATUS_OFFSET, resp->cmd_resp_status);

	/* write CMD_RESPONSE_READY = 0 */
	clrbits_le32((u32 *)(uintptr_t)(io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET)
					, IOSSM_STATUS_COMMAND_RESPONSE_READY);

	resp->cmd_resp_status = readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET);
	debug("%s: CMD_RESPONSE_READY 0x%llx: 0x%x\n", __func__, io96b_csr_addr +
		IOSSM_CMD_RESPONSE_STATUS_OFFSET, resp->cmd_resp_status);

	return 0;
}

/*
 * Initial function to be called to set memory interface IP type and instance ID
 * IP type and instance ID need to be determined before sending mailbox command
 */
void io96b_mb_init(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_resp usr_resp;
	u8 ip_type_ret, instance_id_ret;
	int i, j, k;

	debug("%s: num_instance %d\n", __func__, io96b_ctrl->num_instance);
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		debug("%s: get memory interface IO96B %d\n", __func__, i);
		switch (i) {
		case 0:
			/* Get memory interface IP type & instance ID (IP identifier) */
			io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr, 0, 0
					, CMD_GET_SYS_INFO, GET_MEM_INTF_INFO
					, 0, 0, 0, 0, 0, 0, 0, 2, &usr_resp);
			debug("%s: get response from memory interface IO96B %d\n", __func__, i);
			/* Retrieve number of memory interface(s) */
			io96b_ctrl->io96b_0.mb_ctrl.num_mem_interface =
				IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status) & 0x3;

			/* Retrieve memory interface IP type and instance ID (IP identifier) */
			j = 0;
			for (k = 0; k < MAX_MEM_INTERFACES_SUPPORTED; k++) {
				switch (k) {
				case 0:
					ip_type_ret = (usr_resp.cmd_resp_data_0 >> 29) & 0x7;
					instance_id_ret = (usr_resp.cmd_resp_data_0 >> 24) & 0x1F;
					break;
				case 1:
					ip_type_ret = (usr_resp.cmd_resp_data_1 >> 29) & 0x7;
					instance_id_ret = (usr_resp.cmd_resp_data_1 >> 24) & 0x1F;
					break;
				}

				if (ip_type_ret) {
					io96b_ctrl->io96b_0.mb_ctrl.ip_type[j] = ip_type_ret;
					io96b_ctrl->io96b_0.mb_ctrl.ip_instance_id[j] =
						instance_id_ret;
					j++;
				}
			}
			break;
		case 1:
			/* Get memory interface IP type and instance ID (IP identifier) */
			io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr, 0, 0, CMD_GET_SYS_INFO
					, GET_MEM_INTF_INFO, 0, 0, 0, 0, 0, 0, 0, 2, &usr_resp);
			debug("%s: get response from memory interface IO96B %d\n", __func__, i);
			/* Retrieve number of memory interface(s) */
			io96b_ctrl->io96b_1.mb_ctrl.num_mem_interface =
				IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status) & 0x3;
			debug("%s: IO96B %d: num_mem_interface: 0x%x\n", __func__, i
				, io96b_ctrl->io96b_1.mb_ctrl.num_mem_interface);

			/* Retrieve memory interface IP type and instance ID (IP identifier) */
			j = 0;
			for (k = 0; k < MAX_MEM_INTERFACES_SUPPORTED; k++) {
				switch (k) {
				case 0:
					ip_type_ret = (usr_resp.cmd_resp_data_0 >> 29) & 0x7;
					instance_id_ret = (usr_resp.cmd_resp_data_0 >> 24) & 0x1F;
					break;
				case 1:
					ip_type_ret = (usr_resp.cmd_resp_data_1 >> 29) & 0x7;
					instance_id_ret = (usr_resp.cmd_resp_data_1 >> 24) & 0x1F;
					break;
				}

				if (ip_type_ret) {
					io96b_ctrl->io96b_1.mb_ctrl.ip_type[j] = ip_type_ret;
					io96b_ctrl->io96b_1.mb_ctrl.ip_instance_id[j] =
						instance_id_ret;
					j++;
				}
			}
			break;
		}
		debug("%s: IO96B %d: ip_type_ret: 0x%x\n", __func__, i, ip_type_ret);
		debug("%s: IO96B %d: instance_id_ret: 0x%x\n", __func__, i, instance_id_ret);
	}
}

int io96b_cal_status(phys_addr_t addr)
{
	int ret;
	u32 cal_success, cal_fail;
	phys_addr_t status_addr = addr + IOSSM_STATUS_OFFSET;
	/* Ensure calibration completed */
	ret = wait_for_bit_le32((const void *)status_addr, IOSSM_STATUS_CAL_BUSY, false
							, TIMEOUT, false);
	if (ret) {
		printf("%s: SDRAM calibration IO96b instance 0x%llx timeout\n", __func__
			, status_addr);
		hang();
	}

	/* Calibration status */
	cal_success = readl(status_addr) & IOSSM_STATUS_CAL_SUCCESS;
	cal_fail = readl(status_addr) & IOSSM_STATUS_CAL_FAIL;

	if (cal_success && !cal_fail)
		return 0;
	else
		return -EPERM;
}

void init_mem_cal(struct io96b_info *io96b_ctrl)
{
	int count, i, ret;

	/* Initialize overall calibration status */
	io96b_ctrl->overall_cal_status = false;

	/* Check initial calibration status for the assigned IO96B*/
	count = 0;
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		switch (i) {
		case 0:
			ret = is_ddr_csr_clkgen_locked(DDR_CSR_CLKGEN_LOCKED_IO96B0_MASK);
			if (ret) {
				printf("%s: ckgena_lock iossm IO96B_0 is not locked\n", __func__);
				hang();
			}
			ret = io96b_cal_status(io96b_ctrl->io96b_0.io96b_csr_addr);
			if (ret) {
				io96b_ctrl->io96b_0.cal_status = false;
				printf("%s: Initial DDR calibration IO96B_0 failed %d\n", __func__
						, ret);
				break;
			}
			io96b_ctrl->io96b_0.cal_status = true;
			printf("%s: Initial DDR calibration IO96B_0 succeed\n", __func__);
			count++;
			break;
		case 1:
			ret = is_ddr_csr_clkgen_locked(DDR_CSR_CLKGEN_LOCKED_IO96B1_MASK);
			if (ret) {
				printf("%s: ckgena_lock iossm IO96B_1 is not locked\n", __func__);
				hang();
			}
			ret = io96b_cal_status(io96b_ctrl->io96b_1.io96b_csr_addr);
			if (ret) {
				io96b_ctrl->io96b_1.cal_status = false;
				printf("%s: Initial DDR calibration IO96B_1 failed %d\n", __func__
						, ret);
				break;
			}
			io96b_ctrl->io96b_1.cal_status = true;
			printf("%s: Initial DDR calibration IO96B_1 succeed\n", __func__);
			count++;
			break;
		}
	}

	if (count == io96b_ctrl->num_instance)
		io96b_ctrl->overall_cal_status = true;
}

/*
 * Trying 3 times re-calibration if initial calibration failed
 */
int trig_mem_cal(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_resp usr_resp;
	bool recal_success;
	int i;
	u8 cal_stat;

	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		switch (i) {
		case 0:
			if (!(io96b_ctrl->io96b_0.cal_status)) {
				/* Get the memory calibration status for first memory interface */
				io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr, 0, 0
						, CMD_TRIG_MEM_CAL_OP, GET_MEM_CAL_STATUS, 0, 0, 0
						, 0, 0, 0, 0, 2, &usr_resp);

				recal_success = false;

				/* Re-calibration first memory interface with failed calibration */
				for (i = 0; i < 3; i++) {
					cal_stat = usr_resp.cmd_resp_data_0 & GENMASK(2, 0);
					if (cal_stat < 0x2) {
						recal_success = true;
						break;
					}
					io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr
						, io96b_ctrl->io96b_0.mb_ctrl.ip_type[0]
						, io96b_ctrl->io96b_0.mb_ctrl.ip_instance_id[0]
						, CMD_TRIG_MEM_CAL_OP, TRIG_MEM_CAL, 0, 0, 0, 0, 0
						, 0, 0, 2, &usr_resp);

					udelay(1);

					io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr, 0, 0
							, CMD_TRIG_MEM_CAL_OP, GET_MEM_CAL_STATUS
							, 0, 0, 0, 0, 0, 0, 0, 2, &usr_resp);
				}

				if (!recal_success) {
					printf("%s: Error as SDRAM calibration failed\n", __func__);
					hang();
				}

				/* Get the memory calibration status for second memory interface */
				io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr, 0, 0
						, CMD_TRIG_MEM_CAL_OP, GET_MEM_CAL_STATUS, 0, 0, 0
						, 0, 0,	0, 0, 2, &usr_resp);

				recal_success = false;

				/* Re-calibration second memory interface with failed calibration */
				for (i = 0; i < 3; i++) {
					cal_stat = usr_resp.cmd_resp_data_1 & GENMASK(2, 0);
					if (cal_stat < 0x2) {
						recal_success = true;
						break;
					}
					io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr
						, io96b_ctrl->io96b_0.mb_ctrl.ip_type[1]
						, io96b_ctrl->io96b_0.mb_ctrl.ip_instance_id[1]
						, CMD_TRIG_MEM_CAL_OP, TRIG_MEM_CAL, 0, 0, 0, 0, 0
						, 0, 0, 2, &usr_resp);

					udelay(1);

					io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr, 0, 0
							, CMD_TRIG_MEM_CAL_OP, GET_MEM_CAL_STATUS
							, 0, 0, 0, 0, 0, 0, 0, 2, &usr_resp);
				}

				if (!recal_success) {
					printf("%s: Error as SDRAM calibration failed\n", __func__);
					hang();
				}

				io96b_ctrl->io96b_0.cal_status = true;
			}
			break;
		case 1:
			if (!(io96b_ctrl->io96b_1.cal_status)) {
				/* Get the memory calibration status for first memory interface */
				io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr, 0, 0
						, CMD_TRIG_MEM_CAL_OP, GET_MEM_CAL_STATUS, 0, 0, 0
						, 0, 0, 0, 0, 2, &usr_resp);

				recal_success = false;

				/* Re-calibration first memory interface with failed calibration */
				for (i = 0; i < 3; i++) {
					cal_stat = usr_resp.cmd_resp_data_0 & GENMASK(2, 0);
					if (cal_stat < 0x2) {
						recal_success = true;
						break;
					}
					io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr
						, io96b_ctrl->io96b_1.mb_ctrl.ip_type[0]
						, io96b_ctrl->io96b_1.mb_ctrl.ip_instance_id[0]
						, CMD_TRIG_MEM_CAL_OP, TRIG_MEM_CAL, 0, 0, 0, 0, 0
						, 0, 0, 2, &usr_resp);

					udelay(1);

					io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr, 0, 0
							, CMD_TRIG_MEM_CAL_OP, GET_MEM_CAL_STATUS
							, 0, 0, 0, 0, 0, 0, 0, 2, &usr_resp);
				}

				if (!recal_success) {
					printf("%s: Error as SDRAM calibration failed\n", __func__);
					hang();
				}

				/* Get the memory calibration status for second memory interface */
				io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr, 0, 0
						, CMD_TRIG_MEM_CAL_OP, GET_MEM_CAL_STATUS, 0, 0, 0
						, 0, 0,	0, 0, 2, &usr_resp);

				recal_success = false;

				/* Re-calibration second memory interface with failed calibration */
				for (i = 0; i < 3; i++) {
					cal_stat = usr_resp.cmd_resp_data_0 & GENMASK(2, 0);
					if (cal_stat < 0x2) {
						recal_success = true;
						break;
					}
					io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr
						, io96b_ctrl->io96b_1.mb_ctrl.ip_type[1]
						, io96b_ctrl->io96b_1.mb_ctrl.ip_instance_id[1]
						, CMD_TRIG_MEM_CAL_OP, TRIG_MEM_CAL, 0, 0, 0, 0, 0
						, 0, 0, 2, &usr_resp);

					udelay(1);

					io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr, 0, 0
							, CMD_TRIG_MEM_CAL_OP, GET_MEM_CAL_STATUS
							, 0, 0, 0, 0, 0, 0, 0, 2, &usr_resp);
				}

				if (!recal_success) {
					printf("%s: Error as SDRAM calibration failed\n", __func__);
					hang();
				}

				io96b_ctrl->io96b_1.cal_status = true;
			}
			break;
		}
	}

	if (io96b_ctrl->io96b_0.cal_status && io96b_ctrl->io96b_1.cal_status) {
		debug("%s: Overall SDRAM calibration success\n", __func__);
		io96b_ctrl->overall_cal_status = true;
	}

	return 0;
}

int get_mem_technology(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_resp usr_resp;
	int i, j;
	u8 ddr_type_ret;

	/* Initialize ddr type */
	io96b_ctrl->ddr_type = ddr_type_list[6];

	/* Get and ensure all memory interface(s) same DDR type */
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		switch (i) {
		case 0:
			for (j = 0; j < io96b_ctrl->io96b_0.mb_ctrl.num_mem_interface; j++) {
				io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr
						, io96b_ctrl->io96b_0.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_0.mb_ctrl.ip_instance_id[j]
						, CMD_GET_MEM_INFO, GET_MEM_TECHNOLOGY, 0, 0, 0, 0
						, 0, 0, 0, 0, &usr_resp);

				ddr_type_ret =
					IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
					& GENMASK(2, 0);

				if (!strcmp(io96b_ctrl->ddr_type, "UNKNOWN"))
					io96b_ctrl->ddr_type = ddr_type_list[ddr_type_ret];

				if (ddr_type_list[ddr_type_ret] != io96b_ctrl->ddr_type) {
					printf("%s: Mismatch DDR type on IO96B_0\n", __func__);
					return -ENOEXEC;
				}
			}
			break;
		case 1:
			for (j = 0; j < io96b_ctrl->io96b_1.mb_ctrl.num_mem_interface; j++) {
				io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr
						, io96b_ctrl->io96b_1.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_1.mb_ctrl.ip_instance_id[j]
						, CMD_GET_MEM_INFO, GET_MEM_TECHNOLOGY, 0, 0, 0, 0
						, 0, 0, 0, 0, &usr_resp);

				ddr_type_ret =
					IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
					& GENMASK(2, 0);

				if (!strcmp(io96b_ctrl->ddr_type, "UNKNOWN"))
					io96b_ctrl->ddr_type = ddr_type_list[ddr_type_ret];

				if (ddr_type_list[ddr_type_ret] != io96b_ctrl->ddr_type) {
					printf("%s: Mismatch DDR type on IO96B_1\n", __func__);
					return -ENOEXEC;
				}
			}
			break;
		}
	}

	return 0;
}

int get_mem_width_info(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_resp usr_resp;
	int i, j;
	u16 memory_size;
	u16 total_memory_size = 0;

	/* Get all memory interface(s) total memory size on all instance(s) */
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		switch (i) {
		case 0:
			memory_size = 0;
			for (j = 0; j < io96b_ctrl->io96b_0.mb_ctrl.num_mem_interface; j++) {
				io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr
						, io96b_ctrl->io96b_0.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_0.mb_ctrl.ip_instance_id[j]
						, CMD_GET_MEM_INFO, GET_MEM_WIDTH_INFO, 0, 0, 0, 0
						, 0, 0, 0, 2, &usr_resp);

				memory_size = memory_size +
						(usr_resp.cmd_resp_data_1 & GENMASK(7, 0));
			}

			if (!memory_size) {
				printf("%s: Failed to get valid memory size\n", __func__);
				return -ENOEXEC;
			}

			io96b_ctrl->io96b_0.size = memory_size;

			break;
		case 1:
			memory_size = 0;
			for (j = 0; j < io96b_ctrl->io96b_1.mb_ctrl.num_mem_interface; j++) {
				io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr
						, io96b_ctrl->io96b_1.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_1.mb_ctrl.ip_instance_id[j]
						, CMD_GET_MEM_INFO, GET_MEM_WIDTH_INFO, 0, 0, 0, 0
						, 0, 0, 0, 2, &usr_resp);

				memory_size = memory_size +
						(usr_resp.cmd_resp_data_1 & GENMASK(7, 0));
			}

			if (!memory_size) {
				printf("%s: Failed to get valid memory size\n", __func__);
				return -ENOEXEC;
			}

			io96b_ctrl->io96b_1.size = memory_size;

			break;
		}

		total_memory_size = total_memory_size + memory_size;
	}

	if (!total_memory_size) {
		printf("%s: Failed to get valid memory size\n", __func__);
		return -ENOEXEC;
	}

	io96b_ctrl->overall_size = total_memory_size;

	return 0;
}

int ecc_enable_status(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_resp usr_resp;
	int i, j;
	bool ecc_stat_set = false;
	bool ecc_stat;

	/* Initialize ECC status */
	io96b_ctrl->ecc_status = false;

	/* Get and ensure all memory interface(s) same ECC status */
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		switch (i) {
		case 0:
			for (j = 0; j < io96b_ctrl->io96b_0.mb_ctrl.num_mem_interface; j++) {
				debug("%s: ECC_ENABLE_STATUS\n", __func__);
				io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr
						, io96b_ctrl->io96b_0.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_0.mb_ctrl.ip_instance_id[j]
						, CMD_TRIG_CONTROLLER_OP, ECC_ENABLE_STATUS, 0, 0, 0
						, 0, 0, 0, 0, 0, &usr_resp);

				ecc_stat = ((IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
						& GENMASK(1, 0)) == 0 ? false : true);

				if (!ecc_stat_set) {
					io96b_ctrl->ecc_status = ecc_stat;
					ecc_stat_set = true;
				}

				if (ecc_stat != io96b_ctrl->ecc_status) {
					printf("%s: Mismatch DDR ECC status on IO96B_0\n"
						, __func__);
					return -ENOEXEC;
				}
			}
			break;
		case 1:
			for (j = 0; j < io96b_ctrl->io96b_1.mb_ctrl.num_mem_interface; j++) {
				debug("%s: ECC_ENABLE_STATUS\n", __func__);
				io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr
						, io96b_ctrl->io96b_1.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_1.mb_ctrl.ip_instance_id[j]
						, CMD_TRIG_CONTROLLER_OP, ECC_ENABLE_STATUS, 0, 0, 0
						, 0, 0, 0, 0, 0, &usr_resp);

				ecc_stat = ((IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
						& GENMASK(1, 0)) == 0 ? false : true);

				if (!ecc_stat_set) {
					io96b_ctrl->ecc_status = ecc_stat;
					ecc_stat_set = true;
				}

				if (ecc_stat != io96b_ctrl->ecc_status) {
					printf("%s: Mismatch DDR ECC status on IO96B_1\n"
						, __func__);
					return -ENOEXEC;
				}
			}
			break;
		}
	}
	return 0;
}

int bist_mem_init_start(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_resp usr_resp;
	int i, j;
	bool bist_start, bist_success;
	u32 start;

	/* Full memory initialization BIST performed on all memory interface(s) */
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		switch (i) {
		case 0:
			for (j = 0; j < io96b_ctrl->io96b_0.mb_ctrl.num_mem_interface; j++) {
				bist_start = false;
				bist_success = false;

				/* Start memory initialization BIST on full memory address */
				io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr
						, io96b_ctrl->io96b_0.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_0.mb_ctrl.ip_instance_id[j]
						, CMD_TRIG_CONTROLLER_OP, BIST_MEM_INIT_START, 0x40
						, 0, 0, 0, 0, 0, 0, 0, &usr_resp);

				bist_start =
					(IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
					& BIT(0));

				if (!bist_start) {
					printf("%s: Failed to initialized memory on IO96B_0\n"
						, __func__);
					printf("%s: BIST_MEM_INIT_START Error code 0x%x\n", __func__
					, (IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
					& GENMASK(2, 1)) > 0x1);
					return -ENOEXEC;
				}

				/* Polling for the initiated memory initialization BIST status */
				start = get_timer(0);
				while (!bist_success) {
					io96b_mb_req(io96b_ctrl->io96b_0.io96b_csr_addr
						, io96b_ctrl->io96b_0.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_0.mb_ctrl.ip_instance_id[j]
						, CMD_TRIG_CONTROLLER_OP, BIST_MEM_INIT_STATUS, 0
						, 0, 0, 0, 0, 0, 0, 0, &usr_resp);

					bist_success = (IOSSM_CMD_RESPONSE_DATA_SHORT
							(usr_resp.cmd_resp_status) & BIT(0));

					if (!bist_success && (get_timer(start) > TIMEOUT)) {
						printf("%s: Timeout initialize memory on IO96B_0\n"
							, __func__);
						printf("%s: BIST_MEM_INIT_STATUS Error code 0x%x\n"
							, __func__, (IOSSM_CMD_RESPONSE_DATA_SHORT
							(usr_resp.cmd_resp_status)
							& GENMASK(2, 1)) > 0x1);
						return -ETIMEDOUT;
					}

					udelay(1);
				}
			}

			debug("%s: Memory initialized successfully on IO96B_0\n", __func__);
			break;
		case 1:
			for (j = 0; j < io96b_ctrl->io96b_1.mb_ctrl.num_mem_interface; j++) {
				bist_start = false;
				bist_success = false;

				/* Start memory initialization BIST on full memory address */
				io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr
						, io96b_ctrl->io96b_1.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_1.mb_ctrl.ip_instance_id[j]
						, CMD_TRIG_CONTROLLER_OP, BIST_MEM_INIT_START, 0x40
						, 0, 0, 0, 0, 0, 0, 0, &usr_resp);

				bist_start =
					(IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
					& BIT(0));

				if (!bist_start) {
					printf("%s: Failed to initialized memory on IO96B_1\n"
						, __func__);
					printf("%s: BIST_MEM_INIT_START Error code 0x%x\n", __func__
					, (IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
					& GENMASK(2, 1)) > 0x1);
					return -ENOEXEC;
				}

				/* Polling for the initiated memory initialization BIST status */
				start = get_timer(0);
				while (!bist_success) {
					io96b_mb_req(io96b_ctrl->io96b_1.io96b_csr_addr
						, io96b_ctrl->io96b_1.mb_ctrl.ip_type[j]
						, io96b_ctrl->io96b_1.mb_ctrl.ip_instance_id[j]
						, CMD_TRIG_CONTROLLER_OP, BIST_MEM_INIT_STATUS, 0
						, 0, 0, 0, 0, 0, 0, 0, &usr_resp);

					bist_success = (IOSSM_CMD_RESPONSE_DATA_SHORT
							(usr_resp.cmd_resp_status) & BIT(0));

					if (!bist_success && (get_timer(start) > TIMEOUT)) {
						printf("%s: Timeout initialize memory on IO96B_1\n"
							, __func__);
						printf("%s: BIST_MEM_INIT_STATUS Error code 0x%x\n"
							, __func__, (IOSSM_CMD_RESPONSE_DATA_SHORT
							(usr_resp.cmd_resp_status)
							& GENMASK(2, 1)) > 0x1);
						return -ETIMEDOUT;
					}

					udelay(1);
				}
			}

			debug("%s: Memory initialized successfully on IO96B_1\n", __func__);
			break;
		}
	}
	return 0;
}
