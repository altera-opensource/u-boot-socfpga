#ifndef __SDRAM_CONFIG_H
#define __SDRAM_CONFIG_H

/* option to configure the DDR based on IOCSR instead from handoff */
#undef CONFIG_HPS_SDR_SKIP_HANDOFF

/* number of serr before interrupt triggered */
#define CONFIG_HPS_SDR_SERRCNT			(8)

/* DDR IO size: 0 for x16, 1 for x32 and 2 for x64 */
#define CONFIG_HPS_SDR_IO_SIZE			(1)

/* ECC enable: 1 for enabled */
#define CONFIG_HPS_SDR_ECC_EN			(0)

/* DDRCONF - Predefined DDR configuration. Mainly to optimize the DDR
transaction before reaching to DDR controller. Please refer CSR
for the value and its respective row, bank and column configuration */
#define CONFIG_HPS_SDR_DDRCONF			(0)

/* ACTTOACT - Minimum number of scheduler clock cycles between two
consecutive DRAM Activate commands on the same bank. */
#define CONFIG_HPS_SDR_DDRTIMING_ACTTOACT	(0x1C)

/* RDTOMISS - Minimum number of scheduler clock cycles between last DRAM read
command and a new Read or Write command in another page of the same bank. */
#define CONFIG_HPS_SDR_DDRTIMING_RDTOMISS	(0x13)

/* WRTOMISS - Minimum number of scheduler clock cycles between last DRAM write
command and a new Read or Write command in another page of the same bank. */
#define CONFIG_HPS_SDR_DDRTIMING_WRTOMISS	(0x21)

/* BURSTLEN - DRAM burst duration on the DRAM data bus in scheduler clock
cycles. Also equal to scheduler clock cycles between two DRAM commands. */
#define CONFIG_HPS_SDR_DDRTIMING_BURSTLEN	(0x2)

/* RDTOWR - Minimum number of scheduler clock cycles between the last DRAM
Read command and a Write command. */
#define CONFIG_HPS_SDR_DDRTIMING_RDTOWR		(0x1)

/* WRTORD - Minimum number of scheduler clock cycles between the last DRAM
Write command and a Read command. */
#define CONFIG_HPS_SDR_DDRTIMING_WRTORD		(0xB)

/* BWRATIO - Number of cycle minus 1 the DDR chip needs to process one
Generic socket word. */
#define CONFIG_HPS_SDR_DDRTIMING_BWRATIO	(0x0)

/* AUTOPRECHARGE - When set, pages are automatically closed after each access,
when clear, pages are left opened until an access in a different page occurs */
#define CONFIG_HPS_SDR_DDRMODE_AUTOPRECHARGE	(0x0)

/* BWRATIOEXTENDED - When set to 1, support for 4x Bwratio. */
#define CONFIG_HPS_SDR_DDRMODE_BWRATIOEXTENDED	(0x0)

/* READLATENCY - Maximun delay between read request and first data response. */
#define CONFIG_HPS_SDR_READLATENCY		(0x13)

/* RRD - Number of cycle between two consecutive Activate commands on different
Banks of the same device. */
#define CONFIG_HPS_SDR_ACTIVATE_RRD		(0x2)

/* FAW - Number of cycle of the FAW period. */
#define CONFIG_HPS_SDR_ACTIVATE_FAW		(0xD)

/* FAWBANK - Number of Bank of a given device involved in the FAW period. */
#define CONFIG_HPS_SDR_ACTIVATE_FAWBANK		(0x1)

/* BUSRDTORD - number of cycle between the last read data of a device and
the first read data of another device. */
#define CONFIG_HPS_SDR_DEVTODEV_BUSRDTORD	(0x1)

/* BUSRDTOWR - number of cycle between the last read data of a device and
the first write data of another device. */
#define CONFIG_HPS_SDR_DEVTODEV_BUSRDTOWR	(0x1)

/* BUSWRTORD - number of cycle between the last write data of a device and
the first read data of another device. */
#define CONFIG_HPS_SDR_DEVTODEV_BUSWRTORD	(0x1)


/* Firewall setup for DDR
 * This applicable for master when its not in secure state (as secure master
 * can access to entiree DDR). For non secure master, you would need to
 * specify the memory region that can be accessed.
 *
 * ENABLE - 1 for enable while 0 for disable
 * START & END - valid value 0 to 0xFFFF
 *		Example: if start=0, end=0, enable=1, the first 64kB region
 *		can be accessed by non secure master (either MPU, L3 or FPGA)
 */
#define CONFIG_HPS_SDR_MPU0_ENABLE	(1)
#define CONFIG_HPS_SDR_MPU0_START	(0)
#define CONFIG_HPS_SDR_MPU0_END		(0xFFFF)

