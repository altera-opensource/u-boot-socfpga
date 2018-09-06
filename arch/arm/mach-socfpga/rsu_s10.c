/*
 *  Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <linux/errno.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/rsu_s10.h>
#include <command.h>
#include <spi.h>
#include <spi_flash.h>

DECLARE_GLOBAL_DATA_PTR;

struct socfpga_rsu_s10_cpb rsu_cpb = {0};
struct socfpga_rsu_s10_spt rsu_spt = {0};
u32 rsu_spt0_offset = 0, rsu_spt1_offset = 0;

static u32 rsu_print_status(void)
{
	struct socfpga_rsu_s10_status rsu_status;

	if (mbox_rsu_status((u32 *)&rsu_status, sizeof(rsu_status) / 4)) {
		puts("RSU: Error from mbox_rsu_status\n");
		return -ECOMM;
	}
	puts("RSU: Remote System Update Status\n");
	printf("Current Image\t: 0x%08x\n", rsu_status.current_image[0]);
	printf("Last Fail Image\t: 0x%08x\n", rsu_status.last_failing_image[0]);
	printf("State\t\t: 0x%08x\n", rsu_status.state);
	printf("Version\t\t: 0x%08x\n", rsu_status.version);
	printf("Error locaton\t: 0x%08x\n", rsu_status.error_location);
	printf("Error details\t: 0x%08x\n", rsu_status.error_details);

	return 0;
}

static void rsu_print_spt_slot(void)
{
	int i;

	puts("RSU: Sub-partition table content\n");
	for (i = 0; i < rsu_spt.entries; i++) {
		printf("%16s\tOffset: 0x%08x%08x\tLength: 0x%08x\tFlag : 0x%08x\n",
		       rsu_spt.spt_slot[i].name,
		       rsu_spt.spt_slot[i].offset[1],
		       rsu_spt.spt_slot[i].offset[0],
		       rsu_spt.spt_slot[i].length,
		       rsu_spt.spt_slot[i].flag);
	}
}

static void rsu_print_cpb_slot(void)
{
	int i, j = 1;

	puts("RSU: CMF pointer block's image pointer list\n");
	for (i = rsu_cpb.nslots - 1; i >= 0; i--) {
		if (rsu_cpb.pointer_slot[i] != ~0 &&
		    rsu_cpb.pointer_slot[i] != 0) {
			printf("Priority %d Offset: 0x%016llx nslot: %d\n",
			       j, rsu_cpb.pointer_slot[i], i);
			j++;
		    }
	}
}

static u32 rsu_spt_slot_find_cpb(void)
{
	int i;

	for (i = 0; i < rsu_spt.entries; i++) {
		if (strstr(rsu_spt.spt_slot[i].name, "CPB0") != NULL)
			return rsu_spt.spt_slot[i].offset[0];
	}
	puts("RSU: Cannot find SPT0 entry from sub-partition table\n");
	return 0;
}

static u32 rsu_get_boot_part_len(void)
{
	int i;
	u32 offset = 0, len = 0;

	/* look for last entry that has largest offset */
	for (i = 0; i < rsu_spt.entries; i++) {
		if (rsu_spt.spt_slot[i].offset[0] > offset) {
			offset = rsu_spt.spt_slot[i].offset[0];
			len = rsu_spt.spt_slot[i].length;
		}
	}

	/* With the len, we shall know the boot partition size */
	len += offset;
	return roundup(len, 64 << 10);	/* align to 64kB, flash sector size */
}

