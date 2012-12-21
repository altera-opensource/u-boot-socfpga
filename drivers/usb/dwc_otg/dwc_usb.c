/*
 * Copyright (C) 2012
 * Altera Corporation <www.altera.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */

#include <asm/types.h>
#include <common.h>

#include <usb.h>
#include <usb_defs.h>

#define CONFIG_SYS_USB0_BASE 0xffb00000

#define dwc_read32(x) (*(volatile unsigned long *)(x))
#define dwc_write32(x, y) (*(volatile unsigned long *)(x) = (y))

/* MSC control transfers */
#define USB_MSC_BBB_RESET		0xFF
#define USB_MSC_BBB_GET_MAX_LUN		0xFE

#define CONFIG_DWC_USB_NPTXFDEP		8192
#define CONFIG_DWC_USB_NPTXFSTADDR	0
#define CONFIG_DWC_USB_RXFDEP		8192

#define DWC_USBCFG_ULPI_EXT_VBUS_DRV	1

#define internal_base CONFIG_SYS_USB0_BASE

#define	DWC_GINTSTS			0x014
#define DWC_GINTSTS_CURMOD		(1<<0)
#define DWC_GINTSTS_CONIDSTSCHNG	(1<<28)
#define DWC_GINTSTS_RXFLVL		(1<<4)
#define DWC_GINTSTS_SOF			(1<<3)
#define DWC_GINTSTS_USBRESET		(1<<12)
#define DWC_GINTSTS_PRTINT		(1<<24)

#define	DWC_GOTGCTL			0x000
#define DWC_GOTGCTL_CONIDSTS		(1<<16)
#define DWC_GOTGCTL_HNPREQ		(1<<9)
#define DWC_GOTGCTL_DEVHNPEN		(1<<11)

#define	DWC_GINTMSK			0x018
#define DWC_GINTMSK_SOF			(1<<3)
#define DWC_GINTMSK_PRTINTMSK		(1<<24)
#define DWC_GINTMSK_ENUMDONE		(1<<13)
#define DWC_GINTMSK_EARLYSUSP		(1<<10)
#define DWC_GINTMSK_USBSUSP		(1<<11)
#define DWC_GINTMSK_SOFINTR		(1<<3)
#define DWC_GINTMSK_NPTXEMPMSK		(1<<5)
#define DWC_GINTMSK_RXFLVL		(1<<4)
#define DWC_GINTMSK_OTGINTR		(1<<2)
#define DWC_GINTMSK_MODEMIS		(1<<1)
#define DWC_GINTMSK_USBRESET		(1<<12)

#define DWC_HPRT			0x0440
#define DWC_HPRT_PRTPWR			(1<<12)
#define DWC_HPRT_PRTRST			(1<<8)
#define DWC_HPRT_PRTENA			(1<<2)
#define DWC_HPRT_PRTDET			(1<<1)
#define DWC_HPRT_PRTRST			(1<<8)
#define DWC_HPRT_PRTENCHNG		(1<<3)
#define DWC_HPRT_PRTSPD			(1<<17)
#define DWC_HPRT_PRTSPD_MSK		(3<<17)
#define DWC_HPRT_PRTSPD_HS		(0<<17)
#define DWC_HPRT_PRTSPD_FS		(1<<17)
#define DWC_HPRT_PRTSPD_LS		(2<<17)
#define DWC_HPRT_FSLSUPP		(1<<2)
#define DWC_HPRT_PRTCONNSTS		(1<<0)

#define DWC_HCTSIZN(x)			(0x510+0x20*x)
#define DWC_HCTSIZN_PKTCNT(c)		(c << 19)
#define DWC_HCTSIZN_XFERSIZE(x)		(x << 0)
#define DWC_HCTSIZN_PID(x)		(x << 29)

#define DWC_HCINT(x)			(0x508+0x20*x)
#define DWC_HCINTMSK			0x50C
#define DWC_HCINT_XFERCOMP		(1<<0)

