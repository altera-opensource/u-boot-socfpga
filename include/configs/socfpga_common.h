/*
 *  Copyright Altera Corporation (C) 2013. All rights reserved
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms and conditions of the GNU General Public License,
 *  version 2, as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CONFIG_COMMON_H
#define __CONFIG_COMMON_H

#include <asm/arch/socfpga_base_addrs.h>
#include "../../board/altera/socfpga/build.h"
#include "../../board/altera/socfpga/pinmux_config.h"
#include "../../board/altera/socfpga/pll_config.h"

/* Enabled for U-Boot debug message printout? */
/*#define DEBUG*/
/* if panic, will call hang as watchdog will come and trigger warm reset */
#if defined(DEBUG) && defined(CONFIG_SPL_BUILD)
#define CONFIG_PANIC_HANG
#endif

/*
 * Quick tips for debugging
 * > Before relocation, load elf for debugger as normal usage
 * > After relocation, you need to type below command at gdb
 * (gdb) symbol-file
 * (gdb) add-symbol-file u-boot <u-boot new address offset>
 *
 * > you can get the offset from "bdinfo" console command under reloc_off
 */

/*
 * High level configuration
 */
/* Running on virtual target? */
#undef CONFIG_SOCFPGA_VIRTUAL_TARGET
/* ARMv7 CPU Core */
#define CONFIG_ARMV7
/* Cache policy for CA-9 is write back, write allocate */
#define CONFIG_SYS_ARM_CACHE_WRITEBACK_WRITEALLOCATE
/* Support for IRQs - for ocram and SDRAM ECC */
#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define CONFIG_USE_IRQ
#endif
/* miscellaneous platform dependent initialisations */
#define CONFIG_MISC_INIT_R
/* Use as single bootloader where no boot ROM and preloader */
#define CONFIG_SINGLE_BOOTLOADER
/* SOCFPGA Specific function */
#define CONFIG_SOCFPGA
/* base address for .text section */
#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define CONFIG_SYS_TEXT_BASE		0x08000040
#else
#define CONFIG_SYS_TEXT_BASE		0x01000040
#endif
/* Default load address */
#define CONFIG_SYS_LOAD_ADDR		0x8000
#ifdef CONFIG_USE_IRQ
/* Enable board late init for ECC setup if IRQ enabled */
#define CONFIG_BOARD_LATE_INIT
#endif

/*
 * Display CPU and Board Info
 */
#define CONFIG_DISPLAY_CPUINFO
#define CONFIG_DISPLAY_BOARDINFO

/*
 * Enable early stage initialization at C environment
 */
#define CONFIG_BOARD_EARLY_INIT_F

/*
 * Kernel Info
 */
/* flat device tree */
#define CONFIG_OF_LIBFDT
/* skip updating the FDT blob */
#define CONFIG_FDT_BLOB_SKIP_UPDATE
/* Initial Memory map size for Linux, minus 4k alignment for DFT blob */
#define CONFIG_SYS_BOOTMAPSZ		((256*1024*1024) - (4*1024))

/*
 * Memory allocation (MALLOC)
 */
/* Room required on the stack for the environment data */
#define CONFIG_ENV_SIZE			4096
/* Size of DRAM reserved for malloc() use */
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE + 128*1024)

/*
 * Stack setup
 */
#ifdef CONFIG_USE_IRQ
/* IRQ stack */
#define CONFIG_STACKSIZE_IRQ		(1 << 10)
/* FIQ stack */
#define CONFIG_STACKSIZE_FIQ		(1 << 10)
#endif
/* SP location before relocation, must use scratch RAM */
#define CONFIG_SYS_INIT_RAM_ADDR	0xFFFF0000
/* Reserving 0x100 space at back of scratch RAM for debug info */
#define CONFIG_SYS_INIT_RAM_SIZE	(0x10000 - 0x100)
/* Stack pointer prior relocation, must situated at on-chip RAM */
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_INIT_RAM_ADDR + \
					 CONFIG_SYS_INIT_RAM_SIZE - \
					 GENERATED_GBL_DATA_SIZE)

/*
 * Command line configuration.
 */
#define CONFIG_SYS_NO_FLASH
#include <config_cmd_default.h>
/* FAT file system support */
#define CONFIG_CMD_FAT
/* Enable FAT write support */
#define CONFIG_FAT_WRITE
/* EXT2 file system support */
#define CONFIG_CMD_EXT2
/* MMC support */
#define CONFIG_CMD_MMC

