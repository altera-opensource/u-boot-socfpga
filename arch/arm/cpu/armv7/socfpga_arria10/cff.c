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

DECLARE_GLOBAL_DATA_PTR;

static const const char *boot_flash_type = CONFIG_BOOT_FLASH_TYPE;

/* Local functions */
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
#ifdef CONFIG_MMC
	/* we are looking at the FAT partition */
	if (fs_set_blk_dev("mmc", cff_flashinfo->sdmmc_flashinfo.dev_part,
		FS_TYPE_FAT)) {
		printf("Failed to set filesystem to FAT.\n");
		return -2;
	}
#endif
#ifdef CONFIG_NAND_DENALI
	if (nand_curr_device < 0) {
		printf("FPGA: NAND device not present or not recognized.\n");
		return -10;
	}

	cff_flashinfo->raw_flashinfo.flash =
		&nand_info[nand_curr_device];
#endif

	return 1;
}

static int flash_read(struct cff_flash_info *cff_flashinfo,
	size_t size_read,
	u32 *buffer_ptr)
{
#ifdef CONFIG_NAND_DENALI
	int ret = 0;
#endif
	size_t bytesread = 1;

#ifdef CONFIG_CADENCE_QSPI
	spi_flash_read(cff_flashinfo->raw_flashinfo.flash,
		cff_flashinfo->flash_offset,
		size_read,
		buffer_ptr);
#endif
#ifdef CONFIG_MMC
	bytesread = file_fat_read_at(cff_flashinfo->sdmmc_flashinfo.filename,
			cff_flashinfo->flash_offset, buffer_ptr, size_read);

	if (bytesread != size_read) {
		printf("Failed to read %s from FAT %d ",
			cff_flashinfo->sdmmc_flashinfo.filename, bytesread);
		printf("!= %d.\n", size_read);
		return -3;
	}
#endif
#ifdef CONFIG_NAND_DENALI
	/*
	 * Load data from NAND into buffer
	 * nand reads must be page-aligned (4K)
	 */
	bytesread = size_read;

	ret = nand_read_skip_bad(cff_flashinfo->raw_flashinfo.flash,
		cff_flashinfo->flash_offset, &bytesread, NULL, size_read,
		(u_char *)buffer_ptr);

	if (ret) {
		printf("FPGA: Failed to read data from NAND %d.\n",
		       ret);
		return -1;
	}

	if (bytesread != size_read) {
		printf("FPGA: incomplete reading data from NAND.\n");
		return -12;
	}
#endif
	return bytesread;
}

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

int cff_from_sdmmc_env(void)
{
	int len = 0;
	int rval = -1;
	fpga_fs_info fpga_fsinfo;
	const char *cff = get_cff_filename(gd->fdt_blob, &len);

	if (cff && (len > 0)) {
		mmc_initialize(gd->bd);

		fpga_fsinfo.filename = (char *)cff;

		fpga_fsinfo.interface = "sdmmc";

		fpga_fsinfo.dev_part = getenv("cff_devsel_partition");

		if (NULL == fpga_fsinfo.dev_part) {
			fpga_fsinfo.dev_part = "0:1";

			printf("No SD/MMC partition found in environment. ");
			printf("Assuming 0:1.\n");
		}

		if (is_early_release_fpga_config(gd->fdt_blob))
			fpga_fsinfo.rbftype = "periph";
		else/* monolithic rbf image */
			fpga_fsinfo.rbftype = "combined";

		rval = cff_from_flash(&fpga_fsinfo);
	}

	return rval;
}

#if !defined(CONFIG_CHECK_FPGA_DATA_CRC)
/*
 * This function is called when the optional checksum checking on SDMMC FPGA
 * image is not required.
 */
