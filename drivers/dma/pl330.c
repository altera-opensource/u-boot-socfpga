/*
 * Copyright (C) 2013 Altera Corporation <www.altera.com>
 *
 * Copyright (C) 2010 Samsung Electronics Co Ltd.
 *	Jaswinder Singh <jassi.brar@samsung.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <common.h>
#include <asm/io.h>
#include <pl330.h>

/* To enabled the printout of debug message and microcode command setup */
#ifdef DEBUG
#define PL330_DEBUG_MCGEN
#endif

/* Register and Bit field Definitions */

/* DMA Status */
#define DS			0x0
#define DS_ST_STOP		0x0
#define DS_ST_EXEC		0x1
#define DS_ST_CMISS		0x2
#define DS_ST_UPDTPC		0x3
#define DS_ST_WFE		0x4
#define DS_ST_ATBRR		0x5
#define DS_ST_QBUSY		0x6
#define DS_ST_WFP		0x7
#define DS_ST_KILL		0x8
#define DS_ST_CMPLT		0x9
#define DS_ST_FLTCMP		0xe
#define DS_ST_FAULT		0xf

/* DMA Program Count register */
#define DPC			0x4
/* Interrupt Enable register */
#define INTEN			0x20
/* event-Interrupt Raw Status register */
#define ES			0x24
/* Interrupt Status register */
#define INTSTATUS		0x28
/* Interrupt Clear register */
#define INTCLR			0x2c
/* Fault Status DMA Manager register */
#define FSM			0x30
/* Fault Status DMA Channel register */
#define FSC			0x34
/* Fault Type DMA Manager register */
#define FTM			0x38

/* Fault Type DMA Channel register */
#define _FTC			0x40
#define FTC(n)			(_FTC + (n)*0x4)

/* Channel Status register */
#define _CS			0x100
#define CS(n)			(_CS + (n)*0x8)
#define CS_CNS			(1 << 21)

/* Channel Program Counter register */
#define _CPC			0x104
#define CPC(n)			(_CPC + (n)*0x8)

/* Source Address register */
#define _SA			0x400
#define SA(n)			(_SA + (n)*0x20)

/* Destination Address register */
#define _DA			0x404
#define DA(n)			(_DA + (n)*0x20)

/* Channel Control register */
#define _CC			0x408
#define CC(n)			(_CC + (n)*0x20)

/* Channel Control register (CCR) Setting */
#define CC_SRCINC		(1 << 0)
#define CC_DSTINC		(1 << 14)
#define CC_SRCPRI		(1 << 8)
#define CC_DSTPRI		(1 << 22)
#define CC_SRCNS		(1 << 9)
#define CC_DSTNS		(1 << 23)
#define CC_SRCIA		(1 << 10)
#define CC_DSTIA		(1 << 24)
#define CC_SRCBRSTLEN_SHFT	4
#define CC_DSTBRSTLEN_SHFT	18
#define CC_SRCBRSTSIZE_SHFT	1
#define CC_DSTBRSTSIZE_SHFT	15
#define CC_SRCCCTRL_SHFT	11
#define CC_SRCCCTRL_MASK	0x7
#define CC_DSTCCTRL_SHFT	25
#define CC_DRCCCTRL_MASK	0x7
#define CC_SWAP_SHFT		28

/* Loop Counter 0 register */
#define _LC0			0x40c
#define LC0(n)			(_LC0 + (n)*0x20)

/* Loop Counter 1 register */
#define _LC1			0x410
#define LC1(n)			(_LC1 + (n)*0x20)

/* Debug Status register */
#define DBGSTATUS		0xd00
#define DBG_BUSY		(1 << 0)

/* Debug Command register */
#define DBGCMD			0xd04
/* Debug Instruction 0 register */
#define DBGINST0		0xd08
/* Debug Instruction 1 register */
#define DBGINST1		0xd0c

/* Configuration register */
#define CR0			0xe00
#define CR1			0xe04
#define CR2			0xe08
#define CR3			0xe0c
#define CR4			0xe10
#define CRD			0xe14

/* Peripheral Identification register */
#define PERIPH_ID		0xfe0
/* Component Identification register */
#define PCELL_ID		0xff0

/* Configuration register value */
#define CR0_PERIPH_REQ_SET	(1 << 0)
#define CR0_BOOT_EN_SET		(1 << 1)
#define CR0_BOOT_MAN_NS		(1 << 2)
#define CR0_NUM_CHANS_SHIFT	4
#define CR0_NUM_CHANS_MASK	0x7
#define CR0_NUM_PERIPH_SHIFT	12
#define CR0_NUM_PERIPH_MASK	0x1f
#define CR0_NUM_EVENTS_SHIFT	17
#define CR0_NUM_EVENTS_MASK	0x1f

/* Configuration register value */
#define CR1_ICACHE_LEN_SHIFT		0
#define CR1_ICACHE_LEN_MASK		0x7
#define CR1_NUM_ICACHELINES_SHIFT	4
#define CR1_NUM_ICACHELINES_MASK	0xf

/* Configuration register value */
#define CRD_DATA_WIDTH_SHIFT	0
#define CRD_DATA_WIDTH_MASK	0x7
#define CRD_WR_CAP_SHIFT	4
#define CRD_WR_CAP_MASK		0x7
#define CRD_WR_Q_DEP_SHIFT	8
#define CRD_WR_Q_DEP_MASK	0xf
#define CRD_RD_CAP_SHIFT	12
#define CRD_RD_CAP_MASK		0x7
#define CRD_RD_Q_DEP_SHIFT	16
#define CRD_RD_Q_DEP_MASK	0xf
#define CRD_DATA_BUFF_SHIFT	20
#define CRD_DATA_BUFF_MASK	0x3ff