/*
 * Misc
 */
/* Enable DOS partition */
#define CONFIG_DOS_PARTITION            1


/* OSE operating system support */
#define CONFIG_BOOTM_OSE

/* Support loading of zImage */
#define CONFIG_CMD_BOOTZ


/*
 * Environment setup
 */

/* Delay before automatically booting the default image */
#define CONFIG_BOOTDELAY		5
/* write protection for vendor parameters is completely disabled */
#define CONFIG_ENV_OVERWRITE
/* Enable auto completion of commands using TAB */
#define CONFIG_AUTO_COMPLETE
/* Additional help message */
#define CONFIG_SYS_LONGHELP
/* use "hush" command parser */
#define CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#define CONFIG_CMD_RUN

#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define CONFIG_BOOTCOMMAND "run ramboot"
#else
#define CONFIG_BOOTCOMMAND "run mmcload; run mmcboot"
#endif

/*
 * arguments passed to the bootz command. The value of
 * CONFIG_BOOTARGS goes into the environment value "bootargs".
 * Do note the value will overide also the chosen node in FDT blob.
 */
#define CONFIG_BOOTARGS "console=ttyS0," __stringify(CONFIG_BAUDRATE)

#define CONFIG_EXTRA_ENV_SETTINGS \
	"verify=n\0" \
	"loadaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"fdtaddr=0x00000100\0" \
	"bootimage=zImage\0" \
	"bootimagesize=0x600000\0" \
	"fdtimage=socfpga.dtb\0" \
	"fdtimagesize=0x5000\0" \
	"mmcloadcmd=fatload\0" \
	"mmcloadpart=1\0" \
	"mmcroot=/dev/mmcblk0p2\0" \
	"qspiloadcs=0\0" \
	"qspibootimageaddr=0xa0000\0" \
	"qspifdtaddr=0x50000\0" \
	"qspiroot=/dev/mtdblock1\0" \
	"qspirootfstype=jffs2\0" \
	"ramboot=setenv bootargs " CONFIG_BOOTARGS ";" \
		"bootz ${loadaddr} - ${fdtaddr}\0" \
	"mmcload=mmc rescan;" \
		"${mmcloadcmd} mmc 0:${mmcloadpart} ${loadaddr} ${bootimage};" \
		"${mmcloadcmd} mmc 0:${mmcloadpart} ${fdtaddr} ${fdtimage}\0" \
	"mmcboot=setenv bootargs " CONFIG_BOOTARGS \
		" root=${mmcroot} rw rootwait;" \
		"bootz ${loadaddr} - ${fdtaddr}\0" \
	"netboot=dhcp ${bootimage} ; " \
		"tftp ${fdtaddr} ${fdtimage} ; run ramboot\0" \
	"qspiload=sf probe ${qspiloadcs};" \
		"sf read ${loadaddr} ${qspibootimageaddr} ${bootimagesize};" \
		"sf read ${fdtaddr} ${qspifdtaddr} ${fdtimagesize};\0" \
	"qspiboot=setenv bootargs " CONFIG_BOOTARGS \
		" root=${qspiroot} rw rootfstype=${qspirootfstype};"\
		"bootz ${loadaddr} - ${fdtaddr}\0" \
	"fpga=0\0" \
	"fpgadata=0x2000000\0" \
	"fpgadatasize=0x700000\0"


/*
 * Environment setup
 */
/* using environment setting for stdin, stdout, stderr */
#define CONFIG_SYS_CONSOLE_IS_IN_ENV
/* Enable the call to overwrite_console() */
#define CONFIG_SYS_CONSOLE_OVERWRITE_ROUTINE
/* Enable overwrite of previous console environment settings */
#define CONFIG_SYS_CONSOLE_ENV_OVERWRITE
/* Environment will save to boot device/flash */
#if (CONFIG_PRELOADER_BOOT_FROM_SDMMC == 1)
/* Store envirnoment in MMC card */
#define CONFIG_ENV_IS_IN_MMC
#else
/* Store envirnoment in SPI flash */
#define CONFIG_ENV_IS_IN_SPI_FLASH
#endif

