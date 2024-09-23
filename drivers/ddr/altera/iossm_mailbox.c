// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022-2024 Intel Corporation <www.intel.com>
 */

#define DEBUG
#include <hang.h>
#include <string.h>
#include <wait_bit.h>
#include <asm/arch/base_addr_soc64.h>
#include <asm/io.h>
#include <linux/bitfield.h>
#include "iossm_mailbox.h"

#define ECC_INTSTATUS_SERR SOCFPGA_SYSMGR_ADDRESS + 0x9C
#define ECC_INISTATUS_DERR SOCFPGA_SYSMGR_ADDRESS + 0xA0
#define DDR_CSR_CLKGEN_LOCKED_IO96B0_MASK BIT(16)
#define DDR_CSR_CLKGEN_LOCKED_IO96B1_MASK BIT(17)

#define IO96B_MB_REQ_SETUP(v, w, x, y, z) \
	usr_req.ip_type = v; \
	usr_req.ip_id = w; \
	usr_req.usr_cmd_type = x; \
	usr_req.usr_cmd_opcode = y; \
	usr_req.cmd_param[0] = z; \
	for (n = 1; n < NUM_CMD_PARAM; n++) \
		usr_req.cmd_param[n] = 0
#define MAX_RETRY_COUNT 3
#define NUM_CMD_RESPONSE_DATA 3

#define IO96B0_PLL_A_MASK	BIT(0)
#define IO96B0_PLL_B_MASK	BIT(1)
#define IO96B1_PLL_A_MASK	BIT(2)
#define IO96B1_PLL_B_MASK	BIT(3)

/* supported DDR type list */
static const char *ddr_type_list[7] = {
		"DDR4", "DDR5", "DDR5_RDIMM", "LPDDR4", "LPDDR5", "QDRIV", "UNKNOWN"
};

static int is_ddr_csr_clkgen_locked(u8 io96b_pll)
{
	int ret = 0;
	const char *pll_names[MAX_IO96B_SUPPORTED][2] = {
		{"io96b_0 clkgenA", "io96b_0 clkgenB"},
		{"io96b_1 clkgenA", "io96b_1 clkgenB"}
	};
	u32 masks[MAX_IO96B_SUPPORTED][2] = {
		{IO96B0_PLL_A_MASK, IO96B0_PLL_B_MASK},
		{IO96B1_PLL_A_MASK, IO96B1_PLL_B_MASK}
	};
	u32 lock_masks[MAX_IO96B_SUPPORTED] = {
		DDR_CSR_CLKGEN_LOCKED_IO96B0_MASK,
		DDR_CSR_CLKGEN_LOCKED_IO96B1_MASK
	};

	for (int i = 0; i < MAX_IO96B_SUPPORTED ; i++) {
		/* Check for PLL_A */
		if (io96b_pll & masks[i][0]) {
			ret = wait_for_bit_le32((const void *)(ECC_INTSTATUS_SERR), lock_masks[i],
						true, TIMEOUT, false);

			if (ret) {
				debug("%s: ddr csr %s locked is timeout\n",
				      __func__, pll_names[i][0]);
				goto err;
			} else {
				debug("%s: ddr csr %s is successfully locked\n",
				      __func__, pll_names[i][0]);
			}
		}

		/* Check for PLL_B */
		if (io96b_pll & masks[i][1]) {
			ret = wait_for_bit_le32((const void *)(ECC_INISTATUS_DERR), lock_masks[i],
						true, TIMEOUT, false);

			if (ret) {
				debug("%s: ddr csr %s locked is timeout\n",
				      __func__, pll_names[i][1]);
				goto err;
			} else {
				debug("%s: ddr csr %s is successfully locked\n",
				      __func__, pll_names[i][1]);
			}
		}
	}

err:
	return ret;
}

/*
 * Mailbox request function
 * This function will send the request to IOSSM mailbox and wait for response return
 *
 * @io96b_csr_addr: CSR address for the target IO96B
 * @req:            Structure contain command request for IOSSM mailbox command
 * @resp_data_len:  User desire extra response data fields other than
 *		    CMD_RESPONSE_DATA_SHORT field on CMD_RESPONSE_STATUS
 * @resp:           Structure contain responses returned from the requested IOSSM
 *		    mailbox command
 */