/* Microcode opcode value */
#define CMD_DMAADDH		0x54
#define CMD_DMAEND		0x00
#define CMD_DMAFLUSHP		0x35
#define CMD_DMAGO		0xa0
#define CMD_DMALD		0x04
#define CMD_DMALDP		0x25
#define CMD_DMALP		0x20
#define CMD_DMALPEND		0x28
#define CMD_DMAKILL		0x01
#define CMD_DMAMOV		0xbc
#define CMD_DMANOP		0x18
#define CMD_DMARMB		0x12
#define CMD_DMASEV		0x34
#define CMD_DMAST		0x08
#define CMD_DMASTP		0x29
#define CMD_DMASTZ		0x0c
#define CMD_DMAWFE		0x36
#define CMD_DMAWFP		0x30
#define CMD_DMAWMB		0x13

/* the size of opcode plus opcode required settings */
#define SZ_DMAADDH		3
#define SZ_DMAEND		1
#define SZ_DMAFLUSHP		2
#define SZ_DMALD		1
#define SZ_DMALDP		2
#define SZ_DMALP		2
#define SZ_DMALPEND		2
#define SZ_DMAKILL		1
#define SZ_DMAMOV		6
#define SZ_DMANOP		1
#define SZ_DMARMB		1
#define SZ_DMASEV		2
#define SZ_DMAST		1
#define SZ_DMASTP		2
#define SZ_DMASTZ		1
#define SZ_DMAWFE		2
#define SZ_DMAWFP		2
#define SZ_DMAWMB		1
#define SZ_DMAGO		6

/* Use this _only_ to wait on transient states */
#define UNTIL(u, t, s)	while (!(pl330_getstate(u, t) & (s)));

static unsigned cmd_line;

/* debug message printout */
#ifdef PL330_DEBUG_MCGEN
#define PL330_DBGCMD_DUMP(off, x...)	do { \
						printf("%x:", cmd_line); \
						printf(x); \
						cmd_line += off; \
					} while (0)
#else
#define PL330_DBGCMD_DUMP(off, x...)	do {} while (0)
#endif

/* Enum declaration */
enum dmamov_dst {
	SAR = 0,
	CCR,
	DAR,
};

enum pl330_dst {
	SRC = 0,
	DST,
};

enum pl330_cond {
	SINGLE,
	BURST,
	ALWAYS,
};

/* Structure will be used by _emit_LPEND function */
struct _arg_LPEND {
	enum pl330_cond cond;
	int forever;
	unsigned loop;
	u8 bjump;
};

/* Structure will be used by _emit_GO function */
struct _arg_GO {
	u8 chan;
	u32 addr;
	unsigned ns;
};

/*
 * Function:	add opcode DMAEND into microcode (end)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 */
static inline u32 _emit_END(u8 buf[])
{
	buf[0] = CMD_DMAEND;
	PL330_DBGCMD_DUMP(SZ_DMAEND, "\tDMAEND\n");
	return SZ_DMAEND;
}

/*
 * Function:	add opcode DMAFLUSHP into microcode (flush peripheral)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		peri -> peripheral ID as listed in DMA NPP
 */
static inline u32 _emit_FLUSHP(u8 buf[], u8 peri)
{
	buf[0] = CMD_DMAFLUSHP;
	peri &= 0x1f;
	peri <<= 3;
	buf[1] = peri;
	PL330_DBGCMD_DUMP(SZ_DMAFLUSHP, "\tDMAFLUSHP %u\n", peri >> 3);
	return SZ_DMAFLUSHP;
}

/*
 * Function:	add opcode DMALD into microcode (load)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		cond -> execution criteria such as single, burst or always
 */
static inline u32 _emit_LD(u8 buf[], enum pl330_cond cond)
{
	buf[0] = CMD_DMALD;
	if (cond == SINGLE)
		buf[0] |= (0 << 1) | (1 << 0);
	else if (cond == BURST)
		buf[0] |= (1 << 1) | (1 << 0);
	PL330_DBGCMD_DUMP(SZ_DMALD, "\tDMALD%c\n",
		cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'A'));
	return SZ_DMALD;
}

/*
 * Function:	add opcode DMALP into microcode (loop)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		loop -> selection of using counter 0 or 1  (valid value 0 or 1)
 *		cnt -> number of iteration (valid value 1-256)
 */
static inline u32 _emit_LP(u8 buf[], unsigned loop, u8 cnt)
{
	buf[0] = CMD_DMALP;
	if (loop)
		buf[0] |= (1 << 1);
	/* Decrements by 1 will be done by DMAC assembler. But at here, this
	will be done by driver here*/
	cnt--;
	buf[1] = cnt;
	PL330_DBGCMD_DUMP(SZ_DMALP, "\tDMALP_%c %u\n", loop ? '1' : '0', cnt);
	return SZ_DMALP;
}

/*
 * Function:	add opcode DMALPEND into microcode (loop end)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		arg -> structure _arg_LPEND which contain all needed info
 *		arg->cond -> execution criteria such as single, burst or always
 *		arg->forever -> forever loop? used if DMALPFE started the loop
 *		arg->bjump -> backwards jump (relative location of
 *			      1st instruction in the loop
 */
static inline u32 _emit_LPEND(u8 buf[],	const struct _arg_LPEND *arg)
{
	enum pl330_cond cond = arg->cond;
	int forever = arg->forever;
	unsigned loop = arg->loop;
	u8 bjump = arg->bjump;
	buf[0] = CMD_DMALPEND;
	if (loop)
		buf[0] |= (1 << 2);
	if (!forever)
		buf[0] |= (1 << 4);
	if (cond == SINGLE)
		buf[0] |= (0 << 1) | (1 << 0);
	else if (cond == BURST)
		buf[0] |= (1 << 1) | (1 << 0);
	buf[1] = bjump;
	PL330_DBGCMD_DUMP(SZ_DMALPEND, "\tDMALP%s%c_%c bjmpto_%x\n",
			forever ? "FE" : "END",
			cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'A'),
			loop ? '1' : '0',
			bjump);
	return SZ_DMALPEND;
}

/*
 * Function:	add opcode DMAKILL into microcode (kill)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 */
