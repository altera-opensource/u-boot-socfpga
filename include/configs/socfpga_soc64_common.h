/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2017-2019 Intel Corporation <www.intel.com>
 *
 */

#ifndef __CONFIG_SOCFPGA_SOC64_COMMON_H__
#define __CONFIG_SOCFPGA_SOC64_COMMON_H__

#include <asm/arch/base_addr_soc64.h>
#include <asm/arch/handoff_soc64.h>
#include <linux/stringify.h>

/*
 * U-Boot general configurations
 */
#define CONFIG_SYS_MONITOR_BASE		CONFIG_SYS_TEXT_BASE
/* sysmgr.boot_scratch_cold4 & 5 (64bit) will be used for PSCI_CPU_ON call */
#define CPU_RELEASE_ADDR		0xFFD12210

/*
 * sysmgr.boot_scratch_cold6 & 7 (64bit) will be used by master CPU to
 * store its VBAR_EL3 value. Other slave CPUs will read from this
 * location and update their VBAR_EL3 respectively
 */
#define VBAR_EL3_BASE_ADDR		0xFFD12218
/*
 * Share sysmgr.boot_scratch_cold6 & 7 (64bit) with VBAR_LE3_BASE_ADDR
 * Indicate L2 reset is done. HPS should trigger warm reset via RMR_EL3.
 */
#define L2_RESET_DONE_REG		0xFFD12218
/* Magic word to indicate L2 reset is completed */
#define L2_RESET_DONE_STATUS		0x1228E5E7
#define CONFIG_SYS_CACHELINE_SIZE	64
#define CONFIG_SYS_MEM_RESERVE_SECURE	0	/* using OCRAM, not DDR */

/*
 * U-Boot console configurations
 */
#define CONFIG_SYS_MAXARGS		64
#define CONFIG_SYS_CBSIZE		2048
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE

/* Extend size of kernel image for uncompression */
#define CONFIG_SYS_BOOTM_LEN		(64 * 1024 * 1024)

/*
 * U-Boot run time memory configurations
 */
#define CONFIG_SYS_INIT_RAM_ADDR	0xFFE00000
#define CONFIG_SYS_INIT_RAM_SIZE	0x40000
#ifdef CONFIG_SPL_BUILD
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_INIT_RAM_ADDR  \
					+ CONFIG_SYS_INIT_RAM_SIZE \
					- SOC64_HANDOFF_SIZE)
#else
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_TEXT_BASE \
					+ 0x100000)
#endif
#define CONFIG_SYS_INIT_SP_OFFSET	(CONFIG_SYS_INIT_SP_ADDR)

/*
 * U-Boot environment configurations
 */

/*
 * QSPI support
 */
 #ifdef CONFIG_CADENCE_QSPI
/* Enable it if you want to use dual-stacked mode */
/*#define CONFIG_QSPI_RBF_ADDR		0x720000*/

/* Flash device info */

#ifndef CONFIG_SPL_BUILD
#define MTDIDS_DEFAULT			"nor0=ff705000.spi.0"
#endif /* CONFIG_SPL_BUILD */

#ifndef __ASSEMBLY__
unsigned int cm_get_qspi_controller_clk_hz(void);
#define CONFIG_CQSPI_REF_CLK		cm_get_qspi_controller_clk_hz()
#endif

#endif /* CONFIG_CADENCE_QSPI */

 /*
 * NAND support
 */
#ifdef CONFIG_NAND_DENALI
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_SYS_MAX_NAND_DEVICE	1

#define CONFIG_SYS_NAND_U_BOOT_SIZE	(1 * 1024 * 1024)
#define CONFIG_SYS_NAND_U_BOOT_DST	CONFIG_SYS_TEXT_BASE
#endif /* CONFIG_NAND_DENALI */

/*
 * Environment variable
 */

#if IS_ENABLED(CONFIG_SPL_ATF)
#define CONFIG_BOOTFILE "Image"
#else
#define CONFIG_BOOTFILE "Image"
#endif

