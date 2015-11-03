/*
 * Designware master SPI core controller driver
 *
 * Copyright (C) 2015 Altera Corporation
 *
 * Copyright (C) 2014 Stefan Roese <sr@denx.de>
 *
 * Very loosely based on the Linux driver:
 * drivers/spi/spi-dw.c, which is:
 * Copyright (c) 2009, Intel Corporation.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <spi.h>
#include <fdtdec.h>
#include <linux/compat.h>
#include <asm/io.h>
#include <asm/arch/clock_manager.h>

DECLARE_GLOBAL_DATA_PTR;

/* Bit fields in CTRLR0 */
#define SPI_DFS_OFFSET			0

#define SPI_FRF_OFFSET			4
#define SPI_FRF_SPI			0x0
#define SPI_FRF_SSP			0x1
#define SPI_FRF_MICROWIRE		0x2
#define SPI_FRF_RESV			0x3

#define SPI_MODE_OFFSET			6
#define SPI_SCPH_OFFSET			6
#define SPI_SCOL_OFFSET			7

#define SPI_TMOD_OFFSET			8
#define SPI_TMOD_MASK			(0x3 << SPI_TMOD_OFFSET)
#define	SPI_TMOD_TR			0x0		/* xmit & recv */
#define SPI_TMOD_TO			0x1		/* xmit only */
#define SPI_TMOD_RO			0x2		/* recv only */
#define SPI_TMOD_EPROMREAD		0x3		/* eeprom read mode */

#define SPI_SLVOE_OFFSET		10
#define SPI_SRL_OFFSET			11
#define SPI_CFS_OFFSET			12

/* Bit fields in SR, 7 bits */
#define SR_MASK				0x7f		/* cover 7 bits */
#define SR_BUSY				(1 << 0)
#define SR_TF_NOT_FULL			(1 << 1)
#define SR_TF_EMPT			(1 << 2)
#define SR_RF_NOT_EMPT			(1 << 3)
#define SR_RF_FULL			(1 << 4)
#define SR_TX_ERR			(1 << 5)
#define SR_DCOL				(1 << 6)

#define RX_TIMEOUT			1000		/* timeout in ms */

#ifndef CONFIG_SYS_ALTERA_SPI_LIST
	#define CONFIG_SYS_ALTERA_SPI_LIST { CONFIG_SYS_SPI_BASE }
#endif

static ulong dw_spi_base_list[] = CONFIG_SYS_ALTERA_SPI_LIST;

/* DesignWare spi register set */
struct dw_spi_regs {
	u32 DW_SPI_CTRL0;	/* 0x00 */
	u32 DW_SPI_CTRL1;	/* 0x04 */
	u32 DW_SPI_SSIENR;	/* 0x08 */
	u32 DW_SPI_MWCR;	/* 0x0C */
	u32 DW_SPI_SER;		/* 0x10 */
	u32 DW_SPI_BAUDR;	/* 0x14 */
	u32 DW_SPI_TXFLTR;	/* 0x18 */
	u32 DW_SPI_RXFLTR;	/* 0x1C */
	u32 DW_SPI_TXFLR;	/* 0x20 */
	u32 DW_SPI_RXFLR;	/* 0x24 */
	u32 DW_SPI_SR;		/* 0x28 */
	u32 DW_SPI_IMR;		/* 0x2c */
	u32 DW_SPI_ISR;		/* 0x30 */
	u32 DW_SPI_RISR;	/* 0x34 */
	u32 DW_SPI_TXOICR;	/* 0x38 */
	u32 DW_SPI_RXOICR;	/* 0x3c */
	u32 DW_SPI_RXUICR;	/* 0x40 */
	u32 DW_SPI_MSTICR;	/* 0x44 */
	u32 DW_SPI_ICR;		/* 0x48 */
	u32 DW_SPI_DMACR;	/* 0x4c */
	u32 DW_SPI_DMATDLR;	/* 0x50 */
	u32 DW_SPI_DMARDLR;	/* 0x54 */
	u32 DW_SPI_IDR;		/* 0x58 */
	u32 DW_SPI_VERSION;	/* 0x5c */
	u32 DW_SPI_DR;		/* 0x60 */
};