#define DWC_HCCHAR			(0x500)
#define DWC_HCCHARN(x)			(0x500+0x20*x)
#define DWC_HCCHARN_CHENA		(1 << 31)
#define DWC_HCCHARN_CHDIS		(1 << 30)
#define DWC_HCCHARN_EPDIR		(1 << 15)
#define DWC_HCCHARN_EPNUM(x)		(x << 11)
#define DWC_HCCHARN_MPS(x)		(x << 0)
#define DWC_HCCHARN_LSPDDEV(x)		(x<<17)
#define DWC_HCCHARN_EPTYPE(x)		(x<<18)
#define DWC_HCCHARN_MC(x)		(x<<20)
#define DWC_HCCHARN_DEVADDR(x)		(x<<22)

#define DWC_GAHBCFG			(0x8)
#define DWC_GAHBCFG_BURSTLEN(x)		(x<<1)
#define DWC_GAHBCFG_GLBLINTRMSK		(1<<0)
#define DWC_GAHBCFG_NPTXFEMPLVL		0x111
#define DWC_GAHBCFG_PTXFEMPLVL		0x111
#define DWC_GAHBCFG_DMAEN		(1<<5)

#define	DWC_GRXSTSP			0x020
#define DWC_GRXSTSP_PKTSTS(x)		(x<<17)

#define GNPTXFSIZ_NPTXFDEP(x)		(x<<16)

#define	DWC_GOTGINT			0x004
#define DWC_GOTGINT_DBNCEDONE		(1<<19)

#define DWC_DAINT		0x818
#define DWC_DEACHINT		0x838
#define DWC_DIEPINT(x)		(0x908+x*0x20)
#define DWC_DOEPINT(x)		(0xb08+x*0x20)

/* general bits */
#define DWC_GLBINTRMASK				0x0001
#define DWC_DMAENABLE				0x0020
#define DWC_NPTXEMPTYLVL_EMPTY			0x0080
#define DWC_NPTXEMPTYLVL_HALFEMPTY		0x0000
#define DWC_PTXEMPTYLVL_EMPTY			0x0100
#define DWC_PTXEMPTYLVL_HALFEMPTY		0x0000

#define DWC_SLAVE_ONLY_ARCH			0
#define DWC_EXT_DMA_ARCH			1
#define DWC_INT_DMA_ARCH			2

#define DWC_MODE_HNP_SRP_CAPABLE		0
#define DWC_MODE_SRP_ONLY_CAPABLE		1

#define DWC_MODE_NO_HNP_SRP_CAPABLE		2
#define DWC_MODE_SRP_CAPABLE_DEVICE		3
#define DWC_MODE_NO_SRP_CAPABLE_DEVICE		4
#define DWC_MODE_SRP_CAPABLE_HOST		5
#define DWC_MODE_NO_SRP_CAPABLE_HOST		6

/*
 * The Host Global Registers structure defines the size and relative field
 * offsets for the Host Mode Global Registers.  Host Global Registers offsets
 * 400h-7FFh.
*/
#define DWC_HCFG	0x400
#define DWC_HFIR	0x404
#define DWC_HFNUM	0x408
#define DWC_HPTXSTS	0x410
#define DWC_HAINT	0x414
#define DWC_HAINTMSK	0x418

/*
 * Host Channel Specific Registers. 500h-5FCh
 */

#define DWC_HCSPLT	0x504
#define DWC_HCTSIZ	0x510
#define DWC_HCDMA	0x514

#define	DWC_GUSBCFG	0x00C
#define	DWC_GRSTCTL	0x010


#define	DWC_GRXSTSR	0x01C
#define	DWC_GRXFSIZ	0x024

#define	DWC_GNPTXSTS	0x02C
#define	DWC_GI2CCTL	0x030
#define	DWC_VDCTL	0x034
#define	DWC_GGPIO	0x038
#define	DWC_GUID	0x03C
#define	DWC_GSNPSID	0x040
#define	DWC_GHWCFG1	0x044
#define	DWC_GHWCFG2	0x048
#define	DWC_GHWCFG3	0x04c
#define	DWC_GHWCFG4	0x050
#define	DWC_HPTXFSIZ	0x100
#define	DWC_DPTX_FSIZ_DIPTXF(x)	(0x104 + x * 4)	/* 15 <= x > 1 */


