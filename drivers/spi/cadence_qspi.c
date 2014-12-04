/*
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
#include <malloc.h>
#include <spi.h>
#include "cadence_qspi.h"

#define CQSPI_STIG_READ			0
#define CQSPI_STIG_WRITE		1
#define CQSPI_INDIRECT_READ		2
#define CQSPI_INDIRECT_WRITE		3

static int qspi_is_init;
static unsigned int qspi_calibrated_hz;
static unsigned int qspi_calibrated_cs;

struct cadence_qspi_slave {
	struct spi_slave slave;
	unsigned int	mode;
	unsigned int	max_hz;
	void		*regbase;
	void		*ahbbase;
	size_t		cmd_len;
	u8		cmd_buf[32];
	size_t		data_len;
};

#define to_cadence_qspi_slave(s)		\
		container_of(s, struct cadence_qspi_slave, slave)

void spi_set_speed(struct spi_slave *slave, uint hz)
{
	struct cadence_qspi_slave *cadence_qspi = to_cadence_qspi_slave(slave);
	void *base = cadence_qspi->regbase;

	cadence_qspi_apb_config_baudrate_div(base, CONFIG_CQSPI_REF_CLK, hz);

	/* Reconfigure delay timing if speed is changed. */
	cadence_qspi_apb_delay(base, CONFIG_CQSPI_REF_CLK, hz,
		CONFIG_CQSPI_TSHSL_NS, CONFIG_CQSPI_TSD2D_NS,
		CONFIG_CQSPI_TCHSH_NS, CONFIG_CQSPI_TSLCH_NS);
	return;
}

struct spi_slave *spi_setup_slave(unsigned int bus, unsigned int cs,
		unsigned int max_hz, unsigned int mode)
{
	struct cadence_qspi_slave *cadence_qspi;

	debug("%s: bus %d cs %d max_hz %dMHz mode %d\n", __func__,
		bus, cs, max_hz/1000000, mode);

	if (!spi_cs_is_valid(bus, cs))
		return NULL;

	cadence_qspi = malloc(sizeof(struct cadence_qspi_slave));
	if (!cadence_qspi) {
		printf("QSPI: Can't allocate struct cadence_qspi_slave. "
			"Bus %d cs %d\n", bus, cs);
		return NULL;
	}

	cadence_qspi->slave.bus = bus;
	cadence_qspi->slave.cs = cs;
	cadence_qspi->mode = mode;
	cadence_qspi->max_hz = max_hz;
	cadence_qspi->regbase = (void *)QSPI_BASE;
	cadence_qspi->ahbbase = (void *)QSPI_AHB_BASE;

	if (!qspi_is_init)
		spi_init();

	return &cadence_qspi->slave;
}

void spi_free_slave(struct spi_slave *slave)
{
	struct cadence_qspi_slave *cadence_qspi = to_cadence_qspi_slave(slave);
	free(cadence_qspi);
	return;
}

void spi_init(void)
{
	cadence_qspi_apb_controller_init((void *)QSPI_BASE);
	qspi_is_init = 1;
	return;
}

/* calibration sequence to determine the read data capture delay register */
int spi_calibration(struct spi_slave *slave)
{
	struct cadence_qspi_slave *cadence_qspi = to_cadence_qspi_slave(slave);
	void *base = cadence_qspi->regbase;
	u8 opcode_rdid = 0x9F;
	unsigned int idcode = 0, temp = 0;
	int err = 0, i, range_lo = -1, range_hi = -1;

	/* start with slowest clock (1 MHz) */
	spi_set_speed(slave, 1000000);

	/* configure the read data capture delay register to 0 */
	cadence_qspi_apb_readdata_capture(base, 1, 0);

	/* Enable QSPI */
	cadence_qspi_apb_controller_enable(base);

	/* read the ID which will be our golden value */
	err = cadence_qspi_apb_command_read(base, 1, &opcode_rdid,
		3, (u8 *)&idcode);
	if (err) {
		puts("SF: Calibration failed (read)\n");
		return err;
	}

	/* use back the intended clock and find low range */
	spi_set_speed(slave, cadence_qspi->max_hz);
	for (i = 0; i < CQSPI_READ_CAPTURE_MAX_DELAY; i++) {
		/* Disable QSPI */
		cadence_qspi_apb_controller_disable(base);

		/* reconfigure the read data capture delay register */
		cadence_qspi_apb_readdata_capture(base, 1, i);

		/* Enable back QSPI */
		cadence_qspi_apb_controller_enable(base);

		/* issue a RDID to get the ID value */
		err = cadence_qspi_apb_command_read(base, 1, &opcode_rdid,
			3, (u8 *)&temp);
		if (err) {
			puts("SF: Calibration failed (read)\n");
			return err;
		}

		/* search for range lo */
		if (range_lo == -1 && temp == idcode) {
			range_lo = i;
			continue;
		}

		/* search for range hi */
		if (range_lo != -1 && temp != idcode) {
			range_hi = i - 1;
			break;
		}
		range_hi = i;
	}

	if (range_lo == -1) {
		puts("SF: Calibration failed (low range)\n");
		return err;
	}

	/* Disable QSPI for subsequent initialization */
	cadence_qspi_apb_controller_disable(base);

	/* configure the final value for read data capture delay register */
	cadence_qspi_apb_readdata_capture(base, 1, (range_hi + range_lo) / 2);
	printf("SF: Read data capture delay calibrated to %i (%i - %i)\n",
		(range_hi + range_lo) / 2, range_lo, range_hi);

	/* just to ensure we do once only when speed or chip select change */
	qspi_calibrated_hz = cadence_qspi->max_hz;
	qspi_calibrated_cs = slave->cs;
	return 0;
}

