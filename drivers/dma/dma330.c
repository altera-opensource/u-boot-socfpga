/*
 * Copyright (C) 2018 Intel Corporation <www.intel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/io.h>
#include <dma330.h>
#include <wait_bit.h>

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
#define FTC(n)			(_FTC + (n) * 0x4)

/* Channel Status register */
#define _CS			0x100
#define CS(n)			(_CS + (n) * 0x8)
#define CS_CNS			(1 << 21)

/* Channel Program Counter register */
#define _CPC			0x104
#define CPC(n)			(_CPC + (n) * 0x8)

/* Source Address register */
#define _SA			0x400
#define SA(n)			(_SA + (n) * 0x20)

/* Destination Address register */
#define _DA			0x404
#define DA(n)			(_DA + (n) * 0x20)

/* Channel Control register */
#define _CC			0x408
#define CC(n)			(_CC + (n) * 0x20)

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
#define LC0(n)			(_LC0 + (n) * 0x20)

/* Loop Counter 1 register */
#define _LC1			0x410
#define LC1(n)			(_LC1 + (n) * 0x20)

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
#define UNTIL(t, s)	do {} while (!(dma330_getstate((t)) & (s)));

static unsigned cmd_line;

/* debug message printout */
#ifdef DEBUG
#define DMA330_DBGCMD_DUMP(off, x...)	do { \
						printf("%x:", cmd_line); \
						printf((x)); \
						cmd_line += (off); \
					} while (0)
#else
#define DMA330_DBGCMD_DUMP(off, x...)	do {} while (0)
#endif

/* Enum declaration */
enum dmamov_dst {
	SAR = 0,
	CCR,
	DAR,
};

enum dma330_dst {
	SRC = 0,
	DST,
};

enum dma330_cond {
	SINGLE,
	BURST,
	ALWAYS,
};

/* Structure will be used by _emit_lpend function */
struct _arg_lpend {
	enum dma330_cond cond;
	int forever;
	unsigned loop;
	u8 bjump;
};

/* Structure will be used by _emit_go function */
struct _arg_go {
	u8 chan;
	u32 addr;
	unsigned ns;
};

/*
 * _emit_end - Add opcode DMAEND into microcode (end).
 *
 * @buf: The buffer which stored the microcode program.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_end(u8 buf[])
{
	buf[0] = CMD_DMAEND;
	DMA330_DBGCMD_DUMP(SZ_DMAEND, "\tDMAEND\n");
	return SZ_DMAEND;
}

/*
 * _emit_flushp - Add opcode DMAFLUSHP into microcode (flush peripheral).
 *
 * @buf -> The buffer which stored the microcode program.
 * @peri -> Peripheral ID as listed in DMA NPP.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_flushp(u8 buf[], u8 peri)
{
	volatile u8 *buffer = buf;
	buffer[0] = CMD_DMAFLUSHP;
	peri &= 0x1f;
	peri <<= 3;
	buffer[1] = peri;
	DMA330_DBGCMD_DUMP(SZ_DMAFLUSHP, "\tDMAFLUSHP %u\n", peri >> 3);
	return SZ_DMAFLUSHP;
}

/**
 * _emit_ld - Add opcode DMALD into microcode (load).
 *
 * @buf: The buffer which stored the microcode program.
 * @cond: Execution criteria such as single, burst or always.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_ld(u8 buf[], enum dma330_cond cond)
{
	buf[0] = CMD_DMALD;
	if (cond == SINGLE) {
		buf[0] |= (0 << 1) | (1 << 0);
	} else if (cond == BURST) {
		buf[0] |= (1 << 1) | (1 << 0);
	}
	DMA330_DBGCMD_DUMP(SZ_DMALD, "\tDMALD%c\n",
			  cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'A'));
	return SZ_DMALD;
}

/**
 * _emit_lp - Add opcode DMALP into microcode (loop).
 *
 * @buf: The buffer which stored the microcode program.
 * @loop: Selection of using counter 0 or 1  (valid value 0 or 1).
 * @cnt: number of iteration (valid value 1-256).
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_lp(u8 buf[], u32 loop, u32 cnt)
{
	volatile u8 *buffer = buf;
	buffer[0] = CMD_DMALP;
	if (loop)
		buffer[0] |= (1 << 1);
	/*
	 * Decrements by 1 will be done by DMAC assembler. But at here, this
	 * will be done by driver here
	 */
	cnt--;
	buffer[1] = cnt;
	DMA330_DBGCMD_DUMP(SZ_DMALP, "\tDMALP_%c %u\n", loop ? '1' : '0', cnt);
	return SZ_DMALP;
}