#define DWC_GLBINTRMASK			0x0001
#define DWC_DMAENABLE			0x0020
#define DWC_NPTXEMPTYLVL_EMPTY		0x0080
#define DWC_NPTXEMPTYLVL_HALFEMPTY	0x0000
#define DWC_PTXEMPTYLVL_EMPTY		0x0100
#define DWC_PTXEMPTYLVL_HALFEMPTY	0x0000

#define DWC_SLAVE_ONLY_ARCH		0
#define DWC_EXT_DMA_ARCH		1
#define DWC_INT_DMA_ARCH		2

#define DWC_MODE_HNP_SRP_CAPABLE	0
#define DWC_MODE_SRP_ONLY_CAPABLE	1
#define DWC_MODE_NO_HNP_SRP_CAPABLE	2
#define DWC_MODE_SRP_CAPABLE_DEVICE	3
#define DWC_MODE_NO_SRP_CAPABLE_DEVICE	4
#define DWC_MODE_SRP_CAPABLE_HOST	5
#define DWC_MODE_NO_SRP_CAPABLE_HOST	6

/*
 * The Host Global Registers structure defines the size and relative field
 * offsets for the Host Mode Global Registers.  Host Global Registers offsets
 * 400h-7FFh.
*/
#define DWC_HCFG	0x400
#define DWC_HFIR	0x404
#define DWC_HFNUM	0x408
#define DWC_HPTXSTS	0x410
#define DWC_HAINT	0x414
#define DWC_HAINTMSK	0x418

#define DWC_HCSPLT	 0x504
#define DWC_HCTSIZ	 0x510
#define DWC_HCDMA	 0x514

#define HCSPLT_SPLT_ENABLE(x) 0x0504

#define DWC_GUSBCFG_HNPCAP (1<<9)
#define DWC_GUSBCFG_SRPCAP (1<<8)
#define DWC_GUSBCFG_PHYSEL (1<<6)
#define DWC_GUSBCFG_ULPI_UTIM_SEL (1<<4)
#define DWC_GUSBCFG_PHYIF (1<<3)

#define DWC_GRXFSIX_RXFDEP(x) 1
#define	DWC_GNPTXFSIZ		0x028
#define DWC_GNPTXFSIZ_NPTXFDEP(x) 1
#define DWC_GNPTXFSIZ_NPTXSTADDR(x) 1

#define FIFO(channel) 0x1000*(channel+1)

struct core_init {
	int dma;
	int usb;
	int hnp_capable;
	int srp_capable;
	int fs;
};

struct hwcfg {
	int perio_tx_fifo_size[16];
	int tx_fifo_size[16];
	int num_in_eps;
	int num_out_eps;

} hwcfg;

void dwc_modify32(int address, u32 clear, u32 set)
{
	dwc_write32(address, (dwc_read32(address) & ~clear) | set);
}

int wait_for_connection(void)
{
	int loop = 0;
	u32 gintmsk = dwc_read32(internal_base + DWC_GINTMSK);
	gintmsk |= DWC_GINTMSK_PRTINTMSK;
	dwc_write32(internal_base + DWC_GINTMSK, gintmsk);

	u32 hcfg = dwc_read32(internal_base + DWC_HCFG);
	hcfg &= ~DWC_HPRT_FSLSUPP;
	dwc_write32(internal_base + DWC_HCFG, hcfg);


	dwc_write32(internal_base+DWC_GAHBCFG, 0x1);
	dwc_write32(internal_base+DWC_GUSBCFG, 0x00800000);
	dwc_write32(internal_base+DWC_HCFG, 0x4);

	u32 hprt = dwc_read32(internal_base + DWC_HPRT);
	hprt |= DWC_HPRT_PRTPWR;
	dwc_write32(internal_base + DWC_HPRT, hprt);

	/*
	 * ensure core is in A-host mode and connected
	 * we'll have to loop a couple of times, and wait a little
	 * so that prtpwr gets its time to init
	 */
	while (!(dwc_read32(internal_base+DWC_HPRT) & DWC_HPRT_PRTCONNSTS)
			&& loop < 500) {
		udelay(1000);
		loop++;
	}

	if ((dwc_read32(internal_base+DWC_GINTSTS) & DWC_GINTSTS_CURMOD) != 1)
		return -1;

	/* clears the port connected interrupt */
	dwc_write32(internal_base+DWC_HPRT,
		dwc_read32(internal_base+DWC_HPRT) | DWC_HPRT_PRTDET);

	return 0;
}

