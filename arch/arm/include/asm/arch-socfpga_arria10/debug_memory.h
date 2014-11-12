/*
 * Copyright (C) 2014 Altera Corporation <www.altera.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _SOCFPGA_DEBUG_MEMORY_H_
#define _SOCFPGA_DEBUG_MEMORY_H_

/* No debug memory features as we will use semihosting option */
#define DEBUG_MEMORY

/* semihosting support for ARM debugger */
#define SEMI_PRINTF	4

#ifndef __ASSEMBLY__
int semihosting_write(const char *buffer);
#endif /* __ASSEMBLY__ */

#endif	/* _SOCFPGA_DEBUG_MEMORY_H_ */