struct dw_spi_slave {
	struct spi_slave slave;
	struct dw_spi_regs *base; /* Base address of the SPI core */
	unsigned int plat_freq;	/* Default clock frequency, -1 for none */
	unsigned int freq;	/* Current frequency */
	unsigned int max_hz;	/* User defined frequency */
	unsigned int mode;	/* Identifies the SPI mode to use */
	u8 tmode;		/* TR/TO/RO/EEPROM */
	u8 type;		/* SPI/SSP/MicroWire */
	int len;		/* Bytes of data to send or receive */
	u8 nbytes;		/* Data frame in byte */
	u32 fifo_len;		/* Depth of the FIFO buffer */
	void *tx;		/* Pointer to the start of transmit data */
	void *tx_end;		/* Pointer to the end of transmit data */
	void *rx;		/* Pointer to the start of receive data */
	void *rx_end;		/* Pointer to the start receive data */
};

#define to_dw_spi_slave(s) container_of(s, struct dw_spi_slave, slave)

int spi_cs_is_valid(unsigned int bus, unsigned int cs)
{
	return bus < ARRAY_SIZE(dw_spi_base_list) && cs < 4;
}

static inline void spi_enable_chip(struct dw_spi_slave *dwspi, int enable)
{
	__raw_writel((enable ? 1 : 0), &dwspi->base->DW_SPI_SSIENR);
}

void spi_free_slave(struct spi_slave *slave)
{
	struct dw_spi_slave *dwspi = to_dw_spi_slave(slave);

	free(dwspi);

	return;
}

void spi_release_bus(struct spi_slave *slave)
{
	struct dw_spi_slave *dwspi = to_dw_spi_slave(slave);

	debug("%s: bus:%i cs:%i\n", __func__, slave->bus, slave->cs);

	/* Slave select disabled */
	spi_enable_chip(dwspi, 0);
	spi_cs_deactivate(&dwspi->slave);

	return;
}

struct spi_slave *spi_setup_slave(unsigned int bus, unsigned int cs,
					unsigned int max_hz, unsigned int mode)
{
	struct dw_spi_slave *dwspi;

	if (!spi_cs_is_valid(bus, cs)) {
		debug("%s: Invalid bus/chip select %d, %d\n", __func__,
		      bus, cs);
		return NULL;
	}

	dwspi = spi_alloc_slave(struct dw_spi_slave, bus, cs);

	if (!dwspi) {
		debug("%s: Could not allocate spi_slave\n", __func__);
		return NULL;
	}

	/* Currently only bits_per_word == 8 supported */
	dwspi->slave.wordlen = 8;
	dwspi->nbytes = dwspi->slave.wordlen >> 3;
	dwspi->tmode = 0; /* Tx & Rx */
	dwspi->mode = mode;
	dwspi->type = SPI_FRF_SPI;
	dwspi->base = (struct dw_spi_regs *)dw_spi_base_list[bus];
	dwspi->plat_freq = SPI_MAX_FREQUENCY;
	dwspi->max_hz = max_hz;

	return &dwspi->slave;
}