int port_reset(void)
{
	u32 hprt = dwc_read32(internal_base+DWC_HPRT);
	hprt |= DWC_HPRT_PRTRST;
	dwc_write32(internal_base + DWC_HPRT, hprt);
	{
		/*
		 * wait for minimum of 10ms for the reset complete
		 * wait 15ms instead, just to be sure
		 */
		udelay(15000);
	}
	hprt &= ~DWC_HPRT_PRTRST;
	dwc_write32(internal_base + DWC_HPRT, hprt);
	return 0;
}

int wait_for_debounce(void)
{
	/* if not hnp and srp capable, just return 0 */
	return 0;
/*	while (!(dwc_read32(internal_base+DWC_GOTGINT) &
		DWC_GOTGINT_DBNCEDONE)){
		udelay(100);
	}
	return 0;*/
}

int port_enabled(void)
{
	while (!(dwc_read32(internal_base+DWC_GINTSTS) & DWC_GINTSTS_PRTINT))
		udelay(100);
	return 0;
}

int port_speed(u32 speed)
{
	u32 hprt = dwc_read32(internal_base+DWC_HPRT);
	hprt &= ~DWC_HPRT_PRTSPD_MSK;
	hprt |= speed;

	while (!(dwc_read32(internal_base+DWC_GINTSTS) & DWC_GINTSTS_PRTINT))
		udelay(100);
	return 0;
}

int dyn_fifo_config(void)
{
	dwc_write32(internal_base+DWC_GRXFSIZ,
			DWC_GRXFSIX_RXFDEP(CONFIG_DWC_USB_RXFDEP));
	dwc_write32(internal_base+DWC_GNPTXFSIZ,
			DWC_GNPTXFSIZ_NPTXFDEP(CONFIG_DWC_USB_NPTXFDEP));
	dwc_write32(internal_base+DWC_GNPTXFSIZ,
			DWC_GNPTXFSIZ_NPTXSTADDR(CONFIG_DWC_USB_NPTXFSTADDR));
	return 0;
}

int dwc_host_init(void)
{
	if (wait_for_connection())
		return 1<<2;
	if (port_reset())
		return 1<<3;
	if (port_enabled())
		return 1<<5;
	if (port_speed(DWC_HPRT_PRTSPD_HS))
		return 1<<4;
	if (wait_for_debounce())
		return 1<<4;
	return 0;
}

void dwc_core_init(struct core_init init)
{
	int otg_base = internal_base;
	u32 gahbcfg = dwc_read32(otg_base + DWC_GAHBCFG);
	if (init.dma)
		gahbcfg |= DWC_GAHBCFG_DMAEN;
	else
		gahbcfg &= ~DWC_GAHBCFG_DMAEN;
	gahbcfg |= DWC_GAHBCFG_BURSTLEN(0);
	gahbcfg |= DWC_GAHBCFG_GLBLINTRMSK;
	dwc_write32(otg_base + DWC_GAHBCFG, gahbcfg);

	/*
	 * Configuring the txfifo, only in slave mode we need this interrupt
	 * don't really need this, since we're not using interrupt
	 */
	if (!init.dma) {
		dwc_modify32(otg_base+DWC_GAHBCFG, 0, DWC_GAHBCFG_NPTXFEMPLVL);
		dwc_modify32(otg_base+DWC_GAHBCFG, 0, DWC_GAHBCFG_PTXFEMPLVL);
	}

	u32 gintmsk = dwc_read32(otg_base+DWC_GINTMSK);
	gintmsk &= ~(DWC_GINTMSK_RXFLVL);
	dwc_write32(otg_base + DWC_GINTMSK, gintmsk);

	u32 gusbcfg = dwc_read32(otg_base+DWC_GUSBCFG);
	gusbcfg = init.hnp_capable ? gusbcfg | DWC_GUSBCFG_HNPCAP
			: gusbcfg & ~(DWC_GUSBCFG_HNPCAP);
	gusbcfg = init.srp_capable ? gusbcfg | DWC_GUSBCFG_SRPCAP
			: gusbcfg & ~(DWC_GUSBCFG_SRPCAP);
	gusbcfg &= ~DWC_GUSBCFG_PHYSEL;
	gusbcfg |= DWC_GUSBCFG_ULPI_UTIM_SEL;
	gusbcfg &= ~DWC_GUSBCFG_PHYIF;
	dwc_write32(otg_base+DWC_GUSBCFG, gusbcfg);

	gintmsk |= DWC_GINTMSK_OTGINTR;
	gintmsk |= DWC_GINTMSK_MODEMIS;
	dwc_write32(otg_base + DWC_GINTMSK, gintmsk);
}

