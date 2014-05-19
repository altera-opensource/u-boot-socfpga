/*
 * Copyright (C) 2012 Oleksandr Tymoshenko <gonzo at freebsd.org>
 *
 * SPDX-License-Identifier:     GPL-2.0
 */

#include <common.h>
#include <usb.h>
#include <malloc.h>
#include "dwc2_otg.h"
#include "dwc2_otg_regs.h"
#include "dwc2_otg_core_if.h"

#define STATUS_ACK_HLT_COMPL 0x23
#define CHANNEL 0

#define DWC_OTG_HCD_STATUS_BUF_SIZE 64
#define DWC_OTG_HCD_DATA_BUF_SIZE (64 * 1024)

static int root_hub_devnum;

static uint8_t *allocated_buffer;
static uint8_t *aligned_buffer;
static uint8_t *status_buffer;

static dwc_otg_core_if_t g_core_if;

#define MAX_DEVICE 16
#define MAX_ENDPOINT 16
int bulk_data_toggle[MAX_DEVICE][MAX_ENDPOINT];
int control_data_toggle[MAX_DEVICE][MAX_ENDPOINT];

void do_hang(int line, uint32_t d)
{
	printf("DWC OTG: hang at line %d: %08x\n", line, d);
	while (1)
		udelay(10);
}

void handle_error(int line, uint32_t d)
{
	hcint_data_t hcint;
	hcint.d32 = d;

	printf("Error condition at line %d: ", line);
	if (hcint.b.ahberr)
		printf(" AHBERR");
	if (hcint.b.stall)
		printf(" STALL");
	if (hcint.b.bblerr)
		printf(" NAK");
	if (hcint.b.ack)
		printf(" ACK");
	if (hcint.b.nyet)
		printf(" NYET");
	if (hcint.b.xacterr)
		printf(" XACTERR");
	if (hcint.b.datatglerr)
		printf(" DATATGLERR");
	printf("\n");
}

/*
 * U-Boot USB interface
 */
int usb_lowlevel_init(int index, void **controller)
{
	/*
	 * We need doubleword-aligned buffers for DMA transfers
	 */
	allocated_buffer = (uint8_t *)malloc(DWC_OTG_HCD_STATUS_BUF_SIZE + DWC_OTG_HCD_DATA_BUF_SIZE + 8);
	uint32_t addr = (uint32_t)allocated_buffer;
	aligned_buffer = (uint8_t *) ((addr + 7) & ~7);
	status_buffer = (uint8_t *)((uint32_t)aligned_buffer + DWC_OTG_HCD_DATA_BUF_SIZE);
	int i, j;
	hprt0_data_t hprt0 = {.d32 = 0 };

	root_hub_devnum = 0;
	memset(&g_core_if, 0, sizeof(g_core_if));
	dwc_otg_cil_init(&g_core_if, (uint32_t *)CONFIG_SYS_USB_ADDRESS);

	if ((g_core_if.snpsid & 0xFFFFF000) !=
		0x4F542000) {
		printf("SNPSID is invalid (not DWC OTG device): %08x\n", g_core_if.snpsid);
		return -1;
	}

	dwc_otg_core_init(&g_core_if);
	dwc_otg_core_host_init(&g_core_if);

	hprt0.d32 = dwc_otg_read_hprt0(&g_core_if);
	hprt0.b.prtrst = 1;
	dwc_write_reg32(g_core_if.host_if->hprt0, hprt0.d32);
	mdelay(50);
	hprt0.b.prtrst = 0;
	dwc_write_reg32(g_core_if.host_if->hprt0, hprt0.d32);

	for (i = 0; i < MAX_DEVICE; i++) {
		for (j = 0; j < MAX_ENDPOINT; j++) {
			control_data_toggle[i][j] = DWC_OTG_HC_PID_DATA1;
			bulk_data_toggle[i][j] = DWC_OTG_HC_PID_DATA0;
		}
	}

	return 0;
}

int usb_lowlevel_stop(int index)
{
	free(allocated_buffer);

	return 0;
}

/* based on usb_ohci.c */
#define min_t(type, x, y) \
		({ type __x = (x); type __y = (y); __x < __y ? __x : __y; })
