/*
 *  Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <linux/errno.h>
#include <asm/arch/mailbox_s10.h>
#include <asm/arch/rsu.h>
#include <asm/arch/rsu_s10.h>
#include <command.h>
#include <spi.h>
#include <spi_flash.h>

DECLARE_GLOBAL_DATA_PTR;

struct socfpga_rsu_s10_cpb rsu_cpb = {0};
struct socfpga_rsu_s10_spt rsu_spt = {0};
u32 rsu_spt0_offset = 0, rsu_spt1_offset = 0;

static int initialized;

static int rsu_print_status(void)
{
	struct rsu_status_info status_info;

	if (mbox_rsu_status((u32 *)&status_info, sizeof(status_info) / 4)) {
		puts("RSU: Firmware or flash content not supporting RSU\n");
		return -ENOTSUPP;
	}
	puts("RSU: Remote System Update Status\n");
	printf("Current Image\t: 0x%08llx\n", status_info.current_image);
	printf("Last Fail Image\t: 0x%08llx\n", status_info.fail_image);
	printf("State\t\t: 0x%08x\n", status_info.state);
	printf("Version\t\t: 0x%08x\n", status_info.version);
	printf("Error location\t: 0x%08x\n", status_info.error_location);
	printf("Error details\t: 0x%08x\n", status_info.error_details);
	if (RSU_VERSION_ACMF_VERSION(status_info.version) &&
	    RSU_VERSION_DCMF_VERSION(status_info.version))
		printf("Retry counter\t: 0x%08x\n", status_info.retry_counter);

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

int rsu_spt_cpb_list(int argc, char * const argv[])
{
	u32 spt_offset[4];
	u32 cpb_offset;
	int err;
	struct spi_flash *flash;

	if (argc != 1)
		return CMD_RET_USAGE;

	/* print the RSU status */
	err = rsu_print_status();
	if (err)
		return err;

	/* retrieve the sub-partition table (spt) offset from firmware */
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

	flash_offset[0] = lower_32_bits(addr);
	flash_offset[1] = upper_32_bits(addr);

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
	int err;

	/* Extracting RSU info from bitstream */
	err = rsu_spt_cpb_list(argc, argv);
	if (err == -ENOTSUPP)
		return 0;
	else if (err)
		return err;

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

static int slot_count(int argc, char * const argv[])
{
	int count;

	if (argc != 1)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	count = rsu_slot_count();
	if (count < 0)
		return CMD_RET_FAILURE;

	printf("Number of slots = %d.\n", count);

	return CMD_RET_SUCCESS;
}

static int slot_by_name(int argc, char * const argv[])
{
	char *name = argv[1];
	int slot;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = rsu_slot_by_name(name);
	if (slot < 0)
		return CMD_RET_FAILURE;

	printf("Slot name '%s' is %d.\n", name, slot);
	return CMD_RET_SUCCESS;
}

static int slot_get_info(int argc, char * const argv[])
{
	int slot;
	char *endp;
	struct rsu_slot_info info;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	ret = rsu_slot_get_info(slot, &info);
	if (ret)
		return CMD_RET_FAILURE;

	printf("NAME: %s\n", info.name);
	printf("OFFSET: 0x%016llX\n", info.offset);
	printf("SIZE: 0x%08X\n", info.size);
	if (info.priority)
		printf("PRIORITY: %i\n", info.priority);
	else
		printf("PRIORITY: [disabled]\n");

	return CMD_RET_SUCCESS;
}

static int slot_size(int argc, char * const argv[])
{
	int slot;
	char *endp;
	int size;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	size = rsu_slot_size(slot);
	if (size < 0)
		return CMD_RET_FAILURE;

	printf("Slot %d size = %d.\n", slot, size);
	return CMD_RET_SUCCESS;
}

static int slot_priority(int argc, char * const argv[])
{
	int slot;
	char *endp;
	int priority;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	priority = rsu_slot_priority(slot);
	if (priority < 0)
		return CMD_RET_FAILURE;

	printf("Slot %d priority = %d.\n", slot, priority);
	return CMD_RET_SUCCESS;
}