int io96b_mb_req(phys_addr_t io96b_csr_addr, struct io96b_mb_req req,
		 u32 resp_data_len, struct io96b_mb_resp *resp)
{
	int i, ret;
	u32 cmd_req;

	if (!resp) {
		ret = -EINVAL;
		goto err;
	}

	/* Zero initialization for responses */
	resp->cmd_resp_status = 0;

	/* Ensure CMD_REQ is cleared before write any command request */
	ret = wait_for_bit_le32((const void *)(io96b_csr_addr + IOSSM_CMD_REQ_OFFSET),
				GENMASK(31, 0), false, TIMEOUT, false);
	if (ret) {
		printf("%s: Timeout of waiting DDR mailbox ready to be functioned!\n",
		       __func__);
		goto err;
	}

	/* Write CMD_PARAM_* */
	for (i = 0; i < NUM_CMD_PARAM ; i++) {
		switch (i) {
		case 0:
			if (req.cmd_param[0])
				writel(req.cmd_param[0], io96b_csr_addr + IOSSM_CMD_PARAM_0_OFFSET);
			break;
		case 1:
			if (req.cmd_param[1])
				writel(req.cmd_param[1], io96b_csr_addr + IOSSM_CMD_PARAM_1_OFFSET);
			break;
		case 2:
			if (req.cmd_param[2])
				writel(req.cmd_param[2], io96b_csr_addr + IOSSM_CMD_PARAM_2_OFFSET);
			break;
		case 3:
			if (req.cmd_param[3])
				writel(req.cmd_param[3], io96b_csr_addr + IOSSM_CMD_PARAM_3_OFFSET);
			break;
		case 4:
			if (req.cmd_param[4])
				writel(req.cmd_param[4], io96b_csr_addr + IOSSM_CMD_PARAM_4_OFFSET);
			break;
		case 5:
			if (req.cmd_param[5])
				writel(req.cmd_param[5], io96b_csr_addr + IOSSM_CMD_PARAM_5_OFFSET);
			break;
		case 6:
			if (req.cmd_param[6])
				writel(req.cmd_param[6], io96b_csr_addr + IOSSM_CMD_PARAM_6_OFFSET);
			break;
		default:
			printf("%s: Invalid command parameter\n", __func__);
		}
	}

	/* Write CMD_REQ (IP_TYPE, IP_INSTANCE_ID, CMD_TYPE and CMD_OPCODE) */
	cmd_req = FIELD_PREP(CMD_TARGET_IP_TYPE_MASK, req.ip_type) |
		FIELD_PREP(CMD_TARGET_IP_INSTANCE_ID_MASK, req.ip_id) |
		FIELD_PREP(CMD_TYPE_MASK, req.usr_cmd_type) |
		FIELD_PREP(CMD_OPCODE_MASK, req.usr_cmd_opcode);
	writel(cmd_req, io96b_csr_addr + IOSSM_CMD_REQ_OFFSET);

	debug("%s: Write 0x%x to IOSSM_CMD_REQ_OFFSET 0x%llx\n", __func__, cmd_req,
	      io96b_csr_addr + IOSSM_CMD_REQ_OFFSET);

	/* Read CMD_RESPONSE_READY in CMD_RESPONSE_STATUS */
	ret = wait_for_bit_le32((const void *)(io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET),
				IOSSM_STATUS_COMMAND_RESPONSE_READY, true, TIMEOUT, false);

	/* read CMD_RESPONSE_STATUS */
	resp->cmd_resp_status = readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET);

	debug("%s: CMD_RESPONSE_STATUS 0x%llx: 0x%x\n", __func__, io96b_csr_addr +
	      IOSSM_CMD_RESPONSE_STATUS_OFFSET, resp->cmd_resp_status);

	if (ret) {
		printf("%s: CMD_RESPONSE ERROR:\n", __func__);

		printf("%s: STATUS_GENERAL_ERROR: 0x%lx\n", __func__,
		       IOSSM_STATUS_GENERAL_ERROR(resp->cmd_resp_status));
		printf("%s: STATUS_CMD_RESPONSE_ERROR: 0x%lx\n", __func__,
		       IOSSM_STATUS_CMD_RESPONSE_ERROR(resp->cmd_resp_status));
		goto err;
	}

	/* read CMD_RESPONSE_DATA_* */
	for (i = 0; i < resp_data_len; i++) {
		switch (i) {
		case 0:
			resp->cmd_resp_data[i] =
					readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_0_OFFSET);

			debug("%s: IOSSM_CMD_RESPONSE_DATA_0_OFFSET 0x%llx: 0x%x\n", __func__,
			      io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_0_OFFSET,
			      resp->cmd_resp_data[i]);
			break;
		case 1:
			resp->cmd_resp_data[i] =
					readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_1_OFFSET);

			debug("%s: IOSSM_CMD_RESPONSE_DATA_1_OFFSET 0x%llx: 0x%x\n", __func__,
			      io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_1_OFFSET,
			      resp->cmd_resp_data[i]);
			break;
		case 2:
			resp->cmd_resp_data[i] =
					readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_2_OFFSET);

			debug("%s: IOSSM_CMD_RESPONSE_DATA_2_OFFSET 0x%llx: 0x%x\n", __func__,
			      io96b_csr_addr + IOSSM_CMD_RESPONSE_DATA_2_OFFSET,
			      resp->cmd_resp_data[i]);
			break;
		default:
			resp->cmd_resp_data[i] = 0;
			printf("%s: Invalid response data\n", __func__);
		}
	}

	/* write CMD_RESPONSE_READY = 0 */
	clrbits_le32((u32 *)(uintptr_t)(io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET),
		     IOSSM_STATUS_COMMAND_RESPONSE_READY);

	debug("%s: After clear CMD_RESPONSE_READY bit: 0x%llx: 0x%x\n", __func__,
	      io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET,
	      readl(io96b_csr_addr + IOSSM_CMD_RESPONSE_STATUS_OFFSET));

