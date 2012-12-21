/*
 *
 * Copyright (C) 2012
 * Altera Corporation <www.altera.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */

#include <common.h>
#include <mmc.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <watchdog.h>
#include "dw_mmc.h"

struct mmc mmc_dev;

static inline void dw_mmc_clear_bits(unsigned int *addr, unsigned int val)
{
	unsigned int read;
	read = readl(addr);
	read &= (~val);
	writel(read, addr);
}

static inline void dw_mmc_set_bits(unsigned int *addr, unsigned int val)
{
	unsigned int read;
	read = readl(addr);
	read |= (val);
	writel(read, addr);
}

int dw_poll_cmd_register(struct dw_host *host)
{
	u32 num_of_retries = 0;

	/* Reset watchdog before start polling. */
	WATCHDOG_RESET();

	while (1) {
		if (num_of_retries > CMD_MAX_RETRIES) {
			printf("CMD register Polling failed 0x%08x"
				" after %d attempts\n",
				readl(&host->reg->cmd), num_of_retries);
			WATCHDOG_RESET();
			return TIMEOUT;
		}
		if ((readl(&host->reg->cmd) & SDMMC_CMD_START) == 0)
			break;
		num_of_retries++;
		udelay(1);
	}
	WATCHDOG_RESET();
	return 0;
}

static void dw_setup_data(struct dw_host *host, struct mmc_data *data)
{
	writel((data->blocks*data->blocksize), &host->reg->bytcnt);
	writel(data->blocksize, &host->reg->blksiz);
}

static int dw_start_cmd(struct dw_host *host, unsigned int cmd,
	unsigned int cmdarg)
{
	writel(cmdarg, &host->reg->cmdarg);

	cmd |= SDMMC_CMD_START;
	writel(cmd, &host->reg->cmd);

	debug("Send CMD%d CMD=0x%08x ARG=0x%08x\n",
		SDMMC_CMD_INDX(cmd), cmd, cmdarg);

	return dw_poll_cmd_register(host);
}

int dw_update_clk_cmd(struct dw_host *host)
{
	return dw_start_cmd(host, SDMMC_CMD_UPD_CLK |
		SDMMC_CMD_PRV_DAT_WAIT, 0);
}


static int dw_host_reset_fifo(struct dw_host *host)
{
	unsigned int timeout = 100;
	/* Reset FIFO */
	dw_mmc_set_bits(&host->reg->ctrl, CTRL_FIFO_RESET);

	/* Reset watchdog before start polling. */
	WATCHDOG_RESET();

	/* hw clears the bit when it's done */
	while (readl(&host->reg->ctrl) & CTRL_FIFO_RESET) {
		if (timeout == 0) {
			printf("%s: CTRL_FIFO_RESET timeout error\n", __func__);
			WATCHDOG_RESET();
			return -1;
		}
		timeout--;
		udelay(100);
	}

	WATCHDOG_RESET();
	return 0;
}

static int dw_finish_data(unsigned int stat)
{
	int data_error = 0;

	if (stat & STATUS_INT_ERR) {
		printf("request failed. status: 0x%08x\n",
				stat);
		if (stat & INTMSK_DCRC)
			data_error = -EILSEQ;
		else if (stat & INTMSK_EBE)
			data_error = -EILSEQ;
		else if (stat & INTMSK_DTO)
			data_error = TIMEOUT;
		else
			data_error = -EIO;
	}

	return data_error;
}

