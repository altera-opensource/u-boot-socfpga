/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Intel Corporation <www.intel.com>
 *
 */

#ifndef __CONFIG_SOCFGPA_DM_H__
#define __CONFIG_SOCFGPA_DM_H__

#include <configs/socfpga_soc64_common.h>

#undef CONFIG_BOOTARGS
#ifdef CONFIG_SOCFPGA_SECURE_VAB_AUTH
#define CONFIG_BOOTARGS "earlycon panic=-1 mem=2048M"
#else
#define CONFIG_BOOTARGS "earlycon panic=-1 earlyprintk=ttyS0,4800"
#endif

#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
	"loadaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"bootfile=" CONFIG_BOOTFILE "\0" \
	"fdt_addr=1100000\0" \
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
	"ramboot=setenv bootargs " CONFIG_BOOTARGS";" \
		"booti ${loadaddr} - ${fdt_addr}\0" \
	"linux_qspi_enable=if sf probe; then " \
		"echo Enabling QSPI at Linux DTB...;" \
		"fdt addr ${fdt_addr}; fdt resize;" \
		"fdt set /soc/spi@ff8d2000 status okay;" \
		"fdt set /soc/clkmgr/clocks/qspi_clk clock-frequency " \
		" ${qspi_clock}; fi; \0" \
	"scriptaddr=0x02100000\0" \
	"scriptfile=u-boot.scr\0" \
	"fatscript=if fatload mmc 0:1 ${scriptaddr} ${scriptfile};" \
		   "then source ${scriptaddr}; fi\0"

#endif	/* __CONFIG_SOCFGPA_DM_H__ */
