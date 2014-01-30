/*
 * (C) Copyright 2000-2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Copyright 2008, Network Appliance Inc.
 * Jason McMullan <mcmullan@netapp.com>
 *
 * Copyright (C) 2004-2007 Freescale Semiconductor, Inc.
 * TsiChung Liew (Tsi-Chung.Liew@freescale.com)
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <malloc.h>
#include <spi_flash.h>

#include "spi_flash_internal.h"

/* M25Pxx-specific commands */
#define CMD_M25PXX_RES		0xab	/* Release from DP, and Read Signature */

/* Read flag status register */
#define CMD_READ_FLAG_STATUS		0x70
#define FLAG_STATUS_READY		0x80

/* Read volatile configuration register */
#define CMD_N25QXX_RVCR		0x85

/* Write volatile configuration register */
#define CMD_N25QXX_WVCR		0x81

/* Enter 4-byte address mode */
#define CMD_N25QXX_EN4B		0xB7

/* Exit 4-byte address mode */
#define CMD_N25QXX_EX4B		0xE9

#define VCR_XIP_SHIFT			(0x03)
#define VCR_XIP_MASK			(0x08)
#define VCR_DUMMY_CLK_CYCLES_SHIFT	(0x04)
#define VCR_DUMMY_CLK_CYCLES_MASK	(0xF0)

struct stmicro_spi_flash_params {
	u16 id;
	u16 pages_per_sector;
	u16 nr_sectors;
	const char *name;
};

static const struct stmicro_spi_flash_params stmicro_spi_flash_table[] = {
	{
		.id = 0x2011,
		.pages_per_sector = 128,
		.nr_sectors = 4,
		.name = "M25P10",
	},
	{
		.id = 0x2015,
		.pages_per_sector = 256,
		.nr_sectors = 32,
		.name = "M25P16",
	},
	{
		.id = 0x2012,
		.pages_per_sector = 256,
		.nr_sectors = 4,
		.name = "M25P20",
	},
	{
		.id = 0x2016,
		.pages_per_sector = 256,
		.nr_sectors = 64,
		.name = "M25P32",
	},
	{
		.id = 0x2013,
		.pages_per_sector = 256,
		.nr_sectors = 8,
		.name = "M25P40",
	},
	{
		.id = 0x2017,
		.pages_per_sector = 256,
		.nr_sectors = 128,
		.name = "M25P64",
	},
	{
		.id = 0x2014,
		.pages_per_sector = 256,
		.nr_sectors = 16,
		.name = "M25P80",
	},
	{
		.id = 0x2018,
		.pages_per_sector = 1024,
		.nr_sectors = 64,
		.name = "M25P128",
	},

	/* Numonyx */
	{
		.id = 0xba16,
		.pages_per_sector = 256,
		.nr_sectors = 64,
		.name = "N25Q32",
	},
	{
		.id = 0xbb16,
		.pages_per_sector = 256,
		.nr_sectors = 64,
		.name = "N25Q32A",
	},
	{
		.id = 0xba17,
		.pages_per_sector = 256,
		.nr_sectors = 128,
		.name = "N25Q64",
	},
	{
		.id = 0xba17,
		.pages_per_sector = 256,
		.nr_sectors = 128,
		.name = "N25Q64A",
	},
	{
		.id = 0xba18,
		.pages_per_sector = 256,
		.nr_sectors = 256,
		.name = "N25Q128",
	},
	{
		.id = 0xbb18,
		.pages_per_sector = 256,
		.nr_sectors = 256,
		.name = "N25Q128A",
	},
	{
		.id = 0xba19,
		.pages_per_sector = 256,
		.nr_sectors = 512,
		.name = "N25Q256",
	},
	{
		.id = 0xbb19,
		.pages_per_sector = 256,
		.nr_sectors = 512,
		.name = "N25Q256A",
	},
	{
		.id = 0xba20,
		.pages_per_sector = 256,
		.nr_sectors = 1024,
		.name = "N25Q512",
	},
	{
		.id = 0xbb20,
		.pages_per_sector = 256,
		.nr_sectors = 1024,
		.name = "N25Q512A",
	},
	{
		.id = 0xba21,
		.pages_per_sector = 256,
		.nr_sectors = 2048,
		.name = "N25Q00",
	},
	{
		.id = 0xbb21,
		.pages_per_sector = 256,
		.nr_sectors = 2048,
		.name = "N25Q00A",
	},
};

