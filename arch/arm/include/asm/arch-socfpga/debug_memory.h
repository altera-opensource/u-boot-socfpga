/*
 *  Copyright (C) 2012 Altera Corporation <www.altera.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DEBUG_MEMORY_H_
#define _DEBUG_MEMORY_H_

extern unsigned char *debug_memory_write_addr __attribute__((section(".data")));

#ifdef CONFIG_SPL_BUILD
#if (CONFIG_PRELOADER_DEBUG_MEMORY_WRITE == 1)
#define DEBUG_MEMORY	debug_memory(__FILE__, __LINE__);
#else
#define DEBUG_MEMORY
#endif
#else
#define DEBUG_MEMORY
#endif


/* function declaration */
void debug_memory (char *file, unsigned line);
void debug_memory_print (void);

/* File name and its file ID which append in front (bit 15-12)  */
#define DM_SPL_C_FILE			"spl.c"
#define DM_SPL_C_ID			(0x1 << 12)
#define DM_CLOCK_MANANGER_C_FILE	"clock_manager.c"
#define DM_CLOCK_MANANGER_C_ID		(0x2 << 12)
#define DM_ECC_RAM_C_FILE		"ecc_ram.c"
#define DM_ECC_RAM_C_ID			(0x3 << 12)
#define DM_FPGA_MANAGER_C_FILE		"fpga_manager.c"
#define DM_FPGA_MANAGER_C_ID		(0x4 << 12)
#define DM_FREEZE_CONTROLLER_C_FILE	"freeze_controller.c"
#define DM_FREEZE_CONTROLLER_C_ID	(0x5 << 12)
#define DM_INTERRUPT_C_FILE		"interrupts.c"
#define DM_INTERRUPT_C_ID		(0x6 << 12)
#define DM_RESET_MANAGER_C_FILE		"reset_manager.c"
#define DM_RESET_MANAGER_C_ID		(0x7 << 12)
#define DM_SCAN_MANAGER_C_FILE		"scan_manager.c"
#define DM_SCAN_MANAGER_C_ID		(0x8 << 12)
#define DM_SDRAM_C_FILE			"sdram.c"
#define DM_SDRAM_C_ID			(0x9 << 12)
#define DM_SINIT_C_FILE			"s_init.c"
#define DM_SINIT_C_ID			(0xA << 12)
/*
 * note ID 12 and above are allocated for SDRAM sequencer
 * It is because sequencer line are around 10k
 */
#define DM_SEQUENCER_C_FILE		"sequencer.c"
#define DM_SEQUENCER_C_ID		(0xC << 12)

/* semihosting support for ARM debugger */
#ifdef CONFIG_SPL_SEMIHOSTING_SUPPORT

#define SEMI_PRINTF	4
/*#define SEMI_LENGTH	100

extern int semihosting_write_handler;

struct struct_semihosting_open {
	char	*file;
	int	permissions;
	int	length;
};

struct struct_semihosting_write {
	int	handle;
	char	*buffer;
	int	length;
};*/

int semihosting_write(const char *buffer);

#endif /* CONFIG_SPL_SEMIHOSTING_SUPPORT */

#endif	/* _DEBUG_MEMORY_H_ */

