/*
 * Copyright (C) 2014-2017 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef	_SOCFPGA_MISC_H_
#define	_SOCFPGA_MISC_H_

#define	REGULAR_BOOT_MAGIC	0xd15ea5e
/* For restoring s2f module of syswarmmask register to default state */
#define REGULAR_BOOT_S2FWARMRESET_RESTORE_MAGIC	0xd15ea5f
#define	RAM_BOOT_EN_MAGIC	0xae9efebc

#ifndef __ASSEMBLY__
void skip_relocation(void);
int is_chosen_boolean_true(const void *blob, const char *name);
int is_external_fpga_config(const void *blob);
int is_early_release_fpga_config(const void *blob);
int config_pins(const void *blob, const char *pin_grp);
unsigned int dedicated_uart_com_port(const void *blob);
unsigned int shared_uart_com_port(const void *blob);
void shared_uart_buffer_to_console(void);
unsigned int uart_com_port(const void *blob);
void set_regular_boot(unsigned int set);
unsigned int is_regular_boot(void);
int qspi_software_reset(void);
void enable_ram_boot(unsigned int enable, unsigned long reentrance_loc);
#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_MISC_H_ */
