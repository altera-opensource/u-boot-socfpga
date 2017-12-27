/*
 * Copyright (C) 2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <altera.h>
#include <asm/arch/mailbox_s10.h>

#define RECONFIG_STATUS_POLL_RESP_TIMEOUT_MS		60000
#define RECONFIG_STATUS_INTERVAL_DELAY_US		1000000

#define RECONFIG_STATUS_RESPONSE_LEN			6

#define RECONFIG_STATUS_STATE				0
#define RECONFIG_STATUS_PIN_STATUS			2
#define RECONFIG_STATUS_SOFTFUNC_STATUS			3

#define MBOX_CFGSTAT_STATE_IDLE				0x00000000
#define MBOX_CFGSTAT_STATE_CONFIG			0x10000000
#define MBOX_CFGSTAT_STATE_FAILACK			0x08000000
#define MBOX_CFGSTAT_STATE_ERROR_INVALID		0xf0000001
#define MBOX_CFGSTAT_STATE_ERROR_CORRUPT		0xf0000002
#define MBOX_CFGSTAT_STATE_ERROR_AUTH			0xf0000003
#define MBOX_CFGSTAT_STATE_ERROR_CORE_IO		0xf0000004
#define MBOX_CFGSTAT_STATE_ERROR_HARDWARE		0xf0000005
#define MBOX_CFGSTAT_STATE_ERROR_FAKE			0xf0000006
#define MBOX_CFGSTAT_STATE_ERROR_BOOT_INFO		0xf0000007
#define MBOX_CFGSTAT_STATE_ERROR_QSPI_ERROR		0xf0000008

#define RCF_PIN_STATUS_NSTATUS				BIT(31)

#define RCF_SOFTFUNC_STATUS_CONF_DONE			BIT(0)
#define RCF_SOFTFUNC_STATUS_INIT_DONE			BIT(1)
#define RCF_SOFTFUNC_STATUS_SEU_ERROR			BIT(3)

static const struct mbox_cfgstat_state {
	int			err_no;
	const char		*error_name;
} mbox_cfgstat_state[] = {
	{MBOX_CFGSTAT_STATE_IDLE, "FPGA in idle mode."},
	{MBOX_CFGSTAT_STATE_CONFIG, "FPGA in config mode."},
	{MBOX_CFGSTAT_STATE_FAILACK, "Acknowledgement failed!"},
	{MBOX_CFGSTAT_STATE_ERROR_INVALID, "Invalid bitstream!"},
	{MBOX_CFGSTAT_STATE_ERROR_CORRUPT, "Corrupted bitstream!"},
	{MBOX_CFGSTAT_STATE_ERROR_AUTH, "Authentication failed!"},
	{MBOX_CFGSTAT_STATE_ERROR_CORE_IO, "I/O error!"},
	{MBOX_CFGSTAT_STATE_ERROR_HARDWARE, "Hardware error!"},
	{MBOX_CFGSTAT_STATE_ERROR_FAKE, "Fake error!"},
	{MBOX_CFGSTAT_STATE_ERROR_BOOT_INFO, "Error in boot info!"},
	{MBOX_CFGSTAT_STATE_ERROR_QSPI_ERROR, "Error in QSPI!"},
	{-ETIMEDOUT, "I/O timeout error"},
	{-1, "Unknown error!"}
};

#define MBOX_CFGSTAT_MAX \
	  (sizeof(mbox_cfgstat_state) / sizeof(struct mbox_cfgstat_state))

static const char *mbox_cfgstat_to_str(int err)
{
	int i;

	for (i = 0; i < MBOX_CFGSTAT_MAX - 1; i++) {
		if (mbox_cfgstat_state[i].err_no == err)
			return mbox_cfgstat_state[i].error_name;
	}

	return mbox_cfgstat_state[MBOX_CFGSTAT_MAX - 1].error_name;
}

static void inc_cmd_id(u8 *id)
{
	u8 val = *id + 1;

	if (val > 15)
		val = 1;

	*id = val;
}

/*
 * Polling the FPGA configuration status.
 * Return 0 for success, non-zero for error.
 */