err:
	return ret;
}

/*
 * Initial function to be called to set memory interface IP type and instance ID
 * IP type and instance ID need to be determined before sending mailbox command
 */
void io96b_mb_init(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_req usr_req;
	struct io96b_mb_resp usr_resp;
	u8 ip_type_ret, instance_id_ret;
	int i, j, k, n, ret;

	debug("%s: num_instance %d\n", __func__, io96b_ctrl->num_instance);

	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		debug("%s: get memory interface IO96B %d\n", __func__, i);

		IO96B_MB_REQ_SETUP(0, 0, CMD_GET_SYS_INFO, GET_MEM_INTF_INFO, 0);

		/* Get memory interface IP type and instance ID (IP identifier) */
		ret = io96b_mb_req(io96b_ctrl->io96b[i].io96b_csr_addr, usr_req, 2, &usr_resp);
		if (ret) {
			printf("%s: get memory interface IO96B %d failed\n", __func__, i);
			hang();
		}

		debug("%s: get response from memory interface IO96B %d\n", __func__, i);

		/* Retrieve number of memory interface(s) */
		io96b_ctrl->io96b[i].mb_ctrl.num_mem_interface =
			IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status) & 0x3;

		debug("%s: IO96B %d: num_mem_interface: 0x%x\n", __func__, i,
		      io96b_ctrl->io96b[i].mb_ctrl.num_mem_interface);

		/* Retrieve memory interface IP type and instance ID (IP identifier) */
		j = 0;
		for (k = 0; k < io96b_ctrl->io96b[i].mb_ctrl.num_mem_interface; k++) {
			ip_type_ret = FIELD_GET(INTF_IP_TYPE_MASK, usr_resp.cmd_resp_data[k]);
			instance_id_ret = FIELD_GET(INTF_INSTANCE_ID_MASK,
						    usr_resp.cmd_resp_data[k]);

			if (ip_type_ret) {
				io96b_ctrl->io96b[i].mb_ctrl.ip_type[j] = ip_type_ret;
				io96b_ctrl->io96b[i].mb_ctrl.ip_id[j] = instance_id_ret;

				debug("%s: IO96B %d mem_interface %d: ip_type_ret: 0x%x\n",
				      __func__, i, j, ip_type_ret);
				debug("%s: IO96B %d mem_interface %d: instance_id_ret: 0x%x\n",
				      __func__, i, j, instance_id_ret);

				j++;
			}
		}
	}
}

