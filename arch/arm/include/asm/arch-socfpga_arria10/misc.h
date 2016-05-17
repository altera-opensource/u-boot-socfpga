/*
 * Copyright (C) 2014-2016 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef	_SOCFPGA_MISC_H_
#define	_SOCFPGA_MISC_H_

#ifndef __ASSEMBLY__
void skip_relocation(void);
int is_chosen_boolean_true(const void *blob, const char *name);
int is_external_fpga_config(const void *blob);
int is_early_release_fpga_config(const void *blob);
int config_pins(const void *blob, const char *pin_grp);

#endif /* __ASSEMBLY__ */

#endif /* _SOCFPGA_MISC_H_ */