static int dw_read_response(struct dw_host *host, struct mmc_cmd *cmd,
				unsigned int stat)
{
	u32 *resp = (u32 *)cmd->response;

	if (!cmd)
		return 0;

	if (stat & INTMSK_RTO) {
		debug("ERROR: response time out\n");
		writel(stat, &host->reg->rintsts);
		return TIMEOUT;
	} else if ((stat & INTMSK_RCRC) && (cmd->resp_type & MMC_RSP_CRC)) {
		printf("ERROR: command CRC error\n");
		writel(stat, &host->reg->rintsts);
		return -EILSEQ;
	} else if (stat & INTMSK_RESP_ERR) {
		printf("ERROR: response error\n");
		writel(stat, &host->reg->rintsts);
		return -EIO;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			resp[3] = readl(&host->reg->resp0);
			resp[2] = readl(&host->reg->resp1);
			resp[1] = readl(&host->reg->resp2);
			resp[0] = readl(&host->reg->resp3);

			debug("%s: Resp0 = 0x%x\n", __FUNCTION__,resp[0]);
			debug("%s: Resp1 = 0x%x\n", __FUNCTION__,resp[1]);
			debug("%s: Resp2 = 0x%x\n", __FUNCTION__,resp[2]);
			debug("%s: Resp3 = 0x%x\n", __FUNCTION__,resp[3]);

		} else {
			resp[0] = readl(&host->reg->resp0);
			resp[1] = 0;
			resp[2] = 0;
			resp[3] = 0;

			debug("%s: Resp0 = 0x%x\n", __FUNCTION__, resp[0]);

		}
	}
	return 0;
}

static unsigned int dw_poll_status(struct dw_host *host, u32 mask)
{
	u32 stat;
	unsigned int ret = 0;
	unsigned long timeout = get_timer_masked() + CONFIG_SYS_HZ;

	/* Reset watchdog before start polling. */
	WATCHDOG_RESET();

	do {
		stat = readl(&host->reg->mintsts);

		debug("%s: MINTSTS 0x%x\n", __func__, stat);

		if (stat & STATUS_INT_ERR) {
			writel(stat, &host->reg->rintsts);
			ret = stat;
			break;
		}

		if (timeout < get_timer_masked()) {
			writel(stat, &host->reg->rintsts);
			ret = INTMSK_DTO;
			break;
		}

		if (stat & mask) {
			writel((stat & mask), &host->reg->rintsts);
			break;
		}
	} while (1);

	WATCHDOG_RESET();
	return ret;
}

#ifdef DEBUG
void dump_buffer(u8 *buffer, u32 size)
{
	u32 i, j;

	for (i = 0; i < size; ) {
		printf("[%03x]: ", i);

		for (j = 0; j < 16; j++) {
			if (i+j >= size)
				break;
			printf("%02x ", buffer[i+j]);
		}
		printf("\n");
		i = i + j;
	}
}
#endif