enum ep_type {
	EP_CTRL = 0,
	EP_ISOC = 1,
	EP_BULK = 2,
	EP_INTR = 3,
};

struct packet {
	int channel;
	int endpoint;
	int packet_count;
	int xfer_size;
	int ep_is_in;
	int devaddr;
	enum ep_type type;
	int mps;
	int pid;
};

int dwc_setup_transfer(struct packet p)
{
	u32 hctsizn = 0x0;
	u32 hccharn = 0x0;
	u32 haintmsk = 0x0;
	u32 hcintmsk = 0x0;

	haintmsk = dwc_read32(internal_base+DWC_HAINTMSK) | (1 << p.channel);
	dwc_write32(internal_base+DWC_HAINTMSK, haintmsk);

	hcintmsk = dwc_read32(internal_base+DWC_HCINTMSK) | DWC_HCINT_XFERCOMP;
	dwc_write32(internal_base+DWC_HCINTMSK, hcintmsk);

	hctsizn |= DWC_HCTSIZN_PKTCNT(p.packet_count);
	hctsizn |= DWC_HCTSIZN_XFERSIZE(p.xfer_size);
	hctsizn |= DWC_HCTSIZN_PID(p.pid);
	dwc_write32(internal_base+DWC_HCTSIZN(p.channel), hctsizn);

	hccharn = p.ep_is_in ? hccharn | DWC_HCCHARN_EPDIR
				: hccharn & ~DWC_HCCHARN_EPDIR;
	hccharn |= DWC_HCCHARN_DEVADDR(p.devaddr);
	hccharn |= DWC_HCCHARN_EPTYPE(p.type);
	hccharn |= DWC_HCCHARN_EPNUM(p.endpoint);
	hccharn |= DWC_HCCHARN_MPS(p.mps);
	hccharn |= 1<<20;
	hccharn |= DWC_HCCHARN_CHENA;
	dwc_write32(internal_base+DWC_HCCHARN(p.channel), hccharn);


	return 0;
}

static int dwc_read_packet(struct packet p, void *odata)
{
	u8 *data = (u8 *)odata;
	/* clear the xfer complete interrupt */
	dwc_write32(internal_base+DWC_HCINT(p.channel), 1);

	/* wait for rxfifo non empty */
	while (!(dwc_read32(internal_base+DWC_GINTSTS) & DWC_GINTSTS_RXFLVL))
		udelay(100);

	int read_total = 0;

	int packet_read = 0;

	/* masking rxfifo not empty interrupt */
	dwc_write32(internal_base+DWC_GINTMSK,
		dwc_read32(internal_base+DWC_GINTMSK) | DWC_GINTSTS_RXFLVL);
	while (1) {
		u32 packet_status = dwc_read32(internal_base+DWC_GRXSTSP);
		if ((packet_status & DWC_GRXSTSP_PKTSTS(0xF)) ==
			DWC_GRXSTSP_PKTSTS(3)) {
			break;
		}
		if ((packet_status & DWC_GRXSTSP_PKTSTS(0xF)) ==
			DWC_GRXSTSP_PKTSTS(2)) {
			int bytecount = ((packet_status >> 4) & (0x7FF));

			packet_read++;
			read_total += bytecount;

			while (bytecount > 0) {
				u32 read = dwc_read32(internal_base +
						FIFO(p.channel));
				if (bytecount--)
					*data++ = read & 0xff;
				if (bytecount--)
					*data++ = (read & (0xff << 8)) >> 8;
				if (bytecount--)
					*data++ = (read & (0xff << 16)) >> 16;
				if (bytecount--)
					*data++ = (read & (0xff << 24)) >> 24;
			}
		}
		/* re-enable the channel if pktcnt is > 0 */
		if (packet_read < p.packet_count) {
			u32 hccharn = dwc_read32(internal_base+
				DWC_HCCHARN(p.channel));
			hccharn |= DWC_HCCHARN_CHENA;
			dwc_write32(internal_base+DWC_HCCHARN(p.channel),
				hccharn);
		}
	}

	/* wait for xfer complete interrupt. */
	while (!(dwc_read32(internal_base+DWC_HCINT(p.channel)) & 1<<0))
		udelay(100);
	/* clear the xfer complete interrupt */
	dwc_write32(internal_base+DWC_HCINT(p.channel), 1);

	return read_total;
}