/* environment setting for SPI flash */
#ifdef CONFIG_ENV_IS_IN_SPI_FLASH
#define CONFIG_ENV_OFFSET		(0x00040000)
#define CONFIG_ENV_SECT_SIZE		(64 * 1024)
#define CONFIG_ENV_SPI_BUS		0
#define CONFIG_ENV_SPI_CS		0
#define CONFIG_ENV_SPI_MODE		SPI_MODE_3
#define CONFIG_ENV_SPI_MAX_HZ		CONFIG_SF_DEFAULT_SPEED
#endif

/* environment setting for MMC */
#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV		0	/* device 0 */
#define CONFIG_ENV_OFFSET		512	/* just after the MBR */
#endif

/*
 * Console setup
 */
/* Console I/O Buffer Size */
#define CONFIG_SYS_CBSIZE		256
/* Print Buffer Size */
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)
/* max number of command args */
#define CONFIG_SYS_MAXARGS		16

/* Boot Argument Buffer Size */
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE

/*
 * Hardware drivers
 */

/*
 * SDRAM Memory Map
 */
/* We have 1 bank of DRAM */
#define CONFIG_NR_DRAM_BANKS		1
/* SDRAM Bank #1 */
#define CONFIG_SYS_SDRAM_BASE		0x00000000
/* SDRAM memory size */
#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define PHYS_SDRAM_1_SIZE		0x80000000
#else
#define PHYS_SDRAM_1_SIZE		0x40000000
#endif
/* SDRAM Bank #1 base address */
#define PHYS_SDRAM_1			CONFIG_SYS_SDRAM_BASE
/* memtest setup */
/* Begin and end addresses of the area used by the simple memory test.c */
#define CONFIG_SYS_MEMTEST_START	0x00000000
#define CONFIG_SYS_MEMTEST_END		PHYS_SDRAM_1_SIZE

/*
 * L2 PL-310
 */
/* for configuring L2 address filtering start address in assembly */
#define SOCFPGA_MPUL2_ADRFLTR_START	(0xC00)

/*
 * Generic Interrupt Controller from ARM
 */
#define SOCFPGA_GIC_CPU_IF		(SOCFPGA_MPUSCU_ADDRESS + 0x100)
#define SOCFPGA_GIC_DIC			(SOCFPGA_MPUSCU_ADDRESS + 0x1000)

/*
 * SCU Non-secure Access Control
 */
#define SOCFPGA_SCU_SNSAC		(SOCFPGA_MPUSCU_ADDRESS + 0x54)

/*
 * NS16550 Configuration
 */
#define CONFIG_SYS_NS16550
#ifdef CONFIG_SYS_NS16550
#define UART0_BASE			SOCFPGA_UART0_ADDRESS
#define CONFIG_SYS_NS16550_SERIAL
#define CONFIG_SYS_NS16550_REG_SIZE	-4
#define CONFIG_SYS_NS16550_CLK          V_NS16550_CLK
#define CONFIG_CONS_INDEX               1
#define CONFIG_SYS_NS16550_COM1		UART0_BASE
#define CONFIG_SYS_BAUDRATE_TABLE {4800, 9600, 19200, 38400, 57600, 115200}
#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define V_NS16550_CLK			1000000
#else
#define V_NS16550_CLK			CONFIG_HPS_CLK_L4_SP_HZ
#endif
#define CONFIG_BAUDRATE			57600
#endif /* CONFIG_SYS_NS16550 */

/*
 * FLASH
 */
#define CONFIG_SYS_NO_FLASH

/*
 * USB
 */
/*#define CONFIG_CMD_USB		1
#define CONFIG_USB_STORAGE		1
#define CONFIG_USB_DWC_OTG_HCD		1
#define CONFIG_DOS_PARTITION		1*/

/*
 * L4 OSC1 Timer 0
 */
/* This timer use eosc1 where the clock frequency is fixed
 * throughout any condition */
#define CONFIG_SYS_TIMERBASE		SOCFPGA_OSC1TIMER0_ADDRESS
/* reload value when timer count to zero */
#define TIMER_LOAD_VAL			0xFFFFFFFF
/* Timer info */
#define CONFIG_SYS_HZ			1000
#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define CONFIG_TIMER_CLOCK_KHZ		2400
#else
#define CONFIG_TIMER_CLOCK_KHZ		(CONFIG_HPS_CLK_OSC1_HZ/1000)
#endif