#if IS_ENABLED(CONFIG_DISTRO_DEFAULTS)
#if IS_ENABLED(CONFIG_CMD_MMC)
#define BOOT_TARGET_DEVICES_MMC(func) func(MMC, mmc, 0)
#else
#define BOOT_TARGET_DEVICES_MMC(func)
#endif

#if IS_ENABLED(CONFIG_CMD_SF)
#define BOOT_TARGET_DEVICES_QSPI(func) func(QSPI, qspi, na)
#else
#define BOOT_TARGET_DEVICES_QSPI(func)
#endif

#define BOOTENV_DEV_QSPI(devtypeu, devtypel, instance) \
	"bootcmd_qspi=sf probe && " \
	"sf read ${scriptaddr} ${qspiscriptaddr} ${scriptsize} && " \
	"echo QSPI: Trying to boot script at ${scriptaddr} && " \
	"source ${scriptaddr}; " \
	"echo QSPI: SCRIPT FAILED: continuing...;\0"

#define BOOTENV_DEV_NAME_QSPI(devtypeu, devtypel, instance) \
	"qspi "

#if IS_ENABLED(CONFIG_CMD_NAND)
# define BOOT_TARGET_DEVICES_NAND(func)	func(NAND, nand, na)
#else
# define BOOT_TARGET_DEVICES_NAND(func)
#endif

#define BOOTENV_DEV_NAND(devtypeu, devtypel, instance) \
	"bootcmd_nand=ubi part root && " \
	"ubi readvol ${scriptaddr} script && " \
	"echo NAND: Trying to boot script at ${scriptaddr} && " \
	"source ${scriptaddr}; " \
	"echo NAND: SCRIPT FAILED: continuing...;\0"

#define BOOTENV_DEV_NAME_NAND(devtypeu, devtypel, instance) \
	"nand "

#define BOOT_TARGET_DEVICES(func) \
	BOOT_TARGET_DEVICES_MMC(func) \
	BOOT_TARGET_DEVICES_QSPI(func) \
	BOOT_TARGET_DEVICES_NAND(func)

#include <config_distro_bootcmd.h>
#define CONFIG_EXTRA_ENV_SETTINGS \
	"kernel_addr_r=0x2000000\0" \
	"fdt_addr_r=0x6000000\0" \
	"qspiscriptaddr=0x02110000\0" \
	"scriptsize=0x00010000\0" \
	"qspibootimageaddr=0x02120000\0" \
	"bootimagesize=0x03200000\0" \
	"loadaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"bootfile=" CONFIG_BOOTFILE "\0" \
	"mmcroot=/dev/mmcblk0p2\0" \
	"mtdids=" CONFIG_MTDIDS_DEFAULT "\0" \
	"mtdparts=" CONFIG_MTDPARTS_DEFAULT "\0" \
	"linux_qspi_enable=if sf probe; then " \
		"echo Enabling QSPI at Linux DTB...;" \
		"fdt addr ${fdt_addr}; fdt resize;" \
		"fdt set /soc/spi@ff8d2000 status okay;" \
		"if fdt set /soc/clocks/qspi-clk clock-frequency" \
		" ${qspi_clock}; then echo QSPI clock frequency updated;" \
		" elif fdt set /soc/clkmgr/clocks/qspi_clk clock-frequency" \
		" ${qspi_clock}; then echo QSPI clock frequency updated;" \
		" else fdt set /clocks/qspi-clk clock-frequency" \
		" ${qspi_clock}; echo QSPI clock frequency updated; fi; fi\0" \
	"scriptaddr=0x05FF0000\0" \
	"scriptfile=boot.scr\0" \
	"nandroot=ubi0:rootfs\0" \
	"socfpga_legacy_reset_compat=1\0" \
	"rsu_status=rsu dtb; rsu display_dcmf_version; "\
		"rsu display_dcmf_status; rsu display_max_retry\0" \
	"smc_fid_rd=0xC2000007\0" \
	"smc_fid_wr=0xC2000008\0" \
	"smc_fid_upd=0xC2000009\0 " \
	BOOTENV

#else