int spi_claim_bus(struct spi_slave *slave)
{
	struct cadence_qspi_slave *cadence_qspi = to_cadence_qspi_slave(slave);
	unsigned int clk_pol = (cadence_qspi->mode & SPI_CPOL) ? 1 : 0;
	unsigned int clk_pha = (cadence_qspi->mode & SPI_CPHA) ? 1 : 0;
	void *base = cadence_qspi->regbase;
	int err = 0;

	debug("%s: bus:%i cs:%i\n", __func__, slave->bus, slave->cs);

	/* Disable QSPI */
	cadence_qspi_apb_controller_disable(base);

	/* Set Chip select */
	cadence_qspi_apb_chipselect(base, slave->cs, CONFIG_CQSPI_DECODER);

	/* Set SPI mode */
	cadence_qspi_apb_set_clk_mode(base, clk_pol, clk_pha);

	/* Set clock speed */
	spi_set_speed(slave, cadence_qspi->max_hz);

	/* calibration required for different SCLK speed or chip select */
	if (qspi_calibrated_hz != cadence_qspi->max_hz ||
		qspi_calibrated_cs != slave->cs) {
		err = spi_calibration(slave);
		if (err)
			return err;
	}

	/* Enable QSPI */
	cadence_qspi_apb_controller_enable(base);

	return 0;
}

void spi_release_bus(struct spi_slave *slave)
{
	return;
}

int spi_xfer(struct spi_slave *slave, unsigned int bitlen, const void *data_out,
		void *data_in, unsigned long flags)
{
	struct cadence_qspi_slave *cadence_qspi = to_cadence_qspi_slave(slave);
	void *base = cadence_qspi->regbase;
	void *ahbbase = cadence_qspi->ahbbase;
	u8 *cmd_buf = cadence_qspi->cmd_buf;
	size_t data_bytes;
	int err = 0;
	u32 mode = CQSPI_STIG_WRITE;

	if (flags & SPI_XFER_BEGIN) {
		/* copy command to local buffer */
		cadence_qspi->cmd_len = bitlen / 8;
		memcpy(cmd_buf, data_out, cadence_qspi->cmd_len);
	}

	if (flags == (SPI_XFER_BEGIN | SPI_XFER_END)) {
		/* if start and end bit are set, the data bytes is 0. */
		data_bytes = 0;
	} else {
		data_bytes = bitlen / 8;
	}

	if ((flags & SPI_XFER_END) || (flags == 0)) {

		if (cadence_qspi->cmd_len == 0) {
			printf("QSPI: Error, command is empty.\n");
			return -1;
		}

		if (data_in && data_bytes) {
			/* read */
			/* Use STIG if no address. */
			if (!CQSPI_IS_ADDR(cadence_qspi->cmd_len))
				mode = CQSPI_STIG_READ;
			else
				mode = CQSPI_INDIRECT_READ;
		} else if (data_out && !(flags & SPI_XFER_BEGIN)) {
			/* write */
			if (!CQSPI_IS_ADDR(cadence_qspi->cmd_len))
				mode = CQSPI_STIG_WRITE;
			else
				mode = CQSPI_INDIRECT_WRITE;
		}

		switch (mode) {
		case CQSPI_STIG_READ:
			err = cadence_qspi_apb_command_read(
				base, cadence_qspi->cmd_len, cmd_buf,
				data_bytes, data_in);

		break;
		case CQSPI_STIG_WRITE:
			err = cadence_qspi_apb_command_write(base,
				cadence_qspi->cmd_len, cmd_buf,
				data_bytes, data_out);
		break;
		case CQSPI_INDIRECT_READ:
			err = cadence_qspi_apb_indirect_read_setup(
				base, QSPI_AHB_BASE,
				cadence_qspi->cmd_len, cmd_buf);
			if (!err) {
				err = cadence_qspi_apb_indirect_read_execute
				(base, ahbbase, data_bytes, data_in);
			}
		break;
		case CQSPI_INDIRECT_WRITE:
			err = cadence_qspi_apb_indirect_write_setup
				(base, QSPI_AHB_BASE,
				cadence_qspi->cmd_len, cmd_buf);
			if (!err) {
				err = cadence_qspi_apb_indirect_write_execute
				(base, ahbbase, data_bytes, data_out);
			}
		break;
		default:
			err = -1;
			break;
		}

		if (flags & SPI_XFER_END) {
			/* clear command buffer */
			memset(cmd_buf, 0, sizeof(cadence_qspi->cmd_buf));
			cadence_qspi->cmd_len = 0;
		}
	}
	return err;
}

int spi_cs_is_valid(unsigned int bus, unsigned int cs)
{
#if (CONFIG_CQSPI_DECODER == 1)
	if (((cs >= 0) && (cs < CQSPI_DECODER_MAX_CS)) && ((bus >= 0) &&
		(bus < CQSPI_DECODER_MAX_CS))) {
		return 1;
	}
#else
	if (((cs >= 0) && (cs < CQSPI_NO_DECODER_MAX_CS)) &&
		((bus >= 0) && (bus < CQSPI_NO_DECODER_MAX_CS))) {
		return 1;
	}
#endif
	printf("QSPI: Invalid bus or cs. Bus %d cs %d\n", bus, cs);
	return 0;
}

void spi_cs_activate(struct spi_slave *slave)
{
	return;
}

void spi_cs_deactivate(struct spi_slave *slave)
{
	return;
}

void spi_enter_xip(struct spi_slave *slave, char xip_dummy)
{
	struct cadence_qspi_slave *cadence_qspi = to_cadence_qspi_slave(slave);
	void *base = cadence_qspi->regbase;
	/* Enter XiP */
	cadence_qspi_apb_enter_xip(base, xip_dummy);
	return;
}
