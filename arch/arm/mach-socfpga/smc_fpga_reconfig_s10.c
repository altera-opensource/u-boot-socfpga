/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/psci.h>
#include <asm/secure.h>
#include <asm/arch/mailbox_s10.h>
#include <linux/intel-smc.h>

#define FPGA_CONFIG_RESEVED_MEM_START		0x1000
#define FPGA_CONFIG_RESERVED_MEM_SIZE		0x1000000	/* 16 MB */

#define SMC_ARG0		0
#define SMC_ARG1		(SMC_ARG0 + 1)
#define SMC_ARG2		(SMC_ARG1 + 1)
#define SMC_ARG3		(SMC_ARG2 + 1)
#define SMC_RETURN_ARGS_MAX	(SMC_ARG3 + 1)

/* Macro functions for allocation and read/write of
   variables to be assigned to registers */
/* Allocate memory for variable */
#define SMC_ALLOC_REG_MEM(var) unsigned long var[SMC_RETURN_ARGS_MAX]
/* Clear variable */
#define SMC_INIT_REG_MEM(var) \
	do { \
		int i; \
		for (i = 0; i < SMC_RETURN_ARGS_MAX; i++) \
			var[i] = 0; \
	} while (0)
/* Read variable */
#define SMC_GET_REG_MEM(var, i) var[i]
/* Write Variable */
#define SMC_ASSIGN_REG_MEM(var, i, val) \
	do { \
		var[i] = (val); \
	} while (0)
/* Assign variables back to registers */
#define SMC_RET_REG_MEM(var) \
	do { \
		asm volatile("ldr x0, %0\n" \
			     "ldr x1, %1\n" \
			     "ldr x2, %2\n" \
			     "ldr x3, %3\n" \
			     : : "m" (var[0]), "m" (var[1]), \
				 "m" (var[2]), "m" (var[3]) : ); \
	} while (0)

#define FPGA_CONFIG_BUF_MAX			16

#define FPGA_BUF_STAT_IDLE			0
#define FPGA_BUF_STAT_PENDING			1
#define FPGA_BUF_STAT_COMPLETED			2
#define FPGA_BUF_STAT_SUCCESS			3
#define FPGA_BUF_STAT_ERROR			4

#define IS_BUF_FREE(x) (x.state == FPGA_BUF_STAT_IDLE)
#define IS_BUF_PENDING(x) (x.state == FPGA_BUF_STAT_PENDING)
#define IS_BUF_SUBMITTED(x) (x.state >= FPGA_BUF_STAT_PENDING && \
			     x.submit_count > 0)
#define IS_BUF_COMPLETED(x) (x.state == FPGA_BUF_STAT_COMPLETED && \
			     x.submit_count > 0)
#define IS_BUF_FULLY_COMPLETED(x) (x.state == FPGA_BUF_STAT_COMPLETED && \
				   x.submit_count == 0)
#define IS_BUF_SUCCESS(x) (x.state == FPGA_BUF_STAT_SUCCESS)
#define IS_BUF_ERROR(x) (x.state == FPGA_BUF_STAT_ERROR)

static __secure_data struct fpga_buf_list {
	u32			state;
	u32			buf_id;
	u64			buf_addr;
	u64			buf_size;
	u32			buf_off;
	u32			submit_count;
} fpga_buf_list[FPGA_CONFIG_BUF_MAX];

static u8 __secure_data fpga_error = 1;
static u8 __secure_data fpga_buf_id = 1;
static u32 __secure_data fpga_xfer_max = 4;
static u32 __secure_data fpga_buf_read_index;
static u32 __secure_data fpga_buf_write_index;
static u32 __secure_data fpga_buf_count;
/* 20bits DMA size with 8 bytes alignment */
static u32 __secure_data fpga_buf_size_max = 0xFFFF8;
/* Number of data blocks received from OS(EL1) */
static u32 __secure_data fpga_buf_rcv_count;
/* Number of data blocks submitted to SDM */
static u32 __secure_data fpga_xfer_submitted_count;

