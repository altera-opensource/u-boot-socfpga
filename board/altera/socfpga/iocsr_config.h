#ifndef _PRELOADER_IOCSR_CONFIG_H_
#define _PRELOADER_IOCSR_CONFIG_H_

#define CONFIG_HPS_IOCSR_SCANCHAIN0_LENGTH        (764)
#define CONFIG_HPS_IOCSR_SCANCHAIN1_LENGTH        (1719)
#define CONFIG_HPS_IOCSR_SCANCHAIN2_LENGTH        (955)
#define CONFIG_HPS_IOCSR_SCANCHAIN3_LENGTH        (16766)

extern const unsigned long iocsr_scan_chain0_table[
	((CONFIG_HPS_IOCSR_SCANCHAIN0_LENGTH / 32) + 1)];
extern const unsigned long iocsr_scan_chain1_table[
	((CONFIG_HPS_IOCSR_SCANCHAIN1_LENGTH / 32) + 1)];
extern const unsigned long iocsr_scan_chain2_table[
	((CONFIG_HPS_IOCSR_SCANCHAIN2_LENGTH / 32) + 1)];
extern const unsigned long iocsr_scan_chain3_table[
	((CONFIG_HPS_IOCSR_SCANCHAIN3_LENGTH / 32) + 1)];


#endif /*_PRELOADER_IOCSR_CONFIG_H_*/