int io96b_cal_status(phys_addr_t addr)
{
	u32 cal_success, cal_fail;
	phys_addr_t status_addr = addr + IOSSM_STATUS_OFFSET;
	u32 start = get_timer(0);

	do {
		if (get_timer(start) > TIMEOUT_60000MS) {
			printf("%s: SDRAM calibration for IO96B instance 0x%llx timeout!\n",
			       __func__, status_addr);
			hang();
		}

		udelay(1);
		schedule();

		/* Polling until getting any calibration result */
		cal_success = readl(status_addr) & IOSSM_STATUS_CAL_SUCCESS;
		cal_fail = readl(status_addr) & IOSSM_STATUS_CAL_FAIL;
	} while (!cal_success && !cal_fail);

	debug("%s: Calibration for IO96B instance 0x%llx done at %ld msec!\n",
	      __func__,  status_addr, get_timer(start));

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

	if (io96b_ctrl->ckgen_lock) {
		ret = is_ddr_csr_clkgen_locked(io96b_ctrl->io96b_pll);
		if (ret) {
			printf("%s: iossm IO96B ckgena_lock is not locked\n", __func__);
			hang();
		}
	}

	/* Check initial calibration status for the assigned IO96B */
	count = 0;
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		ret = io96b_cal_status(io96b_ctrl->io96b[i].io96b_csr_addr);
		if (ret) {
			io96b_ctrl->io96b[i].cal_status = false;

			printf("%s: Initial DDR calibration IO96B_%d failed %d\n", __func__,
			       i, ret);

			hang();
		}

		io96b_ctrl->io96b[i].cal_status = true;

		printf("%s: Initial DDR calibration IO96B_%d succeed\n", __func__, i);

		count++;
	}

	if (count == io96b_ctrl->num_instance)
		io96b_ctrl->overall_cal_status = true;
}

int get_mem_technology(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_req usr_req;
	struct io96b_mb_resp usr_resp;
	int i, j, n, ret = 0;
	u8 ddr_type_ret;

	/* Initialize ddr type */
	io96b_ctrl->ddr_type = ddr_type_list[6];

	/* Get and ensure all memory interface(s) same DDR type */
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		for (j = 0; j < io96b_ctrl->io96b[i].mb_ctrl.num_mem_interface; j++) {
			IO96B_MB_REQ_SETUP(io96b_ctrl->io96b[i].mb_ctrl.ip_type[j],
					   io96b_ctrl->io96b[i].mb_ctrl.ip_id[j],
					   CMD_GET_MEM_INFO, GET_MEM_TECHNOLOGY, 0);

			ret = io96b_mb_req(io96b_ctrl->io96b[i].io96b_csr_addr, usr_req,
					   0, &usr_resp);

			if (ret)
				goto err;

			ddr_type_ret =
				IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
				& GENMASK(2, 0);

			if (!strcmp(io96b_ctrl->ddr_type, "UNKNOWN"))
				io96b_ctrl->ddr_type = ddr_type_list[ddr_type_ret];

			if (ddr_type_list[ddr_type_ret] != io96b_ctrl->ddr_type) {
				printf("%s: Mismatch DDR type on IO96B_%d\n", __func__, i);

				ret = -EINVAL;
				goto err;
			}
		}
	}

err:
	return ret;
}

int get_mem_width_info(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_req usr_req;
	struct io96b_mb_resp usr_resp;
	int i, j, n, ret = 0;
	u16 memory_size;
	u16 total_memory_size = 0;

	/* Get all memory interface(s) total memory size on all instance(s) */
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		memory_size = 0;
		for (j = 0; j < io96b_ctrl->io96b[i].mb_ctrl.num_mem_interface; j++) {
			IO96B_MB_REQ_SETUP(io96b_ctrl->io96b[i].mb_ctrl.ip_type[j],
					   io96b_ctrl->io96b[i].mb_ctrl.ip_id[j],
					   CMD_GET_MEM_INFO, GET_MEM_WIDTH_INFO, 0);

			ret = io96b_mb_req(io96b_ctrl->io96b[i].io96b_csr_addr, usr_req,
					   2, &usr_resp);

			if (ret)
				goto err;

			memory_size = memory_size +
					(usr_resp.cmd_resp_data[1] & GENMASK(7, 0));
		}

		if (!memory_size) {
			printf("%s: Failed to get valid memory size\n", __func__);
			ret = -EINVAL;
			goto err;
		}

		io96b_ctrl->io96b[i].size = memory_size;

		total_memory_size = total_memory_size + memory_size;
	}

	if (!total_memory_size) {
		printf("%s: Failed to get valid memory size\n", __func__);
		ret = -EINVAL;
	}

	io96b_ctrl->overall_size = total_memory_size;

err:
	return ret;
}