int rsu_spt_cpb_list(void)
{
	u32 spt_offset[4];
	u32 cpb_offset;
	struct spi_flash *flash;

	/* print the RSU status */
	if (rsu_print_status())
		return -ECOMM;

	/* retrieve the sub-partition table (spt) offset from SDM */
	if (mbox_rsu_get_spt_offset(spt_offset, 4)) {
		puts("RSU: Error from mbox_rsu_get_spt_offset\n");
		return -ECOMM;
	}
	rsu_spt0_offset = spt_offset[SPT0_INDEX];
	rsu_spt1_offset = spt_offset[SPT1_INDEX];

	/* update into U-Boot env so we can update into DTS later */
	env_set_hex("rsu_sbt0", rsu_spt0_offset);
	env_set_hex("rsu_sbt1", rsu_spt1_offset);
	printf("RSU: Sub-partition table 0 offset 0x%08x\n", rsu_spt0_offset);
	printf("RSU: Sub-partition table 1 offset 0x%08x\n", rsu_spt1_offset);

	/* retrieve sub-partition table (spt) from flash */
	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
				CONFIG_SF_DEFAULT_CS,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		puts("RSU: SPI probe failed.\n");
		return -ENODEV;
	}
	if (spi_flash_read(flash, rsu_spt0_offset, sizeof(rsu_spt), &rsu_spt)) {
		puts("RSU: spi_flash_read failed\n");
		return -EIO;
	}

	/* valid the sub-partition table (spt) magic number */
	if (rsu_spt.magic_number != RSU_S10_SPT_MAGIC_NUMBER) {
		printf("RSU: Sub-partition table magic number not match 0x%08x\n",
		       rsu_spt.magic_number);
		return -EFAULT;
	}

	/* list the sub-partition table (spt) content */
	rsu_print_spt_slot();

	/* locate where is CMF pointer block (cpb) */
	cpb_offset = rsu_spt_slot_find_cpb();
	if (!cpb_offset)
		return -ENXIO;
	printf("RSU: CMF pointer block offset 0x%08x\n", cpb_offset);

	/* retrieve CMF pointer block (cpb) from flash */
	if (spi_flash_read(flash, cpb_offset, sizeof(rsu_cpb), &rsu_cpb)) {
		puts("RSU: spi_flash_read failed\n");
		return -EIO;
	}

	/* valid the CMF pointer block (cpb) magic number */
	if (rsu_cpb.magic_number != RSU_S10_CPB_MAGIC_NUMBER) {
		printf("RSU: CMF pointer block magic number not match 0x%08x\n",
		       rsu_cpb.magic_number);
		return -EFAULT;
	}

	/* list the CMF pointer block (cpb) content */
	rsu_print_cpb_slot();

	return 0;
}

int rsu_update(int argc, char * const argv[])
{
	u32 flash_offset[2];
	u64 addr;
	char *endp;

	if (argc != 2)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[1], &endp, 16);

	flash_offset[0] = addr & 0xFFFFFFFF;
	flash_offset[1] = (addr >> 32) & 0xFFFFFFFF;

	printf("RSU: RSU update to 0x%08x%08x\n",
	       flash_offset[1], flash_offset[0]);
	mbox_rsu_update(flash_offset);
	return 0;
}

int rsu_dtb(int argc, char * const argv[])
{
	char flash0_string[100];
	const char *fdt_flash0;
	int nodeoffset, len;
	u32 reg[2];

	/* Extracting RSU info from bitstream */
	if (rsu_spt_cpb_list())
		return -ECOMM;

	/* Extract the flash0's reg from Linux DTB */
	nodeoffset = fdt_path_offset(working_fdt, "/__symbols__");
	if (nodeoffset < 0) {
		puts("DTB: __symbols__ node not found. Ensure you load kernel"
		     "dtb and fdt addr\n");
		return -ENODEV;
	}
	fdt_flash0 = fdt_getprop(working_fdt, nodeoffset, "qspi_boot", &len);
	if (fdt_flash0 == NULL) {
		puts("DTB: qspi_boot alias node not found. Check your dts\n");
		return -ENODEV;
	}
	strcpy(flash0_string, fdt_flash0);
	printf("DTB: qspi_boot node at %s\n", flash0_string);

	/* assemble new reg value for boot partition */
	len = rsu_get_boot_part_len();
	reg[0] = cpu_to_fdt32(rsu_spt0_offset);
	reg[1] = cpu_to_fdt32(len  - rsu_spt0_offset);

	/* update back to Linux DTB */
	nodeoffset = fdt_path_offset(working_fdt, flash0_string);
	if (nodeoffset < 0) {
		printf("DTB: %s node not found\n", flash0_string);
		return -ENODEV;
	}
	return fdt_setprop(working_fdt, nodeoffset, "reg", reg, sizeof(reg));
}

int do_rsu(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	const char *cmd;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	cmd = argv[1];
	--argc;
	++argv;

	if (strcmp(cmd, "list") == 0)
		ret = rsu_spt_cpb_list();
	else if (strcmp(cmd, "update") == 0)
		ret = rsu_update(argc, argv);
	else if (strcmp(cmd, "dtb") == 0)
		ret = rsu_dtb(argc, argv);
	else
		return CMD_RET_USAGE;

	return ret;
}

U_BOOT_CMD(
	rsu, 3, 1, do_rsu,
	"SoCFPGA Stratix10 SoC Remote System Update",
	"list  - List down the available bitstreams in flash\n"
	"update <flash_offset> - Initiate SDM to load bitstream as specified\n"
	"		       by flash_offset\n"
	"dtb   - Update Linux DTB qspi-boot parition offset with spt0 value\n"
	""
);