static int dw_pull(struct dw_host *host, void *_buf, int bytes)
{
	unsigned int stat;
	u32 *buf = _buf;
	u32 status;
	u32 fifo_count;
	u32 dangling_bytes = 0;
	u32 i;

	while (bytes) {
		stat = dw_poll_status(host,
				INTMSK_RXDR | INTMSK_DAT_OVER | INTMSK_HTO);

		if (stat) {
			printf("ERROR: %s: stat 0x%x\n", __func__, stat);
			return stat;
		}

		status = readl(&host->reg->status);
		fifo_count = (GET_FIFO_COUNT(status));

		if ((fifo_count * FIFO_WIDTH_BYTES) > bytes) {
			dangling_bytes = bytes % FIFO_WIDTH_BYTES;
			fifo_count--;
		}

		debug("%s: Read %d bytes from FIFO\n", __func__, fifo_count*4);

		for (i = 0; i < fifo_count; i++) {
			*buf = readl(&host->reg->fifodata);
			buf++;
			bytes -= 4;
		}

		if (dangling_bytes) {
			u8 *b = (u8 *)buf;
			u32 tmp;
			tmp = readl(&host->reg->fifodata);
			memcpy(b, &tmp, dangling_bytes);
			bytes -= dangling_bytes;
		}
		udelay(1);
	}

	return 0;
}
#ifndef CONFIG_SPL_BUILD
static int dw_push(struct dw_host *host, const void *_buf, int bytes)
{
	u32 pending_dwords, dangling_dwords = 0, dwords_to_write;
	u32 buffer = 0;
	int count;
	u32 *buf = (u32 *)_buf;
	unsigned int stat;

	bytes -= host->num_bytes_written;
	buf += host->num_bytes_written / FIFO_WIDTH_BYTES;

	while (bytes) {
		stat = dw_poll_status(host, INTMSK_TXDR | INTMSK_HTO);
		if (stat) {
			printf("ERROR: %s: stat 0x%x\n", __func__, stat);
			return stat;
		}

		pending_dwords = bytes / FIFO_WIDTH_BYTES;
		dangling_dwords = bytes % FIFO_WIDTH_BYTES;

		if (pending_dwords >=
			(host->fifo_depth - host->fifo_threshold)) {
			dwords_to_write =
				(host->fifo_depth - host->fifo_threshold);

			dangling_dwords = 0;
		} else {
			dwords_to_write = pending_dwords;
		}

		debug("Entered %s pending_dwords %d dwords_to_write %d "
			"dangling_dwords %d\n",
			__func__, pending_dwords,
			dwords_to_write, dangling_dwords);

		for (count = 0; count < dwords_to_write; count++) {
			writel(*buf, &host->reg->fifodata);
			buf++;
			bytes -= 4;
		}

		if (dangling_dwords) {
			buffer = 0;
			memcpy((void *) &buffer, (void *) buf, dangling_dwords);
			writel(buffer, &host->reg->fifodata);
			bytes -= dangling_dwords;
		}
	}

	return dw_poll_status(host, INTMSK_DAT_OVER);
}
#else
static int dw_push(struct dw_host *host, const void *_buf, int bytes)
{
	return 0;
}
#endif
static int dw_transfer_data(struct dw_host *host, struct mmc_data *data)
{
	int stat;
	unsigned long length;

	length = data->blocks * data->blocksize;

	if (data->flags & MMC_DATA_READ) {
		stat = dw_pull(host, data->dest, length);
		if (stat)
			return stat;
	} else {
		stat = dw_push(host, (const void *)(data->src), length);
		if (stat)
			return stat;
	}
	return 0;
}

static int dw_cmd_done(struct dw_host *host, struct mmc_cmd *cmd,
			struct mmc_data *data)
{
	int ret;
	unsigned int stat;
	unsigned int retry = 8000;

	/* Reset watchdog before start polling. */
	WATCHDOG_RESET();

	do {
		stat = readl(&host->reg->mintsts);
		debug("%s: MINTSTS 0x%x rintsts 0x%x\n", __FUNCTION__, stat,
			readl(&host->reg->rintsts));

		if (stat & INTMSK_CMD_DONE) {
			writel(INTMSK_CMD_DONE, &host->reg->rintsts);
			break;
		}
		retry--;
		udelay(1);
	} while (retry);

	if (!retry) {
		return TIMEOUT;
	}

	WATCHDOG_RESET();

	ret = dw_read_response(host, cmd, stat);

	if (data) {
		stat = dw_transfer_data(host, data);
		ret = dw_finish_data(stat);
	}

	return ret;
}


static int dw_request(struct mmc *mmc, struct mmc_cmd *cmd,
			struct mmc_data *data)
{
	struct dw_host *host = mmc->priv;
	int ret;
	u32 cmdr = cmd->cmdidx;

