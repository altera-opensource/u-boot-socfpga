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
#include <fdtdec.h>

DECLARE_GLOBAL_DATA_PTR;

static const char *get_cff_filename(const void *fdt, int *len)
{
	const char *cff_filename = NULL;
	const char *cell;
	int nodeoffset;
	nodeoffset = fdt_subnode_offset(fdt, 0, "chosen");

	if (nodeoffset >= 0) {
		cell = fdt_getprop(fdt, nodeoffset, "cff-file", len);
		if (cell) {
			cff_filename = cell;
		}
	}
	return cff_filename;
}

static int to_fpga_from_fat(const char *filename)
{
	u32 temp[4096] __aligned(ARCH_DMA_MINALIGN);
	u32 malloc_start, filesize, readsize, status, offset = 0;
	const char *filename = get_cff_filename(gd->fdt_blob);

	malloc_start = CONFIG_SYS_INIT_SP_ADDR
		- CONFIG_OCRAM_STACK_SIZE - CONFIG_OCRAM_MALLOC_SIZE;
	mem_malloc_init (malloc_start, CONFIG_OCRAM_MALLOC_SIZE);

	/* initialize the MMC controller */
	puts("MMC:   ");
	mmc_initialize(gd->bd);

	/* we are looking at the FAT partition */
	if (fs_set_blk_dev("mmc", "0:1", FS_TYPE_FAT)) {
		printf("failed to set filesystem to FAT\n");
		return 1;
	}

	/* checking the size of the file */
	filesize = fs_size(filename);
	if (filesize == -1) {
		printf("Error - %s not found within SDMMC\n", filename);
		return 1;
	}

	WATCHDOG_RESET();

	while (filesize) {
		/*
		 * Read the data by small chunk by chunk. At this stage,
		 * use the temp as temporary buffer.
		 */
		if (filesize < sizeof(temp))
			readsize = filesize;
		else
			readsize = sizeof(temp);

		bytesread = file_fat_read_at(filename, offset, temp, readsize);
		if (bytesread != readsize) {
			printf("failed to read %s at offset %d %d != %d\n",
				filename, offset, bytesread, readsize);
			return 1;
		}

		filesize -= readsize;
		offset += readsize;

#ifdef FIXME_ALAN

		/* transfer data to FPGA Manager */
		fpgamgr_program_write((const long unsigned int *)temp,
			sizeof(temp));
#endif
		WATCHDOG_RESET();
	}

	return 0;
}

int cff_from_fat(void)
{
	int slen;
	int len = 0;
	int num_files = 0;
	const char *filename = get_cff_filename(gd->fdt_blob, &len);

	if (NULL == filename) {
		printf("no cff-filename specified\n");
		return 0;
	}

	/* initialize the FPGA Manager */
	WATCHDOG_RESET();
#ifdef FIXME_ALAN
	status = fpgamgr_program_init();
	if (status) {
		printf("FPGA: Init failed with error code %d\n", status);
		return -1;
	}
#endif
	while (len > 0) {
		printf("writing %s to fpga\n", filename);
		if (to_fpga_from_fat(filename))
			return -10;
		num_files++;
		slen = strlen(filename) + 1;
		len -= slen;
		filename += slen;
	}

	/* Ensure the FPGA entering config done */
#ifdef FIXME_ALAN
	status = fpgamgr_program_poll_cd();
	if (status) {
		printf("FPGA: Poll CD failed with error code %d\n", status);
		return -2;
	}
	WATCHDOG_RESET();

	/* Ensure the FPGA entering init phase */
	status = fpgamgr_program_poll_initphase();
	if (status) {
		printf("FPGA: Poll initphase failed with error code %d\n",
			status);
		return -3;
	}
	WATCHDOG_RESET();

	/* Ensure the FPGA entering user mode */
	status = fpgamgr_program_poll_usermode();
	if (status) {
		printf("FPGA: Poll usermode failed with error code %d\n",
			status);
		return -4;
	}
#endif
	WATCHDOG_RESET();
	return num_files;
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