static int dwc_send_packet(struct packet p, u32 *data, int datasize)
{

	/* clear xfer complete interrupt. */
	dwc_write32(internal_base+DWC_HCINT(p.channel), 1);
	int i = 0;
	for (; i < datasize; i++)
		dwc_write32(internal_base + FIFO(p.channel), data[i]);
	while (!(dwc_read32(internal_base+DWC_HCINT(p.channel)) & (1<<0)))
		udelay(100);
	/* disables the xfer complete interrupt */
	dwc_write32(internal_base+DWC_HCINT(p.channel),
		dwc_read32(internal_base+DWC_HCINT(p.channel)) | (1<<0));
	return 0;
}

int usb_lowlevel_init(void)
{
	struct core_init init;
	init.dma = 0;
	init.hnp_capable = 0;
	init.srp_capable = 0;
	init.fs = 0;
	dwc_core_init(init);

	if (dwc_host_init() != 0)
		return -1;

	return 0;
}

int usb_lowlevel_stop(void)
{
	return 0;
}

int submit_bulk_msg(struct usb_device *dev, unsigned long pipe,
			void *buffer, int transfer_len)
{
	int dir_out = usb_pipeout(pipe);
	int ep = usb_pipeendpoint(pipe);

	struct packet bulk_msg;
	bulk_msg.channel = 0;
	bulk_msg.endpoint = ep;
	bulk_msg.packet_count = (transfer_len-1) / 512 + 1;
	bulk_msg.xfer_size = transfer_len;
	bulk_msg.ep_is_in = !dir_out;
	bulk_msg.devaddr = usb_pipedevice(pipe);
	bulk_msg.type = EP_BULK;
	bulk_msg.mps = 512;
	bulk_msg.pid = 0;

	dwc_setup_transfer(bulk_msg);

	if (dir_out)
		dwc_send_packet(bulk_msg, buffer, transfer_len/4+1);
	else {
		dwc_read_packet(bulk_msg, buffer);

		/* halt the channel */
		dwc_write32(internal_base+DWC_HCCHARN(bulk_msg.channel),
			dwc_read32(internal_base+DWC_HCCHARN(bulk_msg.channel))
			| DWC_HCCHARN_CHDIS);
		u32 packet_status = dwc_read32(internal_base+DWC_GRXSTSP);
		if ((packet_status & DWC_GRXSTSP_PKTSTS(0xf)) !=
		DWC_GRXSTSP_PKTSTS(0x7))
			printf("WARNING: channel not halted, pktsts:%x\n",
				packet_status);
	}

	/* bulk transfer is complete */
	dev->status = 0;
	dev->act_len = transfer_len;

	return 0;
}

