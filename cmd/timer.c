// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2001
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * Copyright (C) 2023 Intel Corporation <www.intel.com>
 */

#include <common.h>
#include <command.h>
#include <cpu_func.h>
#include <dm.h>
#include <asm/io.h>
#include <linux/delay.h>

static struct udevice *currdev;
static struct udevice *prevdev;

static int do_timer(struct cmd_tbl *cmdtp, int flag, int argc,
		    char *const argv[])
{
	static ulong start;
	struct uclass *uc;
	int ret;
	struct udevice *dev_list;
	ulong msecs;

	if (argc > 1) {
		ret = uclass_get_device_by_name(UCLASS_TIMER, argv[1], &currdev);
		if (ret) {
			currdev = prevdev;

			if (!currdev) {
				ret = uclass_get(UCLASS_TIMER, &uc);
				if (ret)
					return CMD_RET_FAILURE;

				uclass_foreach_dev(dev_list, uc)
					printf("%s (%s)\n", dev_list->name, dev_list->driver->name);

				printf("No timer device found: %s\n", argv[1]);
				return CMD_RET_FAILURE;
			}
		} else {
			printf("current dev: %s\n", currdev->name);
			prevdev = currdev;
		}
	} else {
		if (!currdev) {
			printf("No timer devices set!\n");
			return CMD_RET_FAILURE;
		}
		printf("dev: %s\n", currdev->name);
	}

	if (!strcmp(argv[1], "list")) {
		ret = uclass_get(UCLASS_TIMER, &uc);
		if (ret)
			return CMD_RET_FAILURE;

		uclass_foreach_dev(dev_list, uc)
			printf("%s (%s)\n", dev_list->name, dev_list->driver->name);
	}

	if (!strcmp(argv[1], "current")) {
		if (!currdev)
			printf("no current dev!\n");
		else
			printf("current dev: %s\n", currdev->name);
	}

	if (!strcmp(argv[1], "delay")) {
		start = get_timer(0);
		msecs = get_timer(start);
		printf("start time: %ld.%03d sec\n", msecs / 1000, (int)(msecs % 1000));
		udelay(simple_strtol(argv[2], NULL, 0));
		msecs = get_timer(start);
		printf("after delay time: %ld.%03d sec\n", msecs / 1000, (int)(msecs % 1000));

		return CMD_RET_SUCCESS;
	}

	if (!strcmp(argv[1], "start"))
		start = get_timer(0);

	if (!strcmp(argv[1], "get")) {
		ulong msecs = get_timer(start) * 1000 / CONFIG_SYS_HZ;
		printf("%ld.%03d sec\n", msecs / 1000, (int)(msecs % 1000));
	}

	return 0;
}

U_BOOT_CMD(
	timer,    3,    1,     do_timer,
	"access the system timer\n",
	"timer start - Reset the timer reference.\n"
	"timer get - Print the time since 'start'.\n"
	"timer <timer name> - Select timer.\n"
	"timer list - List all the available timer devices.\n"
	"timer current - print current timer device.\n"
	"timer delay <delay in msecs> - print the start and delay time.\n"
);
