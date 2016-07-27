/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <fpga.h>
#include <nand.h>
#ifndef	_SOCFPGA_CFF_H_
#define	_SOCFPGA_CFF_H_

#ifndef __ASSEMBLY_
#if defined(CONFIG_CADENCE_QSPI) || defined(CONFIG_NAND_DENALI)
struct raw_flash_info {
#if defined(CONFIG_CADENCE_QSPI)
	struct spi_flash *flash;
#elif defined(CONFIG_NAND_DENALI)
	nand_info_t *flash;
#endif
};
#endif

#if defined(CONFIG_MMC)
struct sdmmc_flash_info {
	char *filename;
	char *dev_part;
};
#endif

struct cff_flash_info {
#if defined(CONFIG_CADENCE_QSPI) || defined(CONFIG_NAND_DENALI)
	struct raw_flash_info raw_flashinfo;
	u32 buffer[1024] __aligned(ARCH_DMA_MINALIGN);
#endif
#if defined(CONFIG_MMC)
	struct sdmmc_flash_info sdmmc_flashinfo;
	u32 buffer[4096] __aligned(ARCH_DMA_MINALIGN);
#endif
	u32 remaining;
	u32 flash_offset;
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	u32 datacrc;
#endif
	struct image_header header;
};

int cff_from_sdmmc_env(void);
int cff_from_qspi_env(void);
int cff_from_flash(fpga_fs_info *fpga_fsinfo);
int cff_from_nand_env(void);
const char *get_cff_filename(const void *fdt, int *len);
#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_CFF_H_ */