/*-------------------------------------------------------------------------*
 * Virtual Root Hub
 *-------------------------------------------------------------------------*/

/* Device descriptor */
static __u8 root_hub_dev_des[] = {
	0x12,			/* __u8  bLength; */
	0x01,			/* __u8  bDescriptorType; Device */
	0x10,			/* __u16 bcdUSB; v1.1 */
	0x01,
	0x09,			/* __u8  bDeviceClass; HUB_CLASSCODE */
	0x00,			/* __u8  bDeviceSubClass; */
	0x00,			/* __u8  bDeviceProtocol; */
	0x08,			/* __u8  bMaxPacketSize0; 8 Bytes */
	0x00,			/* __u16 idVendor; */
	0x00,
	0x00,			/* __u16 idProduct; */
	0x00,
	0x00,			/* __u16 bcdDevice; */
	0x00,
	0x01,			/* __u8  iManufacturer; */
	0x02,			/* __u8  iProduct; */
	0x00,			/* __u8  iSerialNumber; */
	0x01			/* __u8  bNumConfigurations; */
};

/* Configuration descriptor */
static __u8 root_hub_config_des[] = {
	0x09,			/* __u8  bLength; */
	0x02,			/* __u8  bDescriptorType; Configuration */
	0x19,			/* __u16 wTotalLength; */
	0x00,
	0x01,			/* __u8  bNumInterfaces; */
	0x01,			/* __u8  bConfigurationValue; */
	0x00,			/* __u8  iConfiguration; */
	0x40,			/* __u8  bmAttributes; */

	0x00,			/* __u8  MaxPower; */

				/* interface */
	0x09,			/* __u8  if_bLength; */
	0x04,			/* __u8  if_bDescriptorType; Interface */
	0x00,			/* __u8  if_bInterfaceNumber; */
	0x00,			/* __u8  if_bAlternateSetting; */
	0x01,			/* __u8  if_bNumEndpoints; */
	0x09,			/* __u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,			/* __u8  if_bInterfaceSubClass; */
	0x00,			/* __u8  if_bInterfaceProtocol; */
	0x00,			/* __u8  if_iInterface; */

				/* endpoint */
	0x07,			/* __u8  ep_bLength; */
	0x05,			/* __u8  ep_bDescriptorType; Endpoint */
	0x81,			/* __u8  ep_bEndpointAddress; IN Endpoint 1 */
	0x03,			/* __u8  ep_bmAttributes; Interrupt */
	0x08,			/* __u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
	0x00,
	0xff			/* __u8  ep_bInterval; 255 ms */
};

static unsigned char root_hub_str_index0[] = {
	0x04,			/*  __u8  bLength; */
	0x03,			/*  __u8  bDescriptorType; String-descriptor */
	0x09,			/*  __u8  lang ID */
	0x04,			/*  __u8  lang ID */
};

static unsigned char root_hub_str_index1[] = {
	32,			/*  __u8  bLength; */
	0x03,			/*  __u8  bDescriptorType; String-descriptor */
	'D',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'W',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'C',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	' ',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'O',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'T',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'G',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	' ',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'R',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'o',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'o',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	't',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'H',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'u',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
	'b',			/*  __u8  Unicode */
	0,			/*  __u8  Unicode */
};

static int dwc_otg_submit_rh_msg(struct usb_device *dev, unsigned long pipe,
				 void *buffer, int transfer_len,
				 struct devrequest *cmd)
{
	int leni = transfer_len;
	int len = 0;
	int stat = 0;
	__u16 bmRType_bReq;
	__u16 wValue;
	__u16 wLength;
	unsigned char data[32];
	hprt0_data_t hprt0 = {.d32 = 0 };

	if (usb_pipeint(pipe)) {
		printf("Root-Hub submit IRQ: NOT implemented");
		return 0;
	}

	bmRType_bReq  = cmd->requesttype | (cmd->request << 8);
	wValue	      = cpu_to_le16 (cmd->value);
	wLength	      = cpu_to_le16 (cmd->length);

