/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <altera.h>
#include <common.h>
#include <asm/io.h>
#include <asm/arch/cff.h>
#include <asm/arch/fpga_manager.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/misc.h>
#include <fat.h>
#include <fs.h>
#include <mmc.h>
#include <malloc.h>
#include <watchdog.h>
#include <fdtdec.h>
#include <spi_flash.h>
#include <nand.h>

DECLARE_GLOBAL_DATA_PTR;

/* Local functions */
#ifdef CONFIG_CADENCE_QSPI
static int cff_flash_read(struct cff_flash_info *cff_flashinfo, u32 *buffer,
	u32 *buffer_sizebytes);
static int cff_flash_preinit(struct cff_flash_info *cff_flashinfo,
	fpga_fs_info *fpga_fsinfo, u32 *buffer, u32 *buffer_sizebytes);
static int cff_flash_probe(struct cff_flash_info *cff_flashinfo)
{
#ifdef CONFIG_CADENCE_QSPI
	/* initialize the Quad SPI controller */
	cff_flashinfo->raw_flashinfo.flash =
		spi_flash_probe(0, 0, CONFIG_SF_DEFAULT_SPEED, SPI_MODE_3);

	if (!(cff_flashinfo->raw_flashinfo.flash)) {
		puts("SPI probe failed.\n");
		return -1;
	}
#endif

	return 1;
}

static void flash_read(struct cff_flash_info *cff_flashinfo,
	u32 flash_offset,
	size_t size_read,
	u32 *buffer_ptr)
{
#ifdef CONFIG_CADENCE_QSPI
	spi_flash_read(cff_flashinfo->raw_flashinfo.flash,
		flash_offset,
		size_read,
		buffer_ptr);
#endif

	return;
}
#endif

#if defined(CONFIG_MMC)
static u32 *rbftosdramaddr;
#endif

#if defined(CONFIG_CMD_FPGA_LOADFS)
static const struct socfpga_system_manager *system_manager_base =
		(void *)SOCFPGA_SYSMGR_ADDRESS;
#endif

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

int cff_from_mmc_fat(char *dev_part, const char *filename, int len,
	int do_init, int wait_early)
{
	u32 temp_ocr[4096] __aligned(ARCH_DMA_MINALIGN);
	u32 temp_sizebytes = sizeof(temp_ocr);
	u32 *temp =  temp_ocr;
	int slen, status, num_files = 0, ret;

	if (filename == NULL) {
		printf("no filename specified\n");
		return 0;
	}

	WATCHDOG_RESET();

	if (!(do_init || wait_early)) {
		/* using SDRAM, faster than OCRAM, only for core rbf */
		if (NULL != rbftosdramaddr && 0 != (gd->ram_size)) {
			temp = rbftosdramaddr;
			/* Calculating available DDR size */
			temp_sizebytes = (gd->ram_size) - (u32)rbftosdramaddr;
		}
	}

	if (do_init) {
		ret = read_rbf_header_from_fat(dev_part, filename,
			temp, temp_sizebytes);
		if (ret) {
			printf("cff_from_mmc_fat: error reading rbf header\n");
			return ret;
		}

		WATCHDOG_RESET();

		/* initialize the FPGA Manager */
		status = fpgamgr_program_init(temp, temp_sizebytes);
		if (status) {
			printf("FPGA: Init failed with error ");
			printf("code %d\n", status);
			return -1;
		}
	}

	while (len > 0) {
		printf("FPGA: writing %s\n", filename);
		if (to_fpga_from_fat(dev_part, filename, temp,
			temp_sizebytes))
			return -10;
		num_files++;
		slen = strlen(filename) + 1;
		len -= slen;
		filename += slen;
	}

	if (wait_early) {
		if (-ETIMEDOUT != fpgamgr_wait_early_user_mode()) {
			printf("FPGA: Early Release\n");
		} else {
			printf("FPGA: Failed to see Early Release\n");
			return -5;
		}
	} else {
		/* Ensure the FPGA entering config done */
		status = fpgamgr_program_fini();
		if (status)
			return status;

	}
	WATCHDOG_RESET();

	return num_files;
}
#else /* helper function supports both QSPI and NAND */
static int get_cff_offset(const void *fdt)
{
	int nodeoffset = 0;

	nodeoffset = fdt_subnode_offset(fdt, 0, "chosen");

	if (nodeoffset >= 0) {
#if defined(CONFIG_CADENCE_QSPI) && defined(CONFIG_QSPI_RBF_ADDR)
		return fdtdec_get_int(fdt, nodeoffset, "cff-offset",
			CONFIG_QSPI_RBF_ADDR);
#elif defined(CONFIG_NAND_DENALI) && defined(CONFIG_NAND_RBF_ADDR)
		return fdtdec_get_int(fdt, nodeoffset, "cff-offset",
			CONFIG_NAND_RBF_ADDR);
#else
		return fdtdec_get_int(fdt, nodeoffset, "cff-offset", -1);
#endif
	}
	return -1;
}
#endif /* #ifdef CONFIG_MMC */

