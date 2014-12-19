/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/fpga_manager.h>
#include <fat.h>
#include <fs.h>
#include <mmc.h>
#include <malloc.h>
#include <watchdog.h>

DECLARE_GLOBAL_DATA_PTR;

int cff_from_fat(void)
{
	u32 temp[4096] __aligned(ARCH_DMA_MINALIGN);
	u32 malloc_start, filesize, readsize, status, offset = 0;
	char *filename = "soc_system.rbf";

	malloc_start = CONFIG_SYS_INIT_SP_ADDR
		- CONFIG_OCRAM_STACK_SIZE - CONFIG_OCRAM_MALLOC_SIZE;
	mem_malloc_init (malloc_start, CONFIG_OCRAM_MALLOC_SIZE);

	/* initialize the MMC controller */
	puts("MMC:   ");
	mmc_initialize(gd->bd);

	/* we are looking at the FAT partition */
	if (fs_set_blk_dev("mmc", "0:1", FS_TYPE_FAT))
		return 1;

	/* checking the size of the file */
	filesize = file_fat_read_at(filename, 0, NULL, 0);
	if (filesize == -1) {
		printf("Error - %s not found within SDMMC\n", filename);
		return 1;
	}

	/* initialize the FPGA Manager */
	WATCHDOG_RESET();
	status = fpgamgr_program_init();
	if (status) {
		printf("FPGA: Init failed with error code %d\n", status);
		hang();
	}

	while (filesize) {
		printf("bytes left %d\n", filesize);
		/*
		 * Read the data by small chunk by chunk. At this stage,
		 * use the temp as temporary buffer.
		 */
		if (filesize < sizeof(temp))
			readsize = filesize;
		else
			readsize = sizeof(temp);

		if (file_fat_read_at(filename, offset, temp, readsize)
			!= readsize) {
			puts("Error - altr_cff.sys read error\n");
			return 1;
		}

		filesize -= readsize;
		offset += readsize;

		/* transfer data to FPGA Manager */
		fpgamgr_program_write((const long unsigned int *)temp,
			sizeof(temp));
		WATCHDOG_RESET();
	}

	/* Ensure the FPGA entering config done */
	status = fpgamgr_program_poll_cd();
	if (status) {
		printf("FPGA: Poll CD failed with error code %d\n", status);
		return 1;
	}
	WATCHDOG_RESET();

	/* Ensure the FPGA entering init phase */
	status = fpgamgr_program_poll_initphase();
	if (status) {
		printf("FPGA: Poll initphase failed with error code %d\n",
			status);
		return 1;
	}
	WATCHDOG_RESET();

	/* Ensure the FPGA entering user mode */
	status = fpgamgr_program_poll_usermode();
	if (status) {
		printf("FPGA: Poll usermode failed with error code %d\n",
			status);
		return 1;
	}
	WATCHDOG_RESET();
	return 0;
}


void cff_from_qspi(unsigned long flash_offset)
{
#if 0
	struct spi_flash *flash;
	struct image_header header;
	u32 flash_addr, status;
	u32 temp[64];

	/* initialize the Quad SPI controller */
	flash = spi_flash_probe(0, 0, CONFIG_SF_DEFAULT_SPEED, SPI_MODE_3);
	if (!flash) {
		puts("SPI probe failed.\n");
		hang();
	}

	/* Load mkimage header on top of rbf file */
	spi_flash_read(flash, flash_offset,
		sizeof(struct image_header), &header);
	spl_parse_image_header(&header);

#ifdef CONFIG_HW_WATCHDOG
	WATCHDOG_RESET();
#endif

	/* initialize the FPGA Manager */
	status = fpgamgr_program_init();
	if (status) {
		printf("FPGA: Init failed with error code %d\n", status);
		hang();
	}

	/* start loading the data from flash and send to FPGA Manager */
	flash_addr = CONFIG_SPL_FPGA_QSPI_ADDR + sizeof(struct image_header);

	while (spl_image.size) {
		/*
		 * Read the data by small chunk by chunk. At this stage,
		 * use the temp as temporary buffer.
		 */
		 if (spl_image.size > sizeof(temp)) {
			spi_flash_read(flash, flash_addr,
				sizeof(temp), temp);
			/* update the counter */
			spl_image.size -= sizeof(temp);
			flash_addr += sizeof(temp);
		 }  else {
			spi_flash_read(flash, flash_addr,
				spl_image.size, temp);
			spl_image.size = 0;
		}

		/* transfer data to FPGA Manager */
		fpgamgr_program_write((const long unsigned int *)temp,
			sizeof(temp));
#ifdef CONFIG_HW_WATCHDOG
		WATCHDOG_RESET();
#endif
	}

	/* Ensure the FPGA entering config done */
	status = fpgamgr_program_poll_cd();
	if (status) {
		printf("FPGA: Poll CD failed with error code %d\n", status);
		hang();
	}
#ifdef CONFIG_HW_WATCHDOG
		WATCHDOG_RESET();
#endif

	/* Ensure the FPGA entering init phase */
	status = fpgamgr_program_poll_initphase();
	if (status) {
		printf("FPGA: Poll initphase failed with error code %d\n",
			status);
		hang();
	}
#ifdef CONFIG_HW_WATCHDOG
		WATCHDOG_RESET();
#endif

	/* Ensure the FPGA entering user mode */
	status = fpgamgr_program_poll_usermode();
	if (status) {
		printf("FPGA: Poll usermode failed with error code %d\n",
			status);
		hang();
	}
#ifdef CONFIG_HW_WATCHDOG
		WATCHDOG_RESET();
#endif
#endif
}



