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

#include <common.h>
#include <asm/io.h>
#include <asm/arch/freeze_controller.h>
#include <asm/arch/scan_manager.h>
#include <asm/arch/debug_memory.h>

/*
 * scan_mgr_io_scan_chain_prg
 * Program HPS IO Scan Chain
 */
unsigned long
scan_mgr_io_scan_chain_prg(
	IOScanChainSelect io_scan_chain_id,
	uint32_t io_scan_chain_len_in_bits,
	const unsigned long *iocsr_scan_chain)
{

	uint16_t tdi_tdo_header;
	uint32_t io_program_iter;
	uint32_t io_scan_chain_data_residual;
	uint32_t residual;
	uint32_t i;
	uint32_t index = 0;

	/*
	 * Check if IO bank is in frozen state before proceed to program IO
	 * scan chain.
	 * Note: IO scan chain ID is 1:1 mapping to freeze channel ID
	 */
	DEBUG_MEMORY
	 if (sys_mgr_frzctrl_frzchn_is_frozen(io_scan_chain_id)) {

		/* De-assert reinit if the IO scan chain is intended for HIO */
		if (IO_SCAN_CHAIN_3 == io_scan_chain_id) {
			DEBUG_MEMORY
			clrbits_le32((SOCFPGA_SYSMGR_ADDRESS+
				SYSMGR_FRZCTRL_HIOCTRL_ADDRESS),
				SYSMGR_FRZCTRL_HIOCTRL_DLLRST_MASK);
		} /* if (HIO) */

		/*
		 * Check if the scan chain engine is inactive and the
		 * WFIFO is empty before enabling the IO scan chain
		 */
		if (SCAN_MGR_IO_SCAN_ENGINE_STATUS_IDLE
			!= scan_mgr_io_scan_chain_engine_is_idle(
			MAX_WAITING_DELAY_IO_SCAN_ENGINE)) {
			DEBUG_MEMORY
			return 1;
		}

		/*
		 * Enable IO Scan chain based on scan chain id
		 * Note: only one chain can be enabled at a time
		 */
		setbits_le32((SOCFPGA_SCANMGR_ADDRESS +
			SCANMGR_EN_ADDRESS),
			1 << io_scan_chain_id);

		/*
		 * Calculate number of iteration needed for
		 * full 128-bit (4 x32-bits) bits shifting.
		 * Each TDI_TDO packet can shift in maximum 128-bits
		 */
		io_program_iter
			= io_scan_chain_len_in_bits >>
			IO_SCAN_CHAIN_128BIT_SHIFT;
		io_scan_chain_data_residual
			= io_scan_chain_len_in_bits &
			IO_SCAN_CHAIN_128BIT_MASK;

		/*
		 * Construct TDI_TDO packet for
		 * 128-bit IO scan chain (2 bytes)
		 */
		tdi_tdo_header = TDI_TDO_HEADER_FIRST_BYTE
			| (TDI_TDO_MAX_PAYLOAD <<
			TDI_TDO_HEADER_SECOND_BYTE_SHIFT);

		/* Program IO scan chain in 128-bit iteration */
		DEBUG_MEMORY
		for (i = 0; i < io_program_iter; i++) {

			/* write TDI_TDO packet header to scan manager */
			writel(tdi_tdo_header,
				(SOCFPGA_SCANMGR_ADDRESS +
				SCANMGR_FIFODOUBLEBYTE_ADDRESS));

			/* calculate array index */
			index = i * 4;

			/*
			 * write 4 successive 32-bit IO scan
			 * chain data into WFIFO
			 */
			writel(iocsr_scan_chain[index],
				(SOCFPGA_SCANMGR_ADDRESS +
				SCANMGR_FIFOQUADBYTE_ADDRESS));
			writel(iocsr_scan_chain[index + 1],
				(SOCFPGA_SCANMGR_ADDRESS +
				SCANMGR_FIFOQUADBYTE_ADDRESS));
			writel(iocsr_scan_chain[index + 2],
				(SOCFPGA_SCANMGR_ADDRESS +
				SCANMGR_FIFOQUADBYTE_ADDRESS));
			writel(iocsr_scan_chain[index + 3],
				(SOCFPGA_SCANMGR_ADDRESS +
				SCANMGR_FIFOQUADBYTE_ADDRESS));

			/*
			 * Check if the scan chain engine has completed the
			 * IO scan chain data shifting
			 */
			if (SCAN_MGR_IO_SCAN_ENGINE_STATUS_IDLE
				!= scan_mgr_io_scan_chain_engine_is_idle(
				MAX_WAITING_DELAY_IO_SCAN_ENGINE)) {
				DEBUG_MEMORY
				/* Disable IO Scan chain when error detected */
				clrbits_le32((SOCFPGA_SCANMGR_ADDRESS +
					SCANMGR_EN_ADDRESS),
					1 << io_scan_chain_id);

				return 1;
			}
		}

		/* Calculate array index for final TDI_TDO packet */
		index = io_program_iter * 4;

		/* Final TDI_TDO packet if any */
		DEBUG_MEMORY
		if (0 != io_scan_chain_data_residual) {

			/*
			 * Calculate number of quad bytes FIFO write
			 * needed for the final TDI_TDO packet
			 */
			io_program_iter
				= io_scan_chain_data_residual >>
				IO_SCAN_CHAIN_32BIT_SHIFT;

			/*
			 * Construct TDI_TDO packet for remaining IO
			 * scan chain (2 bytes)
			 */
			tdi_tdo_header
				= TDI_TDO_HEADER_FIRST_BYTE |
				((io_scan_chain_data_residual - 1)
				<< TDI_TDO_HEADER_SECOND_BYTE_SHIFT);

			/*
			 * Program the last part of IO scan chain
			 * write TDI_TDO packet header (2 bytes) to
			 * scan manager
			 */
			writel(tdi_tdo_header,
				(SOCFPGA_SCANMGR_ADDRESS +
				SCANMGR_FIFODOUBLEBYTE_ADDRESS));

			DEBUG_MEMORY
			for (i = 0; i < io_program_iter; i++) {

				/*
				 * write remaining scan chain data into scan
				 * manager WFIFO with 4 bytes write
				*/
				writel(iocsr_scan_chain[index + i],
					(SOCFPGA_SCANMGR_ADDRESS +
					SCANMGR_FIFOQUADBYTE_ADDRESS));
			}

			index += io_program_iter;
			residual = io_scan_chain_data_residual &
				IO_SCAN_CHAIN_32BIT_MASK;

			if (IO_SCAN_CHAIN_PAYLOAD_24BIT < residual) {
				/*
				 * write the last 4B scan chain data
				 * into scan manager WFIFO
				 */
				DEBUG_MEMORY
				writel(iocsr_scan_chain[index],
					(SOCFPGA_SCANMGR_ADDRESS +
					SCANMGR_FIFOQUADBYTE_ADDRESS));

			} else {
				/*
				 * write the remaining 1 - 3 bytes scan chain
				 * data into scan manager WFIFO byte by byte
				 * to prevent JTAG engine shifting unused data
				 * from the FIFO and mistaken the data as a
				 * valid command (even though unused bits are
				 * set to 0, but just to prevent hardware
				 * glitch)
				 */
				DEBUG_MEMORY
				for (i = 0; i < residual; i += 8) {
					writel(
						((iocsr_scan_chain[index] >> i)
						& IO_SCAN_CHAIN_BYTE_MASK),
						(SOCFPGA_SCANMGR_ADDRESS +
						SCANMGR_FIFOSINGLEBYTE_ADDRESS)
						);

				}
			}

			/*
			 * Check if the scan chain engine has completed the
			 * IO scan chain data shifting
			 */
			if (SCAN_MGR_IO_SCAN_ENGINE_STATUS_IDLE
				!=  scan_mgr_io_scan_chain_engine_is_idle(
				MAX_WAITING_DELAY_IO_SCAN_ENGINE)) {
				DEBUG_MEMORY
				/* Disable IO Scan chain when error detected */
				clrbits_le32((SOCFPGA_SCANMGR_ADDRESS +
					SCANMGR_EN_ADDRESS),
					1 << io_scan_chain_id);
				return 1;
			}
		} /* if (io_scan_chain_data_residual) */

		/* Disable IO Scan chain when configuration done*/
		clrbits_le32((SOCFPGA_SCANMGR_ADDRESS + SCANMGR_EN_ADDRESS),
			1 << io_scan_chain_id);
	} else {
		/*
		 * IOCSR configuration failed as IO bank is not in
		 * frozen state
		 */
		return 1;
	} /* if-else (frozen) */
	return 0;
}
