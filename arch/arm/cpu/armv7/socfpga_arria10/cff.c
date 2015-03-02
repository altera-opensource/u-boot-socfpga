/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <altera.h>
#include <common.h>
#include <asm/io.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/misc.h>
#include <fat.h>
#include <fs.h>
#include <mmc.h>
#include <malloc.h>
#include <watchdog.h>
#include <fdtdec.h>
#include <spi_flash.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_MMC
const char *get_cff_filename(const void *fdt, int *len)
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

/* read the first chunk of the file off the fat */
static int read_rbf_header_from_fat(char *dev_part, const char *filename, u32 *temp, u32 size_of_temp)
{
	u32 filesize, bytesread, readsize;

	/* we are looking at the FAT partition */
	if (fs_set_blk_dev("mmc", dev_part, FS_TYPE_FAT)) {
		printf("failed to set filesystem to FAT\n");
		return -1;
	}

	/* checking the size of the file */
	filesize = fs_size(filename);
	if (filesize == -1) {
		printf("Error - %s not found within SDMMC\n", filename);
		return -2;
	}

	WATCHDOG_RESET();

	if (filesize < size_of_temp)
		readsize = filesize;
	else
		readsize = size_of_temp;

	bytesread = file_fat_read_at(filename, 0, temp, readsize);
	if (bytesread != readsize) {
		printf("read_rbf_header_from_fat: failed to read %s %d != %d\n",
			filename, bytesread, readsize);
		return -3;
	}

	WATCHDOG_RESET();

	return 0;
}

static int to_fpga_from_fat(char *dev_part, const char *filename, u32 *temp, u32 size_of_temp)
{
	u32 filesize, readsize, bytesread, offset = 0;

	/* we are looking at the FAT partition */
	if (fs_set_blk_dev("mmc", dev_part, FS_TYPE_FAT)) {
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
		if (filesize < size_of_temp)
			readsize = filesize;
		else
			readsize = size_of_temp;

		bytesread = file_fat_read_at(filename, offset, temp, readsize);
		if (bytesread != readsize) {
			printf("failed to read %s at offset %d %d != %d\n",
				filename, offset, bytesread, readsize);
			return 1;
		}

		filesize -= readsize;
		offset += readsize;

		/* transfer data to FPGA Manager */
		fpgamgr_program_write((const long unsigned int *)temp, readsize);

		WATCHDOG_RESET();
	}

	return 0;
}

int cff_from_mmc_fat(char *dev_part, const char *filename, int len)
{
	u32 temp[4096] __aligned(ARCH_DMA_MINALIGN);
	int slen, status, num_files = 0, ret;

	if (filename == NULL) {
		printf("no filename specified\n");
		return 0;
	}

	WATCHDOG_RESET();

	ret = read_rbf_header_from_fat(dev_part, filename, temp, sizeof(temp));
	if (ret) {
		printf("cff_from_mmc_fat: error reading rbf header\n");
		return ret;
	}

	WATCHDOG_RESET();

	/* initialize the FPGA Manager */
	status = fpgamgr_program_init(temp, sizeof(temp));
	if (status) {
		printf("FPGA: Init failed with error code %d\n", status);
		return -1;
	}

	while (len > 0) {
		printf("FPGA: writing %s\n", filename);
		if (to_fpga_from_fat(dev_part, filename, temp, sizeof(temp)))
			return -10;
		num_files++;
		slen = strlen(filename) + 1;
		len -= slen;
		filename += slen;
	}

	/* Ensure the FPGA entering config done */
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

	printf("FPGA: Success.\n");
	WATCHDOG_RESET();

	return num_files;
}

/* Get filename from DT, load it to fpga */
int cff_from_mmc_fat_dt(void)
{
	int len = 0;
	const char *filename = get_cff_filename(gd->fdt_blob, &len);

	return cff_from_mmc_fat("0:1", filename, len);
}
#endif /* #ifdef CONFIG_MMC */