static int stmicro_set_vcr(struct spi_flash *flash, u8 clk_cycles, u8 xip)
{
	u8 cmd;
	u8 resp;
	int ret;

	ret = spi_flash_cmd_write_enable(flash);
	if (ret < 0) {
		debug("SF: enabling write failed\n");
		return ret;
	}

	ret = spi_flash_cmd(flash->spi, CMD_N25QXX_RVCR, (void *) &resp,
		sizeof(resp));
	if (ret < 0) {
		debug("SF: read volatile config register failed.\n");
		return ret;
	}

	resp &= ~VCR_DUMMY_CLK_CYCLES_MASK;
	resp |=  (clk_cycles << VCR_DUMMY_CLK_CYCLES_SHIFT) &
			VCR_DUMMY_CLK_CYCLES_MASK;

	/* To enable XIP, set VCR.XIP = 0 */
	if (xip)
		resp &= ~VCR_XIP_MASK;
	else
		resp |= VCR_XIP_MASK;

	cmd = CMD_N25QXX_WVCR;
	ret = spi_flash_cmd_write(flash->spi, &cmd, sizeof(cmd), &resp,
		sizeof(resp));
	if (ret) {
		debug("SF: fail to write vcr register\n");
		return ret;
	}

	return 0;
}

static int stmicro_set_4byte(struct spi_flash *flash, u8 enable)
{
	u8 cmd;
	int ret;

	ret = spi_flash_cmd_write_enable(flash);
	if (ret < 0) {
		debug("SF: enabling write failed\n");
		return ret;
	}

	cmd = enable ? CMD_N25QXX_EN4B : CMD_N25QXX_EX4B;

	ret = spi_flash_cmd_write(flash->spi, &cmd, sizeof(cmd), NULL, 0);
	if (ret) {
		debug("SF: fail to %s 4-byte address mode\n",
			enable ? "enter" : "exit");
		return ret;
	}

	return 0;
}

int stmicro_wait_flag_status_ready(struct spi_flash *flash)
{
	return spi_flash_cmd_poll_bit(flash, SPI_FLASH_PAGE_ERASE_TIMEOUT,
		CMD_READ_FLAG_STATUS, FLAG_STATUS_READY, 1);
}


#ifdef CONFIG_SPL_SPI_XIP
int stmicro_xip_enter(struct spi_flash *flash)
{
	char dummy[4];
	int ret;

	/* enable XiP in volatile configuration register */
	ret = stmicro_set_vcr(flash, 8, 1);
	if (ret) {
		debug("SF: enable XiP in volatile configuration register"
			"failed\n");
		return ret;
	}
	/* send fast read to start with xip confirmation bit 0 */
	ret = spi_flash_cmd_read_fast(flash, 0, 4, dummy);
	if (ret) {
		debug("SF: Send fast read with xip confirmation failed\n");
		return ret;
	}
	/* dummy cycle = 0 to keep XiP state */
	spi_enter_xip(flash->spi, 0);
	return 0;
}
#endif

struct spi_flash *spi_flash_probe_stmicro(struct spi_slave *spi, u8 * idcode)
{
	const struct stmicro_spi_flash_params *params;
	struct spi_flash *flash;
	unsigned int i;
	u16 id;

	if (idcode[0] == 0xff) {
		i = spi_flash_cmd(spi, CMD_M25PXX_RES,
				  idcode, 4);
		if (i)
			return NULL;
		if ((idcode[3] & 0xf0) == 0x10) {
			idcode[0] = 0x20;
			idcode[1] = 0x20;
			idcode[2] = idcode[3] + 1;
		} else
			return NULL;
	}

	id = ((idcode[1] << 8) | idcode[2]);

	for (i = 0; i < ARRAY_SIZE(stmicro_spi_flash_table); i++) {
		params = &stmicro_spi_flash_table[i];
		if (params->id == id) {
			break;
		}
	}

	if (i == ARRAY_SIZE(stmicro_spi_flash_table)) {
		debug("SF: Unsupported STMicro ID %04x\n", id);
		return NULL;
	}

	flash = calloc(sizeof(*flash), 1);
	if (!flash) {
		debug("SF: Failed to allocate memory\n");
		return NULL;
	}

	flash->spi = spi;
	flash->name = params->name;

	flash->write = spi_flash_cmd_write_multi;
	flash->erase = spi_flash_cmd_erase;
	flash->read = spi_flash_cmd_read_fast;
	flash->page_size = 256;
	flash->sector_size = 256 * params->pages_per_sector;
	flash->size = flash->sector_size * params->nr_sectors;
#ifdef CONFIG_SPL_SPI_XIP
	flash->xip_enter = stmicro_xip_enter;
#endif

	/* Micron flash above 512Mbit (512Mbit and 1Gbit) needs to poll
	flag status register after erase and write. */
	if (flash->size >= 0x4000000)	/* 512Mbit equal 64MByte */
		flash->poll_read_status = stmicro_wait_flag_status_ready;

	/*
	 * Numonyx flash have default 15 dummy clocks. Set dummy clocks to 8
	 * and XIP off.
	 */
	if (((id & 0xFF00) == 0xBA00) || ((id & 0xFF00) == 0xBB00))
		stmicro_set_vcr(flash, 8, 0);

	/* Enter 4-byte address mode if the device is exceed 16MiB. */
	if (flash->size > 0x1000000)
		stmicro_set_4byte(flash, 1);

	return flash;
}
