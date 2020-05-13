#!/bin/sh
#
# SPDX-License-Identifier: GPL-2.0+
#
# script to generate FIT image source for Agilex boards with
# U-Boot proper, ATF and device tree for U-Boot proper.
#
# usage: $0 <DT_NAME>

BL31="signed_bl31.bin"
if [ ! -f $BL31 ]; then
	echo "BL31 file \"$BL31\" NOT found!" >&2
	exit 1
fi

BL33="signed_u-boot-nodtb.bin"
if [ ! -f $BL33 ]; then
	echo "BL33 file \"$BL33\" NOT found!" >&2
	exit 1
fi

DT_NAME="signed_u-boot.dtb"
if [ ! -f $DT_NAME ] ; then
	echo "Device Tree Blob \"$DT_NAME\" not found!" >&2
	exit 1
fi

cat << __PREAMBLE_EOF
/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

/dts-v1/;

/ {
	description = "FIT with VAB signed firmware and bootloader";
	#address-cells = <1>;

__PREAMBLE_EOF

cat << __IMAGES_EOF
	images {
		uboot {
			description = "Signed U-Boot SoC64";
			data = /incbin/("$BL33");
			type = "standalone";
			os = "U-Boot";
			arch = "arm64";
			compression = "none";
			load = <0x00200000>;
		};

		atf {
			description = "Signed ARM Trusted Firmware";
			data = /incbin/("$BL31");
			type = "firmware";
			os = "arm-trusted-firmware";
			arch = "arm64";
			compression = "none";
			load = <0x00001000>;
			entry = <0x00001000>;
		};

		fdt {
			description = "Signed U-Boot SoC64 flat device-tree";
			data = /incbin/("$DT_NAME");
			type = "flat_dt";
			compression = "none";
		};
	};

__IMAGES_EOF

cat << __CONFIGS_EOF
	configurations {
		default = "conf";
		conf {
			description = "VAB signed firmware and bootloader";
			firmware = "atf";
			loadables = "uboot";
			fdt = "fdt";
		};
	};
__CONFIGS_EOF

cat << __END_EOF
};
__END_EOF