static inline u32 _emit_KILL(u8 buf[])
{
	buf[0] = CMD_DMAKILL;
	return SZ_DMAKILL;
}

/*
 * Function:	add opcode DMAMOV into microcode (move)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		dst -> destination register (valid value SAR, DAR or CCR)
 *		val -> 32bit value that to be written into destination register
 */
static inline u32 _emit_MOV(u8 buf[],
		enum dmamov_dst dst, u32 val)
{
	buf[0] = CMD_DMAMOV;
	buf[1] = dst;
	buf[2] = val & 0xFF;
	buf[3] = (val >> 8) & 0xFF;
	buf[4] = (val >> 16) & 0xFF;
	buf[5] = (val >> 24) & 0xFF;
	PL330_DBGCMD_DUMP(SZ_DMAMOV, "\tDMAMOV %s 0x%x\n",
		dst == SAR ? "SAR" : (dst == DAR ? "DAR" : "CCR"), val);
	return SZ_DMAMOV;
}

/*
 * Function:	add opcode DMANOP into microcode  (no operation)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 */
static inline u32 _emit_NOP(u8 buf[])
{
	buf[0] = CMD_DMANOP;
	PL330_DBGCMD_DUMP(SZ_DMANOP, "\tDMANOP\n");
	return SZ_DMANOP;
}

/*
 * Function:	add opcode DMARMB into microcode  (read memory barrier)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 */
static inline u32 _emit_RMB(u8 buf[])
{
	buf[0] = CMD_DMARMB;
	PL330_DBGCMD_DUMP(SZ_DMARMB, "\tDMARMB\n");
	return SZ_DMARMB;
}

/*
 * Function:	add opcode DMASEV into microcode  (send event)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		ev -> the event number (valid 0 - 31)
 */
static inline u32 _emit_SEV(u8 buf[], u8 ev)
{
	buf[0] = CMD_DMASEV;
	ev &= 0x1f;
	ev <<= 3;
	buf[1] = ev;
	PL330_DBGCMD_DUMP(SZ_DMASEV, "\tDMASEV %u\n", ev >> 3);
	return SZ_DMASEV;
}

/*
 * Function:	add opcode DMAST into microcode (store)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		cond -> execution criteria such as single, burst or always
 */
static inline u32 _emit_ST(u8 buf[], enum pl330_cond cond)
{
	buf[0] = CMD_DMAST;
	if (cond == SINGLE)
		buf[0] |= (0 << 1) | (1 << 0);
	else if (cond == BURST)
		buf[0] |= (1 << 1) | (1 << 0);
	PL330_DBGCMD_DUMP(SZ_DMAST, "\tDMAST%c\n",
		cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'A'));
	return SZ_DMAST;
}

/*
 * Function:	add opcode DMASTP into microcode (store and notify peripheral)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		cond -> execution criteria such as single, burst or always
 *		peri -> peripheral ID as listed in DMA NPP
 */
static inline u32 _emit_STP(u8 buf[], enum pl330_cond cond, u8 peri)
{
	buf[0] = CMD_DMASTP;
	if (cond == BURST)
		buf[0] |= (1 << 1);
	peri &= 0x1f;
	peri <<= 3;
	buf[1] = peri;
	PL330_DBGCMD_DUMP(SZ_DMASTP, "\tDMASTP%c %u\n",
		cond == SINGLE ? 'S' : 'B', peri >> 3);
	return SZ_DMASTP;
}

/*
 * Function:	add opcode DMASTZ into microcode (store zeroes)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 */
static inline u32 _emit_STZ(u8 buf[])
{
	buf[0] = CMD_DMASTZ;
	PL330_DBGCMD_DUMP(SZ_DMASTZ, "\tDMASTZ\n");
	return SZ_DMASTZ;
}

/*
 * Function:	add opcode DMAWFP into microcode (wait for peripheral)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		cond -> execution criteria such as single, burst or always
 *		peri -> peripheral ID as listed in DMA NPP
 */
static inline u32 _emit_WFP(u8 buf[], enum pl330_cond cond, u8 peri)
{
	buf[0] = CMD_DMAWFP;
	if (cond == SINGLE)
		buf[0] |= (0 << 1) | (0 << 0);
	else if (cond == BURST)
		buf[0] |= (1 << 1) | (0 << 0);
	else
		buf[0] |= (0 << 1) | (1 << 0);
	peri &= 0x1f;
	peri <<= 3;
	buf[1] = peri;
	PL330_DBGCMD_DUMP(SZ_DMAWFP, "\tDMAWFP%c %u\n",
		cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'P'), peri >> 3);
	return SZ_DMAWFP;
}

/*
 * Function:	add opcode DMAWMB into microcode  (write memory barrier)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 */
static inline u32 _emit_WMB(u8 buf[])
{
	buf[0] = CMD_DMAWMB;
	PL330_DBGCMD_DUMP(SZ_DMAWMB, "\tDMAWMB\n");
	return SZ_DMAWMB;
}

/*
 * Function:	add opcode DMALGO into microcode (go)
 * Return:	size of opcode
 * Parameter:	buf -> the buffer which stored the microcode program
 *		arg -> structure _arg_GO which contain all needed info
 *		arg->chan -> channel number
 *		arg->addr -> start address of the microcode program
 *			     which will be wrote into CPC register
 *		arg->ns -> 1 for non secure, 0 for secure
 *			   (if only DMA Manager is in secure)
 */
static inline u32 _emit_GO(u8 buf[], const struct _arg_GO *arg)
{
	u8 chan = arg->chan;
	u32 addr = arg->addr;
	unsigned ns = arg->ns;
	buf[0] = CMD_DMAGO;
	buf[0] |= (ns << 1);
	buf[1] = chan & 0x7;
	buf[2] = addr & 0xFF;
	buf[3] = (addr >> 8) & 0xFF;
	buf[4] = (addr >> 16) & 0xFF;
	buf[5] = (addr >> 24) & 0xFF;
	return SZ_DMAGO;
}

