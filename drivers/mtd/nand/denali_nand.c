/*
 * NAND Flash Controller Device Driver
 * Copyright (C) 2013 Altera Corporation <www.altera.com>
 * Copyright © 2009-2010, Intel Corporation and its suppliers.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <common.h>
#include <nand.h>
#include <asm/errno.h>
#include <asm/io.h>

#include "denali_nand.h"

/* We define a module parameter that allows the user to override
 * the hardware and decide what timing mode should be used.
 */
#define NAND_DEFAULT_TIMINGS	-1

static struct denali_nand_info denali;
static int onfi_timing_mode = NAND_DEFAULT_TIMINGS;

/* We define a macro here that combines all interrupts this driver uses into
 * a single constant value, for convenience. */
#define DENALI_IRQ_ALL	(INTR_STATUS__DMA_CMD_COMP | \
			INTR_STATUS__ECC_TRANSACTION_DONE | \
			INTR_STATUS__ECC_ERR | \
			INTR_STATUS__PROGRAM_FAIL | \
			INTR_STATUS__LOAD_COMP | \
			INTR_STATUS__PROGRAM_COMP | \
			INTR_STATUS__TIME_OUT | \
			INTR_STATUS__ERASE_FAIL | \
			INTR_STATUS__RST_COMP | \
			INTR_STATUS__ERASE_COMP | \
			INTR_STATUS__ECC_UNCOR_ERR | \
			INTR_STATUS__INT_ACT | \
			INTR_STATUS__LOCKED_BLK)

/* indicates whether or not the internal value for the flash bank is
 * valid or not */
#define CHIP_SELECT_INVALID	-1

#define SUPPORT_8BITECC		1

/* This macro divides two integers and rounds fractional values up
 * to the nearest integer value. */
#define CEIL_DIV(X, Y) (((X)%(Y)) ? ((X)/(Y)+1) : ((X)/(Y)))

/* These constants are defined by the driver to enable common driver
 * configuration options. */
#define SPARE_ACCESS		0x41
#define MAIN_ACCESS		0x42
#define MAIN_SPARE_ACCESS	0x43

#define DENALI_UNLOCK_START	0x10
#define DENALI_UNLOCK_END	0x11
#define DENALI_LOCK		0x21
#define DENALI_LOCK_TIGHT	0x31
#define DENALI_BUFFER_LOAD	0x60
#define DENALI_BUFFER_WRITE	0x62

#define DENALI_READ	0
#define DENALI_WRITE	0x100

/* types of device accesses. We can issue commands and get status */
#define COMMAND_CYCLE	0
#define ADDR_CYCLE	1
#define STATUS_CYCLE	2

/* this is a helper macro that allows us to
 * format the bank into the proper bits for the controller */
#define BANK(x) ((x) << 24)

/* Interrupts are cleared by writing a 1 to the appropriate status bit */
static inline void clear_interrupt(uint32_t irq_mask)
{
	uint32_t intr_status_reg = 0;
	intr_status_reg = INTR_STATUS(denali.flash_bank);
	__raw_writel(irq_mask, denali.flash_reg + intr_status_reg);
}

static uint32_t read_interrupt_status(void)
{
	uint32_t intr_status_reg = 0;
	intr_status_reg = INTR_STATUS(denali.flash_bank);
	return __raw_readl(denali.flash_reg + intr_status_reg);
}

static void clear_interrupts(void)
{
	uint32_t status = 0x0;
	status = read_interrupt_status();
	clear_interrupt(status);
	denali.irq_status = 0x0;
}

static void denali_irq_enable(uint32_t int_mask)
{
	int i;
	for (i = 0; i < denali.max_banks; ++i)
		__raw_writel(int_mask, denali.flash_reg + INTR_EN(i));
}

static uint32_t wait_for_irq(uint32_t irq_mask)
{
	unsigned long comp_res = 1000;
	uint32_t intr_status = 0;

	do {
		intr_status = read_interrupt_status() & DENALI_IRQ_ALL;
		if (intr_status & irq_mask) {
			denali.irq_status &= ~irq_mask;
			/* our interrupt was detected */
			break;
		}
		udelay(1);
		comp_res--;
	} while (comp_res != 0);

	if (comp_res == 0) {
		/* timeout */
		printf("Denali timeout with interrupt status %08x\n",
			read_interrupt_status());
		intr_status = 0;
	}
	return intr_status;
}

/* Certain operations for the denali NAND controller use
 * an indexed mode to read/write data. The operation is
 * performed by writing the address value of the command
 * to the device memory followed by the data. This function
 * abstracts this common operation.
*/
static void index_addr(uint32_t address, uint32_t data)
{
	__raw_writel(address, denali.flash_mem);
	__raw_writel(data, denali.flash_mem + 0x10);
}

/* Perform an indexed read of the device */
static void index_addr_read_data(uint32_t address, uint32_t *pdata)
{
	__raw_writel(address, denali.flash_mem);
	*pdata = __raw_readl(denali.flash_mem + 0x10);
}

/* We need to buffer some data for some of the NAND core routines.
 * The operations manage buffering that data. */
static void reset_buf(void)
{
	denali.buf.head = denali.buf.tail = 0;
}

static void write_byte_to_buf(uint8_t byte)
{
	BUG_ON(denali.buf.tail >= sizeof(denali.buf.buf));
	denali.buf.buf[denali.buf.tail++] = byte;
}

