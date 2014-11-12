
/*
 * This file is not generated handoff file. Used to manual overwrite the
 * dedicated IO configuration from Qsys. Useful for initial board bring where
 * you won't need tools to reconfigure pinmux and IOCSR. Only applicable if
 * boot device is located within dedicated IO
 */

#ifndef _PRELOADER_PINMUX_DEDICATED_CONFIG_H_
#define _PRELOADER_PINMUX_DEDICATED_CONFIG_H_

#include "build.h"

/***********************************************************************/

/*
 * Steps to overwrite dedicated IO configuration from Qsys.
 *
 * 1. Select boot option at build.h (only for QSPI and SDMMC boot)
 * 2. Set macro CONFIG_PRELOADER_OVERWRITE_DEDICATED to (1)
 * 3. Configure the UART1 IO location
 * 4. Configure the SD width (for SDMMC boot only)
 * 5. Build and load
 */

/* Option to overwrite dedicated IO configuration from Qsys. */
#define CONFIG_PRELOADER_OVERWRITE_DEDICATED	(1)

/*
 * Location of UART pins at dedicated IOs (valid value from 10 to 17).
 * Will be ignore if boot from NAND. Refer documentation for pin numbers
 */
#define PINMUX_DEDICATED_UART1_TX_IO_LOCATION	(16)
#define PINMUX_DEDICATED_UART1_RX_IO_LOCATION	(17)

/* Number of data pins used (valid value is 4 or 8) */
#define PINMUX_DEDICATED_SD_WIDTH		(4)

/* Voltage (valid value VOLTAGE_SEL_3V, VOLTAGE_SEL_1P8V, VOLTAGE_SEL_2P5V) */
#define PINMUX_DEDICATED_VOLTAGE		VOLTAGE_SEL_3V

/***********************************************************************/

/*
 * You won't need to modify the code below here unless you want to change the
 * IO buffer setting. Do always refer back to documentation when you are about
 * to do this
 */

/* dedicated IO checking and assignment */
#if (CONFIG_PRELOADER_OVERWRITE_DEDICATED == 1)

#define PINMUX_DEDICATED_CFG_VOLTAGE_SEL 	\
	(PINMUX_DEDICATED_VOLTAGE << VOLTAGE_SEL_LSB)

/*
 * Configure IO buffer setting of dedicated IO. Valid configuraton as below
 * > INPUT_BUF_DISABLE, INPUT_BUF_1P8V, INPUT_BUF_2P5V3V
 * > WK_PU_DISABLE, WK_PU_ENABLE
 * > PU_SLW_RT_SLOW, PU_SLW_RT_FAST, PU_SLW_RT_DEFAULT
 * > PD_SLW_RT_SLOW, PD_SLW_RT_FAST, PD_SLW_RT_DEFAULT
 * > PU_DRV_STRG_DEFAULT, PD_DRV_STRG_DEFAULT
 * More details at arch/arm/include/asm/arch-socfpga10/system_manager.h
 */