/*
 * Function:	Populate the CCR register
 * Parameter:	rqc -> Request Configuration.
 */
static inline u32 _prepare_ccr(const struct pl330_reqcfg *rqc)
{
	u32 ccr = 0;

	if (rqc->src_inc)
		ccr |= CC_SRCINC;
	if (rqc->dst_inc)
		ccr |= CC_DSTINC;

	/* We set same protection levels for Src and DST for now */
	if (rqc->privileged)
		ccr |= CC_SRCPRI | CC_DSTPRI;
	if (rqc->nonsecure)
		ccr |= CC_SRCNS | CC_DSTNS;
	if (rqc->insnaccess)
		ccr |= CC_SRCIA | CC_DSTIA;

	ccr |= (((rqc->brst_len - 1) & 0xf) << CC_SRCBRSTLEN_SHFT);
	ccr |= (((rqc->brst_len - 1) & 0xf) << CC_DSTBRSTLEN_SHFT);

	ccr |= (rqc->brst_size << CC_SRCBRSTSIZE_SHFT);
	ccr |= (rqc->brst_size << CC_DSTBRSTSIZE_SHFT);

	ccr |= (rqc->scctl << CC_SRCCCTRL_SHFT);
	ccr |= (rqc->dcctl << CC_DSTCCTRL_SHFT);

	ccr |= (rqc->swap << CC_SWAP_SHFT);
	return ccr;
}

/*
 * Function:	wait until DMA Manager is idle
 * Return:	1 = error / timeout ocurred before idle
 * Parameter:	loop -> number of loop before timeout ocurred
 */
static int pl330_until_dmac_idle(int loops)
{
	do {
		/* Until Manager is Idle */
		if (!(readl(PL330_DMA_BASE + DBGSTATUS) & DBG_BUSY))
			break;
	} while (--loops);
	if (!loops)
		return 1;
	return 0;
}

/*
 * Function:	execute debug instruction such as DMAGO and DMAKILL
 * Parameter:	insn -> the buffer which stored the debug instruction
 *		as_manager -> 1 = debug thread is encoding as DMA manager thread
 *		channel_num -> the channel number (only if as_manager=0)
 *		timeout_loops -> number of loop before timeout ocurred
 */
static inline void pl330_execute_DBGINSN(u8 insn[], int as_manager,
	int channel_num, int timeout_loops)
{
	u32 val;
	val = (insn[0] << 16) | (insn[1] << 24);
	if (!as_manager)
		val |= (1 << 0);
	val |= (channel_num << 8); /* Channel Number */
	writel(val, PL330_DMA_BASE + DBGINST0);
	val = insn[2];
	val = val | (insn[3] << 8);
	val = val | (insn[4] << 16);
	val = val | (insn[5] << 24);
	writel(val, PL330_DMA_BASE + DBGINST1);

	/* If timed out due to halted state-machine */
	if (pl330_until_dmac_idle(timeout_loops)) {
		printf("DMAC halted!\n");
		return;
	}
	/* Get going */
	writel(0, PL330_DMA_BASE + DBGCMD);
}

/*
 * Function:	Get the status of current channel or manager
 * Parameter:	channel0_manager1 -> 0 for channel and 1 for manager
 *		channel_num -> the channel number (only if as_manager=0)
 */
static inline u32 pl330_getstate(int channel0_manager1, int channel_num)
{
	u32 val;
	if (channel0_manager1)
		val = readl(PL330_DMA_BASE + DS) & 0xf;
	else
		val = readl(PL330_DMA_BASE + CS(channel_num)) & 0xf;
	switch (val) {
	case DS_ST_STOP:
		return PL330_STATE_STOPPED;
	case DS_ST_EXEC:
		return PL330_STATE_EXECUTING;
	case DS_ST_CMISS:
		return PL330_STATE_CACHEMISS;
	case DS_ST_UPDTPC:
		return PL330_STATE_UPDTPC;
	case DS_ST_WFE:
		return PL330_STATE_WFE;
	case DS_ST_FAULT:
		return PL330_STATE_FAULTING;
	case DS_ST_ATBRR:
		if (channel0_manager1)
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_ATBARRIER;
	case DS_ST_QBUSY:
		if (channel0_manager1)
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_QUEUEBUSY;
	case DS_ST_WFP:
		if (channel0_manager1)
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_WFP;
	case DS_ST_KILL:
		if (channel0_manager1)
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_KILLING;
	case DS_ST_CMPLT:
		if (channel0_manager1)
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_COMPLETING;
	case DS_ST_FLTCMP:
		if (channel0_manager1)
			return PL330_STATE_INVALID;
		else
			return PL330_STATE_FAULT_COMPLETING;
	default:
		return PL330_STATE_INVALID;
	}
}

/* Function:	Execute the DMAGO through debug instruction
 * Return:	1 for error as DMA is not ready
 * Parameter:	channel_num -> the channel number
 *		buffer_address -> start address of the microcode program
 *				  which will be wrote into CPC register
 *		timeout_loops -> number of loop before timeout ocurred
 */
static int pl330_trigger(int channel_num, u8 *buffer_address, int timeout_loops)
{
	u8 insn[6] = {0, 0, 0, 0, 0, 0};
	struct _arg_GO go;
	int channel0_manager1;

	/*
	 * Return if already ACTIVE. It will ensure DMAGO is executed at
	 * STOPPED state too
	 */
	channel0_manager1 = 0;
	if (pl330_getstate(channel0_manager1, channel_num) !=
		PL330_STATE_STOPPED)
		return 1;

	go.chan = channel_num;
	go.addr = (u32)buffer_address;

	if (PL330_DMA_BASE == PL330_DMA_BASE_NON_SECURE)
		go.ns = 1;	/* non-secure condition */
	else
		go.ns = 0;	/* secure condition */
	_emit_GO(insn, &go);

	/* Only manager can execute GO */
	channel0_manager1 = 1;
	pl330_execute_DBGINSN(insn, channel0_manager1, channel_num,
		timeout_loops);
	return 0;
}