/* resets a specific device connected to the core */
static void reset_bank(void)
{
	uint32_t irq_status = 0;
	uint32_t irq_mask = INTR_STATUS__RST_COMP |
			    INTR_STATUS__TIME_OUT;

	clear_interrupts();

	__raw_writel(1 << denali.flash_bank, denali.flash_reg + DEVICE_RESET);

	irq_status = wait_for_irq(irq_mask);
	if (irq_status & INTR_STATUS__TIME_OUT)
		debug(KERN_ERR "reset bank failed.\n");
}

/* Reset the flash controller */
static uint16_t denali_nand_reset(void)
{
	uint32_t i;

	for (i = 0 ; i < denali.max_banks; i++)
		__raw_writel(INTR_STATUS__RST_COMP | INTR_STATUS__TIME_OUT,
		denali.flash_reg + INTR_STATUS(i));

	for (i = 0 ; i < denali.max_banks; i++) {
		__raw_writel(1 << i, denali.flash_reg + DEVICE_RESET);
		while (!(__raw_readl(denali.flash_reg +	INTR_STATUS(i)) &
			(INTR_STATUS__RST_COMP | INTR_STATUS__TIME_OUT)))
			if (__raw_readl(denali.flash_reg + INTR_STATUS(i)) &
				INTR_STATUS__TIME_OUT)
				debug(KERN_DEBUG "NAND Reset operation "
					"timed out on bank %d\n", i);
	}

	for (i = 0; i < denali.max_banks; i++)
		__raw_writel(INTR_STATUS__RST_COMP | INTR_STATUS__TIME_OUT,
			denali.flash_reg + INTR_STATUS(i));

	return PASS;
}

/* this routine calculates the ONFI timing values for a given mode and
 * programs the clocking register accordingly. The mode is determined by
 * the get_onfi_nand_para routine.
 */
static void nand_onfi_timing_set(uint16_t mode)
{
	uint16_t Trea[6] = {40, 30, 25, 20, 20, 16};
	uint16_t Trp[6] = {50, 25, 17, 15, 12, 10};
	uint16_t Treh[6] = {30, 15, 15, 10, 10, 7};
	uint16_t Trc[6] = {100, 50, 35, 30, 25, 20};
	uint16_t Trhoh[6] = {0, 15, 15, 15, 15, 15};
	uint16_t Trloh[6] = {0, 0, 0, 0, 5, 5};
	uint16_t Tcea[6] = {100, 45, 30, 25, 25, 25};
	uint16_t Tadl[6] = {200, 100, 100, 100, 70, 70};
	uint16_t Trhw[6] = {200, 100, 100, 100, 100, 100};
	uint16_t Trhz[6] = {200, 100, 100, 100, 100, 100};
	uint16_t Twhr[6] = {120, 80, 80, 60, 60, 60};
	uint16_t Tcs[6] = {70, 35, 25, 25, 20, 15};

	uint16_t TclsRising = 1;
	uint16_t data_invalid_rhoh, data_invalid_rloh, data_invalid;
	uint16_t dv_window = 0;
	uint16_t en_lo, en_hi;
	uint16_t acc_clks;
	uint16_t addr_2_data, re_2_we, re_2_re, we_2_re, cs_cnt;

	en_lo = CEIL_DIV(Trp[mode], CLK_X);
	en_hi = CEIL_DIV(Treh[mode], CLK_X);
#if ONFI_BLOOM_TIME
	if ((en_hi * CLK_X) < (Treh[mode] + 2))
		en_hi++;
#endif

	if ((en_lo + en_hi) * CLK_X < Trc[mode])
		en_lo += CEIL_DIV((Trc[mode] - (en_lo + en_hi) * CLK_X), CLK_X);

	if ((en_lo + en_hi) < CLK_MULTI)
		en_lo += CLK_MULTI - en_lo - en_hi;

	while (dv_window < 8) {
		data_invalid_rhoh = en_lo * CLK_X + Trhoh[mode];

		data_invalid_rloh = (en_lo + en_hi) * CLK_X + Trloh[mode];

		data_invalid =
		    data_invalid_rhoh <
		    data_invalid_rloh ? data_invalid_rhoh : data_invalid_rloh;

		dv_window = data_invalid - Trea[mode];

		if (dv_window < 8)
			en_lo++;
	}

	acc_clks = CEIL_DIV(Trea[mode], CLK_X);

	while (((acc_clks * CLK_X) - Trea[mode]) < 3)
		acc_clks++;

	if ((data_invalid - acc_clks * CLK_X) < 2)
		debug(KERN_WARNING "%s, Line %d: Warning!\n",
			__FILE__, __LINE__);

	addr_2_data = CEIL_DIV(Tadl[mode], CLK_X);
	re_2_we = CEIL_DIV(Trhw[mode], CLK_X);
	re_2_re = CEIL_DIV(Trhz[mode], CLK_X);
	we_2_re = CEIL_DIV(Twhr[mode], CLK_X);
	cs_cnt = CEIL_DIV((Tcs[mode] - Trp[mode]), CLK_X);
	if (!TclsRising)
		cs_cnt = CEIL_DIV(Tcs[mode], CLK_X);
	if (cs_cnt == 0)
		cs_cnt = 1;

	if (Tcea[mode]) {
		while (((cs_cnt * CLK_X) + Trea[mode]) < Tcea[mode])
			cs_cnt++;
	}

#if MODE5_WORKAROUND
	if (mode == 5)
		acc_clks = 5;
#endif

	/* Sighting 3462430: Temporary hack for MT29F128G08CJABAWP:B */
	if ((__raw_readl(denali.flash_reg + MANUFACTURER_ID) == 0) &&
		(__raw_readl(denali.flash_reg + DEVICE_ID) == 0x88))
		acc_clks = 6;

	__raw_writel(acc_clks, denali.flash_reg + ACC_CLKS);
	__raw_writel(re_2_we, denali.flash_reg + RE_2_WE);
	__raw_writel(re_2_re, denali.flash_reg + RE_2_RE);
	__raw_writel(we_2_re, denali.flash_reg + WE_2_RE);
	__raw_writel(addr_2_data, denali.flash_reg + ADDR_2_DATA);
	__raw_writel(en_lo, denali.flash_reg + RDWR_EN_LO_CNT);
	__raw_writel(en_hi, denali.flash_reg + RDWR_EN_HI_CNT);
	__raw_writel(cs_cnt, denali.flash_reg + CS_SETUP_CNT);
}

