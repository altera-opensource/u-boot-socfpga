/*
 * Copyright (C) 2012 Oleksandr Tymoshenko <gonzo at freebsd.org>
 *
 * SPDX-License-Identifier:     GPL-2.0
 */

#ifndef __DWC_OTG_H__
#define __DWC_OTG_H__

/* USB HUB CONSTANTS (not OHCI-specific; see hub.h, based on usb_ohci.h) */

/* destination of request */
#define RH_INTERFACE		0x01
#define RH_ENDPOINT		0x02
#define RH_OTHER		0x03

#define RH_CLASS		0x20
#define RH_VENDOR		0x40

/* Requests: bRequest << 8 | bmRequestType */
#define RH_GET_STATUS		0x0080
#define RH_CLEAR_FEATURE	0x0100
#define RH_SET_FEATURE		0x0300
#define RH_SET_ADDRESS		0x0500
#define RH_GET_DESCRIPTOR	0x0680
#define RH_SET_DESCRIPTOR	0x0700
#define RH_GET_CONFIGURATION	0x0880
#define RH_SET_CONFIGURATION	0x0900
#define RH_GET_STATE		0x0280
#define RH_GET_INTERFACE	0x0A80
#define RH_SET_INTERFACE	0x0B00
#define RH_SYNC_FRAME		0x0C80
/* Our Vendor Specific Request */
#define RH_SET_EP		0x2000

/* Hub port features */
#define RH_PORT_CONNECTION	0x00
#define RH_PORT_ENABLE		0x01
#define RH_PORT_SUSPEND		0x02
#define RH_PORT_OVER_CURRENT	0x03
#define RH_PORT_RESET		0x04
#define RH_PORT_POWER		0x08
#define RH_PORT_LOW_SPEED	0x09

#define RH_C_PORT_CONNECTION	0x10
#define RH_C_PORT_ENABLE	0x11
#define RH_C_PORT_SUSPEND	0x12
#define RH_C_PORT_OVER_CURRENT	0x13
#define RH_C_PORT_RESET		0x14

/* Hub features */
#define RH_C_HUB_LOCAL_POWER	0x00
#define RH_C_HUB_OVER_CURRENT	0x01

#define RH_DEVICE_REMOTE_WAKEUP	0x00
#define RH_ENDPOINT_STALL	0x01

#define RH_ACK			0x01
#define RH_REQ_ERR		-1
#define RH_NACK			0x00

/* OHCI ROOT HUB REGISTER MASKS */

/* roothub.portstatus [i] bits */
#define RH_PS_CCS	0x00000001	/* current connect status */
#define RH_PS_PES	0x00000002	/* port enable status */
#define RH_PS_PSS	0x00000004	/* port suspend status */
#define RH_PS_POCI	0x00000008	/* port over current indicator */
#define RH_PS_PRS	0x00000010	/* port reset status */
#define RH_PS_PPS	0x00000100	/* port power status */
#define RH_PS_LSDA	0x00000200	/* low speed device attached */
#define RH_PS_CSC	0x00010000	/* connect status change */
#define RH_PS_PESC	0x00020000	/* port enable status change */
#define RH_PS_PSSC	0x00040000	/* port suspend status change */
#define RH_PS_OCIC	0x00080000	/* over current indicator change */
#define RH_PS_PRSC	0x00100000	/* port reset status change */

/* roothub.status bits */
#define RH_HS_LPS	0x00000001	/* local power status */
#define RH_HS_OCI	0x00000002	/* over current indicator */
#define RH_HS_DRWE	0x00008000	/* device remote wakeup enable */
#define RH_HS_LPSC	0x00010000	/* local power status change */
#define RH_HS_OCIC	0x00020000	/* over current indicator change */
#define RH_HS_CRWE	0x80000000	/* clear remote wakeup enable */

/* roothub.b masks */
#define RH_B_DR		0x0000ffff	/* device removable flags */
#define RH_B_PPCM	0xffff0000	/* port power control mask */

/* roothub.a masks */
#define RH_A_NDP	(0xff << 0)	/* number of downstream ports */
#define RH_A_PSM	(1 << 8)	/* power switching mode */
#define RH_A_NPS	(1 << 9)	/* no power switching */
#define RH_A_DT		(1 << 10)	/* device type (mbz) */
#define RH_A_OCPM	(1 << 11)	/* over current protection mode */
#define RH_A_NOCP	(1 << 12)	/* no over current protection */
#define RH_A_POTPGT	(0xff << 24)	/* power on to power good time */

#define DWC_OTG_HC_PID_DATA0 0
#define DWC_OTG_HC_PID_DATA2 1
#define DWC_OTG_HC_PID_DATA1 2
#define DWC_OTG_HC_PID_MDATA 3
#define DWC_OTG_HC_PID_SETUP 3

/** Macros defined for DWC OTG HW Release verison */
#define OTG_CORE_REV_2_60a	0x4F54260A
#define OTG_CORE_REV_2_71a	0x4F54271A
#define OTG_CORE_REV_2_72a	0x4F54272A
#define OTG_CORE_REV_2_80a	0x4F54280A
#define OTG_CORE_REV_2_81a	0x4F54281A
#define OTG_CORE_REV_2_90a	0x4F54290A

#define DWC_OTG_EP_TYPE_CONTROL	0
#define DWC_OTG_EP_TYPE_ISOC	1
#define DWC_OTG_EP_TYPE_BULK	2
#define DWC_OTG_EP_TYPE_INTR	3

#define DWC_OTG_EP_SPEED_LOW	0
#define DWC_OTG_EP_SPEED_FULL	1
#define DWC_OTG_EP_SPEED_HIGH	2

/** Maximum number of Periodic FIFOs */
#define MAX_PERIO_FIFOS 15
/** Maximum number of Periodic FIFOs */
#define MAX_TX_FIFOS 15

/** Maximum number of Endpoints/HostChannels */
#define MAX_EPS_CHANNELS 16

#endif /* __DWC_OTG_H__ */
