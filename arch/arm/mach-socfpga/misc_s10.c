/*
 * Copyright (C) 2016-2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:    GPL-2.0
 */

#include <common.h>
#include <asm/io.h>
#include <errno.h>
#include <fdtdec.h>
#include <libfdt.h>
#include <altera.h>
#include <miiphy.h>
#include <netdev.h>
#include <watchdog.h>
#include <asm/arch/reset_manager.h>
#include <asm/arch/system_manager.h>
#include <asm/pl310.h>

#include <dt-bindings/reset/altr,rst-mgr-s10.h>

DECLARE_GLOBAL_DATA_PTR;

static struct socfpga_system_manager *sysmgr_regs =
	(struct socfpga_system_manager *)SOCFPGA_SYSMGR_ADDRESS;

/*
 * DesignWare Ethernet initialization
 */
#ifdef CONFIG_ETH_DESIGNWARE
void dwmac_deassert_reset(const unsigned int of_reset_id,
				 const u32 phymode)
{
	/* Put the emac we're using into reset.
	 * This is required before configuring the PHY interface
	 */
	socfpga_emac_manage_reset(of_reset_id, 1);

	clrsetbits_le32(&sysmgr_regs->emac0 + (of_reset_id - EMAC0_RESET),
			SYSMGR_EMACGRP_CTRL_PHYSEL_MASK,
			phymode);

	socfpga_emac_manage_reset(of_reset_id, 0);
}

static u32 dwmac_phymode_to_modereg(const char *phymode, u32 *modereg)
{
	if (!phymode)
		return -EINVAL;

	if (!strcmp(phymode, "mii") || !strcmp(phymode, "gmii")) {
		*modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_GMII_MII;
		return 0;
	}

	if (!strcmp(phymode, "rgmii")) {
		*modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RGMII;
		return 0;
	}

	if (!strcmp(phymode, "rmii")) {
		*modereg = SYSMGR_EMACGRP_CTRL_PHYSEL_ENUM_RMII;
		return 0;
	}

	return -EINVAL;
}

static int socfpga_eth_reset(void)
{
	const void *fdt = gd->fdt_blob;
	struct fdtdec_phandle_args args;
	const char *phy_mode;
	u32 phy_modereg;
	int nodes[2];	/* Max. 3 GMACs */
	int ret, count;
	int i, node;

	count = fdtdec_find_aliases_for_id(fdt, "ethernet",
					   COMPAT_ALTERA_SOCFPGA_DWMAC,
					   nodes, ARRAY_SIZE(nodes));
	for (i = 0; i < count; i++) {
		node = nodes[i];
		if (node <= 0)
			continue;

		ret = fdtdec_parse_phandle_with_args(fdt, node, "resets",
						     "#reset-cells", 1, 0,
						     &args);
		if (ret || (args.args_count != 1)) {
			debug("GMAC%i: Failed to parse DT 'resets'!\n", i);
			continue;
		}

		phy_mode = fdt_getprop(fdt, node, "phy-mode", NULL);
		ret = dwmac_phymode_to_modereg(phy_mode, &phy_modereg);
		if (ret) {
			debug("GMAC%i: Failed to parse DT 'phy-mode'!\n", i);
			continue;
		}

		dwmac_deassert_reset(args.args[0], phy_modereg);
	}

	return 0;
}
#else
static int socfpga_eth_reset(void)
{
	return 0;
};
#endif

/*
 * Print CPU information
 */
#if defined(CONFIG_DISPLAY_CPUINFO)
int print_cpuinfo(void)
{
	puts("CPU:   Intel FPGA SoCFPGA Platform\n");
	puts("FPGA:  Intel FPGA Stratix 10\n");
	return 0;
}
#endif

#ifdef CONFIG_ARCH_MISC_INIT
int arch_misc_init(void)
{
	return socfpga_eth_reset();
}
#endif

#ifdef CONFIG_FPGA
/*
 * FPGA programming support for SoC FPGA Stratix 10
 */
static Altera_desc altera_fpga[] = {
	{
		/* Family */
		Intel_FPGA_Stratix10,
		/* Interface type */
		secure_device_manager_mailbox,
		/* No limitation as additional data will be ignored */
		-1,
		/* No device function table */
		NULL,
		/* Base interface address specified in driver */
		NULL,
		/* No cookie implementation */
		0
	},
};

/* add device descriptor to FPGA device table */
static void socfpga_fpga_add(void)
{
	int i;
	fpga_init();
	for (i = 0; i < ARRAY_SIZE(altera_fpga); i++)
		fpga_add(fpga_altera, &altera_fpga[i]);
}
#endif

int arch_early_init_r(void)
{
#ifdef CONFIG_FPGA
	socfpga_fpga_add();
#endif

	return 0;
}

int do_bridge(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	if (argc != 2)
		return CMD_RET_USAGE;

	argv++;

	switch (*argv[0]) {
	case 'e':	/* Enable */
		socfpga_bridges_reset(1);
		break;
	case 'd':	/* Disable */
		socfpga_bridges_reset(0);
		break;
	default:
		return CMD_RET_USAGE;
	}

	return 0;
}

U_BOOT_CMD(
	bridge, 2, 1, do_bridge,
	"SoCFPGA HPS FPGA bridge control",
	"enable  - Enable HPS-to-FPGA, FPGA-to-HPS, LWHPS-to-FPGA bridges\n"
	"bridge disable - Enable HPS-to-FPGA, FPGA-to-HPS, LWHPS-to-FPGA bridges\n"
	""
);
