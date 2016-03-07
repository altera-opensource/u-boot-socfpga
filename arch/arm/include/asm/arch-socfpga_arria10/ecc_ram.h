/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef	_SOCFPGA_ECC_RAM_H_
#define	_SOCFPGA_ECC_RAM_H_

#define IRQ_ECC_OCRAM_CORRECTED		210
#define IRQ_ECC_OCRAM_UNCORRECTED	211

extern unsigned long irq_cnt_ecc_ocram_corrected;

#ifndef __ASSEMBLY__

struct socfpga_ecc {
	volatile uint32_t  ip_rev_id;
	volatile uint32_t  _pad_0x4_0x7;
	volatile uint32_t  ctrl;
	volatile uint32_t  initstat;
	volatile uint32_t  errinten;
	volatile uint32_t  errintens;
	volatile uint32_t  errintenr;
	volatile uint32_t  intmode;
	volatile uint32_t  intstat;
	volatile uint32_t  inttest;
	volatile uint32_t  modstat;
	volatile uint32_t  derraddra;
	volatile uint32_t  serraddra;
	volatile uint32_t  _pad_0x34_0x3b[2];
	volatile uint32_t  serrcntreg;
	volatile uint32_t  ecc_addrbus;
	volatile uint32_t  ecc_rdata0bus;
	volatile uint32_t  ecc_rdata1bus;
	volatile uint32_t  ecc_rdata2bus;
	volatile uint32_t  ecc_rdata3bus;
	volatile uint32_t  ecc_wdata0bus;
	volatile uint32_t  ecc_wdata1bus;
	volatile uint32_t  ecc_wdata2bus;
	volatile uint32_t  ecc_wdata3bus;
	volatile uint32_t  ecc_rdataecc0bus;
	volatile uint32_t  ecc_rdataecc1bus;
	volatile uint32_t  ecc_wdataecc0bus;
	volatile uint32_t  ecc_wdataecc1bus;
	volatile uint32_t  ecc_dbytectrl;
	volatile uint32_t  ecc_accctrl;
	volatile uint32_t  ecc_startacc;
	volatile uint32_t  ecc_wdctrl;
	volatile uint32_t  _pad_0x84_0x8f[3];
	volatile uint32_t  serrlkupa0;
};

void irq_handler_ecc_ram_serr(void);
void irq_handler_ecc_ram_derr(void);
void enable_ecc_ram_serr_int(void);
void clear_ecc_ocram_ecc_status(void);
int is_ocram_ecc_enabled(void);
#endif /* __ASSEMBLY__ */

#define ALT_ECC_INTSTAT_SERRPENA_SET_MSK	0x00000001
#define ALT_ECC_INTSTAT_DERRPENA_SET_MSK	0x00000100
#define ALT_ECC_ERRINTEN_SERRINTEN_SET_MSK	0x00000001
#define ALT_ECC_CTRL_ECCEN_SET_MSK		0x00000001

#endif /* _SOCFPGA_ECC_RAM_H_ */