static int reconfig_status_polling_resp(void)
{
	u32 reconfig_status_resp_len;
	u32 reconfig_status_resp[RECONFIG_STATUS_RESPONSE_LEN];
	int ret;
	unsigned long start = get_timer(0);

	while (1) {
		reconfig_status_resp_len = RECONFIG_STATUS_RESPONSE_LEN;
		ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_RECONFIG_STATUS,
				    MBOX_CMD_DIRECT, 0, NULL, 0,
				    &reconfig_status_resp_len,
				    reconfig_status_resp);

		if (ret) {
			error("Failure in reconfig status mailbox command!\n");
			return ret;
		}

		/* Check for any error */
		ret = reconfig_status_resp[RECONFIG_STATUS_STATE];
		if (ret && ret != MBOX_CFGSTAT_STATE_CONFIG)
			return ret;

		/* Make sure nStatus is not 0 */
		ret = reconfig_status_resp[RECONFIG_STATUS_PIN_STATUS];
		if (!(ret & RCF_PIN_STATUS_NSTATUS))
			return MBOX_CFGSTAT_STATE_ERROR_HARDWARE;

		ret = reconfig_status_resp[RECONFIG_STATUS_SOFTFUNC_STATUS];
		if (ret & RCF_SOFTFUNC_STATUS_SEU_ERROR)
			return MBOX_CFGSTAT_STATE_ERROR_HARDWARE;

		if ((ret & RCF_SOFTFUNC_STATUS_CONF_DONE) &&
		    (ret & RCF_SOFTFUNC_STATUS_INIT_DONE) &&
		    !reconfig_status_resp[RECONFIG_STATUS_STATE])
			return 0;	/* configuration success */

		if (get_timer(start) > RECONFIG_STATUS_POLL_RESP_TIMEOUT_MS)
			break;	/* time out */

		puts(".");
		udelay(RECONFIG_STATUS_INTERVAL_DELAY_US);
	}

	return -ETIMEDOUT;
}

static u32 get_resp_hdr(u32 *r_index, u32 *w_index, u32 *resp_count,
			u32 *resp_buf, u32 buf_size, u32 client_id)
{
	u32 i;
	u32 mbox_hdr;
	u32 resp_len;
	u32 hdr_len;
	u32 buf[MBOX_RESP_BUFFER_SIZE];

	if (*resp_count < buf_size) {
		u32 rcv_len_max = buf_size - *resp_count;

		if (rcv_len_max > MBOX_RESP_BUFFER_SIZE)
			rcv_len_max = MBOX_RESP_BUFFER_SIZE;
		resp_len = mbox_rcv_resp(buf, rcv_len_max);

		for (i = 0; i < resp_len; i++) {
			resp_buf[(*w_index)++] = buf[i];
			*w_index %= buf_size;
			(*resp_count)++;
		}
	}

	/* No response in buffer */
	if (*resp_count == 0)
		return 0;

	mbox_hdr = resp_buf[*r_index];

	hdr_len = MBOX_RESP_LEN_GET(mbox_hdr);

	/* Insufficient header length to return a mailbox header */
	if ((*resp_count - 1) < hdr_len)
		return 0;

	*r_index += (hdr_len + 1);
	*r_index %= buf_size;
	*resp_count -= (hdr_len + 1);

	/* Make sure response belongs to us */
	if (MBOX_RESP_CLIENT_GET(mbox_hdr) != client_id)
		return 0;

	return mbox_hdr;
}