/* queries the NAND device to see what ONFI modes it supports. */
static uint16_t get_onfi_nand_para(void)
{
	int i;
	/* we needn't to do a reset here because driver has already
	 * reset all the banks before
	 * */
	if (!(__raw_readl(denali.flash_reg + ONFI_TIMING_MODE) &
		ONFI_TIMING_MODE__VALUE))
		return FAIL;

	for (i = 5; i > 0; i--) {
		if (__raw_readl(denali.flash_reg + ONFI_TIMING_MODE) &
			(0x01 << i))
			break;
	}

	nand_onfi_timing_set(i);

	/* By now, all the ONFI devices we know support the page cache */
	/* rw feature. So here we enable the pipeline_rw_ahead feature */
	/* __raw_writel(1, denali.flash_reg + CACHE_WRITE_ENABLE); */
	/* __raw_writel(1, denali.flash_reg + CACHE_READ_ENABLE);  */

	return PASS;
}

static void get_samsung_nand_para(uint8_t device_id)
{
	if (device_id == 0xd3) { /* Samsung K9WAG08U1A */
		/* Set timing register values according to datasheet */
		__raw_writel(5, denali.flash_reg + ACC_CLKS);
		__raw_writel(20, denali.flash_reg + RE_2_WE);
		__raw_writel(12, denali.flash_reg + WE_2_RE);
		__raw_writel(14, denali.flash_reg + ADDR_2_DATA);
		__raw_writel(3, denali.flash_reg + RDWR_EN_LO_CNT);
		__raw_writel(2, denali.flash_reg + RDWR_EN_HI_CNT);
		__raw_writel(2, denali.flash_reg + CS_SETUP_CNT);
	}
}

static void get_toshiba_nand_para(void)
{
	uint32_t tmp;

	/* Workaround to fix a controller bug which reports a wrong */
	/* spare area size for some kind of Toshiba NAND device */
	if ((__raw_readl(denali.flash_reg + DEVICE_MAIN_AREA_SIZE) == 4096) &&
		(__raw_readl(denali.flash_reg + DEVICE_SPARE_AREA_SIZE)
		== 64)){
		__raw_writel(216, denali.flash_reg + DEVICE_SPARE_AREA_SIZE);
		tmp = __raw_readl(denali.flash_reg + DEVICES_CONNECTED) *
			__raw_readl(denali.flash_reg + DEVICE_SPARE_AREA_SIZE);
		__raw_writel(tmp,
				denali.flash_reg + LOGICAL_PAGE_SPARE_SIZE);
#if SUPPORT_15BITECC
		__raw_writel(15, denali.flash_reg + ECC_CORRECTION);
#elif SUPPORT_8BITECC
		__raw_writel(8, denali.flash_reg + ECC_CORRECTION);
#endif
	}
}

static void get_hynix_nand_para(uint8_t device_id)
{
	uint32_t main_size, spare_size;

	switch (device_id) {
	case 0xD5: /* Hynix H27UAG8T2A, H27UBG8U5A or H27UCG8VFA */
	case 0xD7: /* Hynix H27UDG8VEM, H27UCG8UDM or H27UCG8V5A */
		__raw_writel(128, denali.flash_reg + PAGES_PER_BLOCK);
		__raw_writel(4096, denali.flash_reg + DEVICE_MAIN_AREA_SIZE);
		__raw_writel(224, denali.flash_reg + DEVICE_SPARE_AREA_SIZE);
		main_size = 4096 *
			__raw_readl(denali.flash_reg + DEVICES_CONNECTED);
		spare_size = 224 *
			__raw_readl(denali.flash_reg + DEVICES_CONNECTED);
		__raw_writel(main_size,
				denali.flash_reg + LOGICAL_PAGE_DATA_SIZE);
		__raw_writel(spare_size,
				denali.flash_reg + LOGICAL_PAGE_SPARE_SIZE);
		__raw_writel(0, denali.flash_reg + DEVICE_WIDTH);
#if SUPPORT_15BITECC
		__raw_writel(15, denali.flash_reg + ECC_CORRECTION);
#elif SUPPORT_8BITECC
		__raw_writel(8, denali.flash_reg + ECC_CORRECTION);
#endif
		break;
	default:
		debug(KERN_WARNING
			"Spectra: Unknown Hynix NAND (Device ID: 0x%x)."
			"Will use default parameter values instead.\n",
			device_id);
	}
}

