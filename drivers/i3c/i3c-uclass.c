/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Intel Coporation.
 */

#include <common.h>
#include <dm.h>
#include <dw-i3c.h>
#include <errno.h>
#include <i2c.h>
#include <log.h>
#include <dm/device-internal.h>
#include <linux/ctype.h>

int dm_i3c_read(struct udevice *dev, u8 dev_number,
                u8 *buf, int num_bytes)
{
	struct dm_i3c_ops *ops = i3c_get_ops(dev);

	if (!ops->read)
		return -ENOSYS;

	return ops->read(dev, dev_number, buf, num_bytes);
}

int dm_i3c_write(struct udevice *dev, u8 dev_number,
	         u8 *buf, int num_bytes)
{
	struct dm_i3c_ops *ops = i3c_get_ops(dev);

	if (!ops->write)
		return -ENOSYS;

	return ops->write(dev, dev_number, buf, num_bytes);
}

UCLASS_DRIVER(i3c) = {
	.id		= UCLASS_I3C,
	.name		= "i3c",
};