#ifdef CONFIG_CADENCE_QSPI
int cff_from_flash(fpga_fs_info *fpga_fsinfo)
{
	u32 buffer = 0;
	u32 buffer_sizebytes = 0;
	struct cff_flash_info cff_flashinfo;
	u32 status;
	int ret = 0;

	memset(&cff_flashinfo, 0, sizeof(cff_flashinfo));

	if (fpga_fsinfo->filename == NULL) {
		printf("no [periph/core] rbf [filename/offset] specified.\n");
		return 0;
	}

	if (strcmp(fpga_fsinfo->rbftype, "periph") &&
		strcmp(fpga_fsinfo->rbftype, "combined") &&
		strcmp(fpga_fsinfo->rbftype, "core")) {
		printf("Unsupported FGPA raw binary type.\n");
		return 0;
	}

	WATCHDOG_RESET();

	ret = cff_flash_preinit(&cff_flashinfo, fpga_fsinfo, &buffer,
		&buffer_sizebytes);

	if (ret)
		return ret;

	if (!strcmp(fpga_fsinfo->rbftype, "periph") ||
		!strcmp(fpga_fsinfo->rbftype, "combined")) {
		/* initialize the FPGA Manager */
		status = fpgamgr_program_init((u32 *)buffer, buffer_sizebytes);
		if (status) {
			printf("FPGA: Init with %s failed with error. ",
				fpga_fsinfo->rbftype);
			printf("code %d\n", status);
			return -2;
		}
	}

	WATCHDOG_RESET();

	/* transfer data to FPGA Manager */
	fpgamgr_program_write((const long unsigned int *)buffer,
		buffer_sizebytes);

	WATCHDOG_RESET();

	while (cff_flashinfo.remaining) {
		ret = cff_flash_read(&cff_flashinfo, &buffer,
			&buffer_sizebytes);

		if (ret)
			return ret;

		/* transfer data to FPGA Manager */
		fpgamgr_program_write((const long unsigned int *)buffer,
			buffer_sizebytes);

		WATCHDOG_RESET();
	}

	if (!strcmp(fpga_fsinfo->rbftype, "periph")) {
		if (-ETIMEDOUT != fpgamgr_wait_early_user_mode()) {
			printf("FPGA: Early Release Succeeded.\n");
		} else {
			printf("FPGA: Failed to see Early Release.\n");
			return -5;
		}
	} else if ((!strcmp(fpga_fsinfo->rbftype, "combined")) ||
		(!strcmp(fpga_fsinfo->rbftype, "core"))) {
		/* Ensure the FPGA entering config done */
		status = fpgamgr_program_fini();
		if (status)
			return status;
	} else {
		printf("Config Error: Unsupported FGPA raw binary type.\n");
		return -1;
	}

	WATCHDOG_RESET();
	return 1;
}