/**
 * _emit_lpend - Add opcode DMALPEND into microcode (loop end).
 *
 * @buf: The buffer which stored the microcode program.
 * @arg: Structure _arg_lpend which contain all needed info.
 *	arg->cond -> Execution criteria such as single, burst or always
 *	arg->forever -> Forever loop? used if DMALPFE started the loop
 *	arg->bjump -> Backwards jump (relative location of
 *		      1st instruction in the loop.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_lpend(u8 buf[], const struct _arg_lpend *arg)
{
	volatile u8 *buffer = buf;
	enum dma330_cond cond = arg->cond;
	int forever = arg->forever;
	unsigned loop = arg->loop;
	u8 bjump = arg->bjump;
	buffer[0] = CMD_DMALPEND;
	if (loop)
		buffer[0] |= (1 << 2);
	if (!forever)
		buffer[0] |= (1 << 4);
	if (cond == SINGLE) {
		buffer[0] |= (0 << 1) | (1 << 0);
	} else if (cond == BURST) {
		buffer[0] |= (1 << 1) | (1 << 0);
	}
	buffer[1] = bjump;
	DMA330_DBGCMD_DUMP(SZ_DMALPEND, "\tDMALP%s%c_%c bjmpto_%x\n",
			  forever ? "FE" : "END",
			  cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'A'),
			  loop ? '1' : '0',
			  bjump);
	return SZ_DMALPEND;
}

/**
 * _emit_kill - Add opcode DMAKILL into microcode (kill).
 *
 * @buf: The buffer which stored the microcode program.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_kill(u8 buf[])
{
	buf[0] = CMD_DMAKILL;
	return SZ_DMAKILL;
}

/**
 * _emit_mov - Add opcode DMAMOV into microcode (move).
 *
 * @buf: The buffer which stored the microcode program.
 * @dst: Destination register (valid value SAR[0b000], DAR[0b010],
 *	 or CCR[0b001]).
 * @val: 32bit value that to be written into destination register.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_mov(u8 buf[], enum dmamov_dst dst, u32 val)
{
	volatile u8 *buffer = buf;
	buffer[0] = CMD_DMAMOV;
	buffer[1] = dst;
	buffer[2] = val & 0xFF;
	buffer[3] = (val >> 8) & 0xFF;
	buffer[4] = (val >> 16) & 0xFF;
	buffer[5] = (val >> 24) & 0xFF;
	DMA330_DBGCMD_DUMP(SZ_DMAMOV, "\tDMAMOV %s 0x%x\n",
			  dst == SAR ? "SAR" : (dst == DAR ? "DAR" : "CCR"),
			   val);
	return SZ_DMAMOV;
}

/**
 * _emit_nop -	Add opcode DMANOP into microcode (no operation).
 *
 * @buf: The buffer which stored the microcode program.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_nop(u8 buf[])
{
	buf[0] = CMD_DMANOP;
	DMA330_DBGCMD_DUMP(SZ_DMANOP, "\tDMANOP\n");
	return SZ_DMANOP;
}

/**
 * _emit_rmb - Add opcode DMARMB into microcode (read memory barrier).
 *
 * @buf: The buffer which stored the microcode program.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_rmb(u8 buf[])
{
	buf[0] = CMD_DMARMB;
	DMA330_DBGCMD_DUMP(SZ_DMARMB, "\tDMARMB\n");
	return SZ_DMARMB;
}

/**
 * _emit_sev - Add opcode DMASEV into microcode (send event).
 *
 * @buf: The buffer which stored the microcode program.
 * @ev: The event number (valid 0 - 31).
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_sev(u8 buf[], u8 ev)
{
	volatile u8 *buffer = buf;
	buffer[0] = CMD_DMASEV;
	ev &= 0x1f;
	ev <<= 3;
	buffer[1] = ev;
	DMA330_DBGCMD_DUMP(SZ_DMASEV, "\tDMASEV %u\n", ev >> 3);
	return SZ_DMASEV;
}

/**
 * _emit_st - Add opcode DMAST into microcode (store).
 *
 * @buf: The buffer which stored the microcode program.
 * @cond: Execution criteria such as single, burst or always.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_st(u8 buf[], enum dma330_cond cond)
{
	buf[0] = CMD_DMAST;
	if (cond == SINGLE) {
		buf[0] |= (0 << 1) | (1 << 0);
	} else if (cond == BURST) {
		buf[0] |= (1 << 1) | (1 << 0);
	}
	DMA330_DBGCMD_DUMP(SZ_DMAST, "\tDMAST%c\n",
		cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'A'));
	return SZ_DMAST;
}

/**
 * _emit_stp - Add opcode DMASTP into microcode (store and notify peripheral).
 *
 * @buf: The buffer which stored the microcode program.
 * @cond: Execution criteria such as single, burst or always.
 * @peri: Peripheral ID as listed in DMA NPP.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_stp(u8 buf[], enum dma330_cond cond, u8 peri)
{
	volatile u8 *buffer = buf;
	buffer[0] = CMD_DMASTP;
	if (cond == BURST)
		buf[0] |= (1 << 1);
	peri &= 0x1f;
	peri <<= 3;
	buffer[1] = peri;
	DMA330_DBGCMD_DUMP(SZ_DMASTP, "\tDMASTP%c %u\n",
		cond == SINGLE ? 'S' : 'B', peri >> 3);
	return SZ_DMASTP;
}

/**
 * _emit_stz - Add opcode DMASTZ into microcode (store zeroes).
 *
 * @buf -> The buffer which stored the microcode program.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_stz(u8 buf[])
{
	buf[0] = CMD_DMASTZ;
	DMA330_DBGCMD_DUMP(SZ_DMASTZ, "\tDMASTZ\n");
	return SZ_DMASTZ;
}

/**
 * _emit_wfp - Add opcode DMAWFP into microcode (wait for peripheral).
 *
 * @buf: The buffer which stored the microcode program.
 * @cond: Execution criteria such as single, burst or always.
 * @peri: Peripheral ID as listed in DMA NPP.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_wfp(u8 buf[], enum dma330_cond cond, u8 peri)
{
	volatile u8 *buffer = buf;
	buffer[0] = CMD_DMAWFP;
	if (cond == SINGLE)
		buffer[0] |= (0 << 1) | (0 << 0);
	else if (cond == BURST) {
		buffer[0] |= (1 << 1) | (0 << 0);
	} else {
		buffer[0] |= (0 << 1) | (1 << 0);
	}
	peri &= 0x1f;
	peri <<= 3;
	buffer[1] = peri;
	DMA330_DBGCMD_DUMP(SZ_DMAWFP, "\tDMAWFP%c %u\n",
		cond == SINGLE ? 'S' : (cond == BURST ? 'B' : 'P'), peri >> 3);
	return SZ_DMAWFP;
}

/**
 * _emit_wmb - Add opcode DMAWMB into microcode (write memory barrier).
 *
 * @buf: The buffer which stored the microcode program.
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_wmb(u8 buf[])
{
	buf[0] = CMD_DMAWMB;
	DMA330_DBGCMD_DUMP(SZ_DMAWMB, "\tDMAWMB\n");
	return SZ_DMAWMB;
}

/**
 * _emit_go - Add opcode DMALGO into microcode (go).
 *
 * @buf: The buffer which stored the microcode program.
 * @arg: structure _arg_go which contain all needed info
 *	arg->chan -> channel number
 *	arg->addr -> start address of the microcode program
 *		     which will be wrote into CPC register
 *	arg->ns -> 1 for non secure, 0 for secure
 *		   (if only DMA Manager is in secure).
 *
 * Return: Size of opcode.
 */
