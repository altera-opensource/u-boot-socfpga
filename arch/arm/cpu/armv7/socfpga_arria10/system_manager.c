/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/system_manager.h>

/*
 * Configure the pin mux registers
 */
void sysmgr_pinmux_init(unsigned long base_address,
	unsigned long *handoff_data, unsigned long length)
{
	unsigned long i;

	for (i = 0; i < length;	i++)
		writel(*handoff_data++, (base_address + (i * 4)));
}

/*
 * Configure the pin mux registers for dedicated only
 */
void sysmgr_pinmux_init_dedicated(unsigned long base_address_dedicated,
	unsigned long base_address_dedicated_cfg)
{
#if (CONFIG_PRELOADER_OVERWRITE_DEDICATED == 1)

	unsigned long i;

	writel(PINMUX_DEDICATED_CFG_VOLTAGE_SEL, base_address_dedicated_cfg);

/* configure pin mux according to boot source */
#if (CONFIG_PRELOADER_BOOT_FROM_QSPI == 1)
	/* i here is matching with register name which start from 1 */
	for (i = 4; i <= 9; i++)
		writel(4, (base_address_dedicated + (i - 1) * 4));
	writel(PINMUX_DEDICATED_CFG_QSPI_CLK,
		(base_address_dedicated_cfg + 4 * 4));
	writel(PINMUX_DEDICATED_CFG_QSPI_IO,
		(base_address_dedicated_cfg + 5 * 4));
	writel(PINMUX_DEDICATED_CFG_QSPI_SS,
		(base_address_dedicated_cfg + 6 * 4));
	writel(PINMUX_DEDICATED_CFG_QSPI_IO,
		(base_address_dedicated_cfg + 7 * 4));
	writel(PINMUX_DEDICATED_CFG_QSPI_IO,
		(base_address_dedicated_cfg + 8 * 4));
	writel(PINMUX_DEDICATED_CFG_QSPI_IO,
		(base_address_dedicated_cfg + 9 * 4));

#elif (CONFIG_PRELOADER_BOOT_FROM_SDMMC == 1)
	/* i here is matching with register name which start from 1 */
	for (i = 4; i <= 10; i++)
		writel(8, (base_address_dedicated + (i - 1) * 4));
	writel(PINMUX_DEDICATED_CFG_SD_DATA,
		(base_address_dedicated_cfg + 4 * 4));
	writel(PINMUX_DEDICATED_CFG_SD_CMD,
		(base_address_dedicated_cfg + 5 * 4));
	writel(PINMUX_DEDICATED_CFG_SD_CCLK,
		(base_address_dedicated_cfg + 6 * 4));
	writel(PINMUX_DEDICATED_CFG_SD_DATA,
		(base_address_dedicated_cfg + 7 * 4));
	writel(PINMUX_DEDICATED_CFG_SD_DATA,
		(base_address_dedicated_cfg + 8 * 4));
	writel(PINMUX_DEDICATED_CFG_SD_DATA,
		(base_address_dedicated_cfg + 9 * 4));
	writel(PINMUX_DEDICATED_CFG_SD_PWREN,
		(base_address_dedicated_cfg + 10 * 4));
#if (PINMUX_DEDICATED_SD_WIDTH == 8)
	for (i = 12; i <= 15; i++)
		writel(8, (base_address_dedicated + (i - 1) * 4));
	writel(PINMUX_DEDICATED_CFG_SD_DATA,
		(base_address_dedicated_cfg + 12 * 4));
	writel(PINMUX_DEDICATED_CFG_SD_DATA,
		(base_address_dedicated_cfg + 13 * 4));
	writel(PINMUX_DEDICATED_CFG_SD_DATA,
		(base_address_dedicated_cfg + 14 * 4));
	writel(PINMUX_DEDICATED_CFG_SD_DATA,
		(base_address_dedicated_cfg + 15 * 4));
#endif /* PINMUX_DEDICATED_SD_WIDTH == 8 */

#elif (CONFIG_PRELOADER_BOOT_FROM_NAND == 1)
#error "NAND is not supported for this mode"

#else
#error "No boot device selected within build.h handoff file"
#endif

#ifdef PINMUX_DEDICATED_UART1_TX_IO_LOCATION
	writel(13, (base_address_dedicated +
		(PINMUX_DEDICATED_UART1_TX_IO_LOCATION - 1) * 4));
	writel(PINMUX_DEDICATED_CFG_UART_TX, (base_address_dedicated_cfg +
		PINMUX_DEDICATED_UART1_TX_IO_LOCATION * 4));
#endif
#ifdef PINMUX_DEDICATED_UART1_RX_IO_LOCATION
	writel(13, (base_address_dedicated +
		(PINMUX_DEDICATED_UART1_RX_IO_LOCATION - 1) * 4));
	writel(PINMUX_DEDICATED_CFG_UART_RX, (base_address_dedicated_cfg +
		PINMUX_DEDICATED_UART1_RX_IO_LOCATION * 4));
#endif

#endif /* CONFIG_PRELOADER_OVERWRITE_DEDICATED */
}