/*
 * L4 Watchdog
 */
#define CONFIG_HW_WATCHDOG
#define CONFIG_HW_WATCHDOG_TIMEOUT_MS	(2000)
#define CONFIG_DESIGNWARE_WATCHDOG
#define CONFIG_DW_WDT_BASE		SOCFPGA_L4WD0_ADDRESS
#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define CONFIG_DW_WDT_CLOCK_KHZ		2400
#else
#define CONFIG_DW_WDT_CLOCK_KHZ		(CONFIG_HPS_CLK_OSC1_HZ/1000)
#endif

/*
 * network support
 */
#ifndef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define CONFIG_DESIGNWARE_ETH		1
#endif

#ifdef CONFIG_DESIGNWARE_ETH
#define CONFIG_EMAC0_BASE		SOCFPGA_EMAC0_ADDRESS
#define CONFIG_EMAC1_BASE		SOCFPGA_EMAC1_ADDRESS
#ifdef CONFIG_CMD_NET
#undef CONFIG_CMD_NET
#endif
#define CONFIG_CMD_NET			1
#define CONFIG_CMD_PING			1
#define CONFIG_CMD_DHCP			1
#define CONFIG_NET_MULTI		1
#define CONFIG_DW_ALTDESCRIPTOR		1
#define CONFIG_EPHY0_PHY_ADDR		0
#define CONFIG_EPHY1_PHY_ADDR		4
#define CONFIG_DW_SEARCH_PHY		1
#define CONFIG_MII			1
#define CONFIG_CMD_MII			1
#define CONFIG_PHY_GIGE			1
#define CONFIG_DW_AUTONEG		1
#define CONFIG_AUTONEG_TIMEOUT	(15 * CONFIG_SYS_HZ)
#endif

/*
 * MMC support
 */
#define CONFIG_MMC
#ifdef CONFIG_MMC
#define CONFIG_SDMMC_BASE		(SOCFPGA_SDMMC_ADDRESS)
#define CONFIG_SDMMC_HOST_HS
#define CONFIG_GENERIC_MMC		1
#define CONFIG_DWMMC			1
#define CONFIG_ALTERA_DWMMC		1
#define CONFIG_DWMMC_FIFO_DEPTH		1024
#endif

/*
 * MTD
 */
#define CONFIG_CMD_MTDPARTS		/* Enable MTD parts commands */
#define CONFIG_MTD_DEVICE		/* needed for mtdparts commands */
#define CONFIG_MTD_PARTITIONS

/*
 * QSPI support
 */
#define CONFIG_CADENCE_QSPI
#define CONFIG_CQSPI_BASE		(SOCFPGA_QSPIREGS_ADDRESS)
#define CONFIG_CQSPI_AHB_BASE		(SOCFPGA_QSPIDATA_ADDRESS)
#ifdef CONFIG_CADENCE_QSPI
#define CONFIG_SPI_FLASH		/* SPI flash subsystem */
#define CONFIG_SPI_FLASH_STMICRO	/* Micron/Numonyx flash */
#define CONFIG_CMD_SF			/* Serial flash commands */
/* Flash device info */
#define CONFIG_SF_DEFAULT_SPEED		(50000000)
#define CONFIG_SF_DEFAULT_MODE		SPI_MODE_3
#define CONFIG_SPI_FLASH_QUAD		(1)
/* QSPI page size and block size */
#define CONFIG_CQSPI_REF_CLK		CONFIG_HPS_CLK_QSPI_HZ
#define CONFIG_CQSPI_PAGE_SIZE		(256)
#define CONFIG_CQSPI_BLOCK_SIZE		(16)
/* QSPI Delay timing */
#define CONFIG_CQSPI_TSHSL_NS		(200)
#define CONFIG_CQSPI_TSD2D_NS		(255)
#define CONFIG_CQSPI_TCHSH_NS		(20)
#define CONFIG_CQSPI_TSLCH_NS		(20)
#define CONFIG_CQSPI_DECODER		(0)
#ifdef CONFIG_SOCFPGA_VIRTUAL_TARGET
#define CONFIG_CQSPI_4BYTE_ADDR		(0)
#else
#define CONFIG_CQSPI_4BYTE_ADDR		(1)
#endif /* CONFIG_SOCFPGA_VIRTUAL_TARGET */
#define CONFIG_CQSPI_READDATA_DELAY	(2)
#endif	/* CONFIG_CADENCE_QSPI */