static inline u32 _emit_go(u8 buf[], const struct _arg_go *arg)
{
	volatile u8 *buffer = buf;
	u8 chan = arg->chan;
	u32 addr = arg->addr;
	unsigned ns = arg->ns;
	buffer[0] = CMD_DMAGO;
	buffer[0] |= (ns << 1);
	buffer[1] = chan & 0x7;
	buffer[2] = addr & 0xFF;
	buffer[3] = (addr >> 8) & 0xFF;
	buffer[4] = (addr >> 16) & 0xFF;
	buffer[5] = (addr >> 24) & 0xFF;
	return SZ_DMAGO;
}

/**
 * _prepare_ccr - Populate the CCR register.
 * @rqc: Request Configuration.
 *
 * Return: Channel Control register (CCR) Setting.
 */
static inline u32 _prepare_ccr(const struct dma330_reqcfg *rqc)
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

/**
 * dma330_until_dmac_idle - Wait until DMA Manager is idle.
 *
 * @timeout_ms: Timeout (in miliseconds).
 *
 * Return: Negative value for error / timeout ocurred before idle,
 *	   0 for successful.
 */
static int dma330_until_dmac_idle(struct dma330_transfer_struct *dma330,
				  const u32 timeout_ms)
{
	return wait_for_bit(__func__,
			 (const u32 *)(uintptr_t)(dma330->reg_base + DBGSTATUS),
			 DBG_BUSY, 0, timeout_ms, false);
}

/**
 * dma330_execute_dbginsn - Execute debug instruction such as DMAGO and DMAKILL.
 *
 * @insn: The buffer which stored the debug instruction.
 * @dma330: Pointer to dma330_transfer_struct.
 * @timeout_ms: Timeout (in miliseconds).
 *
 * Return: void.
 */
static inline void dma330_execute_dbginsn(u8 insn[],
					  struct dma330_transfer_struct *dma330,
					  const u32 timeout_ms)
{
	u32 val;
	val = (insn[0] << 16) | (insn[1] << 24);
	if (!dma330->channel0_manager1)
		val |= (1 << 0);
	val |= (dma330->channel_num << 8); /* Channel Number */
	writel(val, (u32 *)(uintptr_t)(dma330->reg_base + DBGINST0));
	val = insn[2];
	val = val | (insn[3] << 8);
	val = val | (insn[4] << 16);
	val = val | (insn[5] << 24);
	writel(val, (u32 *)(uintptr_t)(dma330->reg_base + DBGINST1));

	/* If timed out due to halted state-machine */
	if (dma330_until_dmac_idle(dma330, timeout_ms)) {
		debug("DMAC halted!\n");
		return;
	}
	/* Get going */
	writel(0, (u32 *)(uintptr_t)(dma330->reg_base + DBGCMD));
}

/**
 * dma330_getstate - Get the status of current channel or manager.
 *
 * @dma330: Pointer to dma330_transfer_struct.
 *
 * Return: Status state of current channel or current manager.
 */