int cff_flash_preinit(struct cff_flash_info *cff_flashinfo,
	fpga_fs_info *fpga_fsinfo, u32 *buffer, u32 *buffer_sizebytes)
{
	int bytesread = 0;
	int ret = 0;
	size_t buffer_size = *buffer_sizebytes;
	u32 *buffer_ptr = (u32 *)*buffer;

	cff_flashinfo->sdmmc_flashinfo.filename = fpga_fsinfo->filename;
	cff_flashinfo->sdmmc_flashinfo.dev_part = fpga_fsinfo->dev_part;
	cff_flashinfo->flash_offset = 0;

	ret = cff_flash_probe(cff_flashinfo);

	if (0 >= ret) {
		puts("Flash probe failed.\n");
		return ret;
	}

	/* checking the size of the file */
	cff_flashinfo->remaining = fs_size(fpga_fsinfo->filename);
	if (cff_flashinfo->remaining == -1) {
		printf("Error - %s not found within SDMMC.\n",
			fpga_fsinfo->filename);
		return -3;
	}

	WATCHDOG_RESET();

	if (cff_flashinfo->remaining > buffer_size)
		cff_flashinfo->remaining -= buffer_size;
	else {
		buffer_size = cff_flashinfo->remaining;
		cff_flashinfo->remaining = 0;
	}

	bytesread = flash_read(cff_flashinfo, buffer_size, buffer_ptr);

	if (0 >= bytesread) {
		printf(" Failed to read rbf header from FAT.\n");
		return -4;
	}

	/* Update next reading rbf data flash offset */
	cff_flashinfo->flash_offset += bytesread;

	/* Update the size of rbf data to be programmed into FPGA */
	*buffer_sizebytes = buffer_size;

	printf("FPGA: writing %s ...\n", fpga_fsinfo->filename);

	WATCHDOG_RESET();

	return 0;
}

/*
 * This function is called when the optional checksum checking on SDMMC FPGA
 * image is not required.
 */
int cff_flash_read(struct cff_flash_info *cff_flashinfo, u32 *buffer,
	u32 *buffer_sizebytes)
{
	u32 bytesread = 0;
	/* To avoid from keeping re-read the contents */
	u32 flash_addr = cff_flashinfo->flash_offset;
	size_t buffer_size = *buffer_sizebytes;
	u32 *buffer_ptr = (u32 *)*buffer;

	/* Read the data by small chunk by chunk. */
	if (cff_flashinfo->remaining > buffer_size)
		cff_flashinfo->remaining -= buffer_size;
	else {
		buffer_size = cff_flashinfo->remaining;
		cff_flashinfo->remaining = 0;
	}

	bytesread = flash_read(cff_flashinfo, buffer_size, buffer_ptr);

	if (0 >= bytesread) {
		printf(" Failed to read rbf data from FAT.\n");
		return -1;
	}

	/* Update next reading rbf data flash offset */
	flash_addr += bytesread;

	cff_flashinfo->flash_offset = flash_addr;

	/* Update the size of rbf data to be programmed into FPGA */
	*buffer_sizebytes = buffer_size;

	return 0;
}
#endif /* #if !defined(CONFIG_CHECK_FPGA_DATA_CRC) */
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

