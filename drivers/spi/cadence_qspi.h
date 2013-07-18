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

#ifndef __CADENCE_QSPI_H__
#define __CADENCE_QSPI_H__

#define CQSPI_WRITEL		writel
#define CQSPI_READL		readl

#define QSPI_BASE			(CONFIG_CQSPI_BASE)
#define QSPI_AHB_BASE			(CONFIG_CQSPI_AHB_BASE)
#define CQSPI_IS_ADDR(cmd_len)		(cmd_len > 1 ? 1 : 0)

#define CQSPI_NO_DECODER_MAX_CS		(4)
#define CQSPI_DECODER_MAX_CS		(16)
#define CQSPI_READ_CAPTURE_MAX_DELAY	(16)

/* Functions call declaration */
void cadence_qspi_apb_controller_init(void *reg_base_addr);
void cadence_qspi_apb_controller_enable(void *reg_base_addr);
void cadence_qspi_apb_controller_disable(void *reg_base_addr);

int cadence_qspi_apb_command_read(void *reg_base_addr,
	unsigned int cmdlen, const u8 *cmdbuf, unsigned int rxlen, u8* rxbuf);
int cadence_qspi_apb_command_write(void *reg_base_addr,
	unsigned int cmdlen, const u8 *cmdbuf,
	unsigned int txlen,  const u8 *txbuf);

int cadence_qspi_apb_indirect_read_setup(void *reg_base,
	unsigned int ahb_phy_addr, unsigned int cmdlen, const u8 *cmdbuf);
int cadence_qspi_apb_indirect_read_execute(void *reg_base_addr,
	void *ahb_base_addr, unsigned int rxlen, u8 *rxbuf);

int cadence_qspi_apb_indirect_write_setup(void *reg_base,
	unsigned int ahb_phy_addr, unsigned int cmdlen, const u8 *cmdbuf);
int cadence_qspi_apb_indirect_write_execute(void *reg_base_addr,
	void *ahb_base_addr, unsigned int txlen, const u8 *txbuf);

void cadence_qspi_apb_chipselect(void *reg_base,
	unsigned int chip_select, unsigned int decoder_enable);
void cadence_qspi_apb_set_clk_mode(void *reg_base_addr,
	unsigned int clk_pol, unsigned int clk_pha);
void cadence_qspi_apb_config_baudrate_div(void *reg_base,
	unsigned int ref_clk_hz, unsigned int sclk_hz);
void cadence_qspi_apb_delay(void *reg_base,
	unsigned int ref_clk, unsigned int sclk_hz,
	unsigned int tshsl_ns, unsigned int tsd2d_ns,
	unsigned int tchsh_ns, unsigned int tslch_ns);
void cadence_qspi_apb_enter_xip(void *reg_base, char xip_dummy);
void cadence_qspi_apb_readdata_capture(void *reg_base,
	unsigned int bypass, unsigned int delay);

#endif /* __CADENCE_QSPI_H__ */