/*
 * FPGA support
 */
/* Enables FPGA subsystem */
#define CONFIG_FPGA
/* Altera FPGA */
#define CONFIG_FPGA_ALTERA
/* Family type */
#define CONFIG_FPGA_SOCFPGA
/* Only support single device */
#define CONFIG_FPGA_COUNT		(1)
/* Enable FPGA command at console */
#define CONFIG_CMD_FPGA


/*
 * SPL "Second Program Loader" aka Preloader
 */

/* Enable building of SPL globally */
#define CONFIG_SPL

/* Enable the SPL framework under common */
#define CONFIG_SPL_FRAMEWORK

/* TEXT_BASE for linking the SPL binary */
#ifndef CONFIG_PRELOADER_EXE_ON_FPGA
#error "CONFIG_PRELOADER_EXE_ON_FPGA must be defined"
#endif
#if (CONFIG_PRELOADER_EXE_ON_FPGA == 1)
#define CONFIG_SPL_TEXT_BASE		0xC0000000
#else
#define CONFIG_SPL_TEXT_BASE		0xFFFF0000
#endif

/* SPL max size */
#define CONFIG_SPL_MAX_SIZE		(64 * 1024)

/* Support for drivers/serial in SPL binary */
#ifndef CONFIG_PRELOADER_SERIAL_SUPPORT
#error "CONFIG_PRELOADER_SERIAL_SUPPORT must be defined"
#endif
#if (CONFIG_PRELOADER_SERIAL_SUPPORT == 1)
#define CONFIG_SPL_SERIAL_SUPPORT
#endif

/* Enable spl_board_init function call */
#define CONFIG_SPL_BOARD_INIT

/*
 * define the chunk size for crc32_wd().
 * Current define smaller to avoid watchdog timeout
 */
#define CHUNKSZ_CRC32			(1 * 1024)

#define CONFIG_CRC32_VERIFY

/*
 * Linker script for SPL
 */
#if (CONFIG_PRELOADER_EXE_ON_FPGA == 1)
#define CONFIG_SPL_LDSCRIPT "arch/arm/cpu/armv7/socfpga/u-boot-spl-fpga.lds"
#else
#define CONFIG_SPL_LDSCRIPT "arch/arm/cpu/armv7/socfpga/u-boot-spl.lds"
#endif

/*
 * Support for common/libcommon.o in SPL binary
 */
#define CONFIG_SPL_LIBCOMMON_SUPPORT

/*
 * Support for drivers/watchdog.o in SPL binary
 */
#ifndef CONFIG_PRELOADER_WATCHDOG_ENABLE
#error "CONFIG_PRELOADER_WATCHDOG_ENABLE must be defined"
#endif
#if (CONFIG_PRELOADER_WATCHDOG_ENABLE == 1)
#define CONFIG_SPL_WATCHDOG_SUPPORT
#else
#ifdef CONFIG_SPL_BUILD
#undef CONFIG_HW_WATCHDOG
#endif
#endif

/*
 * Boot from SDMMC
 */
#ifndef CONFIG_PRELOADER_BOOT_FROM_SDMMC
#error "CONFIG_PRELOADER_BOOT_FROM_SDMMC must be defined"
#endif
#if (CONFIG_PRELOADER_BOOT_FROM_SDMMC == 1)
/* Support for drivers/mmc/libmmc.o in SPL binary */
#define CONFIG_SPL_MMC_SUPPORT
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR \
	(CONFIG_PRELOADER_SDMMC_NEXT_BOOT_IMAGE / 512)
#endif

/*
 * Boot from QSPI
 */