/*
 * Function:	Execute the command list through DMAGO and debug instruction
 * Return:	1 for error where DMA is not ready
 * Parameter:	channel0_manager1 -> 0 for channel and 1 for manager
 *		channel_num -> the channel number
 *		buffer_address -> start address of the microcode program which
 *				  will be wrote into CPC register
 *		timeout_loops -> number of loop before timeout ocurred
 */
static int pl330_start(int channel0_manager1, int channel_num,
	u8 *buffer_address, int timeout_loops)
{
#ifdef PL330_DEBUG_MCGEN
	puts("INFO: DMA BASE Address = ");
	printf("0x%08x\n", PL330_DMA_BASE);
#endif
	switch (pl330_getstate(channel0_manager1, channel_num)) {
	case PL330_STATE_FAULT_COMPLETING:
		UNTIL(channel0_manager1, channel_num,
			PL330_STATE_FAULTING | PL330_STATE_KILLING);

		if (pl330_getstate(channel0_manager1, channel_num) ==
			PL330_STATE_KILLING)
			UNTIL(channel0_manager1, channel_num,
				PL330_STATE_STOPPED);

	case PL330_STATE_FAULTING:
		pl330_stop(channel0_manager1, channel_num, timeout_loops);

	case PL330_STATE_KILLING:
	case PL330_STATE_COMPLETING:
		UNTIL(channel0_manager1, channel_num, PL330_STATE_STOPPED);

	case PL330_STATE_STOPPED:
		return pl330_trigger(channel_num, buffer_address,
			timeout_loops);

	case PL330_STATE_WFP:
	case PL330_STATE_QUEUEBUSY:
	case PL330_STATE_ATBARRIER:
	case PL330_STATE_UPDTPC:
	case PL330_STATE_CACHEMISS:
	case PL330_STATE_EXECUTING:
		return 1;

	case PL330_STATE_WFE: /* For RESUME, nothing yet */
	default:
		return 1;
	}
}

/* Function:	Stop the manager or channel
 * Parameter:	channel0_manager1 -> 0 for channel and 1 for manager
 *		channel_num -> the channel number (only if as_manager=0)
 *		timeout_loops -> number of loop before timeout ocurred
 */
void pl330_stop(int channel0_manager1, int channel_num, int timeout_loops)
{
	u8 insn[6] = {0, 0, 0, 0, 0, 0};
	/* if fault completing, wait until reach faulting and killing state */
	if (pl330_getstate(channel0_manager1, channel_num) ==
		PL330_STATE_FAULT_COMPLETING)
		UNTIL(channel0_manager1, channel_num,
			PL330_STATE_FAULTING | PL330_STATE_KILLING);

	/* Return if nothing needs to be done */
	if (pl330_getstate(channel0_manager1, channel_num) ==
			PL330_STATE_COMPLETING
		|| pl330_getstate(channel0_manager1, channel_num) ==
			PL330_STATE_KILLING
		|| pl330_getstate(channel0_manager1, channel_num) ==
			PL330_STATE_STOPPED)
		return;

	/* kill it to ensure it reach to stop state */
	_emit_KILL(insn);

	/* Execute the KILL instruction through debug registers */
	pl330_execute_DBGINSN(insn, channel0_manager1, channel_num,
		timeout_loops);
}