/* determines how many NAND chips are connected to the controller. Note for
 * Intel CE4100 devices we don't support more than one device.
 */
static void find_valid_banks(void)
{
	uint32_t id[denali.max_banks];
	int i;

	denali.total_used_banks = 1;
	for (i = 0; i < denali.max_banks; i++) {
		index_addr((uint32_t)(MODE_11 | (i << 24) | 0), 0x90);
		index_addr((uint32_t)(MODE_11 | (i << 24) | 1), 0);
		index_addr_read_data((uint32_t)(MODE_11 | (i << 24) | 2),
			&id[i]);

		if (i == 0) {
			if (!(id[i] & 0x0ff))
				break; /* WTF? */
		} else {
			if ((id[i] & 0x0ff) == (id[0] & 0x0ff))
				denali.total_used_banks++;
			else
				break;
		}
	}
}

/*
 * Use the configuration feature register to determine the maximum number of
 * banks that the hardware supports.
 */
static void detect_max_banks(void)
{
	uint32_t features = __raw_readl(denali.flash_reg + FEATURES);
	denali.max_banks = 2 << (features & FEATURES__N_BANKS);
}

static void detect_partition_feature(void)
{
	/* For MRST platform, denali.fwblks represent the
	 * number of blocks firmware is taken,
	 * FW is in protect partition and MTD driver has no
	 * permission to access it. So let driver know how many
	 * blocks it can't touch.
	 * */
	if (__raw_readl(denali.flash_reg + FEATURES) & FEATURES__PARTITION) {
		if ((__raw_readl(denali.flash_reg + PERM_SRC_ID(1)) &
			PERM_SRC_ID__SRCID) == SPECTRA_PARTITION_ID) {
			denali.fwblks =
			    ((__raw_readl(denali.flash_reg + MIN_MAX_BANK(1)) &
			      MIN_MAX_BANK__MIN_VALUE) *
			     denali.blksperchip)
			    +
			    (__raw_readl(denali.flash_reg + MIN_BLK_ADDR(1)) &
			    MIN_BLK_ADDR__VALUE);
		} else
			denali.fwblks = SPECTRA_START_BLOCK;
	} else
		denali.fwblks = SPECTRA_START_BLOCK;
}

static uint16_t denali_nand_timing_set(void)
{
	uint16_t status = PASS;
	uint32_t id_bytes[5], addr;
	uint8_t i, maf_id, device_id;

	/* Use read id method to get device ID and other
	 * params. For some NAND chips, controller can't
	 * report the correct device ID by reading from
	 * DEVICE_ID register
	 * */
	addr = (uint32_t)MODE_11 | BANK(denali.flash_bank);
	index_addr((uint32_t)addr | 0, 0x90);
	index_addr((uint32_t)addr | 1, 0);
	for (i = 0; i < 5; i++)
		index_addr_read_data(addr | 2, &id_bytes[i]);
	maf_id = id_bytes[0];
	device_id = id_bytes[1];

	if (__raw_readl(denali.flash_reg + ONFI_DEVICE_NO_OF_LUNS) &
		ONFI_DEVICE_NO_OF_LUNS__ONFI_DEVICE) { /* ONFI 1.0 NAND */
		if (FAIL == get_onfi_nand_para())
			return FAIL;
	} else if (maf_id == 0xEC) { /* Samsung NAND */
		get_samsung_nand_para(device_id);
	} else if (maf_id == 0x98) { /* Toshiba NAND */
		get_toshiba_nand_para();
	} else if (maf_id == 0xAD) { /* Hynix NAND */
		get_hynix_nand_para(device_id);
	}

	find_valid_banks();

	detect_partition_feature();

	/* If the user specified to override the default timings
	 * with a specific ONFI mode, we apply those changes here.
	 */
	if (onfi_timing_mode != NAND_DEFAULT_TIMINGS)
		nand_onfi_timing_set(onfi_timing_mode);

	return status;
}

static void denali_set_intr_modes(uint16_t INT_ENABLE)
{
	if (INT_ENABLE)
		__raw_writel(1, denali.flash_reg + GLOBAL_INT_ENABLE);
	else
		__raw_writel(0, denali.flash_reg + GLOBAL_INT_ENABLE);
}

/* validation function to verify that the controlling software is making
 * a valid request
 */
static inline bool is_flash_bank_valid(int flash_bank)
{
	return (flash_bank >= 0 && flash_bank < 4);
}

static void denali_irq_init(void)
{
	uint32_t int_mask = 0;
	int i;

	/* Disable global interrupts */
	denali_set_intr_modes(false);

	int_mask = DENALI_IRQ_ALL;

	/* Clear all status bits */
	for (i = 0; i < denali.max_banks; ++i)
		__raw_writel(0xFFFF, denali.flash_reg + INTR_STATUS(i));

	denali_irq_enable(int_mask);
}

/* This helper function setups the registers for ECC and whether or not
 * the spare area will be transferred. */
