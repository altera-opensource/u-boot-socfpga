/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef	_SOCFPGA_MISC_H_
#define	_SOCFPGA_MISC_H_

#ifndef __ASSEMBLY__
void skip_relocation(void);
int is_chosen_boolean_true(const void *blob, const char *name);
int is_external_fpga_config(const void *blob);
int config_shared_fpga_pins(const void *blob);

#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_MISC_H_ */