static inline u32 dma330_getstate(struct dma330_transfer_struct *dma330)
{
	u32 val;

	if (dma330->channel0_manager1) {
		val = readl((u32 *)(uintptr_t)(dma330->reg_base + DS)) & 0xf;
	} else {
		val = readl((u32 *)(uintptr_t)(dma330->reg_base +
			    CS(dma330->channel_num))) & 0xf;
	}

	switch (val) {
	case DS_ST_STOP:
		return DMA330_STATE_STOPPED;
	case DS_ST_EXEC:
		return DMA330_STATE_EXECUTING;
	case DS_ST_CMISS:
		return DMA330_STATE_CACHEMISS;
	case DS_ST_UPDTPC:
		return DMA330_STATE_UPDTPC;
	case DS_ST_WFE:
		return DMA330_STATE_WFE;
	case DS_ST_FAULT:
		return DMA330_STATE_FAULTING;
	case DS_ST_ATBRR:
		if (dma330->channel0_manager1) {
			return DMA330_STATE_INVALID;
		} else {
			return DMA330_STATE_ATBARRIER;
		}
	case DS_ST_QBUSY:
		if (dma330->channel0_manager1) {
			return DMA330_STATE_INVALID;
		} else {
			return DMA330_STATE_QUEUEBUSY;
		}
	case DS_ST_WFP:
		if (dma330->channel0_manager1) {
			return DMA330_STATE_INVALID;
		} else {
			return DMA330_STATE_WFP;
		}
	case DS_ST_KILL:
		if (dma330->channel0_manager1) {
			return DMA330_STATE_INVALID;
		} else {
			return DMA330_STATE_KILLING;
		}
	case DS_ST_CMPLT:
		if (dma330->channel0_manager1) {
			return DMA330_STATE_INVALID;
		} else {
			return DMA330_STATE_COMPLETING;
		}
	case DS_ST_FLTCMP:
		if (dma330->channel0_manager1) {
			return DMA330_STATE_INVALID;
		} else {
			return DMA330_STATE_FAULT_COMPLETING;
		}
	default:
		return DMA330_STATE_INVALID;
	}
}

/**
 * dma330_trigger - Execute the DMAGO through debug instruction.
 *
 * @dma330: Pointer to dma330_transfer_struct.
 * @timeout_ms: Timeout (in miliseconds).
 *
 * When the DMA manager executes Go for a DMA channel that is in the Stopped
 * state, it moves a 32-bit immediate into the program counter, then setting
 * its security state and updating DMA channel to the Executing state.
 *
 * Return: Negative value for error as DMA is not ready. 0 for successful.
 */
static int dma330_trigger(struct dma330_transfer_struct *dma330,
			  const u32 timeout_ms)
{
	u8 insn[6] = {0, 0, 0, 0, 0, 0};
	struct _arg_go go;

	/*
	 * Return if already ACTIVE. It will ensure DMAGO is executed at
	 * STOPPED state too
	 */
	dma330->channel0_manager1 = 0;
	if (dma330_getstate(dma330) !=
		DMA330_STATE_STOPPED)
		return -EPERM;

	go.chan = dma330->channel_num;
	go.addr = (uintptr_t)dma330->buf;

	if (dma330->reg_base == SOCFPGA_DMANONSECURE_ADDRESS) {
		/* non-secure condition */
		go.ns = 1;
	} else {
		/* secure condition */
		go.ns = 0;
	}

	_emit_go(insn, &go);

	/* Only manager can execute GO */
	dma330->channel0_manager1 = 1;
	dma330_execute_dbginsn(insn, dma330, timeout_ms);
	return 0;
}

/**
 * dma330_start - Execute the command list through DMAGO and debug instruction.
 *
 * @dma330: Pointer to struct dma330_transfer_struct.
 * @timeout_ms: Timeout (in miliseconds).
 *
 * Return: Negative value for error where DMA is not ready. 0 for successful.
 */
static int dma330_start(struct dma330_transfer_struct *dma330,
			const u32 timeout_ms)
{
	debug("INFO: DMA BASE Address = 0x%08x\n",
	      (u32)(uintptr_t)dma330->reg_base);

	switch (dma330_getstate(dma330)) {
	case DMA330_STATE_FAULT_COMPLETING:
		UNTIL(dma330, DMA330_STATE_FAULTING | DMA330_STATE_KILLING);

		if (dma330_getstate(dma330) == DMA330_STATE_KILLING)
			UNTIL(dma330, DMA330_STATE_STOPPED);

	case DMA330_STATE_FAULTING:
		dma330_stop(dma330, timeout_ms);

	case DMA330_STATE_KILLING:
	case DMA330_STATE_COMPLETING:
		UNTIL(dma330, DMA330_STATE_STOPPED);

	case DMA330_STATE_STOPPED:
		return dma330_trigger(dma330, timeout_ms);

	case DMA330_STATE_WFP:
	case DMA330_STATE_QUEUEBUSY:
	case DMA330_STATE_ATBARRIER:
	case DMA330_STATE_UPDTPC:
	case DMA330_STATE_CACHEMISS:
	case DMA330_STATE_EXECUTING:
		return -ESRCH;
	/* For RESUME, nothing yet */
	case DMA330_STATE_WFE:
	default:
		return -ESRCH;
	}
}

/**
 * dma330_stop - Stop the manager or channel.
 *
 * @dma330: Pointer to struct dma330_transfer_struct.
 * @timeout_ms: Timeout (in miliseconds).
 *
 * Stop the manager/channel or killing them and ensure they reach to stop
 * state.
 *
 * Return: void.
 */