/* Check for any responses from SDM and update the status in buffer list */
static void __secure reclaim_completed_buf(void)
{
	u32 i, j;
	u32 resp_len;
	u32 buf[MBOX_RESP_BUFFER_SIZE];

	/* If no buffer has been submitted to SDM */
	if (!fpga_xfer_submitted_count)
		return;

	/* Read the SDM responses asynchronously */
	resp_len = mbox_rcv_resp_psci(buf, MBOX_RESP_BUFFER_SIZE);

	for (i = 0; i < resp_len; i++) {
		/* Skip mailbox response headers which are not belong to us */
		if (MBOX_RESP_LEN_GET(buf[i]) ||
		    MBOX_RESP_CLIENT_GET(buf[i]) != MBOX_CLIENT_ID_UBOOT)
			continue;

		for (j = 0; j < FPGA_CONFIG_BUF_MAX; j++) {
			/* Check buffer id */
			if (fpga_buf_list[j].buf_id !=
			    MBOX_RESP_ID_GET(buf[i]))
				continue;

			if (IS_BUF_SUBMITTED(fpga_buf_list[j])) {
				if (fpga_buf_list[j].submit_count)
					fpga_buf_list[j].submit_count--;
				fpga_xfer_submitted_count--;
				/* Error occur in transaction */
				if (MBOX_RESP_ERR_GET(buf[i])) {
					fpga_error = 1;
					fpga_buf_list[j].state =
						FPGA_BUF_STAT_ERROR;
					fpga_buf_list[j].submit_count = 0;
				} else if (IS_BUF_FULLY_COMPLETED(
					   fpga_buf_list[j])) {
					/* Last chunk in buffer and no error */
					fpga_buf_list[j].state =
						FPGA_BUF_STAT_SUCCESS;
				}
				break;
			} else if (IS_BUF_ERROR(fpga_buf_list[j])) {
				fpga_xfer_submitted_count--;
				break;
			}
		}
	}
}

static void __secure do_xfer_buf(void)
{
	u32 i = fpga_buf_read_index;
	u32 args[3];
	int ret;

	/* No buffer found in buffer list or SDM can't handle xfer anymore */
	if (!fpga_buf_rcv_count ||
	    fpga_xfer_submitted_count == fpga_xfer_max)
		return;

	while (fpga_xfer_submitted_count < fpga_xfer_max) {
		if (IS_BUF_FREE(fpga_buf_list[i]) ||
		    IS_BUF_ERROR(fpga_buf_list[i]))
			break;
		if (IS_BUF_PENDING(fpga_buf_list[i])) {
			args[0] = (1 << 8);
			args[1] = (u32)(fpga_buf_list[i].buf_addr +
				fpga_buf_list[i].buf_off);
			if ((fpga_buf_list[i].buf_size -
			    fpga_buf_list[i].buf_off) > fpga_buf_size_max) {
				args[2] = fpga_buf_size_max;
				fpga_buf_list[i].buf_off += fpga_buf_size_max;
			} else {
				args[2] = (u32)(fpga_buf_list[i].buf_size -
					fpga_buf_list[i].buf_off);
				fpga_buf_list[i].state =
					FPGA_BUF_STAT_COMPLETED;
			}

			ret = mbox_send_cmd_only_psci(fpga_buf_list[i].buf_id,
				MBOX_RECONFIG_DATA, MBOX_CMD_INDIRECT, 3,
				args);
			if (ret) {
				fpga_error = 1;
				fpga_buf_list[i].state =
					FPGA_BUF_STAT_ERROR;
				fpga_buf_list[i].submit_count = 0;
				break;
			} else {
				fpga_buf_list[i].submit_count++;
				fpga_xfer_submitted_count++;
			}

			if (fpga_xfer_submitted_count >= fpga_xfer_max)
				break;
		}

		if (IS_BUF_COMPLETED(fpga_buf_list[i]) ||
		    IS_BUF_SUCCESS(fpga_buf_list[i])) {
			i++;
			i %= FPGA_CONFIG_BUF_MAX;
			if (i == fpga_buf_write_index)
				break;
		}
	}
}

static void __secure smc_socfpga_config_get_mem(unsigned long function_id)
{
	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);
	/* Start physical address of reserved memory */
	SMC_ASSIGN_REG_MEM(r, SMC_ARG1, FPGA_CONFIG_RESEVED_MEM_START);
	/* Size of reserved memory */
	SMC_ASSIGN_REG_MEM(r, SMC_ARG2, FPGA_CONFIG_RESERVED_MEM_SIZE -
			   FPGA_CONFIG_RESEVED_MEM_START);

	SMC_RET_REG_MEM(r);
}