int cff_from_flash(fpga_fs_info *fpga_fsinfo)
{
	u32 buffer = 0;
	u32 buffer_ori = 0;
	u32 buffer_sizebytes = 0;
	u32 buffer_sizebytes_ori = 0;
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

	/* Loading rbf data with DDR, faster than OCRAM,
	   only for core rbf */
	if (!strcmp(fpga_fsinfo->rbftype, "core") &&
		(NULL != fpga_fsinfo->rbftosdramaddr) &&
		(0 != gd->ram_size)) {
			/* Loading mkimage header and rbf data into
			   DDR instead of OCRAM */
			buffer = buffer_ori = (u32)fpga_fsinfo->rbftosdramaddr;

			/* Calculating available DDR size */
			buffer_sizebytes = gd->ram_size -
				(u32)fpga_fsinfo->rbftosdramaddr;

			buffer_sizebytes_ori = buffer_sizebytes;
	} else {
		buffer = buffer_ori = (u32)cff_flashinfo.buffer;

		buffer_sizebytes = buffer_sizebytes_ori = sizeof(cff_flashinfo.buffer);
	}

	/* Note: Both buffer and buffer_sizebytes values can be altered by
	   function below. */
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
		ret = cff_flash_read(&cff_flashinfo, &buffer_ori,
			&buffer_sizebytes_ori);

		if (ret)
			return ret;

		/* transfer data to FPGA Manager */
		fpgamgr_program_write((const long unsigned int *)buffer_ori,
			buffer_sizebytes_ori);

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

#if defined(CONFIG_CADENCE_QSPI) || defined(CONFIG_NAND_DENALI) || \
+(defined(CONFIG_MMC) && defined(CONFIG_CHECK_FPGA_DATA_CRC))
/*
 * This function is called when the optional checksum checking on SDMMC, QSPI,
 * and NAND FPGA image are required
 */
static int cff_flash_preinit(struct cff_flash_info *cff_flashinfo,
	fpga_fs_info *fpga_fsinfo, u32 *buffer, u32 *buffer_sizebytes)
{
	u32 *bufferptr_after_header = NULL;
	u32 buffersize_after_header = 0;
	u32 rbf_header_data_size = 0;
	int ret = 0;
	int bytesread = 0;
	/* To avoid from keeping re-read the contents */
	struct image_header *header = &(cff_flashinfo->header);
	size_t buffer_size = *buffer_sizebytes;
	u32 *buffer_ptr = (u32 *)*buffer;

#if defined(CONFIG_CADENCE_QSPI) || defined(CONFIG_NAND_DENALI)
	cff_flashinfo->flash_offset =
		simple_strtoul(fpga_fsinfo->filename, NULL, 16);
#elif defined(CONFIG_MMC)
	cff_flashinfo->sdmmc_flashinfo.filename = fpga_fsinfo->filename;
	cff_flashinfo->sdmmc_flashinfo.dev_part = fpga_fsinfo->dev_part;
	cff_flashinfo->flash_offset = 0;
#else
#error "Unknown Flash type!"
#endif

	ret = cff_flash_probe(cff_flashinfo);

	if (0 >= ret) {
		puts("Flash probe failed.\n");
		return ret;
	}

	 /* Load mkimage header into buffer */
	bytesread = flash_read(cff_flashinfo,
			sizeof(struct image_header), buffer_ptr);

	if (0 >= bytesread) {
		printf(" Failed to read mkimage header from flash.\n");
		return -4;
	}

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
	bytesread = flash_read(cff_flashinfo, buffer_size, buffer_ptr);

	if (0 >= bytesread) {
		printf(" Failed to read mkimage header and rbf data ");
		printf("from flash.\n");
		return -5;
	}

	/* Getting pointer of rbf data starting address where is it
	   right after mkimage header */
	bufferptr_after_header =
		(u32 *)((u_char *)buffer_ptr + sizeof(struct image_header));

	/* Update next reading rbf data flash offset */
	cff_flashinfo->flash_offset += buffer_size;

	/* Update the starting addr of rbf data to init FPGA & programming
	   into FPGA */
	*buffer = (u32)bufferptr_after_header;

	/* Update the size of rbf data to be programmed into FPGA */
	*buffer_sizebytes = buffersize_after_header;

#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	cff_flashinfo->datacrc =
		crc32(cff_flashinfo->datacrc,
		(u_char *)bufferptr_after_header,
		buffersize_after_header);
#endif
if (0 == cff_flashinfo->remaining) {
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	if (cff_flashinfo->datacrc !=
		image_get_dcrc(&(cff_flashinfo->header))) {
		printf("FPGA: Bad Data Checksum.\n");
		return -8;
	}
#endif
}
	return 0;
}

/*
 * This function is called when the optional checksum checking on SDMMC, QSPI,
 * and NAND FPGA image are required.
 */
static int cff_flash_read(struct cff_flash_info *cff_flashinfo, u32 *buffer,
	u32 *buffer_sizebytes)
{
	int bytesread = 0;
	/* To avoid from keeping re-read the contents */
	size_t buffer_size = *buffer_sizebytes;
	u32 *buffer_ptr = (u32 *)*buffer;
	u32 flash_addr = cff_flashinfo->flash_offset;

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

	bytesread = flash_read(cff_flashinfo, buffer_size, buffer_ptr);

	if (0 >= bytesread) {
		printf(" Failed to read rbf data from flash.\n");
		return -9;
	}

#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	cff_flashinfo->datacrc =
		crc32(cff_flashinfo->datacrc,
			(unsigned char *)buffer_ptr, buffer_size);
#endif

if (0 == cff_flashinfo->remaining) {
#ifdef CONFIG_CHECK_FPGA_DATA_CRC
	if (cff_flashinfo->datacrc !=
		image_get_dcrc(&(cff_flashinfo->header))) {
		printf("FPGA: Bad Data Checksum.\n");
		return -8;
	}
#endif
}
	/* Update next reading rbf data flash offset */
	flash_addr += buffer_size;

	cff_flashinfo->flash_offset = flash_addr;

	/* Update the size of rbf data to be programmed into FPGA */
	*buffer_sizebytes = buffer_size;

	return 0;
}
#endif /* #if defined(CONFIG_CADENCE_QSPI) || defined(CONFIG_NAND_DENALI) || \
(defined(CONFIG_MMC) && defined(CONFIG_CHECK_FPGA_DATA_CRC)) */

#if defined(CONFIG_CADENCE_QSPI)
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
int cff_from_nand_env(void)
{
	int nand_rbf_addr = -1;
	fpga_fs_info fpga_fsinfo;
	char addrToString[32] = {0};

	nand_rbf_addr = get_cff_offset(gd->fdt_blob);

	if (0 > nand_rbf_addr) {
		printf(" Error: No NAND rbf addrress found\n");
		return -1;
	}

	sprintf(addrToString, "%x", nand_rbf_addr);

	fpga_fsinfo.filename = addrToString;

	fpga_fsinfo.interface = "nand";

	/* periph rbf image */
	if (is_early_release_fpga_config(gd->fdt_blob))
		fpga_fsinfo.rbftype = "periph";
	else /* monolithic rbf image */
		fpga_fsinfo.rbftype = "combined";

	return cff_from_flash(&fpga_fsinfo);
}
#endif /* CONFIG_NAND_DENALI */

#if defined(CONFIG_CMD_FPGA_LOADFS)
int socfpga_loadfs(Altera_desc *desc, const void *buf, size_t bsize,
		fpga_fs_info *fsinfo)
{
	int ret = 0;
	int programming_core;
	unsigned int com_port = 0;

	if (!strcmp(fsinfo->rbftype, "core"))
		programming_core = 1;
	else
		programming_core = 0;

	if (strcmp(fsinfo->interface, boot_flash_type)) {
		printf("unsupported flash type: %s != %s\n",
			fsinfo->interface, boot_flash_type);
		return FPGA_FAIL;
	}

	if (programming_core &&
		!(is_fpgamgr_early_user_mode() && !is_fpgamgr_user_mode())) {
		printf("FPGA must be in Early Release mode to program core.\n");
		return FPGA_FAIL;
	}

	/* disable all signals from hps peripheral controller to fpga */
	writel(0, &system_manager_base->fpgaintf_en_global);

	/* disable all axi bridge (hps2fpga, lwhps2fpga & fpga2hps) */
	reset_assert_all_bridges();

	if (!programming_core) {
		com_port = dedicated_uart_com_port(gd->fdt_blob);
		/* UART console is not connected with dedicated IO */
		if (!com_port) {
			gd->uart_ready_for_console = 0;
			gd->have_console = 0;
		}
		reset_assert_shared_connected_peripherals();
	}

	reset_assert_fpga_connected_peripherals();

	ret = cff_from_flash(fsinfo);

	if (ret > 0) {

		/* programming periph or combined */
		if (!programming_core) {
			config_pins(gd->fdt_blob, "shared");
			reset_deassert_shared_connected_peripherals();
			shared_uart_buffer_to_console();
		}

		/* programming core or combined */
		if (strcmp(fsinfo->rbftype, "periph")) {
			config_pins(gd->fdt_blob, "fpga");
			reset_deassert_fpga_connected_peripherals();
		}
		return FPGA_SUCCESS;
	}

	return FPGA_FAIL;
}
#endif