static void setup_ecc_for_xfer(bool ecc_en, bool transfer_spare)
{
	int ecc_en_flag = 0, transfer_spare_flag = 0;

	/* set ECC, transfer spare bits if needed */
	ecc_en_flag = ecc_en ? ECC_ENABLE__FLAG : 0;
	transfer_spare_flag = transfer_spare ? TRANSFER_SPARE_REG__FLAG : 0;

	/* Enable spare area/ECC per user's request. */
	__raw_writel(ecc_en_flag, denali.flash_reg + ECC_ENABLE);
	/* applicable for MAP01 only */
	__raw_writel(transfer_spare_flag,
			denali.flash_reg + TRANSFER_SPARE_REG);
}

/* sends a pipeline command operation to the controller. See the Denali NAND
 * controller's user guide for more information (section 4.2.3.6).
 */
static int denali_send_pipeline_cmd(bool ecc_en, bool transfer_spare,
					int access_type, int op)
{
	uint32_t addr = 0x0, cmd = 0x0, irq_status = 0,	 irq_mask = 0;
	uint32_t page_count = 1;	/* always read a page */

	if (op == DENALI_READ)
		irq_mask = INTR_STATUS__LOAD_COMP;
	else if (op == DENALI_WRITE)
		irq_mask = INTR_STATUS__PROGRAM_COMP |
			INTR_STATUS__PROGRAM_FAIL;
	else
		BUG();

	/* clear interrupts */
	clear_interrupts();

	/* setup ECC and transfer spare reg */
	setup_ecc_for_xfer(ecc_en, transfer_spare);

	addr = BANK(denali.flash_bank) | denali.page;

	/* setup the acccess type */
	cmd = MODE_10 | addr;
	index_addr((uint32_t)cmd, access_type);

	/* setup the pipeline command */
	if (access_type == SPARE_ACCESS && op == DENALI_WRITE)
		index_addr((uint32_t)cmd, DENALI_BUFFER_WRITE);
	else if (access_type == SPARE_ACCESS && op == DENALI_READ)
		index_addr((uint32_t)cmd, DENALI_BUFFER_LOAD);
	else
		index_addr((uint32_t)cmd, 0x2000 | op | page_count);

	/* wait for command to be accepted */
	irq_status = wait_for_irq(irq_mask);
	if ((irq_status & irq_mask) != irq_mask)
		return FAIL;

	if (access_type != SPARE_ACCESS) {
		cmd = MODE_01 | addr;
		__raw_writel(cmd, denali.flash_mem);
	}
	return PASS;
}

/* helper function that simply writes a buffer to the flash */
static int write_data_to_flash_mem(const uint8_t *buf,
							int len)
{
	uint32_t i = 0, *buf32;

	/* verify that the len is a multiple of 4. see comment in
	 * read_data_from_flash_mem() */
	BUG_ON((len % 4) != 0);

	/* write the data to the flash memory */
	buf32 = (uint32_t *)buf;
	for (i = 0; i < len / 4; i++)
		__raw_writel(*buf32++, denali.flash_mem + 0x10);
	return i*4; /* intent is to return the number of bytes read */
}

static void denali_mode_main_access(void)
{
	uint32_t addr, cmd;
	addr = BANK(denali.flash_bank) | denali.page;
	cmd = MODE_10 | addr;
	index_addr((uint32_t)cmd, MAIN_ACCESS);
}

static void denali_mode_main_spare_access(void)
{
	uint32_t addr, cmd;
	addr = BANK(denali.flash_bank) | denali.page;
	cmd = MODE_10 | addr;
	index_addr((uint32_t)cmd, MAIN_SPARE_ACCESS);
}

/* Writes OOB data to the device.
 * This code unused under normal U-Boot console as normally page write raw
 * to be used for write oob data with main data.
 */
static int write_oob_data(struct mtd_info *mtd, uint8_t *buf, int page)
{
	uint32_t cmd;

	denali.page = page;
	debug("* write_oob_data *\n");

	/* We need to write to buffer first through MAP00 command */
	cmd = MODE_00 | BANK(denali.flash_bank);
	__raw_writel(cmd, denali.flash_mem);

	/* send the data into flash buffer */
	write_data_to_flash_mem(buf, mtd->oobsize);

	/* activate the write through MAP10 commands */
	if (denali_send_pipeline_cmd(false, false,
		SPARE_ACCESS, DENALI_WRITE) != PASS)
		return -EIO;

	return 0;
}

/* this function examines buffers to see if they contain data that
 * indicate that the buffer is part of an erased region of flash.
 */
bool is_erased(uint8_t *buf, int len)
{
	int i = 0;
	for (i = 0; i < len; i++)
		if (buf[i] != 0xFF)
			return false;
	return true;
}


/* programs the controller to either enable/disable DMA transfers */
static void denali_enable_dma(bool en)
{
	uint32_t reg_val = 0x0;

	if (en)
		reg_val = DMA_ENABLE__FLAG;

	__raw_writel(reg_val, denali.flash_reg + DMA_ENABLE);
	__raw_readl(denali.flash_reg + DMA_ENABLE);
}

