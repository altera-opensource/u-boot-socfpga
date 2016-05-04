/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <fpga.h>
#ifndef	_SOCFPGA_CFF_H_
#define	_SOCFPGA_CFF_H_

#ifndef __ASSEMBLY_
#if defined(CONFIG_CADENCE_QSPI)
struct raw_flash_info {
	u32 rbf_offset;
	struct image_header header;
	struct spi_flash *flash;
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	u32 datacrc;
#endif
};
#endif

struct cff_flash_info {
#if defined(CONFIG_CADENCE_QSPI)
	struct raw_flash_info raw_flashinfo;
#endif
	u32 remaining;
	u32 buffer[4096] __aligned(ARCH_DMA_MINALIGN);
	u32 flash_offset;
};

int cff_from_qspi_env(void);
int cff_from_flash(fpga_fs_info *fpga_fsinfo);
int cff_from_nand_env(void);
int cff_from_nand(unsigned long flash_offset);
const char *get_cff_filename(const void *fdt, int *len);
int cff_from_mmc_fat(char *dev_part, const char *filename, int len,
	int do_init, int wait_early);
#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_CFF_H_ */