static void __secure smc_socfpga_config_start(unsigned long function_id,
				       unsigned long config_type)
{
	SMC_ALLOC_REG_MEM(r);
	int ret, i;
	u32 resp_len = 2;
	u32 resp_buf[2];

	/* Clear any previous pending SDM reponses */
	mbox_rcv_resp_psci(NULL, MBOX_RESP_BUFFER_SIZE);

	SMC_INIT_REG_MEM(r);

	fpga_error = 0;

	ret = mbox_send_cmd_psci(MBOX_ID_UBOOT, MBOX_RECONFIG, MBOX_CMD_DIRECT,
				 0, NULL, 0, &resp_len, resp_buf);
	if (ret) {
		fpga_error = 1;
		goto ret;
	}

	/* Init state in buffer list  PGA_CONFIG_RESERVED_MEM_SIZ*/
	for (i = 0; i < FPGA_CONFIG_BUF_MAX; i++) {
		fpga_buf_list[i].state = FPGA_BUF_STAT_IDLE;
		fpga_buf_list[i].buf_id = 0;
	}

	/* Read maximum transaction allowed by SDM */
	fpga_xfer_max = resp_buf[0];
	/* Read maximum buffer size allowed by SDM */
	fpga_buf_size_max = resp_buf[1];
	fpga_buf_count = 0;
	fpga_buf_rcv_count = 0;
	fpga_xfer_submitted_count = 0;
	fpga_buf_read_index = 0;
	fpga_buf_write_index = 0;
	fpga_buf_id = 1;
ret:
	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_STATUS_OK);

	SMC_RET_REG_MEM(r);
}

static void __secure smc_socfpga_config_write(
				unsigned long function_id,
				unsigned long phys_addr,
				unsigned long phys_size)
{
	SMC_ALLOC_REG_MEM(r);

	SMC_INIT_REG_MEM(r);

	reclaim_completed_buf();

	if (fpga_error) {
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR);
		SMC_ASSIGN_REG_MEM(r, SMC_ARG1,
				   fpga_buf_list[fpga_buf_read_index].
				   buf_addr);
		SMC_ASSIGN_REG_MEM(r, SMC_ARG2,
				   fpga_buf_list[fpga_buf_read_index].
				   buf_size);
		goto ret;
	}

	do_xfer_buf();

	if (fpga_buf_rcv_count == fpga_xfer_max ||
	    (fpga_buf_count == FPGA_CONFIG_BUF_MAX &&
	     fpga_buf_write_index == fpga_buf_read_index)) {
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_FPGA_CONFIG_STATUS_REJECTED);
		goto ret;
	}

	if (!phys_addr || !phys_size) {
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR);
		SMC_ASSIGN_REG_MEM(r, SMC_ARG1, phys_addr);
		SMC_ASSIGN_REG_MEM(r, SMC_ARG2, phys_size);
		goto ret;
	}

	/* Look for free buffer in buffer list */
	if (IS_BUF_FREE(fpga_buf_list[fpga_buf_write_index])) {
		fpga_buf_list[fpga_buf_write_index].state =
			FPGA_BUF_STAT_PENDING;
		fpga_buf_list[fpga_buf_write_index].buf_addr = phys_addr;
		fpga_buf_list[fpga_buf_write_index].buf_size = phys_size;
		fpga_buf_list[fpga_buf_write_index].buf_off = 0;
		fpga_buf_list[fpga_buf_write_index].buf_id = fpga_buf_id++;
		/* Rollover buffer id */
		if (fpga_buf_id > 15)
			fpga_buf_id = 1;
		fpga_buf_count++;
		fpga_buf_write_index++;
		fpga_buf_write_index %= FPGA_CONFIG_BUF_MAX;
		fpga_buf_rcv_count++;
		if (fpga_buf_rcv_count == fpga_xfer_max)
			SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
					INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY);
		else
			SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
					   INTEL_SIP_SMC_STATUS_OK);
		/* Attempt to submit new buffer to SDM */
		do_xfer_buf();
	} else	{
		/* No free buffer avalable in buffer list */
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_FPGA_CONFIG_STATUS_REJECTED);
	}

ret:
	SMC_RET_REG_MEM(r);
}