/* setups the HW to perform the data DMA */
static void denali_setup_dma_sequence(int op)
{
	const int page_count = 1;
	uint32_t mode;
	uint32_t addr = (uint32_t)denali.buf.dma_buf;

	mode = MODE_10 | BANK(denali.flash_bank);

	/* DMA is a four step process */

	/* 1. setup transfer type and # of pages */
	index_addr(mode | denali.page, 0x2000 | op | page_count);

	/* 2. set memory high address bits 23:8 */
	index_addr(mode | ((uint16_t)(addr >> 16) << 8), 0x2200);

	/* 3. set memory low address bits 23:8 */
	index_addr(mode | ((uint16_t)addr << 8), 0x2300);

	/* 4.  interrupt when complete, burst len = 64 bytes*/
	index_addr(mode | 0x14000, 0x2400);
}

/* Common DMA function */
static uint32_t denali_dma_configuration(uint32_t ops, bool raw_xfer,
	uint32_t irq_mask)
{
	uint32_t irq_status = 0;
	/*
	 * setup_ecc_for_xfer(bool ecc_en, bool transfer_spare)
	 * for raw_xfer, disable ECC and enable spare transfer
	 * for !raw_xfer, enable ECC and disable spare transfer
	 */
	setup_ecc_for_xfer(!raw_xfer, raw_xfer);

	/* clear any previous interrupt flags */
	clear_interrupts();

	/* enable the DMA */
	denali_enable_dma(true);

	/* setup the DMA */
	denali_setup_dma_sequence(ops);

	/* wait for operation to complete */
	irq_status = wait_for_irq(irq_mask);

	/* if ECC fault happen, seems we need delay before turning off DMA.
	 * If not, the controller will go into non responsive condition */
	if (irq_status & INTR_STATUS__ECC_UNCOR_ERR)
		udelay(100);

	/* disable the DMA */
	denali_enable_dma(false);

	return irq_status;
}

static void write_page(struct mtd_info *mtd, struct nand_chip *chip,
			const uint8_t *buf, bool raw_xfer)
{
	uint32_t irq_status = 0;
	uint32_t irq_mask = INTR_STATUS__DMA_CMD_COMP;

	denali.status = PASS;

	/* copy buffer into DMA buffer */
	memcpy((void *)denali.buf.dma_buf, buf, mtd->writesize);

	/* need extra memcpoy for raw transfer */
	if (raw_xfer)
		memcpy((void *)denali.buf.dma_buf + mtd->writesize,
			chip->oob_poi, mtd->oobsize);

	/* setting up DMA */
	irq_status = denali_dma_configuration(DENALI_WRITE, raw_xfer, irq_mask);

	/* if timeout happen, error out */
	if (!(irq_status & INTR_STATUS__DMA_CMD_COMP)) {
		debug("DMA timeout for denali write_page\n");
		denali.status = NAND_STATUS_FAIL;
	}

	if (irq_status & INTR_STATUS__LOCKED_BLK) {
		debug("Failed as write to locked block\n");
		denali.status = NAND_STATUS_FAIL;
	}
}

/* NAND core entry points */

/*
 * this is the callback that the NAND core calls to write a page. Since
 * writing a page with ECC or without is similar, all the work is done
 * by write_page above.
 */
static void denali_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				const uint8_t *buf)
{
	/*
	 * for regular page writes, we let HW handle all the ECC
	 * data written to the device.
	 */
	debug("denali_write_page at page %08x\n", denali.page);

	/* switch to main access only */
	denali_mode_main_access();

	write_page(mtd, chip, buf, false);
}

/*
 * This is the callback that the NAND core calls to write a page without ECC.
 * raw access is similar to ECC page writes, so all the work is done in the
 * write_page() function above.
 */
static void denali_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
					const uint8_t *buf)
{
	/*
	 * for raw page writes, we want to disable ECC and simply write
	 * whatever data is in the buffer.
	 */
	debug("denali_write_page_raw at page %08x\n", denali.page);

	/* switch to main + spare access */
	denali_mode_main_spare_access();

	write_page(mtd, chip, buf, true);
}

static int denali_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
				int page)
{
	return write_oob_data(mtd, chip->oob_poi, page);
}

/* raw include ECC value and all the spare area */
static int denali_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int page)
{
	uint32_t irq_status, irq_mask = INTR_STATUS__DMA_CMD_COMP;

	debug("denali_read_page_raw at page %08x\n", page);
	if (denali.page != page) {
		debug("Missing NAND_CMD_READ0 command\n");
		return -EIO;
	}

	/* switch to main + spare access */
	denali_mode_main_spare_access();

	/* setting up the DMA where ecc_enable is false */
	irq_status = denali_dma_configuration(DENALI_READ, true, irq_mask);

	/* if timeout happen, error out */
	if (!(irq_status & INTR_STATUS__DMA_CMD_COMP)) {
		debug("DMA timeout for denali_read_page_raw\n");
		return -EIO;
	}

	/* splitting the content to destination buffer holder */
	memcpy(chip->oob_poi, (const void *)(denali.buf.dma_buf +
		mtd->writesize), mtd->oobsize);
	memcpy(buf, (const void *)denali.buf.dma_buf, mtd->writesize);
	debug("buf %02x %02x\n", buf[0], buf[1]);
	debug("chip->oob_poi %02x %02x\n", chip->oob_poi[0], chip->oob_poi[1]);
	return 0;
}