static int send_reconfig_data(const void *rbf_data, size_t rbf_size,
			      u32 xfer_max, u32 buf_size_max)
{
	u32 resp_rindex = 0;
	u32 resp_windex = 0;
	u32 resp_count = 0;
	u32 response_buffer[MBOX_RESP_BUFFER_SIZE];
	u8 resp_err = 0;
	u8 cmd_id = 1;
	u32 xfer_count = 0;
	u32 xfer_pending[MBOX_RESP_BUFFER_SIZE];
	u32 args[3];
	int ret = 0;
	u32 i;

	debug("SDM xfer_max = %d\n", xfer_max);
	debug("SDM buf_size_max = %x\n\n", buf_size_max);

	for (i = 0; i < MBOX_RESP_BUFFER_SIZE; i++)
		xfer_pending[i] = 0;

	while (rbf_size || xfer_count) {
		if (!resp_err && rbf_size && xfer_count < xfer_max) {
			args[0] = (1 << 8);
			args[1] = (u32)rbf_data;
			if (rbf_size >= buf_size_max) {
				args[2] = buf_size_max;
				rbf_size -= buf_size_max;
				rbf_data += buf_size_max;
			} else {
				args[2] = (u32)rbf_size;
				rbf_size = 0;
			}

			debug("Sending MBOX_RECONFIG_DATA...\n");

			ret = mbox_send_cmd_only(cmd_id, MBOX_RECONFIG_DATA,
				MBOX_CMD_INDIRECT, 3, args);
			if (ret) {
				resp_err = 1;
			} else {
				xfer_count++;
				for (i = 0; i < MBOX_RESP_BUFFER_SIZE; i++) {
					if (!xfer_pending[i]) {
						xfer_pending[i] = cmd_id;
						inc_cmd_id(&cmd_id);
						break;
					}
				}
				debug("+xfer_count = %d\n", xfer_count);
				debug("xfer ID = %d\n", xfer_pending[i]);
				debug("data offset = %08x\n", args[1]);
				debug("data size = %08x\n", args[2]);
			}
#ifndef DEBUG
			puts(".");
#endif
		} else {
			u32 r_id = 0;
			u32 resp_hdr = get_resp_hdr(&resp_rindex, &resp_windex,
						    &resp_count,
						    response_buffer,
						    MBOX_RESP_BUFFER_SIZE,
						    MBOX_CLIENT_ID_UBOOT);

			/* If no valid response header found */
			if (!resp_hdr)
				continue;

			/* Expect 0 length from RECONFIG_DATA */
			if (MBOX_RESP_LEN_GET(resp_hdr))
				continue;

			/* Check for response's status */
			if (!resp_err) {
				ret = MBOX_RESP_ERR_GET(resp_hdr);
				debug("Response error code: %08x\n", ret);
				/* Error in response */
				if (ret)
					resp_err = 1;
			}

			/* Read the response header's ID */
			r_id = MBOX_RESP_ID_GET(resp_hdr);
			for (i = 0; i < MBOX_RESP_BUFFER_SIZE; i++) {
				if (r_id == xfer_pending[i]) {
					/* Reclaim ID */
					cmd_id = (u32)xfer_pending[i];
					xfer_pending[i] = 0;
					xfer_count--;
					break;
				}
			}

			debug("Reclaimed xfer ID = %d\n", cmd_id);
			debug("-xfer_count = %d\n", xfer_count);
			if (resp_err && !xfer_count)
				return ret;
		}
	}

	return 0;
}

/*
 * This is the interface used by FPGA driver.
 * Return 0 for success, non-zero for error.
 */
int stratix10_load(Altera_desc *desc, const void *rbf_data, size_t rbf_size)
{
	int ret;
	u32 resp_len = 2;
	u32 resp_buf[2];

	debug("Sending MBOX_RECONFIG...\n");
	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_RECONFIG, MBOX_CMD_DIRECT, 0,
			    NULL, 0, &resp_len, resp_buf);
	if (ret) {
		error("Failure in RECONFIG mailbox command!\n");
		return ret;
	}

	ret = send_reconfig_data(rbf_data, rbf_size, resp_buf[0], resp_buf[1]);
	if (ret) {
		error("RECONFIG_DATA error: %08x, %s\n", ret,
		      mbox_cfgstat_to_str(ret));
		return ret;
	}

	/* Make sure we don't send MBOX_RECONFIG_STATUS too fast */
	udelay(RECONFIG_STATUS_INTERVAL_DELAY_US);

	debug("\nPolling with MBOX_RECONFIG_STATUS...\n");
	ret = reconfig_status_polling_resp();
	if (ret) {
		error("%s\n", mbox_cfgstat_to_str(ret));
		return ret;
	}

	puts("\nFPGA reconfiguration OK!\n");

	return ret;
}
