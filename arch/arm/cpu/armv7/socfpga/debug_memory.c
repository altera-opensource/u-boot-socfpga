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

#include <asm/io.h>
#include <common.h>
#include <asm/arch/debug_memory.h>

unsigned char *debug_memory_write_addr;

/* write the ID into debug memory specified by build.h */
void debug_memory (char *file, unsigned line)
{
#if (CONFIG_PRELOADER_DEBUG_MEMORY_WRITE == 1)
	if (file == 0)
		return;

	/* append the file ID in front (bit 15-12) */
	if (0 == strcmp(file, DM_SPL_C_FILE))
		line |= DM_SPL_C_ID;
	else if (0 == strcmp(file, DM_CLOCK_MANANGER_C_FILE))
		line |= DM_CLOCK_MANANGER_C_ID;
	else if (0 == strcmp(file, DM_ECC_RAM_C_FILE))
		line |= DM_ECC_RAM_C_ID;
	else if (0 == strcmp(file, DM_FPGA_MANAGER_C_FILE))
		line |= DM_FPGA_MANAGER_C_ID;
	else if (0 == strcmp(file, DM_FREEZE_CONTROLLER_C_FILE))
		line |= DM_FREEZE_CONTROLLER_C_ID;
	else if (0 == strcmp(file, DM_INTERRUPT_C_FILE))
		line |= DM_INTERRUPT_C_ID;
	else if (0 == strcmp(file, DM_RESET_MANAGER_C_FILE))
		line |= DM_RESET_MANAGER_C_ID;
	else if (0 == strcmp(file, DM_SCAN_MANAGER_C_FILE))
		line |= DM_SCAN_MANAGER_C_ID;
	else if (0 == strcmp(file, DM_SDRAM_C_FILE))
		line |= DM_SDRAM_C_ID;
	else if (0 == strcmp(file, DM_SINIT_C_FILE))
		line |= DM_SINIT_C_ID;
	else if (0 == strcmp(file, DM_SEQUENCER_C_FILE))
		line |= DM_SEQUENCER_C_ID;

	/* initialize if first time used */
	if (debug_memory_write_addr == 0)
		debug_memory_write_addr =
			(unsigned char *)CONFIG_PRELOADER_DEBUG_MEMORY_ADDR;
	/* ensure no write happen after overflow */
	if (debug_memory_write_addr > (unsigned char *)
		(CONFIG_PRELOADER_DEBUG_MEMORY_ADDR +
		CONFIG_PRELOADER_DEBUG_MEMORY_SIZE))
		return;
	/* write to memory content */
	*debug_memory_write_addr = line & 0xFF;
	debug_memory_write_addr++;
	*debug_memory_write_addr = (line >> 8) & 0xFF;
	debug_memory_write_addr++;
	/* always append 0 to indicate end of debug memory */
	*debug_memory_write_addr = 0;
	*(debug_memory_write_addr+1) = 0;
#endif
}

/* print out the debug memory wrote */
void debug_memory_print (void)
{
#if (CONFIG_PRELOADER_DEBUG_MEMORY_WRITE == 1) && \
defined(CONFIG_SPL_SERIAL_SUPPORT)
	unsigned i, value = 1;
	unsigned char *addr;
	addr = (unsigned char *)CONFIG_PRELOADER_DEBUG_MEMORY_ADDR;
	puts("\nDebug memory :");
	for (i = 0; value != 0; i++) {
		if (i % 8 == 0)
			printf("\n%08x - ", (unsigned)(addr+i));
		value = *(addr+i);
		i++;
		value |= (*(addr + i) << 8);

		/*
		 * file decoding (optional as for better decoding)
		 * Must list down from big ID number till smallest ID
		 */
		if ((value & DM_SEQUENCER_C_ID) == DM_SEQUENCER_C_ID)
			printf("SEQ-%04d ", value & ~DM_SEQUENCER_C_ID);
		else if ((value & DM_SINIT_C_ID) ==
			DM_SINIT_C_ID)
			printf("SIN-%03d ", value & 0xFFF);
		else if ((value & DM_SDRAM_C_ID) ==
			DM_SDRAM_C_ID)
			printf("SDR-%03d ", value & 0xFFF);
		else if ((value & DM_SCAN_MANAGER_C_ID) ==
			DM_SCAN_MANAGER_C_ID)
			printf("SCN-%03d ", value & 0xFFF);
		else if ((value & DM_RESET_MANAGER_C_ID) ==
			DM_RESET_MANAGER_C_ID)
			printf("RST-%03d ", value & 0xFFF);
		else if ((value & DM_INTERRUPT_C_ID) ==
			DM_INTERRUPT_C_ID)
			printf("INT-%03d ", value & 0xFFF);
		else if ((value & DM_FREEZE_CONTROLLER_C_ID) ==
			DM_FREEZE_CONTROLLER_C_ID)
			printf("FRC-%03d ", value & 0xFFF);
		else if ((value & DM_FPGA_MANAGER_C_ID) ==
			DM_FPGA_MANAGER_C_ID)
			printf("FPG-%03d ", value & 0xFFF);
		else if ((value & DM_ECC_RAM_C_ID) ==
			DM_ECC_RAM_C_ID)
			printf("ECC-%03d ", value & 0xFFF);
		else if ((value & DM_CLOCK_MANANGER_C_ID) ==
			DM_CLOCK_MANANGER_C_ID)
			printf("CLK-%03d ", value & 0xFFF);
		else if ((value & DM_SPL_C_ID) ==
			DM_SPL_C_ID)
			printf("SPL-%03d ", value & 0xFFF);

	}
	puts("\n\n");
#endif
}

#ifdef CONFIG_SPL_SEMIHOSTING_SUPPORT
/* semihosting operation which supported by ARM debugger */

/* svc function call */
int svc(unsigned operation, void *value)
{
	int temp;
#ifdef CONFIG_SYS_THUMB_BUILD
	asm volatile ("mov r0, %1; mov r1, %2; svc 0xAB; mov %0, r0"
#else
	asm volatile ("mov r0, %1; mov r1, %2; svc 0x123456; mov %0, r0"
#endif
		: "=r" (temp)
		: "r" (operation), "r" (value)
		: "r0", "r1", "r2", "r3", "ip", "lr", "memory", "cc");
	return temp;
}

/* perform a write to console buffer */
int semihosting_write(const char *buffer)
{
	return svc(SEMI_PRINTF, (void *)buffer);
}

#endif

