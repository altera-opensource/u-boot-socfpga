/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __INTEL_SMC_H
#define __INTEL_SMC_H


#include <linux/arm-smccc.h>
#ifndef __ASSEMBLY__
#include <linux/bitops.h>
#endif

/*
 * This file defines the Secure Monitor Call (SMC) message protocol used for
 * service driver in normal world (EL1) to communicate with bootloader
 * in Secure Monitor Exception level 3 (EL3)
 *
 * An ARM SMC instruction takes a function identifier and up to 6 64-bit
 * register values as arguments, and can return up to 4 64-bit register
 * value. The operation of the secure monitor is determined by the parameter
 * values passed in through registers.

 * EL1 and EL3 communicates pointer as physical address rather than the
 * virtual address
 */


/*
 * Functions specified by ARM SMC Calling convention
 *
 * FAST call executes atomic operations, returns when the requested operation
 * has completed
 * STD call starts a operation which can be preempted by a non-secure
 * interrupt. The call can return before the requested operation has
 * completed
 *
 * a0..a7 is used as register names in the descriptions below, on arm32
 * that translates to r0..r7 and on arm64 to w0..w7
 */

#define INTEL_SIP_SMC_STD_CALL_VAL(func_num) \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_STD_CALL, ARM_SMCCC_SMC_64, \
	ARM_SMCCC_OWNER_SIP, (func_num))

#define INTEL_SIP_SMC_FAST_CALL_VAL(func_num) \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64, \
	ARM_SMCCC_OWNER_SIP, (func_num))


/*
 * Return values in INTEL_SIP_SMC_* call
 *
 * INTEL_SIP_SMC_RETURN_UNKNOWN_FUNCTION:
 *		Secure world does not recognize this request
 *
 * INTEL_SIP_SMC_STATUS_OK:
 *		FPGA configuration completed successfully,
 *		In case of FPGA configuration write operation, it
 *		means bootloader can accept the next chunk of FPGA
 *		configuration data
 *
 * INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY:
 *		In case of FPGA configuration write operation, it means
 *		bootloader is still processing previous data & can't accept
 *		the next chunk of data. Service driver needs to issue
 *		INTEL_SIP_SMC_FPGA_CONFIG_COMPLETED_WRITE call to query the
 *		completed block(s)
 *
 * INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR:
 *		There is error during the FPGA configuration process
 */
#define INTEL_SIP_SMC_RETURN_UNKNOWN_FUNCTION		0xFFFFFFFF
#define INTEL_SIP_SMC_STATUS_OK				0x0
#define INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY		0x1
#define INTEL_SIP_SMC_FPGA_CONFIG_STATUS_REJECTED	0x2
#define INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR		0x4

/*
 * a0..a7 is used as register names in the descriptions below, on arm32
 * that translates to r0..r7 and on arm64 to w0..w7.
 */


/*
 * Request INTEL_SIP_SMC_FPGA_CONFIG_START
 *
 * Sync call used by service driver at EL1 to request the FPGA in EL3 to
 * be prepare to receive a new configuration
 *
 * Call register usage:
 * a0 INTEL_SIP_SMC_FPGA_CONFIG_START
 * a1-7 not used
 *
 * Return status
 * a0 INTEL_SIP_SMC_STATUS_OK indicates secure world is ready for
 *	FPGA configuration, or negative value for failure
 * a1-3: not used
 */
#define INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_START 1
#define INTEL_SIP_SMC_FPGA_CONFIG_START \
	INTEL_SIP_SMC_FAST_CALL_VAL(INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_START)


/*
 * Request INTEL_SIP_SMC_FPGA_CONFIG_WRITE
 *
 * Async call used by service driver at EL1 to provide FPGA configuration data
 * to secure world
 *
 * Call register usage:
 * a0 INTEL_SIP_SMC_FPGA_CONFIG_WRITE
 * a1 64bit physical address of the configuration data memory block
 * a2 Size of configuration data block
 * a3-7 not used
 *
 * Return status
 * a0 INTEL_SIP_SMC_STATUS_OK
 * a1 64bit physical address of 1st completed memory block if
 *    any completed block, otherwise zero value
 * a2 64bit physical address of 2nd completed memory block if
 *    any completed block, otherwise zero value
 * a3 64bit physical address of 3rd completed memory block if
 *    any completed block, otherwise zero value
 *
 * Or
 * a0 INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY
 * a1-3 not used
 *
 * Or
 * a0 INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR
 * a1-3 not used
*/
#define INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_WRITE 2
#define INTEL_SIP_SMC_FPGA_CONFIG_WRITE \
	INTEL_SIP_SMC_STD_CALL_VAL(INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_WRITE)


/*
* Request INTEL_SIP_SMC_FPGA_CONFIG_COMPLETED_WRITE
*
* Sync call used by service driver at EL1 to track the completed write
* transactions. This request is called after INTEL_SIP_SMC_FPGA_CONFIG_WRITE
* call with INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY
*
* Call register usage:
* a0 INTEL_SIP_SMC_FPGA_CONFIG_COMPLETED_WRITE
* a1-7 not used
*
* Return status
* a0 INTEL_SIP_SMC_STATUS_OK
* a1 64bit physical address of 1st completed memory block
* a2 64bit physical address of 2nd completed memory block if
*    any completed block, otherwise zero value
* a3 64bit physical address of 3rd completed memory block if
*    any completed block, otherwise zero value
*
* Or
* a0 INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY
* a1-3 not used
*
* Or
* a0 INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR
* a1-3 not used
*/
#define INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_COMPLETED_WRITE 3
#define INTEL_SIP_SMC_FPGA_CONFIG_COMPLETED_WRITE \
	INTEL_SIP_SMC_FAST_CALL_VAL( \
		INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_COMPLETED_WRITE)


/*
* Request INTEL_SIP_SMC_FPGA_CONFIG_ISDONE
*
* Sync call used by service driver at EL1 to inform secure world that all
* data are sent, to check whether or not the secure world completes
* the FPGA configuration process
*
* Call register usage:
* a0 INTEL_SIP_SMC_FPGA_CONFIG_ISDONE
* a1-7 not used
*
* Return status
* a0 INTEL_SIP_SMC_STATUS_OK for FPGA configuration process completed
*    successfully
*
*    or
*	 INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY for FPGA configuration is still
*	 in process
*
*    or
*	 INTEL_SIP_SMC_FPGA_CONFIG_STATUS_ERROR for error encountered during
*	 FPGA configuration
*
* a1-3: not used
*/
#define INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_ISDONE 4
#define INTEL_SIP_SMC_FPGA_CONFIG_ISDONE \
	INTEL_SIP_SMC_FAST_CALL_VAL(INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_ISDONE)



/*
* Request INTEL_SIP_SMC_FPGA_CONFIG_GET_MEM
*
* Sync call used by service driver at EL1 to query the physical address of
* memory block reserved by bootloader
*
* Call register usage:
* a0 INTEL_SIP_SMC_FPGA_CONFIG_GET_MEM
* a1-7 not used
*
* Return status
* a0 INTEL_SIP_SMC_STATUS_OK
* a1: start of physical address of reserved memory block
* a2: size of reserved memory block
* a3: not used
*/
#define INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_GET_MEM 5
#define INTEL_SIP_SMC_FPGA_CONFIG_GET_MEM \
	INTEL_SIP_SMC_FAST_CALL_VAL(INTEL_SIP_SMC_FUNCID_FPGA_CONFIG_GET_MEM)
#endif