/******************************************************************************
DMA transfer setup (MEM2MEM, MEM2PERIPH or PERIPH2MEM)
For Peripheral transfer, the FIFO threshold value is expected at
2 ^ pl330->brst_size * pl330->brst_len.
Return:		1 for error or not successful

channel_num	-	channel number assigned, valid from 0 to 7
src_addr	-	address to transfer from / source
dst_addr	-	address to transfer to / destination
size_byte	-	number of bytes to be transferred
brst_size	-	valid from 0 - 3
			where 0 = 1 (2 ^ 0) bytes and 3 = 8 bytes (2 ^ 3)
single_brst_size -	single transfer size (from 0 - 3)
brst_len	-	valid from 1 - 16 where each burst can trasfer 1 - 16
			data chunk (each chunk size equivalent to brst_size)
peripheral_id	-	assigned peripheral_id, valid from 0 to 31
transfer_type	-	MEM2MEM, MEM2PERIPH or PERIPH2MEM
enable_cache1	-	1 for cache enabled for memory
			(cacheable and bufferable, but do not allocate)
buf_size	-	sizeof(buf)
buf		-	buffer handler which will point to the memory
			allocated for dma microcode
******************************************************************************/
int pl330_transfer_setup(struct pl330_transfer_struct *pl330)
{
	/* Variable declaration */
	int off = 0;			/* buffer offset clear to 0 */
	unsigned loopjmp0, loopjmp1;	/* for DMALPEND */
	unsigned lcnt0 = 0;		/* loop count 0 */
	unsigned lcnt1 = 0;		/* loop count 1 */
	unsigned burst_size = 0;
	unsigned data_size_byte = pl330->size_byte;
	u32 ccr = 0;			/* Channel Control Register */
	struct pl330_reqcfg reqcfg;
	cmd_line = 0;

#ifdef PL330_DEBUG_MCGEN
	if (pl330->transfer_type == MEM2PERIPH)
		puts("INFO: mem2perip");
	else if (pl330->transfer_type == PERIPH2MEM)
		puts("INFO: perip2mem");
	else
		puts("INFO: mem2mem");

	printf(" - 0x%08lx -> 0x%08lx\nsize=%08x brst_size=2^%li "
		"brst_len=%li singles_brst_size=2^%li\n",
		pl330->src_addr, pl330->dst_addr, data_size_byte,
		pl330->brst_size, pl330->brst_len,
		pl330->single_brst_size);
#endif

	/* burst_size = 2 ^ brst_size */
	burst_size = 1 << pl330->brst_size;

	/* Fool proof checking */
	if (pl330->brst_size < 0 || pl330->brst_size >
		PL330_DMA_MAX_BURST_SIZE) {
		printf("ERROR PL330: brst_size must 0-%d (not %li)\n",
			PL330_DMA_MAX_BURST_SIZE, pl330->brst_size);
		return 1;
	}
	if (pl330->single_brst_size < 0 ||
		pl330->single_brst_size > PL330_DMA_MAX_BURST_SIZE) {
		printf("ERROR PL330 : single_brst_size must 0-%d (not %li)\n",
			PL330_DMA_MAX_BURST_SIZE, pl330->single_brst_size);
		return 1;
	}
	if (pl330->brst_len < 1 || pl330->brst_len > 16) {
		printf("ERROR PL330 : brst_len must 1-16 (not %li)\n",
			pl330->brst_len);
		return 1;
	}
	if (pl330->src_addr & (burst_size - 1)) {
		puts("ERROR PL330 : source address unaligned\n");
		return 1;
	}
	if (pl330->dst_addr & (burst_size - 1)) {
		puts("ERROR PL330 : destination address unaligned\n");
		return 1;
	}

	/* Setup the command list */

	/* DMAMOV SAR, x->src_addr */
	off += _emit_MOV(&pl330->buf[off], SAR, pl330->src_addr);
	/* DMAMOV DAR, x->dst_addr */
	off += _emit_MOV(&pl330->buf[off], DAR, pl330->dst_addr);
	/* DMAFLUSHP P(periheral_id) */
	if (pl330->transfer_type != MEM2MEM)
		off += _emit_FLUSHP(&pl330->buf[off], pl330->peripheral_id);

	/* Preparing the CCR value */
	if (pl330->transfer_type == MEM2PERIPH) {
		reqcfg.dst_inc = 0;	/* disable auto increment */
		reqcfg.src_inc = 1;	/* enable auto increment */
	} else if (pl330->transfer_type == PERIPH2MEM) {
		reqcfg.dst_inc = 1;
		reqcfg.src_inc = 0;
	} else {
		/* MEM2MEM */
		reqcfg.dst_inc = 1;
		reqcfg.src_inc = 1;
	}
	if (PL330_DMA_BASE == PL330_DMA_BASE_NON_SECURE)
		reqcfg.nonsecure = 1;	/* Non Secure mode */
	else
		reqcfg.nonsecure = 0;	/* Secure mode */
	if (pl330->enable_cache1 == 0) {
		reqcfg.dcctl = 0x1;	/* noncacheable but bufferable */
		reqcfg.scctl = 0x1;
	} else {
		if (pl330->transfer_type == MEM2PERIPH) {
			reqcfg.dcctl = 0x1;
			reqcfg.scctl = 0x7;	/* cacheable write back */
		} else if (pl330->transfer_type == PERIPH2MEM) {
			reqcfg.dcctl = 0x7;
			reqcfg.scctl = 0x1;
		} else {
			reqcfg.dcctl = 0x7;
			reqcfg.scctl = 0x7;
		}
	}
	reqcfg.privileged = 1;		/* 1 - Priviledge  */
	reqcfg.insnaccess = 0;		/* 0 - data access */
	reqcfg.swap = 0;		/* 0 - no endian swap */
	reqcfg.brst_len = pl330->brst_len;	/* DMA burst length */
	reqcfg.brst_size = pl330->brst_size;	/* DMA burst size */
	/* Preparing the CCR value */
	ccr = _prepare_ccr(&reqcfg);
	/* DMAMOV CCR, ccr */
	off += _emit_MOV(&pl330->buf[off], CCR, ccr);

	/* BURST */
	/* Can initiate a burst? */
	while (data_size_byte >= burst_size * pl330->brst_len) {
		lcnt0 = data_size_byte / (burst_size * pl330->brst_len);
		lcnt1 = 0;
		if (lcnt0 >= 256 * 256)
			lcnt0 = lcnt1 = 256;
		else if (lcnt0 >= 256) {
			lcnt1 = lcnt0 / 256;
			lcnt0 = 256;
		}
		data_size_byte = data_size_byte -
			(burst_size * pl330->brst_len * lcnt0 * lcnt1);

#ifdef PL330_DEBUG_MCGEN
		printf("Transferring 0x%08lx Remain 0x%08x\n", (burst_size *
			pl330->brst_len * lcnt0 * lcnt1), data_size_byte);
		printf("Running burst - brst_size=2^%li, brst_len=%li, "
			"lcnt0=%i, lcnt1=%i\n", pl330->brst_size,
			pl330->brst_len, lcnt0, lcnt1);
#endif

		if (lcnt1) {
			/* DMALP1 */
			off += _emit_LP(&pl330->buf[off], 1, lcnt1);
			loopjmp1 = off;
		}
		/* DMALP0 */
		off += _emit_LP(&pl330->buf[off], 0, lcnt0);
		loopjmp0 = off;
		/* DMAWFP periheral_id, burst */
		if (pl330->transfer_type != MEM2MEM)
			off += _emit_WFP(&pl330->buf[off], BURST,
				pl330->peripheral_id);
		/* DMALD */
		off += _emit_LD(&pl330->buf[off], ALWAYS);
		/* DMARMB */
		off += _emit_RMB(&pl330->buf[off]);
		/* DMASTPB peripheral_id */
		if (pl330->transfer_type != MEM2MEM)
			off += _emit_STP(&pl330->buf[off], BURST,
				pl330->peripheral_id);
		else
			off += _emit_ST(&pl330->buf[off], ALWAYS);
		/* DMAWMB */
		off += _emit_WMB(&pl330->buf[off]);
		/* DMALP0END */
		struct _arg_LPEND lpend;
		lpend.cond = ALWAYS;
		lpend.forever = 0;
		lpend.loop = 0;		/* loop cnt 0 */
		lpend.bjump = off - loopjmp0;
		off += _emit_LPEND(&pl330->buf[off], &lpend);
		/* DMALP1END */
		if (lcnt1) {
			struct _arg_LPEND lpend;
			lpend.cond = ALWAYS;
			lpend.forever = 0;
			lpend.loop = 1;		/* loop cnt 1*/
			lpend.bjump = off - loopjmp1;
			off += _emit_LPEND(&pl330->buf[off], &lpend);
		}
		/* ensure the microcode don't exceed buffer size */
		if (off > pl330->buf_size) {
			puts("ERROR PL330 : Exceed buffer size\n");
			return 1;
		}
	}

	/* SINGLE */
	pl330->brst_size = pl330->single_brst_size;
	pl330->brst_len = 1;
	/* burst_size = 2 ^ brst_size */
	burst_size = (1 << pl330->brst_size);
	lcnt0 = data_size_byte / (burst_size * pl330->brst_len);

	/* ensure all data will be transfered */
	data_size_byte = data_size_byte -
		(burst_size * pl330->brst_len * lcnt0);
	if (data_size_byte)
		puts("ERROR PL330 : Detected the possibility of untransfered"
			"data. Please ensure correct single burst size\n");

	if (lcnt0) {
#ifdef PL330_DEBUG_MCGEN
		printf("Transferring 0x%08lx Remain 0x%08x\n", (burst_size *
			pl330->brst_len * lcnt0 * lcnt1), data_size_byte);
		printf("Running single - brst_size=2^%li, brst_len=%li, "
			"lcnt0=%i\n", pl330->brst_size, pl330->brst_len,
			lcnt0);
#endif

		/* Preparing the CCR value */
		reqcfg.brst_len = pl330->brst_len;	/* DMA burst length */
		reqcfg.brst_size = pl330->brst_size;	/* DMA burst size */
		ccr = _prepare_ccr(&reqcfg);
		/* DMAMOV CCR, ccr */
		off += _emit_MOV(&pl330->buf[off], CCR, ccr);

		/* DMALP0 */
		off += _emit_LP(&pl330->buf[off], 0, lcnt0);
		loopjmp0 = off;
		/* DMAWFP peripheral_id, single */
		if (pl330->transfer_type != MEM2MEM)
			off += _emit_WFP(&pl330->buf[off], SINGLE,
				pl330->peripheral_id);
		/* DMALD */
		off += _emit_LD(&pl330->buf[off], ALWAYS);
		/* DMARMB */
		off += _emit_RMB(&pl330->buf[off]);
		/* DMASTPS peripheral_id */
		if (pl330->transfer_type != MEM2MEM)
			off += _emit_STP(&pl330->buf[off], SINGLE,
				pl330->peripheral_id);
		else
			off += _emit_ST(&pl330->buf[off], ALWAYS);
		/* DMAWMB */
		off += _emit_WMB(&pl330->buf[off]);
		/* DMALPEND */
		struct _arg_LPEND lpend1;
		lpend1.cond = ALWAYS;
		lpend1.forever = 0;
		lpend1.loop = 0;	/* loop cnt 0 */
		lpend1.bjump = off - loopjmp0;
		off += _emit_LPEND(&pl330->buf[off], &lpend1);
		/* ensure the microcode don't exceed buffer size */
		if (off > pl330->buf_size) {
			puts("ERROR PL330 : Exceed buffer size\n");
			return 1;
		}
	}

	/* DMAEND */
	off += _emit_END(&pl330->buf[off]);

	return 0;
}

