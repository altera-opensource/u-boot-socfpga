/*
 * Copyright Altera Corporation (C) 2012-2014. All rights reserved
 *
 * SPDX-License-Identifier:    BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of Altera Corporation nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ALTERA CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_SDRAM_PHY_H_
#define	_SDRAM_PHY_H_

#include <common.h>
#include <asm/io.h>
#include <asm/arch/sdram.h>
#if (CONFIG_PRELOADER_DEBUG_MEMORY_WRITE == 1)
#include <asm/arch/debug_memory.h>
#endif

#define	HPS_SDR_BASE	CONFIG_SDRAM_CONTROLLER

#define write_register(BASE, OFFSET, DATA) \
	writel(DATA, ((BASE) + (OFFSET)))

#define read_register(BASE, OFFSET) \
	readl((BASE) + (OFFSET))

#if (CONFIG_PRELOADER_DEBUG_MEMORY_WRITE == 1)
#define HPS_HW_DEBUG	1
#define sdram_debug_memory(LINE) \
	DEBUG_MEMORY
#endif

#ifdef CONFIG_SPL_SERIAL_SUPPORT
#define HPS_HW_SERIAL_SUPPORT
#endif

#endif /* _SDRAM_PHY_H_ */