#define CONFIG_HPS_SDR_MPU1_ENABLE	(0)
#define CONFIG_HPS_SDR_MPU1_START	(0)
#define CONFIG_HPS_SDR_MPU1_END		(0)

#define CONFIG_HPS_SDR_MPU2_ENABLE	(0)
#define CONFIG_HPS_SDR_MPU2_START	(0)
#define CONFIG_HPS_SDR_MPU2_END		(0)

#define CONFIG_HPS_SDR_MPU3_ENABLE	(0)
#define CONFIG_HPS_SDR_MPU3_START	(0)
#define CONFIG_HPS_SDR_MPU3_END		(0)

#define CONFIG_HPS_SDR_HPS_L3_0_ENABLE	(1)
#define CONFIG_HPS_SDR_HPS_L3_0_START	(0)
#define CONFIG_HPS_SDR_HPS_L3_0_END	(0xFFFF)

#define CONFIG_HPS_SDR_HPS_L3_1_ENABLE	(0)
#define CONFIG_HPS_SDR_HPS_L3_1_START	(0)
#define CONFIG_HPS_SDR_HPS_L3_1_END	(0)

#define CONFIG_HPS_SDR_HPS_L3_2_ENABLE	(0)
#define CONFIG_HPS_SDR_HPS_L3_2_START	(0)
#define CONFIG_HPS_SDR_HPS_L3_2_END	(0)

#define CONFIG_HPS_SDR_HPS_L3_3_ENABLE	(0)
#define CONFIG_HPS_SDR_HPS_L3_3_START	(0)
#define CONFIG_HPS_SDR_HPS_L3_3_END	(0)

#define CONFIG_HPS_SDR_HPS_L3_4_ENABLE	(0)
#define CONFIG_HPS_SDR_HPS_L3_4_START	(0)
#define CONFIG_HPS_SDR_HPS_L3_4_END	(0)

#define CONFIG_HPS_SDR_HPS_L3_5_ENABLE	(0)
#define CONFIG_HPS_SDR_HPS_L3_5_START	(0)
#define CONFIG_HPS_SDR_HPS_L3_5_END	(0)

#define CONFIG_HPS_SDR_HPS_L3_6_ENABLE	(0)
#define CONFIG_HPS_SDR_HPS_L3_6_START	(0)
#define CONFIG_HPS_SDR_HPS_L3_6_END	(0)

#define CONFIG_HPS_SDR_HPS_L3_7_ENABLE	(0)
#define CONFIG_HPS_SDR_HPS_L3_7_START	(0)
#define CONFIG_HPS_SDR_HPS_L3_7_END	(0)

#define CONFIG_HPS_SDR_FPGA2SDRAM0_0_ENABLE	(1)
#define CONFIG_HPS_SDR_FPGA2SDRAM0_0_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM0_0_END	(0xFFFF)

#define CONFIG_HPS_SDR_FPGA2SDRAM0_1_ENABLE	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM0_1_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM0_1_END	(0)

#define CONFIG_HPS_SDR_FPGA2SDRAM0_2_ENABLE	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM0_2_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM0_2_END	(0)

#define CONFIG_HPS_SDR_FPGA2SDRAM0_3_ENABLE	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM0_3_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM0_3_END	(0)

#define CONFIG_HPS_SDR_FPGA2SDRAM1_0_ENABLE	(1)
#define CONFIG_HPS_SDR_FPGA2SDRAM1_0_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM1_0_END	(0xFFFF)

#define CONFIG_HPS_SDR_FPGA2SDRAM1_1_ENABLE	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM1_1_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM1_1_END	(0)

#define CONFIG_HPS_SDR_FPGA2SDRAM1_2_ENABLE	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM1_2_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM1_2_END	(0)

#define CONFIG_HPS_SDR_FPGA2SDRAM1_3_ENABLE	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM1_3_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM1_3_END	(0)

#define CONFIG_HPS_SDR_FPGA2SDRAM2_0_ENABLE	(1)
#define CONFIG_HPS_SDR_FPGA2SDRAM2_0_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM2_0_END	(0xFFFF)

#define CONFIG_HPS_SDR_FPGA2SDRAM2_1_ENABLE	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM2_1_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM2_1_END	(0)

#define CONFIG_HPS_SDR_FPGA2SDRAM2_2_ENABLE	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM2_2_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM2_2_END	(0)

#define CONFIG_HPS_SDR_FPGA2SDRAM2_3_ENABLE	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM2_3_START	(0)
#define CONFIG_HPS_SDR_FPGA2SDRAM2_3_END	(0)

#endif	/*#ifndef__SDRAM_CONFIG_H*/