static int denali_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			    uint8_t *buf, int page)
{
	uint32_t irq_status, irq_mask =	INTR_STATUS__DMA_CMD_COMP;

	debug("denali_read_page at page %08x\n", page);
	if (denali.page != page) {
		debug("Missing NAND_CMD_READ0 command\n");
		return -EIO;
	}

	/* switch to main access only */
	denali_mode_main_access();

	/* setting up the DMA where ecc_enable is true */
	irq_status = denali_dma_configuration(DENALI_READ, false, irq_mask);

	memcpy(buf, (const void *)denali.buf.dma_buf, mtd->writesize);
	debug("buf %02x %02x\n", buf[0], buf[1]);

	/* check whether any ECC error */
	if (irq_status & INTR_STATUS__ECC_UNCOR_ERR) {

		/* is the ECC cause by erase page, check using read_page_raw */
		debug("  Uncorrected ECC detected\n");
		denali_read_page_raw(mtd, chip, buf, denali.page);

		if (is_erased(buf, mtd->writesize) == true &&
			is_erased(chip->oob_poi, mtd->oobsize) == true) {
			debug("  ECC error cause by erased block\n");
			/* false alarm, return the 0xFF */
		} else
			return -EIO;
	}
	memcpy(buf, (const void *)denali.buf.dma_buf, mtd->writesize);
	return 0;
}

static uint8_t denali_read_byte(struct mtd_info *mtd)
{
	uint32_t addr, result;
	addr = (uint32_t)MODE_11 | BANK(denali.flash_bank);
	index_addr_read_data((uint32_t)addr | 2, &result);
	return (uint8_t)result & 0xFF;
}

static int denali_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
				int page, int sndcmd)
{
	debug("denali_read_oob at page %08x\n", page);
	denali.page = page;
	return denali_read_page_raw(mtd, chip, denali.buf.buf, page);
}

static void denali_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	uint32_t i, addr, result;

	/* delay for tR (data transfer from Flash array to data register) */
	udelay(25);

	/* ensure device completed else additional delay and polling */
	wait_for_irq(INTR_STATUS__INT_ACT);

	addr = (uint32_t)MODE_11 | BANK(denali.flash_bank);
	for (i = 0; i < len; i++) {
		index_addr_read_data((uint32_t)addr | 2, &result);
		write_byte_to_buf(result);
	}
	memcpy(buf, denali.buf.buf, len);
}

static void denali_select_chip(struct mtd_info *mtd, int chip)
{
	denali.flash_bank = chip;
}

static int denali_waitfunc(struct mtd_info *mtd, struct nand_chip *chip)
{
	int status = denali.status;
	denali.status = 0;

	return status;
}

static void denali_erase(struct mtd_info *mtd, int page)
{
	uint32_t cmd = 0x0, irq_status = 0;

	debug("denali_erase at page %08x\n", page);

	/* clear interrupts */
	clear_interrupts();

	/* setup page read request for access type */
	cmd = MODE_10 | BANK(denali.flash_bank) | page;
	index_addr((uint32_t)cmd, 0x1);

	/* wait for erase to complete or failure to occur */
	irq_status = wait_for_irq(INTR_STATUS__ERASE_COMP |
		INTR_STATUS__ERASE_FAIL);

	if (irq_status & INTR_STATUS__ERASE_FAIL ||
		irq_status & INTR_STATUS__LOCKED_BLK)
		denali.status = NAND_STATUS_FAIL;
	else
		denali.status = PASS;
}

static void denali_cmdfunc(struct mtd_info *mtd, unsigned int cmd, int col,
			   int page)
{
	uint32_t addr;

	switch (cmd) {
	case NAND_CMD_PAGEPROG:
		break;
	case NAND_CMD_STATUS:
		addr = (uint32_t)MODE_11 | BANK(denali.flash_bank);
		index_addr((uint32_t)addr | 0, cmd);
		break;
	case NAND_CMD_PARAM:
		clear_interrupts();
	case NAND_CMD_READID:
		reset_buf();
		/* sometimes ManufactureId read from register is not right
		 * e.g. some of Micron MT29F32G08QAA MLC NAND chips
		 * So here we send READID cmd to NAND insteand
		 * */
		addr = (uint32_t)MODE_11 | BANK(denali.flash_bank);
		index_addr((uint32_t)addr | 0, cmd);
		index_addr((uint32_t)addr | 1, col & 0xFF);
		break;
	case NAND_CMD_READ0:
	case NAND_CMD_SEQIN:
		denali.page = page;
		break;
	case NAND_CMD_RESET:
		reset_bank();
		break;
	case NAND_CMD_READOOB:
		/* TODO: Read OOB data */
		break;
	case NAND_CMD_ERASE1:
		/*
		 * supporting block erase only, not multiblock erase as
		 * it will cross plane and software need complex calculation
		 * to identify the block count for the cross plane
		 */
		denali_erase(mtd, page);
		break;
	case NAND_CMD_ERASE2:
		/* nothing to do here as it was done during NAND_CMD_ERASE1 */
		break;
	case NAND_CMD_UNLOCK1:
		addr = (uint32_t)MODE_10 | BANK(denali.flash_bank) | page;
		index_addr((uint32_t)addr | 0, DENALI_UNLOCK_START);
		break;
	case NAND_CMD_UNLOCK2:
		addr = (uint32_t)MODE_10 | BANK(denali.flash_bank) | page;
		index_addr((uint32_t)addr | 0, DENALI_UNLOCK_END);
		break;
	case NAND_CMD_LOCK:
		addr = (uint32_t)MODE_10 | BANK(denali.flash_bank);
		index_addr((uint32_t)addr | 0, DENALI_LOCK);
		break;
	case NAND_CMD_LOCK_TIGHT:
		addr = (uint32_t)MODE_10 | BANK(denali.flash_bank);
		index_addr((uint32_t)addr | 0, DENALI_LOCK_TIGHT);
		break;
	default:
		printf(": unsupported command received 0x%x\n", cmd);
		break;
	}
}