static void __secure smc_socfpga_config_completed_write(unsigned long function_id)
{
	SMC_ALLOC_REG_MEM(r);
	int i;
	int count = 3, r_index = 1;

	SMC_INIT_REG_MEM(r);

	reclaim_completed_buf();
	do_xfer_buf();

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
			   INTEL_SIP_SMC_STATUS_OK);

	for (i = 0; i < FPGA_CONFIG_BUF_MAX; i++) {
		if (IS_BUF_SUCCESS(fpga_buf_list[fpga_buf_read_index])) {
			SMC_ASSIGN_REG_MEM(r, r_index++,
				fpga_buf_list[fpga_buf_read_index].buf_addr);
			fpga_buf_list[fpga_buf_read_index].state =
				FPGA_BUF_STAT_IDLE;
			fpga_buf_list[fpga_buf_read_index].buf_id = 0;
			fpga_buf_count--;
			fpga_buf_read_index++;
			fpga_buf_read_index %= FPGA_CONFIG_BUF_MAX;
			fpga_buf_rcv_count--;
			count--;
			if (!count)
				break;
		} else if (IS_BUF_ERROR(fpga_buf_list[fpga_buf_read_index]) &&
			   !fpga_buf_list[fpga_buf_read_index].submit_count) {
			SMC_INIT_REG_MEM(r);
			SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR);
			SMC_ASSIGN_REG_MEM(r, SMC_ARG1,
				fpga_buf_list[fpga_buf_read_index].buf_addr);
			SMC_ASSIGN_REG_MEM(r, SMC_ARG2,
				fpga_buf_list[fpga_buf_read_index].buf_size);
			goto ret;
		}
	}

	/* No completed buffers found */
	if (r_index == 1 && fpga_xfer_submitted_count)
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY);

ret:
	SMC_RET_REG_MEM(r);
}

static void __secure smc_socfpga_config_isdone(unsigned long function_id)
{
	SMC_ALLOC_REG_MEM(r);
	u32 reconfig_status_resp_len;
	u32 reconfig_status_resp[RECONFIG_STATUS_RESPONSE_LEN];
	int ret;

	SMC_INIT_REG_MEM(r);

	SMC_ASSIGN_REG_MEM(r, SMC_ARG0, INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY);

	reclaim_completed_buf();
	do_xfer_buf();

	if (fpga_error) {
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR);
		goto ret;
	}

	if (fpga_xfer_submitted_count)
		goto ret;

	reconfig_status_resp_len = RECONFIG_STATUS_RESPONSE_LEN;
	ret = mbox_send_cmd_psci(MBOX_ID_UBOOT, MBOX_RECONFIG_STATUS,
			    MBOX_CMD_DIRECT, 0, NULL, 0,
			    &reconfig_status_resp_len,
			    reconfig_status_resp);

	if (ret) {
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR);
		goto ret;
	}

	/* Check for any error */
	ret = reconfig_status_resp[RECONFIG_STATUS_STATE];
	if (ret && ret != MBOX_CFGSTAT_STATE_CONFIG)
		goto ret;

	/* Make sure nStatus is not 0 */
	ret = reconfig_status_resp[RECONFIG_STATUS_PIN_STATUS];
	if (!(ret & RCF_PIN_STATUS_NSTATUS)) {
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR);
		goto ret;
	}

	ret = reconfig_status_resp[RECONFIG_STATUS_SOFTFUNC_STATUS];
	if (ret & RCF_SOFTFUNC_STATUS_SEU_ERROR) {
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR);
		goto ret;
	}

	if ((ret & RCF_SOFTFUNC_STATUS_CONF_DONE) &&
	    (ret & RCF_SOFTFUNC_STATUS_INIT_DONE) &&
	    !reconfig_status_resp[RECONFIG_STATUS_STATE]) {
		/* FPGA configuration completed successfully */
		SMC_ASSIGN_REG_MEM(r, SMC_ARG0,
				   INTEL_SIP_SMC_STATUS_OK);
		goto ret;
	}

ret:
	if (SMC_GET_REG_MEM(r, SMC_ARG0) ==
	    INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR)
		fpga_error = 1;

	SMC_RET_REG_MEM(r);
}

DECLARE_SECURE_SVC(config_get_mem, INTEL_SIP_SMC_FPGA_CONFIG_GET_MEM,
		   smc_socfpga_config_get_mem);
DECLARE_SECURE_SVC(config_start, INTEL_SIP_SMC_FPGA_CONFIG_START,
		   smc_socfpga_config_start);
DECLARE_SECURE_SVC(config_write, INTEL_SIP_SMC_FPGA_CONFIG_WRITE,
		   smc_socfpga_config_write);
DECLARE_SECURE_SVC(config_completed_write,
		   INTEL_SIP_SMC_FPGA_CONFIG_COMPLETED_WRITE,
		   smc_socfpga_config_completed_write);
DECLARE_SECURE_SVC(config_isdone, INTEL_SIP_SMC_FPGA_CONFIG_ISDONE,
		   smc_socfpga_config_isdone);