static int ctrlreq_setup_phase(struct usb_device *dev,
				struct devrequest *setup, unsigned long pipe) {
	struct packet control_msg;

	control_msg.channel = 0;
	control_msg.endpoint = 0;
	control_msg.packet_count = 1;
	control_msg.xfer_size = sizeof(struct devrequest);
	control_msg.ep_is_in = 0;
	control_msg.devaddr = usb_pipedevice(pipe);
	control_msg.type = EP_CTRL;
	control_msg.mps = 64;
	control_msg.pid = 3;

	/* clear xfer complete interrupt. */
	dwc_write32(internal_base+DWC_HCINT(control_msg.channel), 1);

	dwc_setup_transfer(control_msg);

	dwc_send_packet(control_msg, (void *)setup,
		sizeof(struct devrequest)/4);

	control_msg.xfer_size = 0;

	/* wait for xfer complete */
	while ((dwc_read32(internal_base+DWC_HCINT(control_msg.channel))
		& (1<<31)) != 0)
		udelay(100);
	return 0;
}

static int ctrlreq_out_data_phase(struct usb_device *dev, unsigned long pipe,
			int len, void *buffer) {
	int result;
	int devnum = usb_pipedevice(pipe);
	int epnum = usb_pipeendpoint(pipe);
	int txlen = 0;
	int nextlen = 0;
	u8 maxpktsize = (1<<dev->maxpacketsize) * 8;
	int packet_count = len / maxpktsize + 1;

	struct packet control_msg;
	while (txlen < len) {

		nextlen = ((len-txlen) > maxpktsize) ? maxpktsize : (len-txlen);

		control_msg.channel = 0;
		control_msg.endpoint = epnum;
		control_msg.packet_count = packet_count;
		control_msg.xfer_size = nextlen;
		control_msg.ep_is_in = 0;
		control_msg.devaddr = devnum;
		control_msg.type = EP_CTRL;
		control_msg.mps = 3;
		control_msg.pid = 3;

		result = dwc_send_packet(control_msg, buffer, txlen);

		if (result != 0)
			break;

		txlen += nextlen;
		dev->act_len = txlen;
	}

	/* wait for xfer complete */
	while ((dwc_read32(internal_base+DWC_HCINT(control_msg.channel))
		& (1<<31)) != 0) {
		udelay(100);
	}
	return result;
}

static int ctrlreq_out_status_phase(struct usb_device *dev, unsigned long pipe)
{
	struct packet control_msg;

	control_msg.channel = 0;
	control_msg.endpoint = 0;
	control_msg.packet_count = 1;
	control_msg.xfer_size = 0;
	control_msg.ep_is_in = 1;
	control_msg.devaddr = usb_pipedevice(pipe);
	control_msg.type = EP_CTRL;
	control_msg.mps = 1;
	control_msg.pid = 2;

	dwc_write32(internal_base+DWC_HCINT(control_msg.channel), 0x1);

	dwc_setup_transfer(control_msg);
	dwc_write32(internal_base+DWC_HCCHARN(control_msg.channel),
		dwc_read32(internal_base+DWC_HCCHARN(control_msg.channel))
		| DWC_HCCHARN_CHENA);

	/* status phase, shouldn't have any IN data, use a tmp variable
	to read */
	u32 tmp[10];
	dwc_read_packet(control_msg, tmp);

	dwc_write32(internal_base+DWC_HCINT(control_msg.channel),
		dwc_read32(internal_base+DWC_HCINT(control_msg.channel))
		| (1<<0));

	/* nothing to send here, just remove the xfer complete status */
	dwc_write32(internal_base+DWC_HCINT(control_msg.channel),
		dwc_read32(internal_base+DWC_HCINT(control_msg.channel))
		& ~(1<<0));

	return 0;
}