	if (cmdr == MMC_CMD_STOP_TRANSMISSION)
		cmdr |= SDMMC_CMD_STOP;
	else
		cmdr |= SDMMC_CMD_PRV_DAT_WAIT;

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		/* We expect a response, so set this bit */
		cmdr |= SDMMC_CMD_RESP_EXP;
		if (cmd->resp_type & MMC_RSP_136)
			cmdr |= SDMMC_CMD_RESP_LONG;
	}

	if (cmd->resp_type & MMC_RSP_CRC)
		cmdr |= SDMMC_CMD_RESP_CRC;

	if (host->need_init_cmd) {
		/* Set initialization bit for first command */
		host->need_init_cmd = 0;
		cmdr |= SDMMC_CMD_INIT;
	}

	if (host->use_hold_reg && host->use_hold_reg())
		cmdr |= SDMMC_CMD_USE_HOLD_REG;

	if (data) {
		unsigned int count;
		unsigned int num_bytes = data->blocks * data->blocksize;
		unsigned int num_of_dwords = 0;
		u32 *buf32 = (u32 *)data->src;

		/* Reset FIFO */
		ret = dw_host_reset_fifo(host);
		if (ret)
			return ret;

		dw_setup_data(host, data);
		cmdr |= SDMMC_CMD_DAT_EXP;

		if (data->flags & MMC_DATA_WRITE) {
			cmdr |= SDMMC_CMD_DAT_WR;

			/* Fill up FIFO before sending command. */
			if (((num_bytes) / FIFO_WIDTH_BYTES) >=
				host->fifo_depth) {
				/* Write fifo_depth dwords */
				num_of_dwords = host->fifo_depth;
			} else {
				/* Just write complete buffer out */
				num_of_dwords = (num_bytes) / FIFO_WIDTH_BYTES;
			}

			for (count = 0; count < num_of_dwords; count++) {
				writel(*buf32, &host->reg->fifodata);
				buf32++;
			}

			host->num_bytes_written =
				num_of_dwords * FIFO_WIDTH_BYTES;
		}
	}

	ret = dw_start_cmd(host, cmdr, cmd->cmdarg);
	if (ret)
		return ret;

	return dw_cmd_done(host, cmd, data);
}


static int dw_host_set_clock(struct dw_host *host, u32 output_clk)
{
	u32 clk_in = host->clock_in;
	u32 div;

	/* output clock must less than or equal to clock in */
	if (output_clk > clk_in)
		return -1;

	/* We only use divider0 */
	if (clk_in == output_clk) {
		/* bypass */
		div = 0;
	} else if (clk_in % (output_clk * 2)) {
		/*
		* move the + 1 after the divide to prevent
		* over-clocking the card.
		*/
		div = ((clk_in / output_clk) >> 1) + 1;
	} else {
		div = (clk_in  / output_clk) >> 1;
	}

	debug("Bus speed  %dHz (slot req %dHz, actual %dHZ"
		" div = %d)\n", clk_in, output_clk,
		div ? ((clk_in / div) >> 1) : clk_in, div);

	/* disable clock */
	writel(0, &host->reg->clkena);

	/* inform controller */
	dw_update_clk_cmd(host);

	/* Call platform dependent clock timing function before
	 * clock is enabled. */
	if (host->set_timing)
		host->set_timing(output_clk);

	/* set clock to desired speed */
	writel(div, &host->reg->clkdiv);
	writel(0, &host->reg->clksrc);

	/* inform controller */
	dw_update_clk_cmd(host);

	/* enable clock */
	writel(SDMMC_CLKEN_ENABLE, &host->reg->clkena);

	/* inform controller */
	dw_update_clk_cmd(host);

	return 0;
}

static void dw_mmc_set_ios(struct mmc *mmc)
{
	struct dw_host *host = mmc->priv;

	debug("bus_width: %x, clock: %d\n", mmc->bus_width, mmc->clock);

	if (mmc->clock) {
		if (mmc->clock >= mmc->f_max)
			mmc->clock = mmc->f_max;

		dw_host_set_clock(host, mmc->clock);
	}

	/* Set the bus width */
	if (mmc->bus_width) {
		u32 buswidth = SDMMC_CTYPE_1BIT;

		switch (mmc->bus_width) {
		case 1:
			buswidth = SDMMC_CTYPE_1BIT;
			break;
		case 4:
			buswidth = SDMMC_CTYPE_4BIT;
			break;
		case 8:
			buswidth = SDMMC_CTYPE_8BIT;
			break;
		default:
			printf("Invalid bus width\n");
			break;
		}
		writel(buswidth, &host->reg->ctype);
	}
}