void dma330_stop(struct dma330_transfer_struct *dma330, const u32 timeout_ms)
{
	u8 insn[6] = {0, 0, 0, 0, 0, 0};

	/* If fault completing, wait until reach faulting and killing state */
	if (dma330_getstate(dma330) ==
		DMA330_STATE_FAULT_COMPLETING)
		UNTIL(dma330, DMA330_STATE_FAULTING | DMA330_STATE_KILLING);

	/* Return if nothing needs to be done */
	if (dma330_getstate(dma330) ==
			DMA330_STATE_COMPLETING
		|| dma330_getstate(dma330) ==
			DMA330_STATE_KILLING
		|| dma330_getstate(dma330) ==
			DMA330_STATE_STOPPED)
		return;

	/* Kill it to ensure it reach to stop state */
	_emit_kill(insn);

	/* Execute the KILL instruction through debug registers */
	dma330_execute_dbginsn(insn, dma330, timeout_ms);
}

/**
 * dma330_transfer_setup - DMA transfer setup (MEM2MEM, MEM2PERIPH
 *			   or PERIPH2MEM).
 *
 * @dma330: Pointer to struct dma330_transfer_struct.
 *
 * For Peripheral transfer, the FIFO threshold value is expected at
 * 2 ^ dma330->brst_size * dma330->brst_len.
 *
 * Return: Negative value for error or not successful. 0 for successful.
 */