static int cff_flash_preinit(struct cff_flash_info *cff_flashinfo,
	fpga_fs_info *fpga_fsinfo, u32 *buffer, u32 *buffer_sizebytes)
{
	u32 *bufferptr_after_header = NULL;
	u32 buffersize_after_header = 0;
	u32 rbf_header_data_size = 0;
	int ret = 0;
	/* To avoid from keeping re-read the contents */
	struct image_header *header = &(cff_flashinfo->raw_flashinfo.header);
	size_t buffer_size = sizeof(cff_flashinfo->buffer);
	u32 *buffer_ptr = cff_flashinfo->buffer;

	cff_flashinfo->raw_flashinfo.rbf_offset =
		simple_strtoul(fpga_fsinfo->filename, NULL, 16);

	ret = cff_flash_probe(cff_flashinfo);

	if (0 >= ret) {
		puts("Flash probe failed.\n");
		return ret;
	}

	 /* Load mkimage header into buffer */
	flash_read(cff_flashinfo, cff_flashinfo->raw_flashinfo.rbf_offset,
		sizeof(struct image_header), buffer_ptr);

	WATCHDOG_RESET();

	memcpy(header, (u_char *)buffer_ptr, sizeof(*header));

	if (!image_check_magic(header)) {
		printf("FPGA: Bad Magic Number.\n");
		return -6;
	}

	if (!image_check_hcrc(header)) {
		printf("FPGA: Bad Header Checksum.\n");
		return -7;
	}

	/* Getting rbf data size */
	cff_flashinfo->remaining =
		image_get_data_size(header);

	/* Calculate total size of both rbf data with mkimage header */
	rbf_header_data_size = cff_flashinfo->remaining +
				sizeof(struct image_header);

	/* Loading to buffer chunk by chunk, normally for OCRAM buffer */
	if (rbf_header_data_size > buffer_size) {
		/* Calculate size of rbf data in the buffer */
		buffersize_after_header =
			buffer_size - sizeof(struct image_header);
		cff_flashinfo->remaining -= buffersize_after_header;
	} else {
	/* Loading whole rbf image into buffer, normally for DDR buffer */
		buffer_size = rbf_header_data_size;
		/* Calculate size of rbf data in the buffer */
		buffersize_after_header =
			buffer_size - sizeof(struct image_header);
		cff_flashinfo->remaining = 0;
	}

	/* Loading mkimage header and rbf data into buffer */
	flash_read(cff_flashinfo, cff_flashinfo->raw_flashinfo.rbf_offset,
		buffer_size, buffer_ptr);

	/* Getting pointer of rbf data starting address where is it
	   right after mkimage header */
	bufferptr_after_header =
		(u32 *)((u_char *)buffer_ptr + sizeof(struct image_header));

	/* Update next reading rbf data flash offset */
	cff_flashinfo->flash_offset = cff_flashinfo->raw_flashinfo.rbf_offset +
					buffer_size;

	/* Update the starting addr of rbf data to init FPGA & programming
	   into FPGA */
	*buffer = (u32)bufferptr_after_header;

	/* Update the size of rbf data to be programmed into FPGA */
	*buffer_sizebytes = buffersize_after_header;

#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	cff_flashinfo->raw_flashinfo.datacrc =
		crc32(cff_flashinfo->raw_flashinfo.datacrc,
		(u_char *)bufferptr_after_header,
		buffersize_after_header);
#endif
if (0 == (cff_flashinfo->remaining)) {
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	if (cff_flashinfo->raw_flashinfo.datacrc !=
		image_get_dcrc(&(cff_flashinfo->raw_flashinfo.header))) {
		printf("FPGA: Bad Data Checksum.\n");
		return -8;
	}
#endif
}
	return 0;
}

static int cff_flash_read(struct cff_flash_info *cff_flashinfo, u32 *buffer,
	u32 *buffer_sizebytes)
{
	int ret = 0;
	/* To avoid from keeping re-read the contents */
	size_t buffer_size = sizeof(cff_flashinfo->buffer);
	u32 *buffer_ptr = cff_flashinfo->buffer;
	struct spi_flash *flash = cff_flashinfo->raw_flashinfo.flash;
	u32 flash_addr = cff_flashinfo->flash_offset;

	/* Initialize the flash controller */
	if (NULL == flash) {
		ret = cff_flash_probe(cff_flashinfo);

		if (0 >= ret) {
			puts("Flash probe failed.\n");
			return ret;
		}
	}

	/* Buffer allocated in OCRAM */
	/* Read the data by small chunk by chunk. */
	if (cff_flashinfo->remaining > buffer_size)
		cff_flashinfo->remaining -= buffer_size;
	else {
		/* Buffer allocated in DDR, larger than rbf data most
		  of the time */
		buffer_size = cff_flashinfo->remaining;
		cff_flashinfo->remaining = 0;
	}

	flash_read(cff_flashinfo, flash_addr, buffer_size, buffer_ptr);

#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	cff_flashinfo->raw_flashinfo.datacrc =
		crc32(cff_flashinfo->raw_flashinfo.datacrc,
			(unsigned char *)buffer_ptr, buffer_size);
#endif

if (0 == (cff_flashinfo->remaining)) {
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	if (cff_flashinfo->raw_flashinfo.datacrc !=
		image_get_dcrc(&(cff_flashinfo->raw_flashinfo.header))) {
		printf("FPGA: Bad Data Checksum.\n");
		return -8;
	}
#endif
}
	/* Update next reading rbf data flash offset */
	flash_addr += buffer_size;

	cff_flashinfo->flash_offset = flash_addr;

	/* Update the starting of rbf data to be programmed into FPGA */
	*buffer = (u32)buffer_ptr;

	/* Update the size of rbf data to be programmed into FPGA */
	*buffer_sizebytes = buffer_size;

	return 0;
}