static int slot_erase(int argc, char * const argv[])
{
	int slot;
	char *endp;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	ret = rsu_slot_erase(slot);
	if (ret)
		return CMD_RET_FAILURE;

	printf("Slot %d erased.\n", slot);
	return CMD_RET_SUCCESS;
}

static int slot_program_buf(int argc, char * const argv[])
{
	int slot;
	char *endp;
	u64 address;
	int size;
	int ret;
	int addr_lo;
	int addr_hi;

	if (argc != 4)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	address = simple_strtoul(argv[2], &endp, 16);
	size = simple_strtoul(argv[3], &endp, 16);

	ret = rsu_slot_program_buf(slot, (void *)address, size);
	if (ret)
		return CMD_RET_FAILURE;

	addr_hi = upper_32_bits(address);
	addr_lo = lower_32_bits(address);
	printf("Slot %d was programmed with buffer=0x%08x%08x size=%d.\n",
	       slot, addr_hi, addr_lo, size);

	return CMD_RET_SUCCESS;
}

static int slot_program_factory_update_buf(int argc, char * const argv[])
{
	int slot;
	char *endp;
	u64 address;
	int size;
	int ret;
	int addr_lo;
	int addr_hi;

	if (argc != 4)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	address = simple_strtoul(argv[2], &endp, 16);
	size = simple_strtoul(argv[3], &endp, 16);

	ret = rsu_slot_program_factory_update_buf(slot, (void *)address, size);
	if (ret)
		return CMD_RET_FAILURE;

	addr_hi = upper_32_bits(address);
	addr_lo = lower_32_bits(address);
	printf("Slot %d was programmed with buffer=0x%08x%08x size=%d.\n",
	       slot, addr_hi, addr_lo, size);

	return CMD_RET_SUCCESS;
}

static int slot_program_buf_raw(int argc, char * const argv[])
{
	int slot;
	char *endp;
	u64 address;
	int size;
	int ret;
	int addr_lo;
	int addr_hi;

	if (argc != 4)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	address = simple_strtoul(argv[2], &endp, 16);
	size = simple_strtoul(argv[3], &endp, 16);

	ret = rsu_slot_program_buf_raw(slot, (void *)address, size);
	if (ret)
		return CMD_RET_FAILURE;

	addr_hi = upper_32_bits(address);
	addr_lo = lower_32_bits(address);
	printf("Slot %d was programmed with raw buffer=0x%08x%08x size=%d.\n",
	       slot, addr_hi, addr_lo, size);

	return CMD_RET_SUCCESS;
}

static int slot_verify_buf(int argc, char * const argv[])
{
	int slot;
	char *endp;
	u64 address;
	int size;
	int ret;
	int addr_lo;
	int addr_hi;

	if (argc != 4)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	address = simple_strtoul(argv[2], &endp, 16);
	size = simple_strtoul(argv[3], &endp, 16);

	ret = rsu_slot_verify_buf(slot, (void *)address, size);
	if (ret)
		return CMD_RET_FAILURE;

	addr_hi = upper_32_bits(address);
	addr_lo = lower_32_bits(address);
	printf("Slot %d was verified with buffer=0x%08x%08x size=%d.\n",
	       slot, addr_hi, addr_lo, size);

	return CMD_RET_SUCCESS;
}

static int slot_verify_buf_raw(int argc, char * const argv[])
{
	int slot;
	char *endp;
	u64 address;
	int size;
	int ret;
	int addr_lo;
	int addr_hi;

	if (argc != 4)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	address = simple_strtoul(argv[2], &endp, 16);
	size = simple_strtoul(argv[3], &endp, 16);

	ret = rsu_slot_verify_buf_raw(slot, (void *)address, size);
	if (ret)
		return CMD_RET_FAILURE;

	addr_hi = upper_32_bits(address);
	addr_lo = lower_32_bits(address);
	printf("Slot %d was verified with raw buffer=0x%08x%08x size=%d.\n",
	       slot, addr_hi, addr_lo, size);

	return CMD_RET_SUCCESS;
}

static int slot_enable(int argc, char * const argv[])
{
	int slot;
	char *endp;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	ret = rsu_slot_enable(slot);
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("Slot %d enabled.\n", slot);
	return CMD_RET_SUCCESS;
}