int dma330_transfer_setup(struct dma330_transfer_struct *dma330)
{
	/* Variable declaration */
	/* Buffer offset clear to 0 */
	int off = 0;
	/* For DMALPEND */
	unsigned loopjmp0, loopjmp1;
	/* Loop count 0 */
	unsigned lcnt0 = 0;
	/* Loop count 1 */
	unsigned lcnt1 = 0;
	unsigned burst_size = 0;
	unsigned data_size_byte = dma330->size_byte;
	/* Strong order memory is required to store microcode command list */
	u8 *buf = (u8 *)dma330->buf;
	/* Channel Control Register */
	u32 ccr = 0;
	struct dma330_reqcfg reqcfg;
	cmd_line = 0;

	if (!buf) {
		debug("ERROR DMA330 : DMA Microcode buffer pointer is NULL\n");
		return -EINVAL;
	}

	if (dma330->transfer_type == DMA_MEM_TO_DEV) {
		debug("INFO: mem2perip");
	} else if (dma330->transfer_type == DMA_DEV_TO_MEM) {
		debug("INFO: perip2mem");
	} else {
		debug("INFO: DMA_MEM_TO_MEM");
	}

	debug(" - 0x%08x -> 0x%08x\nsize=%08x brst_size=2^%d ",
	      dma330->src_addr, dma330->dst_addr, data_size_byte,
	      dma330->brst_size);
	debug("brst_len=%d singles_brst_size=2^%d\n", dma330->brst_len,
	      dma330->single_brst_size);

	/* burst_size = 2 ^ brst_size */
	burst_size = 1 << dma330->brst_size;

	/* Fool proof checking */
	if (dma330->brst_size < 0 || dma330->brst_size >
		DMA330_MAX_BURST_SIZE) {
		debug("ERROR DMA330: brst_size must 0-%d (not %d)\n",
			DMA330_MAX_BURST_SIZE, dma330->brst_size);
		return -EINVAL;
	}
	if (dma330->single_brst_size < 0 ||
		dma330->single_brst_size > DMA330_MAX_BURST_SIZE) {
		debug("ERROR DMA330 : single_brst_size must 0-%d (not %d)\n",
			DMA330_MAX_BURST_SIZE, dma330->single_brst_size);
		return -EINVAL;
	}
	if (dma330->brst_len < 1 || dma330->brst_len > 16) {
		debug("ERROR DMA330 : brst_len must 1-16 (not %d)\n",
			dma330->brst_len);
		return -EINVAL;
	}
	if (dma330->src_addr & (burst_size - 1)) {
		debug("ERROR DMA330 : Source address unaligned\n");
		return -EINVAL;
	}
	if (dma330->dst_addr & (burst_size - 1)) {
		debug("ERROR DMA330 : Destination address unaligned\n");
		return -EINVAL;
	}

	/* Setup the command list */
	/* DMAMOV SAR, x->src_addr */
	off += _emit_mov(&buf[off], SAR, dma330->src_addr);
	/* DMAMOV DAR, x->dst_addr */
	off += _emit_mov(&buf[off], DAR, dma330->dst_addr);
	/* DMAFLUSHP P(periheral_id) */
	if (dma330->transfer_type != DMA_MEM_TO_MEM)
		off += _emit_flushp(&buf[off], dma330->peripheral_id);

	/* Preparing the CCR value */
	if (dma330->transfer_type == DMA_MEM_TO_DEV) {
		/* Disable auto increment */
		reqcfg.dst_inc = 0;
		/* Enable auto increment */
		reqcfg.src_inc = 1;
	} else if (dma330->transfer_type == DMA_DEV_TO_MEM) {
		reqcfg.dst_inc = 1;
		reqcfg.src_inc = 0;
	} else {
		/* DMA_MEM_TO_MEM */
		reqcfg.dst_inc = 1;
		reqcfg.src_inc = 1;
	}

	if (dma330->reg_base == DMA330_BASE_NON_SECURE) {
		/* Non Secure mode */
		reqcfg.nonsecure = 1;
	} else {
		/* Secure mode */
		reqcfg.nonsecure = 0;
	}

	if (dma330->enable_cache1 == 0) {
		/* Noncacheable but bufferable */
		reqcfg.dcctl = 0x1;
		reqcfg.scctl = 0x1;
	} else {
		if (dma330->transfer_type == DMA_MEM_TO_DEV) {
			reqcfg.dcctl = 0x1;
			/* Cacheable write back */
			reqcfg.scctl = 0x7;
		} else if (dma330->transfer_type == DMA_DEV_TO_MEM) {
			reqcfg.dcctl = 0x7;
			reqcfg.scctl = 0x1;
		} else {
			reqcfg.dcctl = 0x7;
			reqcfg.scctl = 0x7;
		}
	}
	/* 1 - Priviledge  */
	reqcfg.privileged = 1;
	/* 0 - Data access */
	reqcfg.insnaccess = 0;
	/* 0 - No endian swap */
	reqcfg.swap = 0;
	/* DMA burst length */
	reqcfg.brst_len = dma330->brst_len;
	/* DMA burst size */
	reqcfg.brst_size = dma330->brst_size;
	/* Preparing the CCR value */
	ccr = _prepare_ccr(&reqcfg);
	/* DMAMOV CCR, ccr */
	off += _emit_mov(&buf[off], CCR, ccr);

	/* BURST */
	/* Can initiate a burst? */
	while (data_size_byte >= burst_size * dma330->brst_len) {
		lcnt0 = data_size_byte / (burst_size * dma330->brst_len);
		lcnt1 = 0;
		if (lcnt0 >= 256 * 256) {
			lcnt0 = lcnt1 = 256;
		} else if (lcnt0 >= 256) {
			lcnt1 = lcnt0 / 256;
			lcnt0 = 256;
		}
		data_size_byte = data_size_byte -
			(burst_size * dma330->brst_len * lcnt0 * lcnt1);

		debug("Transferring 0x%08x Remain 0x%08x\n", (burst_size *
			dma330->brst_len * lcnt0 * lcnt1), data_size_byte);
		debug("Running burst - brst_size=2^%d, brst_len=%d, ",
		      dma330->brst_size, dma330->brst_len);
		debug("lcnt0=%d, lcnt1=%d\n", lcnt0, lcnt1);

		if (lcnt1) {
			/* DMALP1 */
			off += _emit_lp(&buf[off], 1, lcnt1);
			loopjmp1 = off;
		}
		/* DMALP0 */
		off += _emit_lp(&buf[off], 0, lcnt0);
		loopjmp0 = off;
		/* DMAWFP periheral_id, burst */
		if (dma330->transfer_type != DMA_MEM_TO_MEM)
			off += _emit_wfp(&buf[off], BURST,
				dma330->peripheral_id);
		/* DMALD */
		off += _emit_ld(&buf[off], ALWAYS);
		/* DMARMB */
		off += _emit_rmb(&buf[off]);
		/* DMASTPB peripheral_id */
		if (dma330->transfer_type != DMA_MEM_TO_MEM) {
			off += _emit_stp(&buf[off], BURST,
				dma330->peripheral_id);
		} else {
			off += _emit_st(&buf[off], ALWAYS);
		}
		/* DMAWMB */
		off += _emit_wmb(&buf[off]);
		/* DMALP0END */
		struct _arg_lpend lpend;
		lpend.cond = ALWAYS;
		lpend.forever = 0;
		/* Loop cnt 0 */
		lpend.loop = 0;
		lpend.bjump = off - loopjmp0;
		off += _emit_lpend(&buf[off], &lpend);
		/* DMALP1END */
		if (lcnt1) {
			struct _arg_lpend lpend;
			lpend.cond = ALWAYS;
			lpend.forever = 0;
			lpend.loop = 1;		/* loop cnt 1*/
			lpend.bjump = off - loopjmp1;
			off += _emit_lpend(&buf[off], &lpend);
		}
		/* Ensure the microcode don't exceed buffer size */
		if (off > dma330->buf_size) {
			debug("ERROR DMA330 : Exceed buffer size\n");
			return -ENOMEM;
		}
	}

	/* SINGLE */
	dma330->brst_size = dma330->single_brst_size;
	dma330->brst_len = 1;
	/* burst_size = 2 ^ brst_size */
	burst_size = (1 << dma330->brst_size);
	lcnt0 = data_size_byte / (burst_size * dma330->brst_len);

	/* Ensure all data will be transfered */
	data_size_byte = data_size_byte -
		(burst_size * dma330->brst_len * lcnt0);
	if (data_size_byte) {
		debug("ERROR DMA330 : Detected the possibility of ");
		debug("untransfered data. Please ensure correct single \n");
		debug("burst size\n");
	}

	if (lcnt0) {
		debug("Transferring 0x%08x Remain 0x%08x\n", (burst_size *
			dma330->brst_len * lcnt0 * lcnt1), data_size_byte);
		debug("Running single - brst_size=2^%d, brst_len=%d, ",
		      dma330->brst_size, dma330->brst_len);
		debug("lcnt0=%d\n", lcnt0);

		/* Preparing the CCR value */
		/* DMA burst length */
		reqcfg.brst_len = dma330->brst_len;
		/* DMA burst size */
		reqcfg.brst_size = dma330->brst_size;
		ccr = _prepare_ccr(&reqcfg);
		/* DMAMOV CCR, ccr */
		off += _emit_mov(&buf[off], CCR, ccr);

		/* DMALP0 */
		off += _emit_lp(&buf[off], 0, lcnt0);
		loopjmp0 = off;
		/* DMAWFP peripheral_id, single */
		if (dma330->transfer_type != DMA_MEM_TO_MEM)
			off += _emit_wfp(&buf[off], SINGLE,
				dma330->peripheral_id);
		/* DMALD */
		off += _emit_ld(&buf[off], ALWAYS);
		/* DMARMB */
		off += _emit_rmb(&buf[off]);
		/* DMASTPS peripheral_id */
		if (dma330->transfer_type != DMA_MEM_TO_MEM) {
			off += _emit_stp(&buf[off], SINGLE,
				dma330->peripheral_id);
		} else {
			off += _emit_st(&buf[off], ALWAYS);
		}
		/* DMAWMB */
		off += _emit_wmb(&buf[off]);
		/* DMALPEND */
		struct _arg_lpend lpend1;
		lpend1.cond = ALWAYS;
		lpend1.forever = 0;
		/* loop cnt 0 */
		lpend1.loop = 0;
		lpend1.bjump = off - loopjmp0;
		off += _emit_lpend(&buf[off], &lpend1);
		/* Ensure the microcode don't exceed buffer size */
		if (off > dma330->buf_size) {
			puts("ERROR DMA330 : Exceed buffer size\n");
			return -ENOMEM;
		}
	}

	/* DMAEND */
	off += _emit_end(&buf[off]);

	return 0;
}