/******************************************************************************
DMA run or start
Return:		1 for error or not successful

channel_num	-	channel number assigned, valid from 0 to 7
buf		-	buffer handler which will point to the memory
			allocated for dma microcode
******************************************************************************/
int pl330_transfer_start(struct pl330_transfer_struct *pl330)
{
	/* Timeout loop */
	int timeout_loops = 10000;

	/* Execute the command list */
	return pl330_start(0, pl330->channel_num, pl330->buf,
		timeout_loops);
}

/******************************************************************************
DMA poll until finish or error
Return:		1 for error or not successful

channel_num	-	channel number assigned, valid from 0 to 7
******************************************************************************/
int pl330_transfer_finish(struct pl330_transfer_struct *pl330)
{
	/* Wait until finish execution to ensure we compared correct result*/
	UNTIL(0, pl330->channel_num, PL330_STATE_STOPPED|PL330_STATE_FAULTING);

	/* check the state */
	if (pl330_getstate(0, pl330->channel_num) == PL330_STATE_FAULTING) {
		printf("FAULT Mode: Channel %li Faulting, FTR = 0x%08x, "
			"CPC = 0x%08x\n", pl330->channel_num,
			readl(PL330_DMA_BASE + FTC(pl330->channel_num)),
			((u32)readl(PL330_DMA_BASE + CPC(pl330->channel_num))
				- (u32)pl330->buf));
		return 1;
	}
	return 0;
}