/* stubs for ECC functions not used by the NAND core */
static int denali_ecc_calculate(struct mtd_info *mtd, const uint8_t *data,
				uint8_t *ecc_code)
{
	debug("Should not be called as ECC handled by hardware\n");
	BUG();
	return -EIO;
}

static int denali_ecc_correct(struct mtd_info *mtd, uint8_t *data,
				uint8_t *read_ecc, uint8_t *calc_ecc)
{
	debug("Should not be called as ECC handled by hardware\n");
	BUG();
	return -EIO;
}

static void denali_ecc_hwctl(struct mtd_info *mtd, int mode)
{
	debug("Should not be called as ECC handled by hardware\n");
	BUG();
}
/* end NAND core entry points */

/* Initialization code to bring the device up to a known good state */
static void denali_hw_init(void)
{
	/*
	 * tell driver how many bit controller will skip before writing
	 * ECC code in OOB. This is normally used for bad block marker
	 */
	__raw_writel(CONFIG_NAND_DENALI_SPARE_AREA_SKIP_BYTES,
		denali.flash_reg + SPARE_AREA_SKIP_BYTES);
	detect_max_banks();
	denali_nand_reset();
	__raw_writel(0x0F, denali.flash_reg + RB_PIN_ENABLED);
	__raw_writel(CHIP_EN_DONT_CARE__FLAG,
			denali.flash_reg + CHIP_ENABLE_DONT_CARE);
	__raw_writel(0xffff, denali.flash_reg + SPARE_AREA_MARKER);

	/* Should set value for these registers when init */
	__raw_writel(0, denali.flash_reg + TWO_ROW_ADDR_CYCLES);
	__raw_writel(1, denali.flash_reg + ECC_ENABLE);
	denali_nand_timing_set();
	denali_irq_init();
}

/*
 * Although controller spec said SLC ECC is forceb to be 4bit, but denali
 * controller in MRST only support 15bit and 8bit ECC correction
 */
#ifdef CONFIG_SYS_NAND_15BIT_HW_ECC_OOBFIRST
#define ECC_15BITS	26
static struct nand_ecclayout nand_15bit_oob = {
	.eccbytes = ECC_15BITS,
};
#else
#define ECC_8BITS	14
static struct nand_ecclayout nand_8bit_oob = {
	.eccbytes = ECC_8BITS,
};
#endif  /* CONFIG_SYS_NAND_15BIT_HW_ECC_OOBFIRST */

void denali_nand_init(struct nand_chip *nand)
{
	denali.flash_reg = (void  __iomem *)CONFIG_SYS_NAND_REGS_BASE;
	denali.flash_mem = (void  __iomem *)CONFIG_SYS_NAND_DATA_BASE;

	nand->chip_delay  = 0;
#ifdef CONFIG_SYS_NAND_USE_FLASH_BBT
	/* check whether flash got BBT table (located at end of flash). As we
	 * use NAND_USE_FLASH_BBT_NO_OOB, the BBT page will start with
	 * bbt_pattern. We will have mirror pattern too */
	nand->options |= NAND_USE_FLASH_BBT;
	/*
	 * We are using main + spare with ECC support. As BBT need ECC support,
	 * we need to ensure BBT code don't write to OOB for the BBT pattern.
	 * All BBT info will be stored into data area with ECC support.
	 */
	nand->options |= NAND_USE_FLASH_BBT_NO_OOB;
#endif

	nand->ecc.mode = NAND_ECC_HW;
	nand->ecc.size = CONFIG_NAND_DENALI_ECC_SIZE;
	nand->ecc.read_oob = denali_read_oob;
	nand->ecc.write_oob = denali_write_oob;
	nand->ecc.read_page = denali_read_page;
	nand->ecc.read_page_raw = denali_read_page_raw;
	nand->ecc.write_page = denali_write_page;
	nand->ecc.write_page_raw = denali_write_page_raw;
#ifdef CONFIG_SYS_NAND_15BIT_HW_ECC_OOBFIRST
	/* 15bit ECC */
	nand->ecc.bytes = 26;
	nand->ecc.layout = &nand_15bit_oob;
#else	/* 8bit ECC */
	nand->ecc.bytes = 14;
	nand->ecc.layout = &nand_8bit_oob;
#endif
	nand->ecc.calculate = denali_ecc_calculate;
	nand->ecc.correct  = denali_ecc_correct;
	nand->ecc.hwctl  = denali_ecc_hwctl;

	/* Set address of hardware control function */
	nand->cmdfunc = denali_cmdfunc;
	nand->read_byte = denali_read_byte;
	nand->read_buf = denali_read_buf;
	nand->select_chip = denali_select_chip;
	nand->waitfunc = denali_waitfunc;
	denali_hw_init();
}

int board_nand_init(struct nand_chip *chip)
{
	puts("NAND:  Denali NAND controller\n");
	denali_nand_init(chip);
	return 0;
}
