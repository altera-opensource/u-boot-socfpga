/*
 *  Copyright (C) 2013 Altera Corporation <www.altera.com>
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

#include <common.h>
#include <asm/io.h>
#include <asm/arch/sdram.h>
#include <watchdog.h>

unsigned int get_next_prbs31(unsigned int prev_value)
{
	unsigned int tmp, next_data;
	int i;
	next_data = prev_value << 1;
	for (i = 0; i < 31; i++) {
		/* bit 1 equal to prev[31] ^ prev[28] */
		tmp = ((next_data >> 31) ^ (next_data >> 28)) & 0x1;
		next_data = next_data << 1;
		next_data |= (tmp << 1);
	}
	next_data |= ((next_data >> 31) ^ (next_data >> 28)) & 0x1;
	return next_data;
}

int test_rand_address(int loop_cnt, unsigned int addr_bgn,
	unsigned int addr_end, int seed)
{
	unsigned int address, data;
	unsigned int seed_org = seed;
	int i;

	for (i = 0; i < loop_cnt; i++) {
		seed =	get_next_prbs31(seed);
		if (seed > addr_end)
			seed = seed % addr_end;
		address = (seed - (seed  % 4)) + addr_bgn;
		writel(seed, address);
#ifdef CONFIG_HW_WATCHDOG
		WATCHDOG_RESET();
#endif
	}

	seed = seed_org;
	for (i = 0; i < loop_cnt; i++) {
		seed =	get_next_prbs31(seed);
		if (seed > addr_end)
			seed = seed % addr_end;
		address = (seed - (seed  % 4)) + addr_bgn;
		data = readl(address);
		if (data != seed) {
			printf("Iteration %0d:, expect 0x%08x from address "
				"0x%08x, read 0x%08x instead",
				i, address, seed, data);
			return 0;
		}
#ifdef CONFIG_HW_WATCHDOG
		WATCHDOG_RESET();
#endif
	}
	return 1;
}

/*
 * Valid arguments for coverage
 * SDRAM_TEST_FAST -> quick test which run around 5s
 * SDRAM_TEST_NORMAL -> normal test which run around 30s
 * SDRAM_TEST_LONG -> long test which run in minutes
 */
int hps_emif_diag_test(int coverage, unsigned int addr_bgn,
	unsigned int addr_end)
{
	int cnt_max = 1000000, status;
	unsigned int seed = 0xdeadbeef;

	if (coverage == SDRAM_TEST_NORMAL)
		cnt_max *= 10;
	else if (coverage == SDRAM_TEST_LONG)
		cnt_max *= 1000;

	puts("SDRAM: Running EMIF Diagnostic Test ...");
	status = test_rand_address(cnt_max, addr_bgn, addr_end, seed);

	if (status)
		puts("Passed\n");
	else
		puts("Failed\n");
	return status;
}

