/*
 * Copyright (C) 2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <altera.h>
#include <asm/arch/mailbox_s10.h>

#define RECONFIG_STATUS_POLL_RESP_TIMEOUT		60000 /* ms */
#define RECONFIG_STATUS_INTERVAL_DELAY			2000000 /* 2 seconds */

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
	{-1, "Uknown error!"}
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
		ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_RECONFIG_STATUS, 0,
				      NULL, 0, &reconfig_status_resp_len,
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

		if (get_timer(start) > RECONFIG_STATUS_POLL_RESP_TIMEOUT)
			break;	/* time out */

		puts(".");
		udelay(RECONFIG_STATUS_INTERVAL_DELAY);
	}

	return -ETIMEDOUT;
}

/*
 * This is the interface used by FPGA driver.
 * Return 0 for success, non-zero for error.
 */
int stratix10_load(Altera_desc *desc, const void *rbf_data, size_t rbf_size)
{
	u32 reconfig_msel_args[2];
	int ret;

	ret = mbox_qspi_close();
	if (ret) {
		error("Failed to release QSPI!\n");
		return ret;
	}

	reconfig_msel_args[0] = (u32)rbf_data;
	reconfig_msel_args[1] = (u32)rbf_size;

	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_RECONFIG_MSEL, 2,
			      reconfig_msel_args, 0, 0, NULL);
	if (ret) {
		error("FPGA reconfiguration failed!\n");
		goto getqspi;
	}

	/* Make sure we don't send MBOX_RECONFIG_STATUS too fast */
	udelay(RECONFIG_STATUS_INTERVAL_DELAY);

	ret = reconfig_status_polling_resp();
	if (ret) {
		error("%s\n", mbox_cfgstat_to_str(ret));
		goto getqspi;
	}

	puts("\nFPGA reconfiguration OK!\n");

getqspi:
	ret = mbox_qspi_open();
	if (ret)
		error("Warning: Unable to reclaim QSPI!\n");

	return ret;
}