int cff_from_qspi_env(void)
{
	int qspi_rbf_addr = -1;
	fpga_fs_info fpga_fsinfo;
	char addrToString[32] = {0};

	qspi_rbf_addr = get_cff_offset(gd->fdt_blob);

	if (0 > qspi_rbf_addr) {
		printf("Error: No QSPI rbf addrress found.\n");
		return -1;
	}

	sprintf(addrToString, "%x", qspi_rbf_addr);

	fpga_fsinfo.filename = addrToString;

	fpga_fsinfo.interface = "qspi";
	/* periph rbf image */
	if (is_early_release_fpga_config(gd->fdt_blob))
		fpga_fsinfo.rbftype = "periph";
	else { /* monolithic rbf image */
		fpga_fsinfo.rbftype = "combined";
	}

	return cff_from_flash(&fpga_fsinfo);
}
#endif /* #ifdef CONFIG_CADENCE_QSPI */

#ifdef CONFIG_NAND_DENALI
/*
 * This function reads an FPGA image from NAND and programs it
 * into the FPGA.  The seemingly incoherent return codes (negative
 * this, negative that), are actually a clever way to debug
 * errors when there is no serial output, or any output at all.  If
 * this function simply returned -1 for all errors, determing the
 * actual point of failure would be extremely tedious and time-consuming.
 * By examining the return code in a debugger, a developer can quickly
 * determine the cause of failure.
 * Each return code is accompanied by a printf which
 * serves as the description of that failure.
 */
int cff_from_nand(unsigned long flash_offset)
{
	nand_info_t *nand;
	struct image_header header;
	u32 flash_addr, status;
	int ret;
	u_char temp[16384] __aligned(ARCH_DMA_MINALIGN);
	u32 remaining = 0;
	size_t len_read;
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	u32 datacrc = 0;
#endif

	if (nand_curr_device < 0) {
		printf("FPGA: NAND device not present or not recognized\n");
		return -10;
	}
	nand = &nand_info[nand_curr_device];

	/*
	 * Load mkimage header which is on top of rbf file
	 * nand reads must be page-aligned (4K)
	 */
	len_read = sizeof(temp);
	ret = nand_read_skip_bad(nand, flash_offset,
				 &len_read, NULL, sizeof(temp), temp);
	if (ret) {
		printf("FPGA: failed to read mkimage header from NAND %d\n",
		       ret);
		return ret;
	}
	if (len_read != sizeof(temp)) {
		printf("FPGA: incomplete read of mkimage header from NAND\n");
		return -12;
	}
	memcpy(&header, temp, sizeof(header));

	WATCHDOG_RESET();

	if (!image_check_magic(&header)) {
		printf("FPGA: Bad Magic Number\n");
		return -6;
	}

	if (!image_check_hcrc(&header)) {
		printf("FPGA: Bad Header Checksum\n");
		return -7;
	}

	/*
	 * load rbf data into fpga, start with rbf header, which
	 * resides just after the mkimage header
	 */
	len_read -= sizeof(struct image_header);
	status = fpgamgr_program_init((u32 *)&temp[sizeof(struct image_header)],
				      len_read);
	if (status) {
		printf("FPGA: Init failed with error code %d\n", status);
		return -2;
	}
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	datacrc = crc32(datacrc, &temp[sizeof(struct image_header)],
		len_read);
#endif

	/*
	 * now send the data, starting again with rbf header,
	 * just after mkimage header
	 */
	fpgamgr_program_write((const long unsigned int *)
			      &temp[sizeof(struct image_header)], len_read);

	/* now we are started, can loop the rest */
	flash_addr = flash_offset + sizeof(temp);
	remaining = image_get_data_size(&header);
	if (remaining < len_read)
		/* this is unexpected, but could happen... */
		remaining = 0;
	else
		remaining -= len_read;

	while (remaining) {
		/*
		 * Read the data by small chunk by chunk. At this stage,
		 * use the temp as temporary buffer.
		 */
		len_read = sizeof(temp);
		ret = nand_read_skip_bad(nand, flash_addr,
					 &len_read, NULL, sizeof(temp), temp);
		if (ret) {
			printf("FPGA: NAND read failed %d\n", ret);
			return ret;
		}
		if (len_read != sizeof(temp)) {
			printf("FPGA: incomplete read from NAND\n");
			return -13;
		}
		if (remaining > sizeof(temp)) {
			/* transfer data to FPGA Manager */
			fpgamgr_program_write((const long unsigned int *)temp,
					      sizeof(temp));
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
			datacrc = crc32(datacrc, temp, sizeof(temp));
#endif
			/* update the counter */
			remaining -= sizeof(temp);
			flash_addr += sizeof(temp);
		}  else {
			/* transfer data to FPGA Manager */
			fpgamgr_program_write((const long unsigned int *)temp,
					      remaining);
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
			datacrc = crc32(datacrc, temp, remaining);
#endif
			/* set to zero, may have read more than needed */
			remaining = 0;
		}

		WATCHDOG_RESET();
	}

	fpgamgr_program_sync();

#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	if (datacrc !=  image_get_dcrc(&header)) {
		printf("FPGA: Bad Data Checksum\n");
		return -8;
	}