int spi_claim_bus(struct spi_slave *slave)
{
	struct dw_spi_slave *dwspi = to_dw_spi_slave(slave);

	debug("%s: bus:%i cs:%i\n", __func__, slave->bus, slave->cs);

	spi_enable_chip(dwspi, 0);
	__raw_writel(0xff, &dwspi->base->DW_SPI_IMR);
	spi_enable_chip(dwspi, 1);

	spi_set_speed(&dwspi->slave, dwspi->max_hz);

	/*
	 * Try to detect the FIFO depth if not set by interface driver,
	 * the depth could be from 2 to 256 from HW spec
	 */
	if (!dwspi->fifo_len) {
		u32 fifo;

		for (fifo = 1; fifo < 256; fifo++) {
			__raw_writel(fifo, &(dwspi->base->DW_SPI_TXFLTR));
			if (fifo != __raw_readl(&dwspi->base->DW_SPI_TXFLTR))
				break;
		}

		dwspi->fifo_len = (fifo == 1) ? 0 : fifo;
		__raw_writel(0, &dwspi->base->DW_SPI_TXFLTR);
	}
	debug("%s: fifo_len=%d\n", __func__, dwspi->fifo_len);

	return 0;
}

/* Return the max entries we can fill into tx fifo */
static inline u32 tx_max(struct dw_spi_slave *dw_spi)
{
	struct dw_spi_slave *dwspi = dw_spi;
	u32 tx_left, tx_room, rxtx_gap;

	tx_left = (dwspi->tx_end - dwspi->tx) / (dwspi->nbytes);
	tx_room = dwspi->fifo_len - __raw_readl(&dwspi->base->DW_SPI_TXFLTR);

	/*
	 * Another concern is about the tx/rx mismatch, we
	 * thought about using (dwspi->fifo_len - rxflr - txflr) as
	 * one maximum value for tx, but it doesn't cover the
	 * data which is out of tx/rx fifo and inside the
	 * shift registers. So a control from sw point of
	 * view is taken.
	 */
	rxtx_gap = ((dwspi->rx_end - dwspi->rx) - (dwspi->tx_end - dwspi->tx)) /
		(dwspi->nbytes);

	return min3(tx_left, tx_room, (u32)(dwspi->fifo_len - rxtx_gap));
}

/* Return the max entries we should read out of rx fifo */
static inline u32 rx_max(struct dw_spi_slave *dw_spi)
{
	struct dw_spi_slave *dwspi = dw_spi;
	u32 rx_left = (dwspi->rx_end - dwspi->rx) / (dwspi->nbytes);

	return min_t(u32, rx_left, __raw_readl(&dwspi->base->DW_SPI_RXFLR));
}

static void dw_writer(struct dw_spi_slave *dw_spi)
{
	struct dw_spi_slave *dwspi = dw_spi;
	u32 max = tx_max(dwspi);
	u16 txw = 0;

	/*
	 * Cautious: For continuous data transfers, you must ensure that the
	 * transmit FIFO buffer does not become empty before all the data have
	 * been transmitted, so performance impact logic should be avoided
	 * like 'printf'.
	 */
	while (max--) {
		if (dwspi->tx_end - dwspi->len) {
			if (dwspi->slave.wordlen == 8)
				txw = *(u8 *)(dwspi->tx);
			else
				txw = *(u16 *)(dwspi->tx);
		}

		__raw_writel(txw, &dwspi->base->DW_SPI_DR);
		dwspi->tx += dwspi->nbytes;
	}
}

static int dw_reader(struct dw_spi_slave *dw_spi)
{
	struct dw_spi_slave *dwspi = dw_spi;
	unsigned start = get_timer(0);
	u32 max;
	u32 rxw;

	/* Wait for rx data to be ready */
	while (rx_max(dwspi) == 0) {
		if (get_timer(start) > RX_TIMEOUT)
			return -ETIMEDOUT;
	}

	max = rx_max(dwspi);

	while (max--) {
		rxw = __raw_readl(&dwspi->base->DW_SPI_DR);

		/*
		 * Care about rx only if the transfer's original "rx" is
		 * not null
		 */
		if (dwspi->rx_end - dwspi->len) {
			if (dwspi->slave.wordlen == 8)
				*(u8 *)(dwspi->rx) = rxw;
			else
				*(u16 *)(dwspi->rx) = rxw;
		}
		dwspi->rx += dwspi->nbytes;
	}

	return 0;
}