static int slot_disable(int argc, char * const argv[])
{
	int slot;
	char *endp;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	ret = rsu_slot_disable(slot);
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("Slot %d disabled.\n", slot);
	return CMD_RET_SUCCESS;
}

static int slot_load(int argc, char * const argv[])
{
	int slot;
	char *endp;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	ret = rsu_slot_load(slot);
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("Slot %d loading.\n", slot);
	return CMD_RET_SUCCESS;
}

static int slot_load_factory(int argc, char * const argv[])
{
	int ret;

	if (argc != 1)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	ret = rsu_slot_load_factory();
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("Factory loading.\n");
	return CMD_RET_SUCCESS;
}

static int slot_rename(int argc, char * const argv[])
{
	int slot;
	char *endp;
	char *name;
	int ret;

	if (argc != 3)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);
	name = argv[2];

	ret = rsu_slot_rename(slot, name);
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("Slot %d renamed to %s.\n", slot, name);
	return CMD_RET_SUCCESS;
}

static int slot_delete(int argc, char * const argv[])
{
	int slot;
	char *endp;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	slot = simple_strtoul(argv[1], &endp, 16);

	ret = rsu_slot_delete(slot);
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("Slot %d deleted.\n", slot);
	return CMD_RET_SUCCESS;
}

static int slot_create(int argc, char * const argv[])
{
	char *endp;
	char *name;
	int address;
	int size;
	int ret;

	if (argc != 4)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	name = argv[1];
	address = simple_strtoul(argv[2], &endp, 16);
	size = simple_strtoul(argv[3], &endp, 16);

	ret = rsu_slot_create(name, address, size);
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("Slot %s created at 0x%08x with size =  0x%08x bytes.\n", name,
	       address, size);
	return CMD_RET_SUCCESS;
}

static int status_log(int argc, char * const argv[])
{
	struct rsu_status_info info;
	int ret;

	if (argc != 1)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	ret = rsu_status_log(&info);
	if (ret < 0)
		return CMD_RET_FAILURE;

	printf("Current Image\t: 0x%08llx\n", info.current_image);
	printf("Last Fail Image\t: 0x%08llx\n", info.fail_image);
	printf("State\t\t: 0x%08x\n", info.state);
	printf("Version\t\t: 0x%08x\n", info.version);
	printf("Error location\t: 0x%08x\n", info.error_location);
	printf("Error details\t: 0x%08x\n", info.error_details);
	if (info.version)
		printf("Retry counter\t: 0x%08x\n", info.retry_counter);

	return CMD_RET_SUCCESS;
}