#ifdef CONFIG_CADENCE_QSPI
int cff_from_qspi(unsigned long flash_offset)
{
	struct spi_flash *flash;
	struct image_header header;
	u32 flash_addr, status;
	u32 temp[4096] __aligned(ARCH_DMA_MINALIGN);
	u32 remaining = 0;
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	u32 datacrc = 0;
#endif

	/* initialize the Quad SPI controller */
	flash = spi_flash_probe(0, 0, CONFIG_SF_DEFAULT_SPEED, SPI_MODE_3);
	if (!flash) {
		puts("SPI probe failed.\n");
		return -1;
	}

	/* Load mkimage header on top of rbf file */
	spi_flash_read(flash, flash_offset,
		sizeof(struct image_header), &header);
	/* printf("%s rbf datasize %d data location 0x%x\n", __func__, image_get_data_size(&header), image_get_data(&header)); */

	WATCHDOG_RESET();

        if (!image_check_magic(&header)) {
		printf("FPGA: Bad Magic Number\n");
                return -6;
        }

        if (!image_check_hcrc(&header)) {
		printf("FPGA: Bad Header Checksum\n");
                return -7;
        }

	/* start loading the data from flash and send to FPGA Manager */
	flash_addr = flash_offset + sizeof(struct image_header);

	spi_flash_read(flash, flash_addr,
		sizeof(temp), temp);

	/* initialize the FPGA Manager */
	status = fpgamgr_program_init((u32 *)&temp, sizeof(temp));
	if (status) {
		printf("FPGA: Init failed with error code %d\n", status);
		return -2;
	}

	remaining = image_get_data_size(&header);
	while (remaining) {
		/*
		 * Read the data by small chunk by chunk. At this stage,
		 * use the temp as temporary buffer.
		 */
		 if (remaining > sizeof(temp)) {
			spi_flash_read(flash, flash_addr,
				sizeof(temp), temp);
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
			datacrc = crc32(datacrc, (unsigned char*)temp, sizeof(temp));
#endif
			/* update the counter */
			remaining -= sizeof(temp);
			flash_addr += sizeof(temp);
		 }  else {
			spi_flash_read(flash, flash_addr,
				remaining, temp);
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
			datacrc = crc32(datacrc, (unsigned char*)temp, remaining);
#endif
			remaining = 0;
		}

		/* transfer data to FPGA Manager */
		fpgamgr_program_write((const long unsigned int *)temp,
			sizeof(temp));
		WATCHDOG_RESET();

	}

	/* Ensure the FPGA entering config done */
	status = fpgamgr_program_poll_cd();
	if (status) {
		printf("FPGA: Poll CD failed with error code %d\n", status);
		return -3;
	}
	WATCHDOG_RESET();

	/* Ensure the FPGA entering init phase */
	status = fpgamgr_program_poll_initphase();
	if (status) {
		printf("FPGA: Poll initphase failed with error code %d\n",
			status);
		return -4;
	}
	WATCHDOG_RESET();

	/* Ensure the FPGA entering user mode */
	status = fpgamgr_program_poll_usermode();
	if (status) {
		printf("FPGA: Poll usermode failed with error code %d\n",
			status);
		return -5;
	}

#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	if (datacrc !=  image_get_dcrc(&header)) {
		printf("FPGA: Bad Data Checksum\n");
		return -8;
	}
#endif

	printf("FPGA: Success.\n");
	WATCHDOG_RESET();
	return 1;
}

int cff_from_qspi_env(void)
{
	return cff_from_qspi(CONFIG_QSPI_RBF_ADDR);
}
#endif /* #ifdef CONFIG_CADENCE_QSPI */

#if defined(CONFIG_CMD_FPGA_LOADFS)
int socfpga_loadfs(Altera_desc *desc, const void *buf, size_t bsize,
		fpga_fs_info *fsinfo)
{
#ifdef CONFIG_MMC
	if (!strcmp(fsinfo->interface, "mmc")) {
		int ret, i, slen = strlen(fsinfo->filename) + 1;
		
		for (i = 0; i < slen; i++)
			if (fsinfo->filename[i] == ',')
				fsinfo->filename[i] = 0;

		ret = cff_from_mmc_fat(fsinfo->dev_part, fsinfo->filename,
				       slen);
		if (ret > 0) {
			config_shared_fpga_pins(gd->fdt_blob);
			reset_deassert_shared_connected_peripherals();
			reset_deassert_fpga_connected_peripherals();
			return FPGA_SUCCESS;
		} else {
			return FPGA_FAIL;
		}
	}
#endif
#ifdef CONFIG_CADENCE_QSPI
	if (!strcmp(fsinfo->interface, "qspi")) {
		int ret;
		u32 rbfaddr = simple_strtoul(fsinfo->dev_part, NULL, 16);
		ret = cff_from_qspi(rbfaddr);
		if (ret > 0) {
			config_shared_fpga_pins(gd->fdt_blob);
			reset_deassert_shared_connected_peripherals();
			reset_deassert_fpga_connected_peripherals();
			return FPGA_SUCCESS;
		} else {
			return FPGA_FAIL;
		}
	}
#endif

	printf("unsupported interface: %s\n", fsinfo->interface);

	return FPGA_FAIL;
}
#endif