#define CONFIG_EXTRA_ENV_SETTINGS \
	"kernel_comp_addr_r=0x9000000\0" \
	"kernel_comp_size=0x01000000\0" \
	"qspibootimageaddr=0x020E0000\0" \
	"qspifdtaddr=0x020D0000\0" \
	"bootimagesize=0x01F00000\0" \
	"fdtimagesize=0x00010000\0" \
	"qspiload=sf read ${loadaddr} ${qspibootimageaddr} ${bootimagesize};" \
		"sf read ${fdt_addr} ${qspifdtaddr} ${fdtimagesize}\0" \
	"qspiboot=setenv bootargs earlycon root=/dev/mtdblock1 rw " \
		"rootfstype=jffs2 rootwait;booti ${loadaddr} - ${fdt_addr}\0" \
	"qspifitload=sf read ${loadaddr} ${qspibootimageaddr} ${bootimagesize}\0" \
	"qspifitboot=setenv bootargs earlycon root=/dev/mtdblock1 rw " \
		"rootfstype=jffs2 rootwait;bootm ${loadaddr}\0" \
	"loadaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"bootfile=" CONFIG_BOOTFILE "\0" \
	"fdt_addr=8000000\0" \
	"fdtimage=" CONFIG_DEFAULT_DEVICE_TREE ".dtb\0" \
	"mmcroot=/dev/mmcblk0p2\0" \
	"mmcboot=setenv bootargs " CONFIG_BOOTARGS \
		" root=${mmcroot} rw rootwait;" \
		"booti ${loadaddr} - ${fdt_addr}\0" \
	"mmcload=mmc rescan;" \
		"load mmc 0:1 ${loadaddr} ${bootfile};" \
		"load mmc 0:1 ${fdt_addr} ${fdtimage}\0" \
	"mmcfitboot=setenv bootargs " CONFIG_BOOTARGS \
		" root=${mmcroot} rw rootwait;" \
		"bootm ${loadaddr}\0" \
	"mmcfitload=mmc rescan;" \
		"load mmc 0:1 ${loadaddr} ${bootfile}\0" \
	"mtdids=" CONFIG_MTDIDS_DEFAULT "\0" \
	"mtdparts=" CONFIG_MTDPARTS_DEFAULT "\0" \
	"linux_qspi_enable=if sf probe; then " \
		"echo Enabling QSPI at Linux DTB...;" \
		"fdt addr ${fdt_addr}; fdt resize;" \
		"fdt set /soc/spi@ff8d2000 status okay;" \
		"if fdt set /soc/clocks/qspi-clk clock-frequency" \
		" ${qspi_clock}; then echo QSPI clock frequency updated;" \
		" elif fdt set /soc/clkmgr/clocks/qspi_clk clock-frequency" \
		" ${qspi_clock}; then echo QSPI clock frequency updated;" \
		" else fdt set /clocks/qspi-clk clock-frequency" \
		" ${qspi_clock}; echo QSPI clock frequency updated; fi; fi\0" \
	"scriptaddr=0x02100000\0" \
	"scriptfile=u-boot.scr\0" \
	"fatscript=if fatload mmc 0:1 ${scriptaddr} ${scriptfile};" \
		   "then source ${scriptaddr}:script; fi\0" \
	"nandroot=ubi0:rootfs\0" \
	"nandload=ubi part root; ubi readvol ${loadaddr} kernel; ubi readvol ${fdt_addr} dtb\0" \
	"nandboot=setenv bootargs " CONFIG_BOOTARGS \
			" root=${nandroot} rw rootwait rootfstype=ubifs ubi.mtd=1; " \
			"booti ${loadaddr} - ${fdt_addr}\0" \
	"nandfitboot=setenv bootargs " CONFIG_BOOTARGS \
			" root=${nandroot} rw rootwait rootfstype=ubifs ubi.mtd=1; " \
			"bootm ${loadaddr}\0" \
	"nandfitload=ubi part root; ubi readvol ${loadaddr} kernel\0" \
	"socfpga_legacy_reset_compat=1\0" \
	"rsu_status=rsu dtb; rsu display_dcmf_version; "\
		"rsu display_dcmf_status; rsu display_max_retry\0" \
	"smc_fid_rd=0xC2000007\0" \
	"smc_fid_wr=0xC2000008\0" \
	"smc_fid_upd=0xC2000009\0 "