/**
 * dma330_transfer_start - DMA start to run.
 *
 * @dma330: Pointer to struct dma330_transfer_struct.
 *
 * DMA start to excecute microcode command list.
 *
 * Return: Negative value for error or not successful. 0 for successful.
 */
int dma330_transfer_start(struct dma330_transfer_struct *dma330)
{
	const u32 timeout_ms = 1000;

	/* Execute the command list */
	return dma330_start(dma330, timeout_ms);
}

/**
 * dma330_transfer_finish - DMA polling until execution finish or error.
 *
 * @dma330: Pointer to struct dma330_transfer_struct.
 *
 * DMA polling until excution finish and checking the state status.
 *
 * Return: Negative value for state error or not successful. 0 for successful.
 */
int dma330_transfer_finish(struct dma330_transfer_struct *dma330)
{
	if (!dma330->buf) {
		debug("ERROR DMA330 : DMA Microcode buffer pointer is NULL\n");
		return -EINVAL;
	}

	/* Wait until finish execution to ensure we compared correct result*/
	UNTIL(dma330, DMA330_STATE_STOPPED | DMA330_STATE_FAULTING);

	/* check the state */
	if (dma330_getstate(dma330) == DMA330_STATE_FAULTING) {
		debug("FAULT Mode: Channel %d Faulting, FTR = 0x%08x,",
		     dma330->channel_num,
		     readl((uintptr_t)
			  (dma330->reg_base + FTC(dma330->channel_num))));
		debug("CPC = 0x%08lx\n",
		     (readl((u32 *)(uintptr_t)
		     (dma330->reg_base + CPC(dma330->channel_num)))
		     - (uintptr_t)dma330->buf));
		return -EPROTO;
	}
	return 0;
}

/**
 * dma330_transfer_zeroes - DMA transfer zeroes.
 *
 * @dma330: Pointer to struct dma330_transfer_struct.
 *
 * Used to write zeroes to a memory chunk for memory scrubbing purpose.
 *
 * Return: Negative value for error or not successful. 0 for successful.
 */