	switch (bmRType_bReq) {
	case RH_GET_STATUS:
		*(__u16 *)buffer = cpu_to_le16(1);
		len = 2;
		break;
	case RH_GET_STATUS | RH_INTERFACE:
		*(__u16 *)buffer = cpu_to_le16(0);
		len = 2;
		break;
	case RH_GET_STATUS | RH_ENDPOINT:
		*(__u16 *)buffer = cpu_to_le16(0);
		len = 2;
		break;
	case RH_GET_STATUS | RH_CLASS:
		*(__u32 *)buffer = cpu_to_le32(0);
		len = 4;
		break;
	case RH_GET_STATUS | RH_OTHER | RH_CLASS:
		{
			uint32_t port_status = 0;
			uint32_t port_change = 0;
			hprt0.d32 = dwc_read_reg32(g_core_if.host_if->hprt0);
			if (hprt0.b.prtconnsts)
				port_status |= USB_PORT_STAT_CONNECTION;
			if (hprt0.b.prtena)
				port_status |= USB_PORT_STAT_ENABLE;
			if (hprt0.b.prtsusp)
				port_status |= USB_PORT_STAT_SUSPEND;
			if (hprt0.b.prtovrcurract)
				port_status |= USB_PORT_STAT_OVERCURRENT;
			if (hprt0.b.prtrst)
				port_status |= USB_PORT_STAT_RESET;
			if (hprt0.b.prtpwr)
				port_status |= USB_PORT_STAT_POWER;

			port_status |= USB_PORT_STAT_HIGH_SPEED;

			if (hprt0.b.prtenchng)
				port_change |= USB_PORT_STAT_C_ENABLE;
			if (hprt0.b.prtconndet)
				port_change |= USB_PORT_STAT_C_CONNECTION;
			if (hprt0.b.prtovrcurrchng)
				port_change |= USB_PORT_STAT_C_OVERCURRENT;

			*(__u32 *)buffer = cpu_to_le32(port_status |
						(port_change << 16));
			len = 4;
		}
		break;
	case RH_CLEAR_FEATURE | RH_ENDPOINT:
	case RH_CLEAR_FEATURE | RH_CLASS:
		break;

	case RH_CLEAR_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
		case RH_C_PORT_CONNECTION:
			hprt0.d32 = dwc_read_reg32(g_core_if.host_if->hprt0);
			hprt0.b.prtconndet = 1;
			dwc_write_reg32(g_core_if.host_if->hprt0, hprt0.d32);
			break;
		}
		break;

	case RH_SET_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
		case (RH_PORT_SUSPEND):
			break;

		case (RH_PORT_RESET):
			hprt0.d32 = dwc_otg_read_hprt0(&g_core_if);
			hprt0.b.prtrst = 1;
			dwc_write_reg32(g_core_if.host_if->hprt0, hprt0.d32);
			mdelay(50);
			hprt0.b.prtrst = 0;
			dwc_write_reg32(g_core_if.host_if->hprt0, hprt0.d32);

			break;

		case (RH_PORT_POWER):
			hprt0.d32 = dwc_otg_read_hprt0(&g_core_if);
			hprt0.b.prtpwr = 1;
			dwc_write_reg32(g_core_if.host_if->hprt0, hprt0.d32);
			break;