static int ctrlreq_in_data_phase(struct usb_device *dev, unsigned long pipe,
				int len, void *buffer) {
	u32 rxlen = 0;
	u32 nextlen = 0;
	u8  maxpktsize = (1 << dev->maxpacketsize) * 8;
	u32  *rxbuff = (u32 *)buffer;
	u8  rxedlength;
	struct packet control_msg;

	while (rxlen < len) {
		/* Determine the next read length */
		nextlen = ((len-rxlen) > maxpktsize) ? maxpktsize : (len-rxlen);

		/* Set the ReqPkt bit */
		control_msg.channel = 0;
		control_msg.endpoint = 0;
		control_msg.packet_count = 1;
		control_msg.xfer_size = nextlen;
		control_msg.ep_is_in = 1;
		control_msg.devaddr = usb_pipedevice(pipe);
		control_msg.type = EP_CTRL;
		control_msg.mps = 64;
		control_msg.pid = 2;

		dwc_setup_transfer(control_msg);
		dwc_setup_transfer(control_msg);

		rxedlength = dwc_read_packet(control_msg,
			(u32 *)&rxbuff[rxlen]);

		/* halt the channel... */
		dwc_write32(internal_base+DWC_HCCHARN(control_msg.channel),
			dwc_read32(
			internal_base+DWC_HCCHARN(control_msg.channel)
			) | DWC_HCCHARN_CHDIS);
		u32 packet_status = dwc_read32(internal_base+DWC_GRXSTSP);
		if ((packet_status & DWC_GRXSTSP_PKTSTS(0xf))
			!= DWC_GRXSTSP_PKTSTS(0x7))
			printf("WARNING: channel not halted, pktsts:%x\n",
				(u32)packet_status);

		/* clear xfer complete interrupt. */
		dwc_write32(internal_base+DWC_HCINT(control_msg.channel), 1);

		/* short packet? */
		if (rxedlength != nextlen) {
			dev->act_len += rxedlength;
			break;
		}
		rxlen += nextlen;
		dev->act_len = rxlen;
	}

	return 0;
}

static int ctrlreq_in_status_phase(struct usb_device *dev, unsigned long pipe)
{
	struct packet control_msg;

	control_msg.channel = 0;
	control_msg.endpoint = 0;
	control_msg.packet_count = 0;
	control_msg.xfer_size = 0;
	control_msg.ep_is_in = 0;
	control_msg.devaddr = usb_pipedevice(pipe);
	control_msg.type = EP_CTRL;
	control_msg.mps = 64;
	control_msg.pid = 0;

	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));
	dwc_read32(internal_base+DWC_HCINT(control_msg.channel));

	/* clear xfer complete interrupt. */
	dwc_write32(internal_base+DWC_HCINT(control_msg.channel), 1);

	dwc_setup_transfer(control_msg);

	/* should wait for interrupt hcint here... */
	while (!(dwc_read32(internal_base+DWC_HCINT(control_msg.channel))
		& 0x1))
		udelay(100);

	/* clear xfer complete interrupt. */
	dwc_write32(internal_base+DWC_HCINT(control_msg.channel), 1);

	return 0;
}

/*
* Interrupt not implemented, currently only supports mass storage
*/
int submit_int_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
	int transfer_len, int interval) {
	return -1;
}



int submit_control_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
			int transfer_len, struct devrequest *setup) {

	if (ctrlreq_setup_phase(dev, setup, pipe) < 0)
		return 0;

	switch (setup->request) {
	case USB_REQ_GET_DESCRIPTOR:
	case USB_REQ_GET_CONFIGURATION:
	case USB_REQ_GET_INTERFACE:
	case USB_REQ_GET_STATUS:
	case USB_MSC_BBB_GET_MAX_LUN:
		/* control transfer in-data-phase */
		if (ctrlreq_in_data_phase(dev, pipe, transfer_len, buffer) < 0)
			return 0;
		/* control transfer out-status-phase */
		if (ctrlreq_in_status_phase(dev, pipe) < 0)
			return 0;
		break;

	case USB_REQ_SET_CONFIGURATION:
	case USB_REQ_SET_ADDRESS:
	case USB_REQ_SET_FEATURE:
	case USB_REQ_SET_INTERFACE:
	case USB_REQ_CLEAR_FEATURE:
	case USB_MSC_BBB_RESET:
		/* control transfer in status phase */
		if (ctrlreq_out_status_phase(dev, pipe) < 0)
			return 0;
		break;

	case USB_REQ_SET_DESCRIPTOR:
		/* control transfer out data phase */
		if (ctrlreq_out_data_phase(dev, pipe, transfer_len, buffer) < 0)
			return 0;
		/* control transfer in status phase */
		if (ctrlreq_out_status_phase(dev, pipe) < 0)
			return 0;
		break;

	default:
		/* unhandled control transfer */
		return -1;
	}

	dev->status = 0;
	dev->act_len = transfer_len;

	return transfer_len;
}