int dma330_transfer_zeroes(struct dma330_transfer_struct *dma330)
{
	/* Variable declaration */
	/* Buffer offset clear to 0 */
	int off = 0;
	/* For DMALPEND */
	unsigned loopjmp0, loopjmp1;
	/* Loop count 0 */
	unsigned lcnt0 = 0;
	/* Loop count 1 */
	unsigned lcnt1 = 0;
	unsigned burst_size = 0;
	unsigned data_size_byte = dma330->size_byte;
	/* Strong order memory is required to store microcode command list */
	u8 *buf = (u8 *)dma330->buf;
	/* Channel Control Register */
	u32 ccr = 0;
	struct dma330_reqcfg reqcfg;
	cmd_line = 0;

	if (!buf) {
		debug("ERROR DMA330 : DMA Microcode buffer pointer is NULL\n");
		return -EINVAL;
	}

	debug("INFO: Write zeroes -> ");
	debug("0x%08x size=0x%08x\n", dma330->dst_addr, data_size_byte);

	/* for burst, always use the maximum burst size and length */
	dma330->brst_size = DMA330_MAX_BURST_SIZE;
	dma330->brst_len = 16;
	dma330->single_brst_size = 1;

	/* burst_size = 2 ^ brst_size */
	burst_size = 1 << dma330->brst_size;

	/* Setup the command list */
	/* DMAMOV DAR, x->dst_addr */
	off += _emit_mov(&buf[off], DAR, dma330->dst_addr);

	/* Preparing the CCR value */
	/* Enable auto increment */
	reqcfg.dst_inc = 1;
	/* Disable auto increment (not applicable) */
	reqcfg.src_inc = 0;
	/* Noncacheable but bufferable */
	reqcfg.dcctl = 0x1;
	/* Noncacheable and bufferable */
	reqcfg.scctl = 0x1;
	/* 1 - Priviledge  */
	reqcfg.privileged = 1;
	/* 0 - Data access */
	reqcfg.insnaccess = 0;
	/* 0 - No endian swap */
	reqcfg.swap = 0;

	if (dma330->reg_base == DMA330_BASE_NON_SECURE) {
		/* Non Secure mode */
		reqcfg.nonsecure = 1;
	} else {
		/* Secure mode */
		reqcfg.nonsecure = 0;
	}

	/* DMA burst length */
	reqcfg.brst_len = dma330->brst_len;
	/* DMA burst size */
	reqcfg.brst_size = dma330->brst_size;
	/* Preparing the CCR value */
	ccr = _prepare_ccr(&reqcfg);
	/* DMAMOV CCR, ccr */
	off += _emit_mov(&buf[off], CCR, ccr);

	/* BURST */
	/* Can initiate a burst? */
	while (data_size_byte >= burst_size * dma330->brst_len) {
		lcnt0 = data_size_byte / (burst_size * dma330->brst_len);
		lcnt1 = 0;
		if (lcnt0 >= 256 * 256)
			lcnt0 = lcnt1 = 256;
		else if (lcnt0 >= 256) {
			lcnt1 = lcnt0 / 256;
			lcnt0 = 256;
		}

		data_size_byte = data_size_byte -
			(burst_size * dma330->brst_len * lcnt0 * lcnt1);

		debug("Transferring 0x%08x Remain 0x%08x\n", (burst_size *
			dma330->brst_len * lcnt0 * lcnt1), data_size_byte);
		debug("Running burst - brst_size=2^%d, brst_len=%d,",
		      dma330->brst_size, dma330->brst_len);
		debug("lcnt0=%d, lcnt1=%d\n", lcnt0, lcnt1);

		if (lcnt1) {
			/* DMALP1 */
			off += _emit_lp(&buf[off], 1, lcnt1);
			loopjmp1 = off;
		}
		/* DMALP0 */
		off += _emit_lp(&buf[off], 0, lcnt0);
		loopjmp0 = off;
		/* DMALSTZ */
		off += _emit_stz(&buf[off]);
		/* DMALP0END */
		struct _arg_lpend lpend;
		lpend.cond = ALWAYS;
		lpend.forever = 0;
		/* Loop cnt 0 */
		lpend.loop = 0;
		lpend.bjump = off - loopjmp0;
		off += _emit_lpend(&buf[off], &lpend);
		/* DMALP1END */
		if (lcnt1) {
			struct _arg_lpend lpend;
			lpend.cond = ALWAYS;
			lpend.forever = 0;
			/* Loop cnt 1*/
			lpend.loop = 1;
			lpend.bjump = off - loopjmp1;
			off += _emit_lpend(&buf[off], &lpend);
		}
		/* Ensure the microcode don't exceed buffer size */
		if (off > dma330->buf_size) {
			debug("ERROR DMA330 : Exceed buffer size\n");
			return -ENOMEM;
		}
	}

	/* SINGLE */
	dma330->brst_size = dma330->single_brst_size;
	dma330->brst_len = 1;
	/* burst_size = 2 ^ brst_size */
	burst_size = (1 << dma330->brst_size);
	lcnt0 = data_size_byte / (burst_size * dma330->brst_len);

	/* ensure all data will be transfered */
	data_size_byte = data_size_byte -
		(burst_size * dma330->brst_len * lcnt0);
	if (data_size_byte) {
		debug("ERROR DMA330 : Detected the possibility of ");
		debug("untransfered data. Please ensure correct single ");
		debug("burst size\n");
	}

	if (lcnt0) {
		debug("Transferring 0x%08x Remain 0x%08x\n", (burst_size *
		     dma330->brst_len * lcnt0), data_size_byte);
		debug("Running single - brst_size=2^%d, brst_len=%d",
		      dma330->brst_size, dma330->brst_len);
		debug("lcnt0=%d\n", lcnt0);

		/* Preparing the CCR value */
		/* DMA burst length */
		reqcfg.brst_len = dma330->brst_len;
		/* DMA burst size */
		reqcfg.brst_size = dma330->brst_size;
		ccr = _prepare_ccr(&reqcfg);
		/* DMAMOV CCR, ccr */
		off += _emit_mov(&buf[off], CCR, ccr);

		/* DMALP0 */
		off += _emit_lp(&buf[off], 0, lcnt0);
		loopjmp0 = off;
		/* DMALSTZ */
		off += _emit_stz(&buf[off]);
		/* DMALPEND */
		struct _arg_lpend lpend1;
		lpend1.cond = ALWAYS;
		lpend1.forever = 0;
		/* Loop cnt 0 */
		lpend1.loop = 0;
		lpend1.bjump = off - loopjmp0;
		off += _emit_lpend(&buf[off], &lpend1);
		/* Ensure the microcode don't exceed buffer size */
		if (off > dma330->buf_size) {
			debug("ERROR DMA330 : Exceed buffer size\n");
			return -ENOMEM;
		}
	}

	/* DMAEND */
	off += _emit_end(&buf[off]);

	return 0;
}
