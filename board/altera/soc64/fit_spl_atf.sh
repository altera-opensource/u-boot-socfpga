#!/bin/sh
#
# SPDX-License-Identifier: GPL-2.0+
#
# script to generate FIT image source for Agilex boards with
# U-Boot proper, ATF and device tree for U-Boot proper.
#
# usage: $0 <DT_NAME>

BL31="bl31.bin"
if [ ! -f $BL31 ]; then
	echo "BL31 file \"$BL31\" NOT found!" >&2
	exit 1
fi

BL33="u-boot-nodtb.bin"
if [ ! -f $BL33 ]; then
	echo "BL33 file \"$BL33\" NOT found!" >&2
	exit 1
fi

if [ -f "$1" ] ; then
	DT_NAME="$1"
else
	echo "File not found: \"$1\"" >&2
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
	description = "FIT image with U-Boot proper, ATF bl31, U-Boot DTB";
	#address-cells = <1>;

__PREAMBLE_EOF

cat << __IMAGES_EOF
	images {
		uboot {
			description = "U-Boot SoC64";
			data = /incbin/("$BL33");
			type = "standalone";
			os = "U-Boot";
			arch = "arm64";
			compression = "none";
			load = <0x00200000>;
		};

		atf {
			description = "ARM Trusted Firmware";
			data = /incbin/("$BL31");
			type = "firmware";
			os = "arm-trusted-firmware";
			arch = "arm64";
			compression = "none";
			load = <0x00001000>;
			entry = <0x00001000>;
		};

		fdt {
			description = "U-Boot SoC64 flat device-tree";
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
			description = "Intel SoC64 FPGA";
			firmware = "atf";
			loadables = "uboot";
			fdt = "fdt";
		};
	};
__CONFIGS_EOF

cat << __END_EOF
};
__END_EOF