#ifndef CONFIG_PRELOADER_BOOT_FROM_QSPI
#error "CONFIG_PRELOADER_BOOT_FROM_QSPI must be defined"
#endif
#if (CONFIG_PRELOADER_BOOT_FROM_QSPI == 1)
/* Enable spi_spl_load.c */
#define CONFIG_SPL_SPI_LOAD
/* Support for drivers/mtd/spi/libspi_flash.o in SPL binary */
#define CONFIG_SPL_SPI_FLASH_SUPPORT
/* Support for drivers/spi/libspi.o in SPL binary */
#define CONFIG_SPL_SPI_SUPPORT
/* Support for XiP */
#undef CONFIG_SPL_SPI_XIP
/* the XiP address that SPL will jump to */
#define CONFIG_SPL_SPI_XIP_ADDR		0xFFA00040
/* SPL SPI flash Chip select */
#define CONFIG_SPL_SPI_CS		0
/* SPL SPI flash Bus Number */
#define CONFIG_SPL_SPI_BUS		0
/* offset of U-Boot with spi flash */
#define CONFIG_SYS_SPI_U_BOOT_OFFS	CONFIG_PRELOADER_QSPI_NEXT_BOOT_IMAGE
#endif

/*
 * Boot from RAM
 */
#ifndef CONFIG_PRELOADER_BOOT_FROM_RAM
#error "CONFIG_PRELOADER_BOOT_FROM_RAM must be defined"
#endif
#if (CONFIG_PRELOADER_BOOT_FROM_RAM == 1)
#define CONFIG_SPL_RAM_DEVICE
#endif

/*
 * Support for checksum in SPL binary
 */
#ifndef CONFIG_PRELOADER_CHECKSUM_NEXT_IMAGE
#error "CONFIG_PRELOADER_CHECKSUM_NEXT_IMAGE must be defined"
#endif
#if (CONFIG_PRELOADER_CHECKSUM_NEXT_IMAGE == 1)
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_CHECKSUM_NEXT_IMAGE
#endif

/*
 * Preloader State Register address. Write STATE_VALID to STATE_REG to
 * tell BootROM that Preloader run successfully.
 */
#ifndef CONFIG_PRELOADER_STATE_REG_ENABLE
#error "CONFIG_PRELOADER_STATE_REG_ENABLE must be defined"
#endif
#if (CONFIG_PRELOADER_STATE_REG_ENABLE == 1)
#define CONFIG_PRELOADER_STATE_REG	(0xFFD080C8)
#define CONFIG_PRELOADER_STATE_VALID	(0x49535756)
#endif

/*
 * Support for ARM semihosting in SPL
 */
#ifndef CONFIG_PRELOADER_SEMIHOSTING
#error "CONFIG_PRELOADER_SEMIHOSTING must be defined"
#endif
#if defined(CONFIG_SPL_BUILD) && (CONFIG_PRELOADER_SEMIHOSTING == 1)
#define CONFIG_SPL_SEMIHOSTING_SUPPORT
#endif

/*
 * Support for FAT partition if boot from SDMMC
 */
#if (CONFIG_PRELOADER_BOOT_FROM_SDMMC == 1)
/* MMC with FAT partition support */
#undef CONFIG_SPL_FAT_SUPPORT
#endif

#ifdef CONFIG_SPL_FAT_SUPPORT
#define CONFIG_SPL_LIBDISK_SUPPORT
#define CONFIG_SYS_MMC_SD_FAT_BOOT_PARTITION	1
#define CONFIG_SPL_FAT_LOAD_PAYLOAD_NAME	"u-boot.img"
#endif /* CONFIG_SPL_FAT_SUPPORT */

/*
 * Stack setup
 */
#define CONFIG_SPL_STACK		CONFIG_SYS_INIT_SP_ADDR
#define CONFIG_SYS_SPL_MALLOC_START	((unsigned long) (&__malloc_start))
#define CONFIG_SYS_SPL_MALLOC_SIZE	(&__malloc_end - &__malloc_start)

/* Stack size for SPL */
#ifdef CONFIG_SPL_FAT_SUPPORT
#define CONFIG_SPL_STACK_SIZE		(5 * 1024)
#else
#define CONFIG_SPL_STACK_SIZE		(4 * 1024)
#endif

/* MALLOC size for SPL */
#define CONFIG_SPL_MALLOC_SIZE		(5 * 1024)

/*
 * FPGA programming support with SPL
 * FPGA RBF file source (with mkimage header) is located within the same
 * boot device which stored the subsequent boot image (U-Boot).
 */
/* enabled program the FPGA */
#undef CONFIG_SPL_FPGA_LOAD
/* location of FPGA RBF image within QSPI */
#define CONFIG_SPL_FPGA_QSPI_ADDR	(0x800000)


#endif	/* __CONFIG_COMMON_H */