		case (RH_PORT_ENABLE):
			break;
		}
		break;
	case RH_SET_ADDRESS:
		root_hub_devnum = wValue;
		break;
	case RH_GET_DESCRIPTOR:
		switch ((wValue & 0xff00) >> 8) {
		case (0x01): /* device descriptor */
			len = min_t(unsigned int,
				  leni,
				  min_t(unsigned int,
				      sizeof(root_hub_dev_des),
				      wLength));
			memcpy(buffer, root_hub_dev_des, len);
			break;
		case (0x02): /* configuration descriptor */
			len = min_t(unsigned int,
				  leni,
				  min_t(unsigned int,
				      sizeof(root_hub_config_des),
				      wLength));
			memcpy(buffer, root_hub_config_des, len);
			break;
		case (0x03): /* string descriptors */
			if (wValue == 0x0300) {
				len = min_t(unsigned int,
					  leni,
					  min_t(unsigned int,
					      sizeof(root_hub_str_index0),
					      wLength));
				memcpy(buffer, root_hub_str_index0, len);
			}
			if (wValue == 0x0301) {
				len = min_t(unsigned int,
					  leni,
					  min_t(unsigned int,
					      sizeof(root_hub_str_index1),
					      wLength));
				memcpy(buffer, root_hub_str_index1, len);
			}
			break;
		default:
			stat = USB_ST_STALLED;
		}
		break;

	case RH_GET_DESCRIPTOR | RH_CLASS:
	{
		__u32 temp = 0x00000001;

		data[0] = 9;		/* min length; */
		data[1] = 0x29;
		data[2] = temp & RH_A_NDP;
		data[3] = 0;
		if (temp & RH_A_PSM)
			data[3] |= 0x1;
		if (temp & RH_A_NOCP)
			data[3] |= 0x10;
		else if (temp & RH_A_OCPM)
			data[3] |= 0x8;

		/* corresponds to data[4-7] */
		data[5] = (temp & RH_A_POTPGT) >> 24;
		data[7] = temp & RH_B_DR;
		if (data[2] < 7) {
			data[8] = 0xff;
		} else {
			data[0] += 2;
			data[8] = (temp & RH_B_DR) >> 8;
			data[10] = data[9] = 0xff;
		}

		len = min_t(unsigned int, leni,
			    min_t(unsigned int, data[0], wLength));
		memcpy(buffer, data, len);
		break;
	}

	case RH_GET_CONFIGURATION:
		*(__u8 *) buffer = 0x01;
		len = 1;
		break;
	case RH_SET_CONFIGURATION:
		break;
	default:
		printf("unsupported root hub command\n");
		stat = USB_ST_STALLED;
	}

	mdelay(1);

	len = min_t(int, len, leni);

	dev->act_len = len;
	dev->status = stat;

	return stat;

}

int submit_bulk_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
		    int len)
{
	int devnum = usb_pipedevice(pipe);
	int ep = usb_pipeendpoint(pipe);
	int max = usb_maxpacket(dev, pipe);
	int done = 0;
	hctsiz_data_t hctsiz;
	dwc_otg_host_if_t *host_if = g_core_if.host_if;
	dwc_otg_hc_regs_t *hc_regs;
	hcint_data_t hcint;
	hcint_data_t hcint_new;
	uint32_t max_hc_xfer_size = g_core_if.core_params->max_transfer_size;
	uint16_t max_hc_pkt_count = g_core_if.core_params->max_packet_count;
	uint32_t xfer_len;
	uint32_t num_packets;
	int stop_transfer = 0;

	if (len > DWC_OTG_HCD_DATA_BUF_SIZE) {
		printf("submit_bulk_msg: %d is more then available buffer size(%d)\n", len, DWC_OTG_HCD_DATA_BUF_SIZE);
		dev->status = 0;
		dev->act_len = done;
		return -1;
	}

	hc_regs = host_if->hc_regs[CHANNEL];

	if (devnum == root_hub_devnum) {
		dev->status = 0;
		return -1;
	}

	while ((done < len) && !stop_transfer) {
		/* Initialize channel */
		dwc_otg_hc_init(&g_core_if, CHANNEL, devnum, ep, usb_pipein(pipe), DWC_OTG_EP_TYPE_BULK, max);

		xfer_len = (len - done);
		if (xfer_len > max_hc_xfer_size)
			/* Make sure that xfer_len is a multiple of max packet size. */
			xfer_len = max_hc_xfer_size - max + 1;

		if (xfer_len > 0) {
			num_packets = (xfer_len + max - 1) / max;
			if (num_packets > max_hc_pkt_count) {
				num_packets = max_hc_pkt_count;
				xfer_len = num_packets * max;
			}
		} else
			num_packets = 1;

		if (usb_pipein(pipe))
			xfer_len = num_packets * max;

		hctsiz.d32 = 0;

		hctsiz.b.xfersize = xfer_len;
		hctsiz.b.pktcnt = num_packets;
		hctsiz.b.pid = bulk_data_toggle[devnum][ep];

		dwc_write_reg32(&hc_regs->hctsiz, hctsiz.d32);

		memcpy(aligned_buffer, (char *)buffer + done, len - done);
		dwc_write_reg32(&hc_regs->hcdma, (uint32_t)aligned_buffer);

		hcchar_data_t hcchar;
		hcchar.d32 = dwc_read_reg32(&hc_regs->hcchar);
		hcchar.b.multicnt = 1;

		/* Remember original int status */
		hcint.d32 = dwc_read_reg32(&hc_regs->hcint);

		/* Set host channel enable after all other setup is complete. */
		hcchar.b.chen = 1;
		hcchar.b.chdis = 0;
		dwc_write_reg32(&hc_regs->hcchar, hcchar.d32);

		/* TODO: no endless loop */
		while (1) {
			hcint_new.d32 = dwc_read_reg32(&hc_regs->hcint);
			if (hcint.d32 != hcint_new.d32) {
				hcint.d32 = hcint_new.d32;
			}

			if (hcint_new.b.chhltd) {
				if (hcint_new.b.xfercomp) {
					hctsiz.d32 = dwc_read_reg32(&hc_regs->hctsiz);
					if (usb_pipein(pipe)) {
						done += xfer_len - hctsiz.b.xfersize;
						if (hctsiz.b.xfersize)
							stop_transfer = 1;
					} else {
						done += xfer_len;
					}

					if (hctsiz.b.pid == DWC_OTG_HC_PID_DATA1)
						bulk_data_toggle[devnum][ep] =
							DWC_OTG_HC_PID_DATA1;
					else
						bulk_data_toggle[devnum][ep] =
							DWC_OTG_HC_PID_DATA0;
					break;
				} else if (hcint_new.b.stall) {
					printf("DWC OTG: Channel halted\n");
					bulk_data_toggle[devnum][ep] = DWC_OTG_HC_PID_DATA0;

					stop_transfer = 1;
					break;
				}
			}
		}
	}

	if (done && usb_pipein(pipe))
		memcpy(buffer, aligned_buffer, done);

	dwc_write_reg32(&hc_regs->hcintmsk, 0);
	dwc_write_reg32(&hc_regs->hcint, 0xFFFFFFFF);

	dev->status = 0;
	dev->act_len = done;

	return 0;
}