/******************************************************************************
DMA transfer zeroes
Used to write zeroes to a memory chunk for memory scrubbing purpose
Return:		1 for error or not successful

channel_num	-	channel number assigned, valid from 0 to 7
dst_addr	-	address to transfer to / destination
size_byte	-	number of bytes to be transferred
buf_size	-	sizeof(buf)
buf		-	buffer handler which will point to the memory
			allocated for dma microcode
******************************************************************************/
int pl330_transfer_zeroes(struct pl330_transfer_struct *pl330)
{
	/* Variable declaration */
	int off = 0;			/* buffer offset clear to 0 */
	unsigned loopjmp0, loopjmp1;	/* for DMALPEND */
	unsigned lcnt0 = 0;		/* loop count 0 */
	unsigned lcnt1 = 0;		/* loop count 1 */
	unsigned burst_size = 0;
	unsigned data_size_byte = pl330->size_byte;
	u32 ccr = 0;			/* Channel Control Register */
	struct pl330_reqcfg reqcfg;
	cmd_line = 0;

#ifdef PL330_DEBUG_MCGEN
	puts("INFO: Write zeroes -> ");
	printf("0x%08lx size=0x%08x\n", pl330->dst_addr, data_size_byte);
#endif

	/* for burst, always use the maximum burst size and length */
	pl330->brst_size = PL330_DMA_MAX_BURST_SIZE;
	pl330->brst_len = 16;
	pl330->single_brst_size = 1;

	/* burst_size = 2 ^ brst_size */
	burst_size = 1 << pl330->brst_size;

	/* Setup the command list */
	/* DMAMOV DAR, x->dst_addr */
	off += _emit_MOV(&pl330->buf[off], DAR, pl330->dst_addr);

	/* Preparing the CCR value */
	reqcfg.dst_inc = 1;	/* enable auto increment */
	reqcfg.src_inc = 0;	/* disable auto increment (not applicable) */
	reqcfg.dcctl = 0x1;	/* noncacheable but bufferable */
	reqcfg.scctl = 0x1;	/* noncacheable and bufferable */
	reqcfg.privileged = 1;			/* 1 - Priviledge  */
	reqcfg.insnaccess = 0;			/* 0 - data access */
	reqcfg.swap = 0;			/* 0 - no endian swap */
	if (PL330_DMA_BASE == PL330_DMA_BASE_NON_SECURE)
		reqcfg.nonsecure = 1;		/* Non Secure mode */
	else
		reqcfg.nonsecure = 0;		/* Secure mode */
	reqcfg.brst_len = pl330->brst_len;	/* DMA burst length */
	reqcfg.brst_size = pl330->brst_size;	/* DMA burst size */
	/* Preparing the CCR value */
	ccr = _prepare_ccr(&reqcfg);
	/* DMAMOV CCR, ccr */
	off += _emit_MOV(&pl330->buf[off], CCR, ccr);

	/* BURST */
	/* Can initiate a burst? */
	while (data_size_byte >= burst_size * pl330->brst_len) {
		lcnt0 = data_size_byte / (burst_size * pl330->brst_len);
		lcnt1 = 0;
		if (lcnt0 >= 256 * 256)
			lcnt0 = lcnt1 = 256;
		else if (lcnt0 >= 256) {
			lcnt1 = lcnt0 / 256;
			lcnt0 = 256;
		}
		data_size_byte = data_size_byte -
			(burst_size * pl330->brst_len * lcnt0 * lcnt1);

#ifdef PL330_DEBUG_MCGEN
		printf("Transferring 0x%08lx Remain 0x%08x\n", (burst_size *
			pl330->brst_len * lcnt0 * lcnt1), data_size_byte);
		printf("Running burst - brst_size=2^%li, brst_len=%li, "
			"lcnt0=%i, lcnt1=%i\n", pl330->brst_size,
			pl330->brst_len, lcnt0, lcnt1);
#endif

		if (lcnt1) {
			/* DMALP1 */
			off += _emit_LP(&pl330->buf[off], 1, lcnt1);
			loopjmp1 = off;
		}
		/* DMALP0 */
		off += _emit_LP(&pl330->buf[off], 0, lcnt0);
		loopjmp0 = off;
		/* DMALSTZ */
		off += _emit_STZ(&pl330->buf[off]);
		/* DMALP0END */
		struct _arg_LPEND lpend;
		lpend.cond = ALWAYS;
		lpend.forever = 0;
		lpend.loop = 0;		/* loop cnt 0 */
		lpend.bjump = off - loopjmp0;
		off += _emit_LPEND(&pl330->buf[off], &lpend);
		/* DMALP1END */
		if (lcnt1) {
			struct _arg_LPEND lpend;
			lpend.cond = ALWAYS;
			lpend.forever = 0;
			lpend.loop = 1;		/* loop cnt 1*/
			lpend.bjump = off - loopjmp1;
			off += _emit_LPEND(&pl330->buf[off], &lpend);
		}
		/* ensure the microcode don't exceed buffer size */
		if (off > pl330->buf_size) {
			puts("ERROR PL330 : Exceed buffer size\n");
			return 1;
		}
	}

	/* SINGLE */
	pl330->brst_size = pl330->single_brst_size;
	pl330->brst_len = 1;
	/* burst_size = 2 ^ brst_size */
	burst_size = (1 << pl330->brst_size);
	lcnt0 = data_size_byte / (burst_size * pl330->brst_len);

	/* ensure all data will be transfered */
	data_size_byte = data_size_byte -
		(burst_size * pl330->brst_len * lcnt0);
	if (data_size_byte)
		puts("ERROR PL330 : Detected the possibility of untransfered"
			"data. Please ensure correct single burst size\n");

	if (lcnt0) {
#ifdef PL330_DEBUG_MCGEN
		printf("Transferring 0x%08lx Remain 0x%08x\n", (burst_size *
			pl330->brst_len * lcnt0), data_size_byte);
		printf("Running single - brst_size=2^%li, brst_len=%li, "
			"lcnt0=%i\n", pl330->brst_size, pl330->brst_len,
			lcnt0);
#endif

		/* Preparing the CCR value */
		reqcfg.brst_len = pl330->brst_len;	/* DMA burst length */
		reqcfg.brst_size = pl330->brst_size;	/* DMA burst size */
		ccr = _prepare_ccr(&reqcfg);
		/* DMAMOV CCR, ccr */
		off += _emit_MOV(&pl330->buf[off], CCR, ccr);

		/* DMALP0 */
		off += _emit_LP(&pl330->buf[off], 0, lcnt0);
		loopjmp0 = off;
		/* DMALSTZ */
		off += _emit_STZ(&pl330->buf[off]);
		/* DMALPEND */
		struct _arg_LPEND lpend1;
		lpend1.cond = ALWAYS;
		lpend1.forever = 0;
		lpend1.loop = 0;	/* loop cnt 0 */
		lpend1.bjump = off - loopjmp0;
		off += _emit_LPEND(&pl330->buf[off], &lpend1);
		/* ensure the microcode don't exceed buffer size */
		if (off > pl330->buf_size) {
			puts("ERROR PL330 : Exceed buffer size\n");
			return 1;
		}
	}

	/* DMAEND */
	off += _emit_END(&pl330->buf[off]);

	return 0;
}