static int dw_host_reset(struct dw_host *host)
{
	unsigned int timeout;

	/* Wait max 100 ms */
	timeout = 100;

	/* Reset controller */
	dw_mmc_set_bits(&host->reg->ctrl, (CTRL_RESET | CTRL_FIFO_RESET));

	/* Reset watchdog before start polling. */
	WATCHDOG_RESET();

	/* hw clears the bit when it's done */
	while (readl(&host->reg->ctrl) & (CTRL_RESET | CTRL_FIFO_RESET)) {
		if (timeout == 0) {
			printf("%s: Reset host timeout error\n", __func__);
			WATCHDOG_RESET();
			return TIMEOUT;
		}
		timeout--;
		udelay(100);
	}

	WATCHDOG_RESET();
	return 0;
}

static int dw_host_init(struct mmc *mmc)
{
	struct dw_host *host = (struct dw_host *)mmc->priv;

	if (dw_host_reset(host))
		return -ENODEV;

	/* default set to 1-bit mode */
	writel(0x000000, &host->reg->ctype);
#if 0
	dw_mmc_clear_bits(&host->reg->pwren, 1);
	udelay(10);
	writel(1, &host->reg->pwren);
	/* delay 1ms after power enable card. */
	udelay(1000);
#endif
	/* Set up the interrupt mask. */
	writel(0xffffffff,  &host->reg->rintsts);
	writel(0, &host->reg->intmsk);    /* disable all interrupts first */

	/* Set Data and Response timeout to Maximum Value*/
	writel(0xffffffff, &host->reg->tmout);

	/* Set the card Debounce */
	writel(DEFAULT_DEBNCE_VAL, &host->reg->debnce);

	/* Update the watermark levels to half the fifo depth */
	host->fifo_depth = FIFO_WIDTH;
	host->fifo_threshold = FIFO_WIDTH / 2;

	/* Tx Watermark */
	dw_mmc_clear_bits(&host->reg->fifoth, 0xfff);
	dw_mmc_set_bits(&host->reg->fifoth, host->fifo_threshold);
	/* Rx Watermark */
	dw_mmc_clear_bits(&host->reg->fifoth, 0x0fff0000);
	dw_mmc_set_bits(&host->reg->fifoth, (host->fifo_threshold-1) << 16);

	writel((INTMSK_ALL_ENABLED & ~INTMSK_ACD), &host->reg->intmsk);
	dw_mmc_set_bits(&host->reg->ctrl, CTRL_INT_ENABLE);
	return 0;
}

static int dw_mmc_initialize(struct dw_host *dw_host)
{
	struct mmc *mmc;

	mmc = &mmc_dev;

	sprintf(mmc->name, "DESIGNWARE SD/MMC");
	mmc->priv = dw_host;
	mmc->send_cmd = dw_request;
	mmc->set_ios = dw_mmc_set_ios;
	mmc->init = dw_host_init;

	mmc->voltages = (MMC_VDD_32_33 | MMC_VDD_33_34);

	mmc->host_caps = 0;
#ifdef CONFIG_SDMMC_HOST_BUS_WIDTH
	mmc->host_caps |= CONFIG_SDMMC_HOST_BUS_WIDTH;
#endif /* CONFIG_SDMMC_HOST_BUS_WIDTH */

#ifdef CONFIG_SDMMC_HOST_HS
	mmc->host_caps |= MMC_MODE_HS;
#endif /* CONFIG_SDMMC_HOST_HS */

	mmc->f_min = 400000;
	mmc->f_max = dw_host->clock_in;

	mmc_register(mmc);

	return 0;
}

int dw_mmc_init(struct dw_host *dw_host)
{
	return dw_mmc_initialize(dw_host);
}
