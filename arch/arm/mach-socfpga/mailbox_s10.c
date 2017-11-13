/*
 * Copyright (C) 2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <wait_bit.h>
#include <asm/io.h>
#include <asm/arch/mailbox_s10.h>

DECLARE_GLOBAL_DATA_PTR;

static const struct socfpga_mailbox *mbox_base =
		(void *)SOCFPGA_MAILBOX_ADDRESS;

#define MBOX_POLL_RESP_TIMEOUT		50 /* ms */

static int mbox_polling_resp(u32 rout)
{
	u32 rin;
	unsigned long start = get_timer(0);

	while(1) {
		rin = readl(&mbox_base->rin);
		if (rout != rin)
			return 0;

		if (get_timer(start) > MBOX_POLL_RESP_TIMEOUT)
			break;

		udelay(1);
	}

	debug("mailbox: polling response timeout\n");
	return -ETIMEDOUT;
}

/* Check for available slot and write to circular buffer.
 * It also update command valid offset (cin) register.
 */
static int mbox_fill_cmd_circular_buff(u32 header, u32 len, u32 *arg)
{
	u32 cmd_free_offset;
	u32 i;

	/* checking available command free slot */
	cmd_free_offset = readl(&mbox_base->cout);
	if (cmd_free_offset >= MBOX_CMD_BUFFER_SIZE) {
		error("ERROR: Not enough space, cout %d\n", cmd_free_offset);
		return -ENOMEM;
	}

	/* write header to circular buffer */
	writel(header, &mbox_base->cmd_buf[cmd_free_offset++]);
	/* wrapping around when it reach the buffer size */
	cmd_free_offset %= MBOX_CMD_BUFFER_SIZE;

	/* write arguments */
	for (i = 0; i < len; i++) {
		writel(arg[i], &mbox_base->cmd_buf[cmd_free_offset++]);
		/* wrapping around when it reach the buffer size */
		cmd_free_offset %= MBOX_CMD_BUFFER_SIZE;
	}

	/* write command valid offset */
	writel(cmd_free_offset, &mbox_base->cin);
	return 0;
}

/* Support one command and up to 31 words argument length only */
int mbox_send_cmd(u8 id, u32 cmd, u32 len, u32 *arg, u8 urgent,
			u32 *resp_buf_len, u32 *resp_buf)
{
	u32 header;
	u32 rin;
	u32 resp;
	u32 rout;
	u32 status;
	u32 resp_len;
	u32 buf_len;
	int ret;

	/* Total lenght is command + argument length */
	if ((len + 1) > MBOX_CMD_BUFFER_SIZE) {
		error("ERROR: command %d arguments too long, max %d\n", cmd,
			MBOX_CMD_BUFFER_SIZE - 1);
		return -EINVAL;
	}

	if (cmd > MBOX_MAX_CMD_INDEX) {
		error("ERROR: Unsupported command index %d\n", cmd);
		return -EINVAL;
	}

	header = MBOX_CMD_HEADER(MBOX_CLIENT_ID_UBOOT, id , len, cmd);

	ret = mbox_fill_cmd_circular_buff(header, len, arg);
	if (ret)
		return ret;

	if (urgent) {
		/* Send command as urgent command */
		writel(1, &mbox_base->urg);
	}

	/* write doorbell */
	writel(1, MBOX_DOORBELL_TO_SDM_REG);

	while (1) {
		/* Wait for doorbell from SDM */
		ret = wait_for_bit(__func__, (const u32 *)MBOX_DOORBELL_FROM_SDM_REG,
			   1, true, 500000, false);
		if (ret) {
			error("mailbox: timeout from SDM\n");
			return ret;
		}

		/* clear interrupt */
		writel(0, MBOX_DOORBELL_FROM_SDM_REG);

		if (urgent) {
			/* urgent command doesn't has response */
			writel(0, &mbox_base->urg);
			status = readl(&mbox_base->status);
			if (status & MBOX_STATUS_UA_MSK)
				return 0;

			error("mailbox: cmd %d no urgent ACK\n", cmd);
			return -1;
		}

		/* read current response offset */
		rout = readl(&mbox_base->rout);

		/* read response valid offset */
		rin = readl(&mbox_base->rin);

		if (rout != rin) {
			/* Response received */
			resp = readl(&mbox_base->resp_buf[rout]);
			rout++;
			/* wrapping around when it reach the buffer size */
			rout %= MBOX_RESP_BUFFER_SIZE;
			/* update next ROUT */
			writel(rout, &mbox_base->rout);

			/* check client ID and ID */
			if ((MBOX_RESP_CLIENT_GET(resp) == MBOX_CLIENT_ID_UBOOT) &&
			     (MBOX_RESP_ID_GET(resp) == id)) {
				ret = MBOX_RESP_ERR_GET(resp);
				if (ret) {
					error("mailbox send command %d error %d\n",
						cmd, ret);
					return ret;
				}

				if (resp_buf_len) {
					buf_len = *resp_buf_len;
					*resp_buf_len = 0;
				} else {
					buf_len = 0;
				}

				resp_len = MBOX_RESP_LEN_GET(resp);
				while (resp_len) {
					ret = mbox_polling_resp(rout);
					if (ret)
						return ret;
					/* we need to process response buffer even caller doesn't need it */
					resp = readl(&mbox_base->resp_buf[rout]);
					rout++;
					resp_len--;
					rout %= MBOX_RESP_BUFFER_SIZE;
					writel(rout, &mbox_base->rout);
					if (buf_len) {
						/* copy response to buffer */
						resp_buf[*resp_buf_len] = resp;
						(*resp_buf_len)++;
						buf_len--;
					}
				}
				return ret;
			}
		}
	};

	return -EIO;
}

int mbox_init(void)
{
	int ret;

	/* enable mailbox interrupts */
	writel(MBOX_ALL_INTRS, &mbox_base->flags);

	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_RESTART, 0, NULL, 1, 0, NULL);
	if (ret)
		return ret;

	/* Renable mailbox interrupts after MBOX_RESTART */
	writel(MBOX_ALL_INTRS, &mbox_base->flags);

	return 0;
}

#ifdef CONFIG_CADENCE_QSPI
int mbox_qspi_close(void)
{
	return mbox_send_cmd(MBOX_ID_UBOOT, MBOX_QSPI_CLOSE, 0, NULL, 0, 0, NULL);
}

int mbox_qspi_open(void)
{
	int ret;

	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_QSPI_OPEN, 0, NULL, 0, 0, NULL);
	if (ret)
		return ret;

	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_QSPI_DIRECT, 0, NULL, 0, 0, NULL);
	if (ret)
		goto error;

	return ret;

error:
	mbox_qspi_close();

	return ret;
}
#endif /* CONFIG_CADENCE_QSPI */

int mbox_reset_cold(void)
{
	int ret;

	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_REBOOT_HPS, 0, NULL, 0, 0,
			    NULL);
	if (ret) {
		/* mailbox sent failure, wait for watchdog to kick in */
		while (1)
			;
	}
	return 0;
}