static int poll_transfer(struct dw_spi_slave *dw_spi)
{
	struct dw_spi_slave *dwspi = dw_spi;
	int ret;

	do {
		dw_writer(dwspi);
		ret = dw_reader(dwspi);
		if (ret < 0)
			return ret;
	} while (dwspi->rx_end > dwspi->rx);

	return 0;
}

int spi_xfer(struct spi_slave *slave, unsigned int bitlen, const void *dout,
	      void *din, unsigned long flags)
{
	struct dw_spi_slave *dwspi = to_dw_spi_slave(slave);
	const u8 *tx = dout;
	u8 *rx = din;
	int ret = 0;
	u32 cr0 = 0;

/* spi core configured to do 8 bit transfers */
	if (bitlen % 8) {
		debug("Non byte aligned SPI transfer.\n");
		return -1;
	}

	cr0 = (dwspi->slave.wordlen - 1) | (dwspi->type << SPI_FRF_OFFSET) |
		(dwspi->mode << SPI_MODE_OFFSET) |
		(dwspi->tmode << SPI_TMOD_OFFSET);

	if (rx && tx)
		dwspi->tmode = SPI_TMOD_TR;
	else if (rx)
		dwspi->tmode = SPI_TMOD_RO;
	else
		dwspi->tmode = SPI_TMOD_TO;

	cr0 &= ~SPI_TMOD_MASK;
	cr0 |= (dwspi->tmode << SPI_TMOD_OFFSET);

	dwspi->len = bitlen >> 3;
	debug("%s: rx=%p tx=%p len=%d [bytes]\n", __func__, rx, tx, dwspi->len);

	dwspi->tx = (void *)tx;
	dwspi->tx_end = dwspi->tx + dwspi->len;
	dwspi->rx = rx;
	dwspi->rx_end = dwspi->rx + dwspi->len;

	/* Disable controller before writing control registers */
	spi_enable_chip(dwspi, 0);

	debug("%s: cr0=%08x\n", __func__, cr0);

	/* Reprogram cr0 only if changed */
	if (__raw_readl(&dwspi->base->DW_SPI_CTRL0) != cr0)
		__raw_writel(cr0, &dwspi->base->DW_SPI_CTRL0);

	spi_cs_activate(&dwspi->slave);

	/* Enable controller after writing control registers */
	spi_enable_chip(dwspi, 1);

	/* Start transfer in a polling loop */
	ret = poll_transfer(dwspi);

	return ret;
}

void spi_set_speed(struct spi_slave *slave, uint hz)
{
	struct dw_spi_slave *dwspi = to_dw_spi_slave(slave);
	u16 clk_div;

	if (hz > dwspi->plat_freq)
		hz = dwspi->plat_freq;

	/* Disable controller before writing control registers */
	spi_enable_chip(dwspi, 0);

	/* clk_div doesn't support odd number */
	clk_div = CONFIG_SPI_REF_CLK / hz;
	clk_div = (clk_div + 1) & 0xfffe;
	__raw_writel(clk_div, &dwspi->base->DW_SPI_BAUDR);

	/* Enable controller after writing control registers */
	spi_enable_chip(dwspi, 1);

	dwspi->freq = hz;

	return;
}

void spi_cs_activate(struct spi_slave *slave)
{
	struct dw_spi_slave *dwspi = to_dw_spi_slave(slave);

	/*
	 * Configure the desired SS (slave select 0...3) in the controller.
	 * If no slaves are enabled prior to writing to the Data Register (DR),
	 * the transfer does not begin until a slave is enabled.
	 */
	__raw_writel((1 << dwspi->slave.cs), &dwspi->base->DW_SPI_SER);

	return;
}

void spi_cs_deactivate(struct spi_slave *slave)
{
	struct dw_spi_slave *dwspi = to_dw_spi_slave(slave);

	__raw_writel(0, &dwspi->base->DW_SPI_SER);

	return;
}

void spi_init(void)
{
	/* nothing to do */
	return;
}