int ecc_enable_status(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_req usr_req;
	struct io96b_mb_resp usr_resp;
	int i, j, n, ret = 0;
	bool ecc_stat_set = false;
	bool ecc_stat;

	/* Initialize ECC status */
	io96b_ctrl->ecc_status = false;

	/* Get and ensure all memory interface(s) same ECC status */
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		for (j = 0; j < io96b_ctrl->io96b[i].mb_ctrl.num_mem_interface; j++) {
			IO96B_MB_REQ_SETUP(io96b_ctrl->io96b[i].mb_ctrl.ip_type[j],
					   io96b_ctrl->io96b[i].mb_ctrl.ip_id[j],
					   CMD_TRIG_CONTROLLER_OP, ECC_ENABLE_STATUS, 0);

			ret = io96b_mb_req(io96b_ctrl->io96b[i].io96b_csr_addr,
					   usr_req, 0, &usr_resp);

			if (ret)
				goto err;

			ecc_stat = (IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
					& GENMASK(1, 0)) == 0 ? false : true;

			if (!ecc_stat_set) {
				io96b_ctrl->ecc_status = ecc_stat;
				ecc_stat_set = true;
			}

			if (ecc_stat != io96b_ctrl->ecc_status) {
				printf("%s: Mismatch DDR ECC status on IO96B_%d\n", __func__, i);

				ret = -EINVAL;
				goto err;
			}
		}
	}

	debug("%s: ECC enable status: %d\n", __func__, io96b_ctrl->ecc_status);

err:
	return ret;
}

int bist_mem_init_start(struct io96b_info *io96b_ctrl)
{
	struct io96b_mb_req usr_req;
	struct io96b_mb_resp usr_resp;
	int i, j, n, ret = 0;
	bool bist_start, bist_success;
	u32 start;

	/* Full memory initialization BIST performed on all memory interface(s) */
	for (i = 0; i < io96b_ctrl->num_instance; i++) {
		for (j = 0; j < io96b_ctrl->io96b[i].mb_ctrl.num_mem_interface; j++) {
			bist_start = false;
			bist_success = false;

			/* Start memory initialization BIST on full memory address */
			IO96B_MB_REQ_SETUP(io96b_ctrl->io96b[i].mb_ctrl.ip_type[j],
					   io96b_ctrl->io96b[i].mb_ctrl.ip_id[j],
					   CMD_TRIG_CONTROLLER_OP, BIST_MEM_INIT_START,
					   BIST_FULL_MEM);

			ret = io96b_mb_req(io96b_ctrl->io96b[i].io96b_csr_addr,
					   usr_req, 0, &usr_resp);
			if (ret)
				goto err;

			bist_start = IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
				& BIT(0);

			if (!bist_start) {
				printf("%s: Failed to initialize memory on IO96B_%d\n", __func__,
				       i);
				printf("%s: BIST_MEM_INIT_START Error code 0x%lx\n", __func__,
				       IOSSM_STATUS_CMD_RESPONSE_ERROR(usr_resp.cmd_resp_status));

				ret = -EINVAL;
				goto err;
			}

			/* Polling for the initiated memory initialization BIST status */
			start = get_timer(0);
			while (!bist_success) {
				IO96B_MB_REQ_SETUP(io96b_ctrl->io96b[i].mb_ctrl.ip_type[j],
						   io96b_ctrl->io96b[i].mb_ctrl.ip_id[j],
						   CMD_TRIG_CONTROLLER_OP, BIST_MEM_INIT_STATUS, 0);

				io96b_mb_req(io96b_ctrl->io96b[i].io96b_csr_addr, usr_req, 0,
					     &usr_resp);

				bist_success =
					IOSSM_CMD_RESPONSE_DATA_SHORT(usr_resp.cmd_resp_status)
					& BIT(0);

				if (!bist_success && (get_timer(start) > TIMEOUT)) {
					printf("%s: Timeout initialize memory on IO96B_%d\n",
					       __func__, i);
					printf("%s: BIST_MEM_INIT_STATUS Error code 0x%lx\n",
					       __func__,
					IOSSM_STATUS_CMD_RESPONSE_ERROR(usr_resp.cmd_resp_status));

					ret = -ETIMEDOUT;
					goto err;
				}

				udelay(1);
			}
		}

		debug("%s: Memory initialized successfully on IO96B_%d\n", __func__, i);
	}

err:
	return ret;
}
