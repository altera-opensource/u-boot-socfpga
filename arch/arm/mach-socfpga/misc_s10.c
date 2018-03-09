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
#include <asm/arch/sdram_s10.h>
#include <asm/arch/system_manager.h>
#include <asm/pl310.h>

#include <dt-bindings/reset/altr,rst-mgr-s10.h>

DECLARE_GLOBAL_DATA_PTR;

static struct socfpga_system_manager *sysmgr_regs =
	(struct socfpga_system_manager *)SOCFPGA_SYSMGR_ADDRESS;
#if defined(CONFIG_SOCFPGA_SDRAM_SBE_ECC_CHECKING) || \
defined(CONFIG_SOCFPGA_SDRAM_DBE_ECC_CHECKING) || \
defined(CONFIG_SOCFPGA_SDRAM_DBE_ECC_CHECKING)
static const struct socfpga_ecc_hmc *socfpga_ecc_hmc_base =
		(void *)SOCFPGA_SDR_ADDRESS;
#endif

#ifdef CONFIG_SOCFPGA_SDRAM_SBE_ECC_CHECKING
void sdram_sbe_ecc_checking(void)
{
	u32 ecc_data = 0x6db7d7d3;
	u32 data = 0x11223344;
	u32 location = 0x800000;
	u32 sbe_data = 0x11223345;

	printf("SDRAM single bit error test starting . . .\n");
	printf("Enable SBE interrupt . . .\n");
	setbits_le32(&socfpga_ecc_hmc_base->errinten,
		     DDR_HMC_ERRINTEN_SERRINTEN_EN_SET_MSK);
	setbits_le32(&socfpga_ecc_hmc_base->intmode,
		     (DDR_HMC_INTMODE_INTMODE_SET_MSK));
	printf("Writing data %x into SDRAM address %lx\n", data,
	       (uintptr_t)location);
	writel(data, (uintptr_t)location);
	printf("%lx: %x\n", (uintptr_t)location, readl((uintptr_t)location));
	printf("Writing ecc data %x into ecc_reg2wreccdatabus register\n",
	       ecc_data);
	writel(ecc_data, &socfpga_ecc_hmc_base->ecc_reg2wreccdatabus);
	printf("Enable eccdiagon AND wrdiagon at ecc_diagon  . . .\n");
	setbits_le32(&socfpga_ecc_hmc_base->ecc_diagon,
		     (DDR_HMC_ECC_DIAGON_ECCDIAGON_EN_SET_MSK |
		      DDR_HMC_ECC_DIAGON_WRDIAGON_EN_SET_MSK));
	printf("Injecting SBE data %x into SDRAM address %lx\n", sbe_data,
	       (uintptr_t)location);
	writel(sbe_data, (uintptr_t)location);
	printf("Reading data at SDRAM address %lx: %x\n", (uintptr_t)location,
	       readl((uintptr_t)location));

	if (readl(&socfpga_ecc_hmc_base->ecc_decstat) & 0x1) {
		printf("Decoder for data [63:0] has detected SBE\n");

		printf("Corrected SDRAM ECC @ 0x%08x\n",
		       readl(&socfpga_ecc_hmc_base->autowb_corraddr));

		if (readl(&socfpga_ecc_hmc_base->intstat) & 0x1) {
			printf("SBE interrupt is pending . . .\n");
			printf("Clearing the pending SBE\n");
			setbits_le32(&socfpga_ecc_hmc_base->intstat,
				     DDR_HMC_INTSTAT_SERRPENA_SET_MSK);
		}
	}

	printf("Disable eccdiagon AND wrdiagon at ecc_diagon  . . .\n");
	clrbits_le32(&socfpga_ecc_hmc_base->ecc_diagon,
		     (DDR_HMC_ECC_DIAGON_ECCDIAGON_EN_SET_MSK |
		      DDR_HMC_ECC_DIAGON_WRDIAGON_EN_SET_MSK));
}
#endif

#ifdef CONFIG_SOCFPGA_SDRAM_DBE_ECC_CHECKING
void sdram_dbe_ecc_checking(void)
{
	u32 ecc_data = 0x6db7d7d3;
	u32 data = 0x11223344;
	u32 location = 0x800000;
	u32 dbe_data = 0x11003344;

	printf("SDRAM double bit error test starting . . .\n");
	printf("Writing data %x into SDRAM address %lx\n", data,
	       (uintptr_t)location);
	writel(data, (uintptr_t)location);
	printf("%lx: %x\n", (uintptr_t)location, readl((uintptr_t)location));
	printf("Writing ecc data %x into ecc_reg2wreccdatabus register\n",
	       ecc_data);
	writel(ecc_data, &socfpga_ecc_hmc_base->ecc_reg2wreccdatabus);
	printf("Enable eccdiagon AND wrdiagon at ecc_diagon  . . .\n");
	setbits_le32(&socfpga_ecc_hmc_base->ecc_diagon,
		     (DDR_HMC_ECC_DIAGON_ECCDIAGON_EN_SET_MSK |
		      DDR_HMC_ECC_DIAGON_WRDIAGON_EN_SET_MSK));
	printf("Injecting DBE data %x into SDRAM address %lx\n", dbe_data,
	       (uintptr_t)location);
	writel(dbe_data, (uintptr_t)location);
}
#endif

#ifdef CONFIG_SOCFPGA_SDRAM_ADDR_MISMATCH_ECC_CHECKING
void sdram_addr_mismatch_ecc_checking(void)
{
	u32 ecc_data = 0x88523236;
	u32 data = 0x11223344;
	u32 location = 0x800000;

	printf("ECC address mismatch test starting . . .\n");
	printf("Writing data %x into SDRAM address %lx\n", data,
	       (uintptr_t)location);
	writel(data, (uintptr_t)location);
	printf("%lx: %x\n", (uintptr_t)location, readl((uintptr_t)location));
	printf("Writing ecc data %x into ecc_reg2wreccdatabus register\n",
	       ecc_data);
	writel(ecc_data, &socfpga_ecc_hmc_base->ecc_reg2wreccdatabus);
	printf("Enable eccdiagon AND wrdiagon at ecc_diagon  . . .\n");
	setbits_le32(&socfpga_ecc_hmc_base->ecc_diagon,
		     (DDR_HMC_ECC_DIAGON_ECCDIAGON_EN_SET_MSK |
		      DDR_HMC_ECC_DIAGON_WRDIAGON_EN_SET_MSK));
	printf("First write %x into SDRAM address %lx after ecc_diagon is on\n",
	       data, (uintptr_t)location);
	writel(data, (uintptr_t)location);
	printf("Reading data at SDRAM address %lx: %x\n", (uintptr_t)location,
	       readl((uintptr_t)location));
}
#endif

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
	char qspi_string[13];

	sprintf(qspi_string, "<0x%08x>", cm_get_qspi_controller_clk_hz());
	env_set("qspi_clock", qspi_string);

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