#endif /*#if IS_ENABLED(CONFIG_DISTRO_DEFAULTS)*/

/*
 * External memory configurations
 */
#define PHYS_SDRAM_1			0x0
#define PHYS_SDRAM_1_SIZE		(1 * 1024 * 1024 * 1024)
#define CONFIG_SYS_SDRAM_BASE		0

/*
 * Serial / UART configurations
 */
#define CONFIG_SYS_NS16550_CLK		100000000
#define CONFIG_SYS_NS16550_MEM32

/*
 * Timer & watchdog configurations
 */
#define COUNTER_FREQUENCY		400000000

/*
 * SDMMC configurations
 */
#ifdef CONFIG_CMD_MMC
#define CONFIG_SYS_MMC_MAX_BLK_COUNT	256
#endif
/*
 * Flash configurations
 */

/* Ethernet on SoC (EMAC) */
#if defined(CONFIG_CMD_NET)
#define CONFIG_DW_ALTDESCRIPTOR
#endif /* CONFIG_CMD_NET */

/*
 * L4 Watchdog
 */
#ifndef CONFIG_SPL_BUILD
#undef CONFIG_DESIGNWARE_WATCHDOG
#endif
#define CONFIG_DW_WDT_BASE		SOCFPGA_L4WD0_ADDRESS
#ifdef CONFIG_TARGET_SOCFPGA_STRATIX10
#ifndef __ASSEMBLY__
unsigned int cm_get_l4_sys_free_clk_hz(void);
#define CONFIG_DW_WDT_CLOCK_KHZ		(cm_get_l4_sys_free_clk_hz() / 1000)
#endif
#else
#define CONFIG_DW_WDT_CLOCK_KHZ		100000
#endif

/*
 * SPL memory layout
 *
 * On chip RAM
 * 0xFFE0_0000 ...... Start of OCRAM
 * SPL code, rwdata
 * empty space
 * 0xFFEx_xxxx ...... Top of stack (grows down)
 * 0xFFEy_yyyy ...... Global Data
 * 0xFFEz_zzzz ...... Malloc prior relocation (size CONFIG_SYS_MALLOC_F_LEN)
 * 0xFFE3_F000 ...... Hardware handdoff blob (size 4KB)
 * 0xFFE3_FFFF ...... End of OCRAM
 *
 * SDRAM
 * 0x0000_0000 ...... Start of SDRAM_1
 * unused / empty space for image loading
 * Size 64MB   ...... MALLOC (size CONFIG_SYS_SPL_MALLOC_SIZE)
 * Size 1MB    ...... BSS (size CONFIG_SPL_BSS_MAX_SIZE)
 * 0x8000_0000 ...... End of SDRAM_1 (assume 2GB)
 *
 */
#define CONFIG_SPL_TARGET		"spl/u-boot-spl-dtb.hex"
#define CONFIG_SPL_MAX_SIZE		CONFIG_SYS_INIT_RAM_SIZE
#define CONFIG_SPL_STACK		CONFIG_SYS_INIT_SP_ADDR
#define CONFIG_SPL_BSS_MAX_SIZE		0x100000	/* 1 MB */
#define CONFIG_SPL_BSS_START_ADDR	(PHYS_SDRAM_1 + PHYS_SDRAM_1_SIZE \
					- CONFIG_SPL_BSS_MAX_SIZE)
#define CONFIG_SYS_SPL_MALLOC_SIZE	(CONFIG_SYS_MALLOC_LEN)
#define CONFIG_SYS_SPL_MALLOC_START	(CONFIG_SPL_BSS_START_ADDR \
					- CONFIG_SYS_SPL_MALLOC_SIZE)

/* SPL SDMMC boot support */
#ifdef CONFIG_SPL_LOAD_FIT
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME		"u-boot.itb"
#else
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME		"u-boot.img"
#endif

#endif	/* __CONFIG_SOCFPGA_SOC64_COMMON_H__ */