static int notify(int argc, char * const argv[])
{
	u32 stage;
	char *endp;
	int ret;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	stage = simple_strtoul(argv[1], &endp, 16);
	ret = rsu_notify(stage);
	if (ret)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int clear_error_status(int argc, char * const argv[])
{
	int ret;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	ret = rsu_clear_error_status();
	if (ret)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int reset_retry_counter(int argc, char * const argv[])
{
	int ret;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	ret = rsu_reset_retry_counter();
	if (ret)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

static int display_dcmf_version(int argc, char * const argv[])
{
	int i, ret;
	u32 versions[4];

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	ret = rsu_dcmf_version(versions);
	if (ret)
		return CMD_RET_FAILURE;

	for (i = 0; i < 4; i++)
		printf("DCMF%d version = %d.%d.%d\n", i,
		       (int)DCMF_VERSION_MAJOR(versions[i]),
		       (int)DCMF_VERSION_MINOR(versions[i]),
		       (int)DCMF_VERSION_UPDATE(versions[i]));

	return CMD_RET_SUCCESS;
}

static int display_dcmf_status(int argc, char * const argv[])
{
	int i, ret;
	u16 status[4];

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	ret = rsu_dcmf_status(status);
	if (ret)
		return CMD_RET_FAILURE;

	for (i = 0; i < 4; i++)
		printf("DCMF%d: %s\n", i, status[i] ? "Corrupted" : "OK");

	return CMD_RET_SUCCESS;
}

static int display_max_retry(int argc, char * const argv[])
{
	int ret;
	u8 value;

	if (!initialized) {
		if (rsu_init(NULL))
			return CMD_RET_FAILURE;

		initialized = 1;
	}

	ret = rsu_max_retry(&value);
	if (ret)
		return CMD_RET_FAILURE;

	printf("max_retry = %d\n", (int)value);

	return CMD_RET_SUCCESS;
}

struct func_t {
	const char *cmd_string;
	int (*func_ptr)(int cmd_argc, char * const cmd_argv[]);
};

static const struct func_t rsu_func_t[] = {
	{"dtb", rsu_dtb},
	{"list", rsu_spt_cpb_list},
	{"slot_by_name", slot_by_name},
	{"slot_count", slot_count},
	{"slot_disable", slot_disable},
	{"slot_enable", slot_enable},
	{"slot_erase", slot_erase},
	{"slot_get_info", slot_get_info},
	{"slot_load", slot_load},
	{"slot_load_factory", slot_load_factory},
	{"slot_priority", slot_priority},
	{"slot_program_buf", slot_program_buf},
	{"slot_program_buf_raw", slot_program_buf_raw},
	{"slot_program_factory_update_buf", slot_program_factory_update_buf},
	{"slot_rename", slot_rename},
	{"slot_delete", slot_delete},
	{"slot_create", slot_create},
	{"slot_size", slot_size},
	{"slot_verify_buf", slot_verify_buf},
	{"slot_verify_buf_raw", slot_verify_buf_raw},
	{"status_log", status_log},
	{"update", rsu_update},
	{"notify", notify},
	{"clear_error_status", clear_error_status},
	{"reset_retry_counter", reset_retry_counter},
	{"display_dcmf_version", display_dcmf_version},
	{"display_dcmf_status", display_dcmf_status},
	{"display_max_retry", display_max_retry}
};

int do_rsu(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	const char *cmd;
	int i;

	if (argc < 2)
		return CMD_RET_USAGE;

	cmd = argv[1];
	--argc;
	++argv;

	for (i = 0; i < ARRAY_SIZE(rsu_func_t); i++) {
		if (!strcmp(cmd, rsu_func_t[i].cmd_string))
			return rsu_func_t[i].func_ptr(argc, argv);
	}

	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	rsu, 5, 1, do_rsu,
#ifdef CONFIG_TARGET_SOCFPGA_AGILEX
	"Agilex SoC Remote System Update",
#else
	"Stratix 10 SoC Remote System Update",
#endif
	"dtb   - Update Linux DTB qspi-boot parition offset with spt0 value\n"
	"list  - List down the available bitstreams in flash\n"
	"slot_by_name <name> - find slot by name and display the slot number\n"
	"slot_count - display the slot count\n"
	"slot_disable <slot> - remove slot from CPB\n"
	"slot_enable <slot> - make slot the highest priority\n"
	"slot_erase <slot> - erase slot\n"
	"slot_get_info <slot> - display slot information\n"
	"slot_load <slot> - load slot immediately\n"
	"slot_load_factory - load factory immediately\n"
	"slot_priority <slot> - display slot priority\n"
	"slot_program_buf <slot> <buffer> <size> - program buffer into slot, and make it highest priority\n"
	"slot_program_buf_raw <slot> <buffer> <size> - program raw buffer into slot\n"
	"slot_program_factory_update_buf <slot> <buffer> <size> - program factory update buffer into slot, and make it highest priority\n"
	"slot_rename <slot> <name> - rename slot\n"
	"slot_delete <slot> - delete slot\n"
	"slot_create <name> <address> <size> - create slot\n"
	"slot_size <slot> - display slot size\n"
	"slot_verify_buf <slot> <buffer> <size> - verify slot contents against buffer\n"
	"slot_verify_buf_raw <slot> <buffer> <size> - verify slot contents against raw buffer\n"
	"status_log - display RSU status\n"
	"update <flash_offset> - Initiate firmware to load bitstream as specified by flash_offset\n"
	"notify <value> - Let SDM know the current state of HPS software\n"
	"clear_error_status - clear the RSU error status\n"
	"reset_retry_counter - reset the RSU retry counter\n"
	"display_dcmf_version - display DCMF versions and store them for SMC handler usage\n"
	"display_dcmf_status - display DCMF status and store it for SMC handler usage\n"
	"display_max_retry - display max_retry parameter, and store it for SMC handler usage\n"
	""
);