#if (CONFIG_PRELOADER_BOOT_FROM_QSPI == 1)
#if (PINMUX_DEDICATED_VOLTAGE == 1)
/* For QSPI 1.8V */
#define PINMUX_DEDICATED_CFG_QSPI_CLK			\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_QSPI_SS			\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_ENABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_QSPI_IO			\
	(0x2			<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_1P8V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#else
/* For QSPI 3.3V and 2.5V */
#define PINMUX_DEDICATED_CFG_QSPI_CLK			\
	(0x5		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_QSPI_SS			\
	(0x5		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_ENABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_QSPI_IO			\
	(0x5			<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_2P5V3V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#endif /* PINMUX_DEDICATED_VOLTAGE */

#elif (CONFIG_PRELOADER_BOOT_FROM_SDMMC == 1)
#if (PINMUX_DEDICATED_VOLTAGE == 1)
/* For SDMMC 1.8V */
#define PINMUX_DEDICATED_CFG_SD_CMD			\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_1P8V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_SD_CCLK			\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_SD_PWREN			\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_ENABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_SD_DATA			\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2			<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_1P8V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#else
/* For SDMMC 3.3V and 2.5V */
#define PINMUX_DEDICATED_CFG_SD_CMD			\
	(0x5		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_2P5V3V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_SD_CCLK			\
	(0x5		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_SD_PWREN			\
	(0x5		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_ENABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_SD_DATA			\
	(0x5		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6			<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_2P5V3V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#endif /* PINMUX_DEDICATED_VOLTAGE */


#elif (CONFIG_PRELOADER_BOOT_FROM_NAND == 1)
#if (PINMUX_DEDICATED_VOLTAGE == 1)
/* For NAND 1.8V */
#define PINMUX_DEDICATED_CFG_NAND_CE_RE_WE		\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_ENABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_NAND_ALE_CLE		\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_NAND_RB			\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_1P8V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_NAND_DATA			\
	(0x2		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x2			<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_1P8V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#else
/* For NAND 3.3V and 2.5V */
#define PINMUX_DEDICATED_CFG_NAND_CE_RE_WE		\
	(0x5		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_ENABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_NAND_ALE_CLE		\
	(0x5 			<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_NAND_RB			\
	(0x5 			<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6		 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_2P5V3V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_NAND_DATA			\
	(0x5		 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_DEFAULT 	<< PD_SLW_RT_LSB) |	\
	(0x6			<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_DEFAULT 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_2P5V3V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#endif /* PINMUX_DEDICATED_VOLTAGE */

#else
#error "No boot device selected within build.h handoff file"
#endif


/*
 * Populate the UART configuration.
 */
#if (PINMUX_DEDICATED_VOLTAGE == 1)
/* UART 1.8V */
#define PINMUX_DEDICATED_CFG_UART_TX			\
	(PD_DRV_STRG_DEFAULT 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_FAST 	<< PD_SLW_RT_LSB) |	\
	(PU_DRV_STRG_DEFAULT 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_FAST 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_UART_RX			\
	(PD_DRV_STRG_DEFAULT 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_FAST 	<< PD_SLW_RT_LSB) |	\
	(PU_DRV_STRG_DEFAULT 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_FAST 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(VOLTAGE_SEL_1P8V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#else
/* UART 3.3V and 2.5V */
#define PINMUX_DEDICATED_CFG_UART_TX			\
	(PD_DRV_STRG_DEFAULT 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_FAST 	<< PD_SLW_RT_LSB) |	\
	(PU_DRV_STRG_DEFAULT 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_FAST 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(INPUT_BUF_DISABLE 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#define PINMUX_DEDICATED_CFG_UART_RX			\
	(PD_DRV_STRG_DEFAULT 	<< PD_DRV_STRG_LSB) |	\
	(PD_SLW_RT_FAST 	<< PD_SLW_RT_LSB) |	\
	(PU_DRV_STRG_DEFAULT 	<< PU_DRV_STRG_LSB) |	\
	(PU_SLW_RT_FAST 	<< PU_SLW_RT_LSB) |	\
	(WK_PU_DISABLE 		<< WK_PU_LSB) |		\
	(VOLTAGE_SEL_1P8V 	<< INPUT_BUF_LSB) |	\
	(0x1		 	<< BIAS_TRIM_LSB)
#endif

/*
 * Do checking ensure no pin conflict between flash and UART
 */
#if (CONFIG_PRELOADER_BOOT_FROM_QSPI == 1)
/* QSPI */
#if !(PINMUX_DEDICATED_UART1_TX_IO_LOCATION >= 10 && \
	PINMUX_DEDICATED_UART1_TX_IO_LOCATION <= 17)
#error "UART1 TX pin conflict with QSPI pins "
#endif
#if !(PINMUX_DEDICATED_UART1_RX_IO_LOCATION >= 10 && \
	PINMUX_DEDICATED_UART1_RX_IO_LOCATION <= 17)
#error "UART1 RX pin conflict with QSPI pins "
#endif

#elif (CONFIG_PRELOADER_BOOT_FROM_SDMMC == 1)
/* SDMMC x8 */
#if (PINMUX_DEDICATED_SD_WIDTH == 8)
#if !(PINMUX_DEDICATED_UART1_TX_IO_LOCATION == 11 | \
	PINMUX_DEDICATED_UART1_TX_IO_LOCATION == 16 | \
	PINMUX_DEDICATED_UART1_TX_IO_LOCATION == 17)
#error "UART1 TX pin conflict with SDMMC x8 pins "
#endif
#if !(PINMUX_DEDICATED_UART1_RX_IO_LOCATION == 11 | \
	PINMUX_DEDICATED_UART1_RX_IO_LOCATION == 16 | \
	PINMUX_DEDICATED_UART1_RX_IO_LOCATION == 17)
#error "UART1 RX pin conflict with SDMMC x8 pins "
#endif
#else	/* PINMUX_DEDICATED_SD_WIDTH == 4 */
/* SDMMC x4 */
#if !(PINMUX_DEDICATED_UART1_TX_IO_LOCATION >= 11 && \
	PINMUX_DEDICATED_UART1_TX_IO_LOCATION <= 17)
#error "UART1 TX pin conflict with SDMMC x4 pins "
#endif
#if !(PINMUX_DEDICATED_UART1_RX_IO_LOCATION >= 11 && \
	PINMUX_DEDICATED_UART1_RX_IO_LOCATION <= 17)
#error "UART1 RX pin conflict with SDMMC x4 pins "
#endif
#endif	/* PINMUX_DEDICATED_SD_WIDTH */

#else
/* NAND use all the pins. No more UART */
#undef PINMUX_DEDICATED_UART1_TX_IO_LOCATION
#undef PINMUX_DEDICATED_UART1_RX_IO_LOCATION
#endif

#endif /* CONFIG_PRELOADER_OVERWRITE_DEDICATED */

/***********************************************************************/

#endif /* _PRELOADER_PINMUX_DEDICATED_CONFIG_H_ */