#endif

	/* Ensure the FPGA entering config done */
	status = fpgamgr_program_fini();
	if (status) {
		return status;
	}

	WATCHDOG_RESET();
	return 1;
}

int cff_from_nand_env(void)
{
	int nand_rbf_addr = 0;

	nand_rbf_addr = get_cff_offset(gd->fdt_blob);

	if (0 > nand_rbf_addr) {
		printf(" Error: No NAND rbf addrress found\n");
		return -1;
	}

	return cff_from_nand(nand_rbf_addr);

}
#endif /* CONFIG_NAND_DENALI */

#if defined(CONFIG_CMD_FPGA_LOADFS)
int socfpga_loadfs(Altera_desc *desc, const void *buf, size_t bsize,
		fpga_fs_info *fsinfo)
{
	int ret = 0;
#if defined(CONFIG_MMC)
	int do_init = 0;
	int wait_early = 0;
	rbftosdramaddr = fsinfo->rbftosdramaddr;

	if (!strcmp(fsinfo->rbftype, "periph")) {
		do_init = 1;
		wait_early = 1;
	} else if (!strcmp(fsinfo->rbftype, "core")) {
			if (is_fpgamgr_early_user_mode() &&
				!is_fpgamgr_user_mode()) {
				do_init = 0;
				wait_early = 0;
			} else {
				if (is_early_release_fpga_config(gd->fdt_blob)
				) {
					printf("Failed: Program core.rbf ");
					printf("must be in Early IO Release ");
					printf("mode!!\n");
				} else {
					/* do nothing, Early IO release
					is not enabled */
					printf("Early IO release is not ");
					printf("enabled!!\n");
				}
				return ret;
			}
	} else if (!strcmp(fsinfo->rbftype, "combined")) {
		do_init = 1;
		wait_early = 0;
	} else {
		printf("Unsupported raw binary type: %s\n", fsinfo->rbftype);
	}
#endif
	/* disable all signals from hps peripheral controller to fpga */
	writel(0, &system_manager_base->fpgaintf_en_global);

	/* disable all axi bridge (hps2fpga, lwhps2fpga & fpga2hps) */
	reset_assert_all_bridges();

	reset_assert_shared_connected_peripherals();

	reset_assert_fpga_connected_peripherals();

#if defined(CONFIG_MMC)
	if (!strcmp(fsinfo->interface, "mmc")) {
		int i, slen = strlen(fsinfo->filename) + 1;

		for (i = 0; i < slen; i++)
			if (fsinfo->filename[i] == ',')
				fsinfo->filename[i] = 0;

		ret = cff_from_mmc_fat(fsinfo->dev_part, fsinfo->filename,
			slen, do_init, wait_early);
	}
#elif defined(CONFIG_CADENCE_QSPI)
	if (!strcmp(fsinfo->interface, "qspi"))
		ret = cff_from_flash(fsinfo);
#elif defined(CONFIG_NAND_DENALI)
	if (!strcmp(fsinfo->interface, "nand")) {
		u32 rbfaddr = simple_strtoul(fsinfo->dev_part, NULL, 16);
		ret = cff_from_nand(rbfaddr);
	}
#else
	printf("unsupported interface: %s\n", fsinfo->interface);
	return FPGA_FAIL;
#endif

	if (ret > 0) {
		config_shared_fpga_pins(gd->fdt_blob);
		reset_deassert_shared_connected_peripherals();
		reset_deassert_fpga_connected_peripherals();
		return FPGA_SUCCESS;
	}
	return FPGA_FAIL;
}
#endif