int submit_control_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
		       int len, struct devrequest *setup)
{
	int done = 0;
	int devnum = usb_pipedevice(pipe);
	int ep = usb_pipeendpoint(pipe);
	int max = usb_maxpacket(dev, pipe);
	hctsiz_data_t hctsiz;
	dwc_otg_host_if_t *host_if = g_core_if.host_if;
	dwc_otg_hc_regs_t *hc_regs = host_if->hc_regs[CHANNEL];
	hcint_data_t hcint_new;
	/* For CONTROL endpoint pid should start with DATA1 */
	int status_direction;

	if (devnum == root_hub_devnum) {
		dev->status = 0;
		dev->speed = USB_SPEED_HIGH;
		return dwc_otg_submit_rh_msg(dev, pipe, buffer, len, setup);
	}

	if (len > DWC_OTG_HCD_DATA_BUF_SIZE) {
		printf("submit_control_msg: %d is more then available buffer size(%d)\n", len, DWC_OTG_HCD_DATA_BUF_SIZE);
		dev->status = 0;
		dev->act_len = done;
		return -1;
	}

	/* Initialize channel, OUT for setup buffer */
	dwc_otg_hc_init(&g_core_if, CHANNEL, devnum, ep, 0, DWC_OTG_EP_TYPE_CONTROL, max);

	/* SETUP stage  */
	hctsiz.d32 = 0;
	hctsiz.b.xfersize = 8;
	hctsiz.b.pktcnt = 1;
	hctsiz.b.pid = DWC_OTG_HC_PID_SETUP;
	dwc_write_reg32(&hc_regs->hctsiz, hctsiz.d32);

	dwc_write_reg32(&hc_regs->hcdma, (uint32_t)setup);

	hcchar_data_t hcchar;
	hcchar.d32 = dwc_read_reg32(&hc_regs->hcchar);
	hcchar.b.multicnt = 1;

	/* Set host channel enable after all other setup is complete. */
	hcchar.b.chen = 1;
	hcchar.b.chdis = 0;
	dwc_write_reg32(&hc_regs->hcchar, hcchar.d32);

	/* TODO: no endless loop */
	while (1) {
		hcint_new.d32 = dwc_read_reg32(&hc_regs->hcint);
		if (hcint_new.b.chhltd) {
			break;
		}
	}

	/* TODO: check for error */
	if (!(hcint_new.b.chhltd && hcint_new.b.xfercomp)) {
		handle_error(__LINE__, hcint_new.d32);
		goto out;
	}

	/* Clear interrupts */
	dwc_write_reg32(&hc_regs->hcintmsk, 0);
	dwc_write_reg32(&hc_regs->hcint, 0xFFFFFFFF);

	if (buffer) {
		/* DATA stage  */
		dwc_otg_hc_init(&g_core_if, CHANNEL, devnum, ep, usb_pipein(pipe), DWC_OTG_EP_TYPE_CONTROL, max);

		/* TODO: check if len < 64 */
		hctsiz.d32 = 0;
		hctsiz.b.xfersize = len;
		hctsiz.b.pktcnt = 1;

		control_data_toggle[devnum][ep] = DWC_OTG_HC_PID_DATA1;
		hctsiz.b.pid = control_data_toggle[devnum][ep];

		dwc_write_reg32(&hc_regs->hctsiz, hctsiz.d32);

		dwc_write_reg32(&hc_regs->hcdma, (uint32_t)buffer);

		hcchar.d32 = dwc_read_reg32(&hc_regs->hcchar);
		hcchar.b.multicnt = 1;

		/* Set host channel enable after all other setup is complete. */
		hcchar.b.chen = 1;
		hcchar.b.chdis = 0;
		dwc_write_reg32(&hc_regs->hcchar, hcchar.d32);

		while (1) {
			hcint_new.d32 = dwc_read_reg32(&hc_regs->hcint);
			if (hcint_new.b.chhltd) {
				if (hcint_new.b.xfercomp) {
					hctsiz.d32 = dwc_read_reg32(&hc_regs->hctsiz);
					if (usb_pipein(pipe))
						done = len - hctsiz.b.xfersize;
					else
						done = len;
				}

				if (hcint_new.b.ack) {
					if (hctsiz.b.pid == DWC_OTG_HC_PID_DATA0)
						control_data_toggle[devnum][ep] = DWC_OTG_HC_PID_DATA0;
					else
						control_data_toggle[devnum][ep] = DWC_OTG_HC_PID_DATA1;
				}

				if (hcint_new.d32 != STATUS_ACK_HLT_COMPL) {
					goto out;
					handle_error(__LINE__, hcint_new.d32);
				}

				break;
			}
		}
	} /* End of DATA stage */

	/* STATUS stage  */
	if ((len == 0) || usb_pipeout(pipe))
		status_direction = 1;
	else
		status_direction = 0;

	dwc_otg_hc_init(&g_core_if, CHANNEL, devnum, ep, status_direction, DWC_OTG_EP_TYPE_CONTROL, max);

	hctsiz.d32 = 0;
	hctsiz.b.xfersize = 0;
	hctsiz.b.pktcnt = 1;
	hctsiz.b.pid = DWC_OTG_HC_PID_DATA1;
	dwc_write_reg32(&hc_regs->hctsiz, hctsiz.d32);

	dwc_write_reg32(&hc_regs->hcdma, (uint32_t)status_buffer);

	hcchar.d32 = dwc_read_reg32(&hc_regs->hcchar);
	hcchar.b.multicnt = 1;

	/* Set host channel enable after all other setup is complete. */
	hcchar.b.chen = 1;
	hcchar.b.chdis = 0;
	dwc_write_reg32(&hc_regs->hcchar, hcchar.d32);

	while (1) {
		hcint_new.d32 = dwc_read_reg32(&hc_regs->hcint);
		if (hcint_new.b.chhltd)
			break;
	}

	if (hcint_new.d32 != STATUS_ACK_HLT_COMPL)
		handle_error(__LINE__, hcint_new.d32);

out:
	dev->act_len = done;
	dev->status = 0;

	return done;
}

int submit_int_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
		   int len, int interval)
{
	printf("dev = %p pipe = %#lx buf = %p size = %d int = %d\n", dev, pipe,
	       buffer, len, interval);
	return -1;
}
