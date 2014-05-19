#include <common.h>
#include <usb.h>
#include <malloc.h>
#include "dwc2_otg.h"
#include "dwc2_otg_regs.h"
#include "dwc2_otg_core_if.h"

#undef DWC_OTG_DEBUG

#ifdef DWC_OTG_DEBUG
	#define PDEBUG(fmt, args...) \
		printf("[%s:%d] " fmt, \
			__PRETTY_FUNCTION__, __LINE__ , ## args)
#else
	#define PDEBUG(fmt, args...) do {} while (0)
#endif

void dwc_write_reg32(volatile uint32_t *addr, uint32_t value)
{
	*addr = value;
}

uint32_t dwc_read_reg32(volatile uint32_t *addr)
{
	return *addr;
}

void dwc_modify_reg32(volatile uint32_t *addr, uint32_t clear_mask, uint32_t set_mask)
{
	uint32_t v = *addr;
	*addr = (v & ~clear_mask) | set_mask;
}

/**
 * This function returns the mode of the operation, host or device.
 *
 * @return 0 - Device Mode, 1 - Host Mode
 */
static inline uint32_t dwc_otg_mode(dwc_otg_core_if_t *_core_if)
{
	return dwc_read_reg32(&_core_if->core_global_regs->gintsts) & 0x1;
}

uint8_t dwc_otg_is_host_mode(dwc_otg_core_if_t *_core_if)
{
	return dwc_otg_mode(_core_if) == DWC_HOST_MODE;
}

static void dwc_otg_set_uninitialized(int32_t *p, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		p[i] = -1;
	}
}

static int dwc_otg_param_initialized(int32_t val)
{
	return val != -1;
}

/**
 * This function returns the Host All Channel Interrupt register
 */
static inline uint32_t dwc_otg_read_host_all_channels_intr(
				dwc_otg_core_if_t *_core_if)
{
	return dwc_read_reg32(&_core_if->host_if->host_global_regs->haint);
}

static inline uint32_t dwc_otg_read_host_channel_intr(
				dwc_otg_core_if_t *_core_if, int hc_num)
{
	return dwc_read_reg32(&_core_if->host_if->hc_regs[hc_num]->hcint);
}

/**
 * This function Reads HPRT0 in preparation to modify.	It keeps the
 * WC bits 0 so that if they are read as 1, they won't clear when you
 * write it back
 */
uint32_t dwc_otg_read_hprt0(dwc_otg_core_if_t *_core_if)
{
	hprt0_data_t hprt0;
	hprt0.d32 = dwc_read_reg32(_core_if->host_if->hprt0);
	hprt0.b.prtena = 0;
	hprt0.b.prtconndet = 0;
	hprt0.b.prtenchng = 0;
	hprt0.b.prtovrcurrchng = 0;
	return hprt0.d32;
}

/* Checks if the parameter is outside of its valid range of values */
#define DWC_OTG_PARAM_TEST(_param_, _low_, _high_) \
		(((_param_) < (_low_)) || \
		((_param_) > (_high_)))

/* Parameter access functions */
int dwc_otg_set_param_otg_cap(dwc_otg_core_if_t *core_if, int32_t val)
{
	int valid;
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 2)) {
		printf("warning: Wrong value for otg_cap parameter\n");
		printf("warning: otg_cap parameter must be 0, 1 or 2\n");
		retval = -1;
		goto out;
	}

	valid = 1;
	switch (val) {
	case DWC_OTG_CAP_PARAM_HNP_SRP_CAPABLE:
		if (core_if->hwcfg2.b.op_mode !=
		    DWC_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG)
			valid = 0;
		break;
	case DWC_OTG_CAP_PARAM_SRP_ONLY_CAPABLE:
		if ((core_if->hwcfg2.b.op_mode !=
		     DWC_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG)
		    && (core_if->hwcfg2.b.op_mode !=
			DWC_HWCFG2_OP_MODE_SRP_ONLY_CAPABLE_OTG)
		    && (core_if->hwcfg2.b.op_mode !=
			DWC_HWCFG2_OP_MODE_SRP_CAPABLE_DEVICE)
		    && (core_if->hwcfg2.b.op_mode !=
		      DWC_HWCFG2_OP_MODE_SRP_CAPABLE_HOST)) {
			valid = 0;
		}
		break;
	case DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE:
		/* always valid */
		break;
	}
	if (!valid) {
		if (dwc_otg_param_initialized(core_if->core_params->otg_cap)) {
			printf
			    ("error: %d invalid for otg_cap paremter. Check HW configuration.\n",
			     val);
		}
		val =
		    (((core_if->hwcfg2.b.op_mode ==
		       DWC_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG)
		      || (core_if->hwcfg2.b.op_mode ==
			  DWC_HWCFG2_OP_MODE_SRP_ONLY_CAPABLE_OTG)
		      || (core_if->hwcfg2.b.op_mode ==
			  DWC_HWCFG2_OP_MODE_SRP_CAPABLE_DEVICE)
		      || (core_if->hwcfg2.b.op_mode ==
			  DWC_HWCFG2_OP_MODE_SRP_CAPABLE_HOST)) ?
		     DWC_OTG_CAP_PARAM_SRP_ONLY_CAPABLE :
		     DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE);
		retval = -1;
	}

	core_if->core_params->otg_cap = val;
      out:
	return retval;
}

int32_t dwc_otg_get_param_otg_cap(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->otg_cap;
}

int dwc_otg_set_param_opt(dwc_otg_core_if_t *core_if, int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong value for opt parameter\n");
		return -1;
	}
	core_if->core_params->opt = val;
	return 0;
}

int32_t dwc_otg_get_param_opt(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->opt;
}

int dwc_otg_set_param_dma_enable(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong value for dma enable\n");
		return -1;
	}

	if ((val == 1) && (core_if->hwcfg2.b.architecture == 0)) {
		if (dwc_otg_param_initialized(core_if->core_params->dma_enable)) {
			printf
			    ("error: %d invalid for dma_enable paremter. Check HW configuration.\n",
			     val);
		}
		val = 0;
		retval = -1;
	}

	core_if->core_params->dma_enable = val;
	if (val == 0) {
		dwc_otg_set_param_dma_desc_enable(core_if, 0);
	}
	return retval;
}

int32_t dwc_otg_get_param_dma_enable(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->dma_enable;
}

int dwc_otg_set_param_dma_desc_enable(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong value for dma_enable\n");
		printf("warning: dma_desc_enable must be 0 or 1\n");
		return -1;
	}

	if ((val == 1)
	    && ((dwc_otg_get_param_dma_enable(core_if) == 0)
		|| (core_if->hwcfg4.b.desc_dma == 0))) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->dma_desc_enable)) {
			printf
			    ("error: %d invalid for dma_desc_enable paremter. Check HW configuration.\n",
			     val);
		}
		val = 0;
		retval = -1;
	}
	core_if->core_params->dma_desc_enable = val;
	return retval;
}

int32_t dwc_otg_get_param_dma_desc_enable(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->dma_desc_enable;
}

int dwc_otg_set_param_host_support_fs_ls_low_power(dwc_otg_core_if_t *core_if,
						   int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong value for host_support_fs_low_power\n");
		printf("warning: host_support_fs_low_power must be 0 or 1\n");
		return -1;
	}
	core_if->core_params->host_support_fs_ls_low_power = val;
	return 0;
}

int32_t dwc_otg_get_param_host_support_fs_ls_low_power(dwc_otg_core_if_t *
						       core_if)
{
	return core_if->core_params->host_support_fs_ls_low_power;
}

int dwc_otg_set_param_enable_dynamic_fifo(dwc_otg_core_if_t *core_if,
					  int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong value for enable_dynamic_fifo\n");
		printf("warning: enable_dynamic_fifo must be 0 or 1\n");
		return -1;
	}

	if ((val == 1) && (core_if->hwcfg2.b.dynamic_fifo == 0)) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->enable_dynamic_fifo)) {
			printf
			    ("error: %d invalid for enable_dynamic_fifo paremter. Check HW configuration.\n",
			     val);
		}
		val = 0;
		retval = -1;
	}
	core_if->core_params->enable_dynamic_fifo = val;
	return retval;
}

int32_t dwc_otg_get_param_enable_dynamic_fifo(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->enable_dynamic_fifo;
}

int dwc_otg_set_param_data_fifo_size(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 32, 32768)) {
		printf("warning: Wrong value for data_fifo_size\n");
		printf("warning: data_fifo_size must be 32-32768\n");
		return -1;
	}

	if (val > core_if->hwcfg3.b.dfifo_depth) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->data_fifo_size)) {
			printf
			    ("error: %d invalid for data_fifo_size parameter. Check HW configuration.\n",
			     val);
		}
		val = core_if->hwcfg3.b.dfifo_depth;
		retval = -1;
	}

	core_if->core_params->data_fifo_size = val;
	return retval;
}

int32_t dwc_otg_get_param_data_fifo_size(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->data_fifo_size;
}

int dwc_otg_set_param_dev_rx_fifo_size(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 16, 32768)) {
		printf("warning: Wrong value for dev_rx_fifo_size\n");
		printf("warning: dev_rx_fifo_size must be 16-32768\n");
		return -1;
	}

	if (val > dwc_read_reg32(&core_if->core_global_regs->grxfsiz)) {
		if (dwc_otg_param_initialized(core_if->core_params->dev_rx_fifo_size)) {
		printf("warning: %d invalid for dev_rx_fifo_size parameter\n", val);
		}
		val = dwc_read_reg32(&core_if->core_global_regs->grxfsiz);
		retval = -1;
	}

	core_if->core_params->dev_rx_fifo_size = val;
	return retval;
}

int32_t dwc_otg_get_param_dev_rx_fifo_size(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->dev_rx_fifo_size;
}

int dwc_otg_set_param_dev_nperio_tx_fifo_size(dwc_otg_core_if_t *core_if,
					      int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 16, 32768)) {
		printf("warning: Wrong value for dev_nperio_tx_fifo\n");
		printf("warning: dev_nperio_tx_fifo must be 16-32768\n");
		return -1;
	}

	if (val > (dwc_read_reg32(&core_if->core_global_regs->gnptxfsiz) >> 16)) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->dev_nperio_tx_fifo_size)) {
			printf
			    ("error: %d invalid for dev_nperio_tx_fifo_size. Check HW configuration.\n",
			     val);
		}
		val =
		    (dwc_read_reg32(&core_if->core_global_regs->gnptxfsiz) >>
		     16);
		retval = -1;
	}

	core_if->core_params->dev_nperio_tx_fifo_size = val;
	return retval;
}

int32_t dwc_otg_get_param_dev_nperio_tx_fifo_size(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->dev_nperio_tx_fifo_size;
}

int dwc_otg_set_param_host_rx_fifo_size(dwc_otg_core_if_t *core_if,
					int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 16, 32768)) {
		printf("warning: Wrong value for host_rx_fifo_size\n");
		printf("warning: host_rx_fifo_size must be 16-32768\n");
		return -1;
	}

	if (val > dwc_read_reg32(&core_if->core_global_regs->grxfsiz)) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->host_rx_fifo_size)) {
			printf
			    ("error: %d invalid for host_rx_fifo_size. Check HW configuration.\n",
			     val);
		}
		val = dwc_read_reg32(&core_if->core_global_regs->grxfsiz);
		retval = -1;
	}

	core_if->core_params->host_rx_fifo_size = val;
	return retval;

}

int32_t dwc_otg_get_param_host_rx_fifo_size(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->host_rx_fifo_size;
}

int dwc_otg_set_param_host_nperio_tx_fifo_size(dwc_otg_core_if_t *core_if,
					       int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 16, 32768)) {
		printf("warning: Wrong value for host_nperio_tx_fifo_size\n");
		printf("warning: host_nperio_tx_fifo_size must be 16-32768\n");
		return -1;
	}

	if (val > (dwc_read_reg32(&core_if->core_global_regs->gnptxfsiz) >> 16)) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->host_nperio_tx_fifo_size)) {
			printf
			    ("error: %d invalid for host_nperio_tx_fifo_size. Check HW configuration.\n",
			     val);
		}
		val =
		    (dwc_read_reg32(&core_if->core_global_regs->gnptxfsiz) >>
		     16);
		retval = -1;
	}

	core_if->core_params->host_nperio_tx_fifo_size = val;
	return retval;
}

int32_t dwc_otg_get_param_host_nperio_tx_fifo_size(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->host_nperio_tx_fifo_size;
}

int dwc_otg_set_param_host_perio_tx_fifo_size(dwc_otg_core_if_t *core_if,
					      int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 16, 32768)) {
		printf("warning: Wrong value for host_perio_tx_fifo_size\n");
		printf("warning: host_perio_tx_fifo_size must be 16-32768\n");
		return -1;
	}

	if (val >
	    ((dwc_read_reg32(&core_if->core_global_regs->hptxfsiz) >> 16))) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->host_perio_tx_fifo_size)) {
			printf
			    ("error: %d invalid for host_perio_tx_fifo_size. Check HW configuration.\n",
			     val);
		}
		val =
		    (dwc_read_reg32(&core_if->core_global_regs->hptxfsiz) >>
		     16);
		retval = -1;
	}

	core_if->core_params->host_perio_tx_fifo_size = val;
	return retval;
}

int32_t dwc_otg_get_param_host_perio_tx_fifo_size(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->host_perio_tx_fifo_size;
}

int dwc_otg_set_param_max_transfer_size(dwc_otg_core_if_t *core_if,
					int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 2047, 524288)) {
		printf("warning: Wrong value for max_transfer_size\n");
		printf("warning: max_transfer_size must be 2047-524288\n");
		return -1;
	}

	if (val >= (1 << (core_if->hwcfg3.b.xfer_size_cntr_width + 11))) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->max_transfer_size)) {
			printf
			    ("error: %d invalid for max_transfer_size. Check HW configuration.\n",
			     val);
		}
		val =
		    ((1 << (core_if->hwcfg3.b.packet_size_cntr_width + 11)) -
		     1);
		retval = -1;
	}

	core_if->core_params->max_transfer_size = val;
	return retval;
}

int32_t dwc_otg_get_param_max_transfer_size(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->max_transfer_size;
}

int dwc_otg_set_param_max_packet_count(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 15, 511)) {
		printf("warning: Wrong value for max_packet_count\n");
		printf("warning: max_packet_count must be 15-511\n");
		return -1;
	}

	if (val > (1 << (core_if->hwcfg3.b.packet_size_cntr_width + 4))) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->max_packet_count)) {
			printf
			    ("error: %d invalid for max_packet_count. Check HW configuration.\n",
			     val);
		}
		val =
		    ((1 << (core_if->hwcfg3.b.packet_size_cntr_width + 4)) - 1);
		retval = -1;
	}

	core_if->core_params->max_packet_count = val;
	return retval;
}

int32_t dwc_otg_get_param_max_packet_count(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->max_packet_count;
}

int dwc_otg_set_param_host_channels(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 1, 16)) {
		printf("warning: Wrong value for host_channels\n");
		printf("warning: host_channels must be 1-16\n");
		return -1;
	}

	if (val > (core_if->hwcfg2.b.num_host_chan + 1)) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->host_channels)) {
			printf
			    ("error: %d invalid for host_channels. Check HW configurations.\n",
			     val);
		}
		val = (core_if->hwcfg2.b.num_host_chan + 1);
		retval = -1;
	}

	core_if->core_params->host_channels = val;
	return retval;
}

int32_t dwc_otg_get_param_host_channels(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->host_channels;
}

int dwc_otg_set_param_dev_endpoints(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 1, 15)) {
		printf("warning: Wrong value for dev_endpoints\n");
		printf("warning: dev_endpoints must be 1-15\n");
		return -1;
	}

	if (val > (core_if->hwcfg2.b.num_dev_ep)) {
		if (dwc_otg_param_initialized
		    (core_if->core_params->dev_endpoints)) {
			printf
			    ("error: %d invalid for dev_endpoints. Check HW configurations.\n",
			     val);
		}
		val = core_if->hwcfg2.b.num_dev_ep;
		retval = -1;
	}

	core_if->core_params->dev_endpoints = val;
	return retval;
}

int32_t dwc_otg_get_param_dev_endpoints(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->dev_endpoints;
}

int dwc_otg_set_param_phy_type(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	int valid = 0;

	if (DWC_OTG_PARAM_TEST(val, 0, 2)) {
		printf("warning: Wrong value for phy_type\n");
		printf("warning: phy_type must be 0, 1 or 2\n");
		return -1;
	}
#ifndef NO_FS_PHY_HW_CHECKS
	if ((val == DWC_PHY_TYPE_PARAM_UTMI) &&
	    ((core_if->hwcfg2.b.hs_phy_type == 1) ||
	     (core_if->hwcfg2.b.hs_phy_type == 3))) {
		valid = 1;
	} else if ((val == DWC_PHY_TYPE_PARAM_ULPI) &&
		   ((core_if->hwcfg2.b.hs_phy_type == 2) ||
		    (core_if->hwcfg2.b.hs_phy_type == 3))) {
		valid = 1;
	} else if ((val == DWC_PHY_TYPE_PARAM_FS) &&
		   (core_if->hwcfg2.b.fs_phy_type == 1)) {
		valid = 1;
	}
	if (!valid) {
		if (dwc_otg_param_initialized(core_if->core_params->phy_type)) {
			printf
			    ("error: %d invalid for phy_type. Check HW configurations.\n",
			     val);
		}
		if (core_if->hwcfg2.b.hs_phy_type) {
			if ((core_if->hwcfg2.b.hs_phy_type == 3) ||
			    (core_if->hwcfg2.b.hs_phy_type == 1)) {
				val = DWC_PHY_TYPE_PARAM_UTMI;
			} else {
				val = DWC_PHY_TYPE_PARAM_ULPI;
			}
		}
		retval = -1;
	}
#endif
	core_if->core_params->phy_type = val;
	return retval;
}

int32_t dwc_otg_get_param_phy_type(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->phy_type;
}

int dwc_otg_set_param_speed(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong value for speed parameter\n");
		printf("warning: max_speed parameter must be 0 or 1\n");
		return -1;
	}
	if ((val == 0)
	    && dwc_otg_get_param_phy_type(core_if) == DWC_PHY_TYPE_PARAM_FS) {
		if (dwc_otg_param_initialized(core_if->core_params->speed)) {
			printf
			    ("error: %d invalid for speed paremter. Check HW configuration.\n",
			     val);
		}
		val =
		    (dwc_otg_get_param_phy_type(core_if) ==
		     DWC_PHY_TYPE_PARAM_FS ? 1 : 0);
		retval = -1;
	}
	core_if->core_params->speed = val;
	return retval;
}

int32_t dwc_otg_get_param_speed(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->speed;
}

int dwc_otg_set_param_host_ls_low_power_phy_clk(dwc_otg_core_if_t *core_if,
						int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf
		    ("warning: Wrong value for host_ls_low_power_phy_clk parameter\n");
		printf("warning: host_ls_low_power_phy_clk must be 0 or 1\n");
		return -1;
	}

	if ((val == DWC_HOST_LS_LOW_POWER_PHY_CLK_PARAM_48MHZ)
	    && (dwc_otg_get_param_phy_type(core_if) == DWC_PHY_TYPE_PARAM_FS)) {
		if (dwc_otg_param_initialized(core_if->core_params->host_ls_low_power_phy_clk)) {
			printf("error: %d invalid for host_ls_low_power_phy_clk. Check HW configuration.\n",
		     val);
		}
		val =
		    (dwc_otg_get_param_phy_type(core_if) ==
		     DWC_PHY_TYPE_PARAM_FS) ?
		    DWC_HOST_LS_LOW_POWER_PHY_CLK_PARAM_6MHZ :
		    DWC_HOST_LS_LOW_POWER_PHY_CLK_PARAM_48MHZ;
		retval = -1;
	}

	core_if->core_params->host_ls_low_power_phy_clk = val;
	return retval;
}

int32_t dwc_otg_get_param_host_ls_low_power_phy_clk(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->host_ls_low_power_phy_clk;
}

int dwc_otg_set_param_phy_ulpi_ddr(dwc_otg_core_if_t *core_if, int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong value for phy_ulpi_ddr\n");
		printf("warning: phy_upli_ddr must be 0 or 1\n");
		return -1;
	}

	core_if->core_params->phy_ulpi_ddr = val;
	return 0;
}

int32_t dwc_otg_get_param_phy_ulpi_ddr(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->phy_ulpi_ddr;
}

int dwc_otg_set_param_phy_ulpi_ext_vbus(dwc_otg_core_if_t *core_if,
					int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong valaue for phy_ulpi_ext_vbus\n");
		printf("warning: phy_ulpi_ext_vbus must be 0 or 1\n");
		return -1;
	}

	core_if->core_params->phy_ulpi_ext_vbus = val;
	return 0;
}

int32_t dwc_otg_get_param_phy_ulpi_ext_vbus(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->phy_ulpi_ext_vbus;
}

int dwc_otg_set_param_phy_utmi_width(dwc_otg_core_if_t *core_if, int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 8, 8) && DWC_OTG_PARAM_TEST(val, 16, 16)) {
		printf("warning: Wrong valaue for phy_utmi_width\n");
		printf("warning: phy_utmi_width must be 8 or 16\n");
		return -1;
	}

	core_if->core_params->phy_utmi_width = val;
	return 0;
}

int32_t dwc_otg_get_param_phy_utmi_width(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->phy_utmi_width;
}

int dwc_otg_set_param_ulpi_fs_ls(dwc_otg_core_if_t *core_if, int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong valaue for ulpi_fs_ls\n");
		printf("warning: ulpi_fs_ls must be 0 or 1\n");
		return -1;
	}

	core_if->core_params->ulpi_fs_ls = val;
	return 0;
}

int32_t dwc_otg_get_param_ulpi_fs_ls(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->ulpi_fs_ls;
}

int dwc_otg_set_param_ts_dline(dwc_otg_core_if_t *core_if, int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong valaue for ts_dline\n");
		printf("warning: ts_dline must be 0 or 1\n");
		return -1;
	}

	core_if->core_params->ts_dline = val;
	return 0;
}

int32_t dwc_otg_get_param_ts_dline(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->ts_dline;
}

int dwc_otg_set_param_i2c_enable(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong valaue for i2c_enable\n");
		printf("warning: i2c_enable must be 0 or 1\n");
		return -1;
	}
#ifndef NO_FS_PHY_HW_CHECK
	if (val == 1 && core_if->hwcfg3.b.i2c == 0) {
		if (dwc_otg_param_initialized(core_if->core_params->i2c_enable)) {
			printf("error: %d invalid for i2c_enable. Check HW configuration.\n",
		     val);
		}
		val = 0;
		retval = -1;
	}
#endif

	core_if->core_params->i2c_enable = val;
	return retval;
}

int32_t dwc_otg_get_param_i2c_enable(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->i2c_enable;
}

int dwc_otg_set_param_dev_perio_tx_fifo_size(dwc_otg_core_if_t *core_if,
					     int32_t val, int fifo_num)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 4, 768)) {
		printf("warning: Wrong value for dev_perio_tx_fifo_size\n");
		printf("warning: dev_perio_tx_fifo_size must be 4-768\n");
		return -1;
	}

	if (val > (dwc_read_reg32(&core_if->core_global_regs->dptxfsiz_dieptxf[fifo_num]))) {
		if (dwc_otg_param_initialized(core_if->core_params->dev_perio_tx_fifo_size[fifo_num])) {
			printf("error: `%d' invalid for parameter `dev_perio_fifo_size_%d'. Check HW configuration.\n",
		     val, fifo_num);
		}
		val = (dwc_read_reg32(&core_if->core_global_regs->dptxfsiz_dieptxf[fifo_num]));
		retval = -1;
	}

	core_if->core_params->dev_perio_tx_fifo_size[fifo_num] = val;
	return retval;
}

int32_t dwc_otg_get_param_dev_perio_tx_fifo_size(dwc_otg_core_if_t *core_if,
						 int fifo_num)
{
	return core_if->core_params->dev_perio_tx_fifo_size[fifo_num];
}

int dwc_otg_set_param_en_multiple_tx_fifo(dwc_otg_core_if_t *core_if,
					  int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong valaue for en_multiple_tx_fifo,\n");
		printf("warning: en_multiple_tx_fifo must be 0 or 1\n");
		return -1;
	}

	if (val == 1 && core_if->hwcfg4.b.ded_fifo_en == 0) {
		if (dwc_otg_param_initialized(core_if->core_params->en_multiple_tx_fifo)) {
			printf("error: %d invalid for parameter en_multiple_tx_fifo. Check HW configuration.\n",
		     val);
		}
		val = 0;
		retval = -1;
	}

	core_if->core_params->en_multiple_tx_fifo = val;
	return retval;
}

int32_t dwc_otg_get_param_en_multiple_tx_fifo(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->en_multiple_tx_fifo;
}

int dwc_otg_set_param_dev_tx_fifo_size(dwc_otg_core_if_t *core_if, int32_t val,
				       int fifo_num)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 4, 768)) {
		printf("warning: Wrong value for dev_tx_fifo_size\n");
		printf("warning: dev_tx_fifo_size must be 4-768\n");
		return -1;
	}

	if (val > (dwc_read_reg32(&core_if->core_global_regs->dptxfsiz_dieptxf[fifo_num]))) {
		if (dwc_otg_param_initialized(core_if->core_params->dev_tx_fifo_size[fifo_num])) {
			printf("error: `%d' invalid for parameter `dev_tx_fifo_size_%d'. Check HW configuration.\n",
		     val, fifo_num);
		}
		val = (dwc_read_reg32(&core_if->core_global_regs->dptxfsiz_dieptxf[fifo_num]));
		retval = -1;
	}

	core_if->core_params->dev_tx_fifo_size[fifo_num] = val;
	return retval;
}

int32_t dwc_otg_get_param_dev_tx_fifo_size(dwc_otg_core_if_t *core_if,
					   int fifo_num)
{
	return core_if->core_params->dev_tx_fifo_size[fifo_num];
}

int dwc_otg_set_param_thr_ctl(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 0, 7)) {
		printf("warning: Wrong value for thr_ctl\n");
		printf("warning: thr_ctl must be 0-7\n");
		return -1;
	}

	if ((val != 0) &&
	    (!dwc_otg_get_param_dma_enable(core_if) ||
	     !core_if->hwcfg4.b.ded_fifo_en)) {
		if (dwc_otg_param_initialized(core_if->core_params->thr_ctl)) {
			printf("error: %d invalid for parameter thr_ctl. Check HW configuration.\n",
		     val);
		}
		val = 0;
		retval = -1;
	}

	core_if->core_params->thr_ctl = val;
	return retval;
}

int32_t dwc_otg_get_param_thr_ctl(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->thr_ctl;
}

int dwc_otg_set_param_lpm_enable(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;

	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: Wrong value for lpm_enable\n");
		printf("warning: lpm_enable must be 0 or 1\n");
		return -1;
	}

	if (val && !core_if->hwcfg3.b.otg_lpm_en) {
		if (dwc_otg_param_initialized(core_if->core_params->lpm_enable)) {
			printf("error: %d invalid for parameter lpm_enable. Check HW configuration.\n",
		     val);
		}
		val = 0;
		retval = -1;
	}

	core_if->core_params->lpm_enable = val;
	return retval;
}

int32_t dwc_otg_get_param_lpm_enable(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->lpm_enable;
}

int dwc_otg_set_param_tx_thr_length(dwc_otg_core_if_t *core_if, int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 8, 128)) {
		printf("warning: Wrong valaue for tx_thr_length\n");
		printf("warning: tx_thr_length must be 8 - 128\n");
		return -1;
	}

	core_if->core_params->tx_thr_length = val;
	return 0;
}

int32_t dwc_otg_get_param_tx_thr_length(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->tx_thr_length;
}

int dwc_otg_set_param_rx_thr_length(dwc_otg_core_if_t *core_if, int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 8, 128)) {
		printf("warning: Wrong valaue for rx_thr_length\n");
		printf("warning: rx_thr_length must be 8 - 128\n");
		return -1;
	}

	core_if->core_params->rx_thr_length = val;
	return 0;
}

int32_t dwc_otg_get_param_rx_thr_length(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->rx_thr_length;
}

int dwc_otg_set_param_dma_burst_size(dwc_otg_core_if_t *core_if, int32_t val)
{
	if (DWC_OTG_PARAM_TEST(val, 1, 1) &&
	    DWC_OTG_PARAM_TEST(val, 4, 4) &&
	    DWC_OTG_PARAM_TEST(val, 8, 8) &&
	    DWC_OTG_PARAM_TEST(val, 16, 16) &&
	    DWC_OTG_PARAM_TEST(val, 32, 32) &&
	    DWC_OTG_PARAM_TEST(val, 64, 64) &&
	    DWC_OTG_PARAM_TEST(val, 128, 128) &&
	    DWC_OTG_PARAM_TEST(val, 256, 256)) {
		printf("warning: `%d' invalid for parameter `dma_burst_size'\n", val);
		return -1;
	}
	core_if->core_params->dma_burst_size = val;
	return 0;
}

int32_t dwc_otg_get_param_dma_burst_size(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->dma_burst_size;
}

int dwc_otg_set_param_pti_enable(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: `%d' invalid for parameter `pti_enable'\n", val);
		return -1;
	}
	if (val && (core_if->snpsid < OTG_CORE_REV_2_72a)) {
		if (dwc_otg_param_initialized(core_if->core_params->pti_enable)) {
			printf("error: %d invalid for parameter pti_enable. Check HW configuration.\n",
			     val);
		}
		retval = -1;
		val = 0;
	}
	core_if->core_params->pti_enable = val;
	return retval;
}

int32_t dwc_otg_get_param_pti_enable(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->pti_enable;
}

int dwc_otg_set_param_mpi_enable(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: `%d' invalid for parameter `mpi_enable'\n", val);
		return -1;
	}
	if (val && (core_if->hwcfg2.b.multi_proc_int == 0)) {
		if (dwc_otg_param_initialized(core_if->core_params->mpi_enable)) {
			printf("error: %d invalid for parameter mpi_enable. Check HW configuration.\n",
			     val);
		}
		retval = -1;
		val = 0;
	}
	core_if->core_params->mpi_enable = val;
	return retval;
}

int32_t dwc_otg_get_param_mpi_enable(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->mpi_enable;
}

int dwc_otg_set_param_ic_usb_cap(dwc_otg_core_if_t *core_if,
					int32_t val)
{
	int retval = 0;
	if (DWC_OTG_PARAM_TEST(val, 0, 1)) {
		printf("warning: `%d' invalid for parameter `ic_usb_cap'\n", val);
		printf("warning: ic_usb_cap must be 0 or 1\n");
		return -1;
	}

	if (val && (core_if->hwcfg3.b.otg_enable_ic_usb == 0)) {
		if (dwc_otg_param_initialized(core_if->core_params->ic_usb_cap)) {
			printf("error: %d invalid for parameter ic_usb_cap. Check HW configuration.\n",
			     val);
		}
		retval = -1;
		val = 0;
	}
	core_if->core_params->ic_usb_cap = val;
	return retval;
}
int32_t dwc_otg_get_param_ic_usb_cap(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->ic_usb_cap;
}

int dwc_otg_set_param_ahb_thr_ratio(dwc_otg_core_if_t *core_if, int32_t val)
{
	int retval = 0;
	int valid = 1;

	if (DWC_OTG_PARAM_TEST(val, 0, 3)) {
		printf("warning: `%d' invalid for parameter `ahb_thr_ratio'\n", val);
		printf("warning: ahb_thr_ratio must be 0 - 3\n");
		return -1;
	}

	if (val && (core_if->snpsid < OTG_CORE_REV_2_81a || !dwc_otg_get_param_thr_ctl(core_if))) {
		valid = 0;
	} else if (val && ((dwc_otg_get_param_tx_thr_length(core_if) / (1 << val)) < 4)) {
		valid = 0;
	}
	if (valid == 0) {
		if (dwc_otg_param_initialized(core_if->core_params->ahb_thr_ratio)) {
			printf("error: %d invalid for parameter ahb_thr_ratio. Chack HW configuration.\n", val);
		}
		retval = -1;
		val = 0;
	}

	core_if->core_params->ahb_thr_ratio = val;
	return retval;
}
int32_t dwc_otg_get_param_ahb_thr_ratio(dwc_otg_core_if_t *core_if)
{
	return core_if->core_params->ahb_thr_ratio;
}


uint32_t dwc_otg_get_hnpstatus(dwc_otg_core_if_t *core_if)
{
	gotgctl_data_t otgctl;
	otgctl.d32 = dwc_read_reg32(&core_if->core_global_regs->gotgctl);
	return otgctl.b.hstnegscs;
}

uint32_t dwc_otg_get_srpstatus(dwc_otg_core_if_t *core_if)
{
	gotgctl_data_t otgctl;
	otgctl.d32 = dwc_read_reg32(&core_if->core_global_regs->gotgctl);
	return otgctl.b.sesreqscs;
}

void dwc_otg_set_hnpreq(dwc_otg_core_if_t *core_if, uint32_t val)
{
	gotgctl_data_t otgctl;
	otgctl.d32 = dwc_read_reg32(&core_if->core_global_regs->gotgctl);
	otgctl.b.hnpreq = val;
	dwc_write_reg32(&core_if->core_global_regs->gotgctl, otgctl.d32);
}

uint32_t dwc_otg_get_gsnpsid(dwc_otg_core_if_t *core_if)
{
	return core_if->snpsid;
}

uint32_t dwc_otg_get_mode(dwc_otg_core_if_t *core_if)
{
	gotgctl_data_t otgctl;
	otgctl.d32 = dwc_read_reg32(&core_if->core_global_regs->gotgctl);
	return otgctl.b.currmod;
}

uint32_t dwc_otg_get_hnpcapable(dwc_otg_core_if_t *core_if)
{
	gusbcfg_data_t usbcfg;
	usbcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->gusbcfg);
	return usbcfg.b.hnpcap;
}

void dwc_otg_set_hnpcapable(dwc_otg_core_if_t *core_if, uint32_t val)
{
	gusbcfg_data_t usbcfg;
	usbcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->gusbcfg);
	usbcfg.b.hnpcap = val;
	dwc_write_reg32(&core_if->core_global_regs->gusbcfg, usbcfg.d32);
}

uint32_t dwc_otg_get_srpcapable(dwc_otg_core_if_t *core_if)
{
	gusbcfg_data_t usbcfg;
	usbcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->gusbcfg);
	return usbcfg.b.srpcap;
}

void dwc_otg_set_srpcapable(dwc_otg_core_if_t *core_if, uint32_t val)
{
	gusbcfg_data_t usbcfg;
	usbcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->gusbcfg);
	usbcfg.b.srpcap = val;
	dwc_write_reg32(&core_if->core_global_regs->gusbcfg, usbcfg.d32);
}

uint32_t dwc_otg_get_busconnected(dwc_otg_core_if_t *core_if)
{
	hprt0_data_t hprt0;
	hprt0.d32 = dwc_read_reg32(core_if->host_if->hprt0);
	return hprt0.b.prtconnsts;
}

uint32_t dwc_otg_get_prtpower(dwc_otg_core_if_t *core_if)
{
	hprt0_data_t hprt0;
	hprt0.d32 = dwc_read_reg32(core_if->host_if->hprt0);
	return hprt0.b.prtpwr;

}

void dwc_otg_set_prtpower(dwc_otg_core_if_t *core_if, uint32_t val)
{
	hprt0_data_t hprt0;
	hprt0.d32 = dwc_read_reg32(core_if->host_if->hprt0);
	hprt0.b.prtpwr = val;
	dwc_write_reg32(core_if->host_if->hprt0, hprt0.d32);
}

uint32_t dwc_otg_get_prtsuspend(dwc_otg_core_if_t *core_if)
{
	hprt0_data_t hprt0;
	hprt0.d32 = dwc_read_reg32(core_if->host_if->hprt0);
	return hprt0.b.prtsusp;

}

void dwc_otg_set_prtsuspend(dwc_otg_core_if_t *core_if, uint32_t val)
{
	hprt0_data_t hprt0;
	hprt0.d32 = dwc_read_reg32(core_if->host_if->hprt0);
	hprt0.b.prtsusp = val;
	dwc_write_reg32(core_if->host_if->hprt0, hprt0.d32);
}

void dwc_otg_set_prtresume(dwc_otg_core_if_t *core_if, uint32_t val)
{
	hprt0_data_t hprt0;
	hprt0.d32 = dwc_read_reg32(core_if->host_if->hprt0);
	hprt0.b.prtres = val;
	dwc_write_reg32(core_if->host_if->hprt0, hprt0.d32);
}

uint32_t dwc_otg_get_lpm_portsleepstatus(dwc_otg_core_if_t *core_if)
{
	glpmcfg_data_t lpmcfg;
	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);

	return lpmcfg.b.prt_sleep_sts;
}

uint32_t dwc_otg_get_lpm_remotewakeenabled(dwc_otg_core_if_t *core_if)
{
	glpmcfg_data_t lpmcfg;
	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);
	return lpmcfg.b.rem_wkup_en;
}

uint32_t dwc_otg_get_lpmresponse(dwc_otg_core_if_t *core_if)
{
	glpmcfg_data_t lpmcfg;
	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);
	return lpmcfg.b.appl_resp;
}

void dwc_otg_set_lpmresponse(dwc_otg_core_if_t *core_if, uint32_t val)
{
	glpmcfg_data_t lpmcfg;
	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);
	lpmcfg.b.appl_resp = val;
	dwc_write_reg32(&core_if->core_global_regs->glpmcfg, lpmcfg.d32);
}

uint32_t dwc_otg_get_hsic_connect(dwc_otg_core_if_t *core_if)
{
	glpmcfg_data_t lpmcfg;
	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);
	return lpmcfg.b.hsic_connect;
}

void dwc_otg_set_hsic_connect(dwc_otg_core_if_t *core_if, uint32_t val)
{
	glpmcfg_data_t lpmcfg;
	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);
	lpmcfg.b.hsic_connect = val;
	dwc_write_reg32(&core_if->core_global_regs->glpmcfg, lpmcfg.d32);
}

uint32_t dwc_otg_get_inv_sel_hsic(dwc_otg_core_if_t *core_if)
{
	glpmcfg_data_t lpmcfg;
	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);
	return lpmcfg.b.inv_sel_hsic;

}

void dwc_otg_set_inv_sel_hsic(dwc_otg_core_if_t *core_if, uint32_t val)
{
	glpmcfg_data_t lpmcfg;
	lpmcfg.d32 = dwc_read_reg32(&core_if->core_global_regs->glpmcfg);
	lpmcfg.b.inv_sel_hsic = val;
	dwc_write_reg32(&core_if->core_global_regs->glpmcfg, lpmcfg.d32);
}

uint32_t dwc_otg_get_gotgctl(dwc_otg_core_if_t *core_if)
{
	return dwc_read_reg32(&core_if->core_global_regs->gotgctl);
}

void dwc_otg_set_gotgctl(dwc_otg_core_if_t *core_if, uint32_t val)
{
	dwc_write_reg32(&core_if->core_global_regs->gotgctl, val);
}

uint32_t dwc_otg_get_gusbcfg(dwc_otg_core_if_t *core_if)
{
	return dwc_read_reg32(&core_if->core_global_regs->gusbcfg);
}

void dwc_otg_set_gusbcfg(dwc_otg_core_if_t *core_if, uint32_t val)
{
	dwc_write_reg32(&core_if->core_global_regs->gusbcfg, val);
}

uint32_t dwc_otg_get_grxfsiz(dwc_otg_core_if_t *core_if)
{
	return dwc_read_reg32(&core_if->core_global_regs->grxfsiz);
}

void dwc_otg_set_grxfsiz(dwc_otg_core_if_t *core_if, uint32_t val)
{
	dwc_write_reg32(&core_if->core_global_regs->grxfsiz, val);
}

uint32_t dwc_otg_get_gnptxfsiz(dwc_otg_core_if_t *core_if)
{
	return dwc_read_reg32(&core_if->core_global_regs->gnptxfsiz);
}

void dwc_otg_set_gnptxfsiz(dwc_otg_core_if_t *core_if, uint32_t val)
{
	dwc_write_reg32(&core_if->core_global_regs->gnptxfsiz, val);
}

uint32_t dwc_otg_get_gpvndctl(dwc_otg_core_if_t *core_if)
{
	return dwc_read_reg32(&core_if->core_global_regs->gpvndctl);
}

void dwc_otg_set_gpvndctl(dwc_otg_core_if_t *core_if, uint32_t val)
{
	dwc_write_reg32(&core_if->core_global_regs->gpvndctl, val);
}

uint32_t dwc_otg_get_ggpio(dwc_otg_core_if_t *core_if)
{
	return dwc_read_reg32(&core_if->core_global_regs->ggpio);
}

void dwc_otg_set_ggpio(dwc_otg_core_if_t *core_if, uint32_t val)
{
	dwc_write_reg32(&core_if->core_global_regs->ggpio, val);
}

uint32_t dwc_otg_get_hprt0(dwc_otg_core_if_t *core_if)
{
	return dwc_read_reg32(core_if->host_if->hprt0);

}

void dwc_otg_set_hprt0(dwc_otg_core_if_t *core_if, uint32_t val)
{
	dwc_write_reg32(core_if->host_if->hprt0, val);
}

uint32_t dwc_otg_get_guid(dwc_otg_core_if_t *core_if)
{
	return dwc_read_reg32(&core_if->core_global_regs->guid);
}

void dwc_otg_set_guid(dwc_otg_core_if_t *core_if, uint32_t val)
{
	dwc_write_reg32(&core_if->core_global_regs->guid, val);
}

uint32_t dwc_otg_get_hptxfsiz(dwc_otg_core_if_t *core_if)
{
	return dwc_read_reg32(&core_if->core_global_regs->hptxfsiz);
}


static int dwc_otg_setup_params(dwc_otg_core_if_t *core_if)
{
	int i;
	core_if->core_params = malloc(sizeof(*core_if->core_params));
	if (!core_if->core_params) {
		return -1;
	}
	dwc_otg_set_uninitialized((int32_t *) core_if->core_params,
				  sizeof(*core_if->core_params) /
				  sizeof(int32_t));
	PDEBUG("Setting default values for core params\n");
	dwc_otg_set_param_otg_cap(core_if, dwc_param_otg_cap_default);
	dwc_otg_set_param_dma_enable(core_if, dwc_param_dma_enable_default);
	dwc_otg_set_param_dma_desc_enable(core_if,
					  dwc_param_dma_desc_enable_default);
	dwc_otg_set_param_opt(core_if, dwc_param_opt_default);
	dwc_otg_set_param_dma_burst_size(core_if,
					 dwc_param_dma_burst_size_default);
	dwc_otg_set_param_host_support_fs_ls_low_power(core_if,
						       dwc_param_host_support_fs_ls_low_power_default);
	dwc_otg_set_param_enable_dynamic_fifo(core_if,
					      dwc_param_enable_dynamic_fifo_default);
	dwc_otg_set_param_data_fifo_size(core_if,
					 dwc_param_data_fifo_size_default);
	dwc_otg_set_param_dev_rx_fifo_size(core_if,
					   dwc_param_dev_rx_fifo_size_default);
	dwc_otg_set_param_dev_nperio_tx_fifo_size(core_if,
						  dwc_param_dev_nperio_tx_fifo_size_default);
	dwc_otg_set_param_host_rx_fifo_size(core_if,
					    dwc_param_host_rx_fifo_size_default);
	dwc_otg_set_param_host_nperio_tx_fifo_size(core_if,
						   dwc_param_host_nperio_tx_fifo_size_default);
	dwc_otg_set_param_host_perio_tx_fifo_size(core_if,
						  dwc_param_host_perio_tx_fifo_size_default);
	dwc_otg_set_param_max_transfer_size(core_if,
					    dwc_param_max_transfer_size_default);
	dwc_otg_set_param_max_packet_count(core_if,
					   dwc_param_max_packet_count_default);
	dwc_otg_set_param_host_channels(core_if,
					dwc_param_host_channels_default);
	dwc_otg_set_param_dev_endpoints(core_if,
					dwc_param_dev_endpoints_default);
	dwc_otg_set_param_phy_type(core_if, dwc_param_phy_type_default);
	dwc_otg_set_param_speed(core_if, dwc_param_speed_default);
	dwc_otg_set_param_host_ls_low_power_phy_clk(core_if,
						    dwc_param_host_ls_low_power_phy_clk_default);
	dwc_otg_set_param_phy_ulpi_ddr(core_if, dwc_param_phy_ulpi_ddr_default);
	dwc_otg_set_param_phy_ulpi_ext_vbus(core_if,
					    dwc_param_phy_ulpi_ext_vbus_default);
	dwc_otg_set_param_phy_utmi_width(core_if,
					 dwc_param_phy_utmi_width_default);
	dwc_otg_set_param_ts_dline(core_if, dwc_param_ts_dline_default);
	dwc_otg_set_param_i2c_enable(core_if, dwc_param_i2c_enable_default);
	dwc_otg_set_param_ulpi_fs_ls(core_if, dwc_param_ulpi_fs_ls_default);
	dwc_otg_set_param_en_multiple_tx_fifo(core_if,
					      dwc_param_en_multiple_tx_fifo_default);
	for (i = 0; i < 15; i++) {
		dwc_otg_set_param_dev_perio_tx_fifo_size(core_if,
							 dwc_param_dev_perio_tx_fifo_size_default,
							 i);
	}

	for (i = 0; i < 15; i++) {
		dwc_otg_set_param_dev_tx_fifo_size(core_if,
						   dwc_param_dev_tx_fifo_size_default,
						   i);
	}
	dwc_otg_set_param_thr_ctl(core_if, dwc_param_thr_ctl_default);
	dwc_otg_set_param_mpi_enable(core_if, dwc_param_mpi_enable_default);
	dwc_otg_set_param_pti_enable(core_if, dwc_param_pti_enable_default);
	dwc_otg_set_param_lpm_enable(core_if, dwc_param_lpm_enable_default);
	dwc_otg_set_param_ic_usb_cap(core_if, dwc_param_ic_usb_cap_default);
	dwc_otg_set_param_tx_thr_length(core_if,
					dwc_param_tx_thr_length_default);
	dwc_otg_set_param_rx_thr_length(core_if,
					dwc_param_rx_thr_length_default);
	dwc_otg_set_param_ahb_thr_ratio(core_if, dwc_param_ahb_thr_ratio_default);
	PDEBUG("Finished setting default values for core params\n");
	return 0;
}

void dwc_otg_cil_init(dwc_otg_core_if_t *core_if, const uint32_t *reg_base_addr)
{
	dwc_otg_host_if_t *host_if = 0;
	uint8_t *reg_base = (uint8_t *) reg_base_addr;
	int i = 0;

	PDEBUG("%s(%p)\n", __func__, reg_base_addr);

	if (core_if == 0) {
		PDEBUG("dwc_otg_core_if_t is NULL\n");
		return;
	}
	core_if->core_global_regs = (dwc_otg_core_global_regs_t *) reg_base;

	/*
	 * Allocate the Host Mode structures.
	 */
	host_if = malloc(sizeof(dwc_otg_host_if_t));

	if (host_if == 0) {
		PDEBUG("Allocation of dwc_otg_host_if_t failed\n");
		return;
	}

	host_if->host_global_regs = (dwc_otg_host_global_regs_t *)
	    (reg_base + DWC_OTG_HOST_GLOBAL_REG_OFFSET);

	host_if->hprt0 =
	    (uint32_t *) (reg_base + DWC_OTG_HOST_PORT_REGS_OFFSET);

	for (i = 0; i < MAX_EPS_CHANNELS; i++) {
		host_if->hc_regs[i] = (dwc_otg_hc_regs_t *)
		    (reg_base + DWC_OTG_HOST_CHAN_REGS_OFFSET +
		     (i * DWC_OTG_CHAN_REGS_OFFSET));
		PDEBUG("hc_reg[%d]->hcchar=%p\n",
			    i, &host_if->hc_regs[i]->hcchar);
	}

	host_if->num_host_channels = MAX_EPS_CHANNELS;
	core_if->host_if = host_if;

	for (i = 0; i < MAX_EPS_CHANNELS; i++) {
		core_if->data_fifo[i] =
		    (uint32_t *) (reg_base + DWC_OTG_DATA_FIFO_OFFSET +
				  (i * DWC_OTG_DATA_FIFO_SIZE));
		PDEBUG("data_fifo[%d]=0x%08x\n",
			    i, (unsigned)core_if->data_fifo[i]);
	}

	core_if->pcgcctl = (uint32_t *) (reg_base + DWC_OTG_PCGCCTL_OFFSET);

	/*
	 * Store the contents of the hardware configuration registers here for
	 * easy access later.
	 */
	core_if->hwcfg1.d32 =
	    dwc_read_reg32(&core_if->core_global_regs->ghwcfg1);
	core_if->hwcfg2.d32 =
	    dwc_read_reg32(&core_if->core_global_regs->ghwcfg2);
	core_if->hwcfg3.d32 =
	    dwc_read_reg32(&core_if->core_global_regs->ghwcfg3);
	core_if->hwcfg4.d32 =
	    dwc_read_reg32(&core_if->core_global_regs->ghwcfg4);

	PDEBUG("hwcfg1=%08x\n", core_if->hwcfg1.d32);
	PDEBUG("hwcfg2=%08x\n", core_if->hwcfg2.d32);
	PDEBUG("hwcfg3=%08x\n", core_if->hwcfg3.d32);
	PDEBUG("hwcfg4=%08x\n", core_if->hwcfg4.d32);

	core_if->hcfg.d32 =
	    dwc_read_reg32(&core_if->host_if->host_global_regs->hcfg);

	PDEBUG("hcfg=%08x\n", core_if->hcfg.d32);

	PDEBUG("op_mode=%0x\n", core_if->hwcfg2.b.op_mode);
	PDEBUG("arch=%0x\n", core_if->hwcfg2.b.architecture);
	PDEBUG("num_dev_ep=%d\n", core_if->hwcfg2.b.num_dev_ep);
	PDEBUG("num_host_chan=%d\n",
		    core_if->hwcfg2.b.num_host_chan);
	PDEBUG("nonperio_tx_q_depth=0x%0x\n",
		    core_if->hwcfg2.b.nonperio_tx_q_depth);
	PDEBUG("host_perio_tx_q_depth=0x%0x\n",
		    core_if->hwcfg2.b.host_perio_tx_q_depth);
	PDEBUG("dev_token_q_depth=0x%0x\n",
		    core_if->hwcfg2.b.dev_token_q_depth);

	PDEBUG("Total FIFO SZ=%d\n",
		    core_if->hwcfg3.b.dfifo_depth);
	PDEBUG("xfer_size_cntr_width=%0x\n",
		    core_if->hwcfg3.b.xfer_size_cntr_width);

	core_if->snpsid = dwc_read_reg32(&core_if->core_global_regs->gsnpsid);

	printf("Core Release: %x.%x%x%x\n",
		   (core_if->snpsid >> 12 & 0xF),
		   (core_if->snpsid >> 8 & 0xF),
		   (core_if->snpsid >> 4 & 0xF), (core_if->snpsid & 0xF));

	dwc_otg_setup_params(core_if);
}

/**
 * Initializes the FSLSPClkSel field of the HCFG register depending on the PHY
 * type.
 */
static void init_fslspclksel(dwc_otg_core_if_t *core_if)
{
	uint32_t val;
	hcfg_data_t hcfg;

	if (((core_if->hwcfg2.b.hs_phy_type == 2) &&
	     (core_if->hwcfg2.b.fs_phy_type == 1) &&
	     (core_if->core_params->ulpi_fs_ls)) ||
	    (core_if->core_params->phy_type == DWC_PHY_TYPE_PARAM_FS)) {
		/* Full speed PHY */
		val = DWC_HCFG_48_MHZ;
	} else {
		/* High speed PHY running at full speed or high speed */
		val = DWC_HCFG_30_60_MHZ;
	}

	PDEBUG("Initializing HCFG.FSLSPClkSel to 0x%1x\n", val);
	hcfg.d32 = dwc_read_reg32(&core_if->host_if->host_global_regs->hcfg);
	hcfg.b.fslspclksel = val;
	dwc_write_reg32(&core_if->host_if->host_global_regs->hcfg, hcfg.d32);
}

/**
 * Flush a Tx FIFO.
 *
 * @param core_if Programming view of DWC_otg controller.
 * @param num Tx FIFO to flush.
 */
void dwc_otg_flush_tx_fifo(dwc_otg_core_if_t *core_if, const int num)
{
	dwc_otg_core_global_regs_t *global_regs = core_if->core_global_regs;
	volatile grstctl_t greset = {.d32 = 0 };
	int count = 0;

	PDEBUG("Flush Tx FIFO %d\n", num);

	greset.b.txfflsh = 1;
	greset.b.txfnum = num;
	dwc_write_reg32(&global_regs->grstctl, greset.d32);

	do {
		greset.d32 = dwc_read_reg32(&global_regs->grstctl);
		if (++count > 10000) {
			printf("%s() HANG! GRSTCTL=%0x GNPTXSTS=0x%08x\n",
				 __func__, greset.d32,
				 dwc_read_reg32(&global_regs->gnptxsts));
			break;
		}
		udelay(1);
	} while (greset.b.txfflsh == 1);

	/* Wait for 3 PHY Clocks */
	udelay(1);
}

/**
 * Flush Rx FIFO.
 *
 * @param core_if Programming view of DWC_otg controller.
 */
void dwc_otg_flush_rx_fifo(dwc_otg_core_if_t *core_if)
{
	dwc_otg_core_global_regs_t *global_regs = core_if->core_global_regs;
	volatile grstctl_t greset = {.d32 = 0 };
	int count = 0;

	PDEBUG("%s\n", __func__);
	/*
	 *
	 */
	greset.b.rxfflsh = 1;
	dwc_write_reg32(&global_regs->grstctl, greset.d32);

	do {
		greset.d32 = dwc_read_reg32(&global_regs->grstctl);
		if (++count > 10000) {
			printf("%s() HANG! GRSTCTL=%0x\n", __func__,
				 greset.d32);
			break;
		}
		udelay(1);
	} while (greset.b.rxfflsh == 1);

	/* Wait for 3 PHY Clocks */
	udelay(1);
}

/**
 * This function initializes the DWC_otg controller registers for
 * host mode.
 *
 * This function flushes the Tx and Rx FIFOs and it flushes any entries in the
 * request queues. Host channels are reset to ensure that they are ready for
 * performing transfers.
 *
 * @param core_if Programming view of DWC_otg controller
 *
 */
void dwc_otg_core_host_init(dwc_otg_core_if_t *core_if)
{
	dwc_otg_core_global_regs_t *global_regs = core_if->core_global_regs;
	dwc_otg_host_if_t *host_if = core_if->host_if;
	dwc_otg_core_params_t *params = core_if->core_params;
	hprt0_data_t hprt0 = {.d32 = 0 };
	fifosize_data_t nptxfifosize;
	fifosize_data_t ptxfifosize;
	int i;
	hcchar_data_t hcchar;
	hcfg_data_t hcfg;
	dwc_otg_hc_regs_t *hc_regs;
	int num_channels;
	gotgctl_data_t gotgctl = {.d32 = 0 };

	PDEBUG("%s(%p)\n", __func__, core_if);

	/* Restart the Phy Clock */
	dwc_write_reg32(core_if->pcgcctl, 0);

	/* Initialize Host Configuration Register */
	init_fslspclksel(core_if);
	if (core_if->core_params->speed == DWC_SPEED_PARAM_FULL) {
		hcfg.d32 = dwc_read_reg32(&host_if->host_global_regs->hcfg);
		hcfg.b.fslssupp = 1;
		dwc_write_reg32(&host_if->host_global_regs->hcfg, hcfg.d32);

	}

	if (core_if->core_params->dma_desc_enable) {
		uint8_t op_mode = core_if->hwcfg2.b.op_mode;
		if (!(core_if->hwcfg4.b.desc_dma && (core_if->snpsid >= OTG_CORE_REV_2_90a) &&
				((op_mode == DWC_HWCFG2_OP_MODE_HNP_SRP_CAPABLE_OTG) ||
				(op_mode == DWC_HWCFG2_OP_MODE_SRP_ONLY_CAPABLE_OTG) ||
				(op_mode == DWC_HWCFG2_OP_MODE_NO_HNP_SRP_CAPABLE_OTG) ||
				(op_mode == DWC_HWCFG2_OP_MODE_SRP_CAPABLE_HOST) ||
				(op_mode == DWC_HWCFG2_OP_MODE_NO_SRP_CAPABLE_HOST)))) {

				printf("Host can't operate in Descriptor DMA mode.\n"
					  "Either core version is below 2.90a or "
					  "GHWCFG2, GHWCFG4 registers' values do not allow Descriptor DMA in host mode.\n"
					  "To run the driver in Buffer DMA host mode set dma_desc_enable "
					  "module parameter to 0.\n");
				return;
		}
		hcfg.d32 = dwc_read_reg32(&host_if->host_global_regs->hcfg);
		hcfg.b.descdma = 1;
		dwc_write_reg32(&host_if->host_global_regs->hcfg, hcfg.d32);
	}

	/* Configure data FIFO sizes */
	if (core_if->hwcfg2.b.dynamic_fifo && params->enable_dynamic_fifo) {
		PDEBUG("Total FIFO Size=%d\n",
			    core_if->total_fifo_size);
		PDEBUG("Rx FIFO Size=%d\n",
			    params->host_rx_fifo_size);
		PDEBUG("NP Tx FIFO Size=%d\n",
			    params->host_nperio_tx_fifo_size);
		PDEBUG("P Tx FIFO Size=%d\n",
			    params->host_perio_tx_fifo_size);

		/* Rx FIFO */
		PDEBUG("initial grxfsiz=%08x\n",
			    dwc_read_reg32(&global_regs->grxfsiz));
		dwc_write_reg32(&global_regs->grxfsiz,
				params->host_rx_fifo_size);
		PDEBUG("new grxfsiz=%08x\n",
			    dwc_read_reg32(&global_regs->grxfsiz));

		/* Non-periodic Tx FIFO */
		PDEBUG("initial gnptxfsiz=%08x\n",
			    dwc_read_reg32(&global_regs->gnptxfsiz));
		nptxfifosize.b.depth = params->host_nperio_tx_fifo_size;
		nptxfifosize.b.startaddr = params->host_rx_fifo_size;
		dwc_write_reg32(&global_regs->gnptxfsiz, nptxfifosize.d32);
		PDEBUG("new gnptxfsiz=%08x\n",
			    dwc_read_reg32(&global_regs->gnptxfsiz));

		/* Periodic Tx FIFO */
		PDEBUG("initial hptxfsiz=%08x\n",
			    dwc_read_reg32(&global_regs->hptxfsiz));
		ptxfifosize.b.depth = params->host_perio_tx_fifo_size;
		ptxfifosize.b.startaddr =
		    nptxfifosize.b.startaddr + nptxfifosize.b.depth;
		dwc_write_reg32(&global_regs->hptxfsiz, ptxfifosize.d32);
		PDEBUG("new hptxfsiz=%08x\n",
			    dwc_read_reg32(&global_regs->hptxfsiz));
	}

	/* Clear Host Set HNP Enable in the OTG Control Register */
	gotgctl.b.hstsethnpen = 1;
	dwc_modify_reg32(&global_regs->gotgctl, gotgctl.d32, 0);

	/* Make sure the FIFOs are flushed. */
	dwc_otg_flush_tx_fifo(core_if, 0x10 /* all Tx FIFOs */);
	dwc_otg_flush_rx_fifo(core_if);

	if (!core_if->core_params->dma_desc_enable) {
		/* Flush out any leftover queued requests. */
		num_channels = core_if->core_params->host_channels;

		for (i = 0; i < num_channels; i++) {
			hc_regs = core_if->host_if->hc_regs[i];
			hcchar.d32 = dwc_read_reg32(&hc_regs->hcchar);
			hcchar.b.chen = 0;
			hcchar.b.chdis = 1;
			hcchar.b.epdir = 0;
			dwc_write_reg32(&hc_regs->hcchar, hcchar.d32);
		}

		/* Halt all channels to put them into a known state. */
		for (i = 0; i < num_channels; i++) {
			int count = 0;
			hc_regs = core_if->host_if->hc_regs[i];
			hcchar.d32 = dwc_read_reg32(&hc_regs->hcchar);
			hcchar.b.chen = 1;
			hcchar.b.chdis = 1;
			hcchar.b.epdir = 0;
			dwc_write_reg32(&hc_regs->hcchar, hcchar.d32);
			PDEBUG("%s: Halt channel %d regs %p\n", __func__, i, hc_regs);
			do {
				hcchar.d32 = dwc_read_reg32(&hc_regs->hcchar);
				if (++count > 1000) {
					printf
					    ("%s: Unable to clear halt on channel %d (timeout HCCHAR 0x%X @%p)\n",
					     __func__, i, hcchar.d32, &hc_regs->hcchar);
					break;
				}
				udelay(1);
			} while (hcchar.b.chen);
		}
	}

	/* Turn on the vbus power. */
	PDEBUG("Init: Port Power? op_state=%d\n", core_if->op_state);
	if (core_if->op_state == A_HOST) {
		hprt0.d32 = dwc_otg_read_hprt0(core_if);
		PDEBUG("Init: Power Port (%d, %08x)\n", hprt0.b.prtpwr, hprt0.d32);
		if (hprt0.b.prtpwr == 0) {
			hprt0.b.prtpwr = 1;
			dwc_write_reg32(host_if->hprt0, hprt0.d32);
		}
	}
}

/**
 * Do core a soft reset of the core.  Be careful with this because it
 * resets all the internal state machines of the core.
 */
void dwc_otg_core_reset(dwc_otg_core_if_t *core_if)
{
	dwc_otg_core_global_regs_t *global_regs = core_if->core_global_regs;
	volatile grstctl_t greset = {.d32 = 0 };
	int count = 0;

	PDEBUG("%s\n", __func__);
	/* Wait for AHB master IDLE state. */
	do {
		udelay(10);
		greset.d32 = dwc_read_reg32(&global_regs->grstctl);
		if (++count > 100000) {
			printf("%s() HANG! AHB Idle GRSTCTL=%0x\n", __func__,
				 greset.d32);
			return;
		}
	} while (greset.b.ahbidle == 0);

	/* Core Soft Reset */
	count = 0;
	greset.b.csftrst = 1;
	dwc_write_reg32(&global_regs->grstctl, greset.d32);
	do {
		greset.d32 = dwc_read_reg32(&global_regs->grstctl);
		if (++count > 1000000) {
			printf("%s() HANG! Soft Reset GRSTCTL=%0x\n",
				 __func__, greset.d32);
			break;
		}
		udelay(1);
	} while (greset.b.csftrst == 1);

	/* Wait for 3 PHY Clocks */
	udelay(100 * 1000);
}



/**
 * This function initializes the DWC_otg controller registers and
 * prepares the core for device mode or host mode operation.
 *
 * @param core_if Programming view of the DWC_otg controller
 *
 */
void dwc_otg_core_init(dwc_otg_core_if_t *core_if)
{
	dwc_otg_core_global_regs_t *global_regs = core_if->core_global_regs;
	gahbcfg_data_t ahbcfg = {.d32 = 0 };
	gusbcfg_data_t usbcfg = {.d32 = 0 };
	gi2cctl_data_t i2cctl = {.d32 = 0 };

	PDEBUG("dwc_otg_core_init(%p) regs at %p\n",
	       core_if, global_regs);

	/* Common Initialization */

	usbcfg.d32 = dwc_read_reg32(&global_regs->gusbcfg);

	/* Program the ULPI External VBUS bit if needed */
	usbcfg.b.ulpi_ext_vbus_drv =
	    (core_if->core_params->phy_ulpi_ext_vbus ==
	     DWC_PHY_ULPI_EXTERNAL_VBUS) ? 1 : 0;

	/* Set external TS Dline pulsing */
	usbcfg.b.term_sel_dl_pulse =
	    (core_if->core_params->ts_dline == 1) ? 1 : 0;
	dwc_write_reg32(&global_regs->gusbcfg, usbcfg.d32);

	/* Reset the Controller */
	dwc_otg_core_reset(core_if);

	/* Initialize parameters from Hardware configuration registers. */
	core_if->total_fifo_size = core_if->hwcfg3.b.dfifo_depth;
	core_if->rx_fifo_size = dwc_read_reg32(&global_regs->grxfsiz);
	core_if->nperio_tx_fifo_size =
	    dwc_read_reg32(&global_regs->gnptxfsiz) >> 16;

	PDEBUG("Total FIFO SZ=%d\n", core_if->total_fifo_size);
	PDEBUG("Rx FIFO SZ=%d\n", core_if->rx_fifo_size);
	PDEBUG("NP Tx FIFO SZ=%d\n",
		    core_if->nperio_tx_fifo_size);

	/* This programming sequence needs to happen in FS mode before any other
	 * programming occurs */
	if ((core_if->core_params->speed == DWC_SPEED_PARAM_FULL) &&
	    (core_if->core_params->phy_type == DWC_PHY_TYPE_PARAM_FS)) {
		/* If FS mode with FS PHY */

		/* core_init() is now called on every switch so only call the
		 * following for the first time through. */
		if (!core_if->phy_init_done) {
			core_if->phy_init_done = 1;
			PDEBUG("FS_PHY detected\n");
			usbcfg.d32 = dwc_read_reg32(&global_regs->gusbcfg);
			usbcfg.b.physel = 1;
			dwc_write_reg32(&global_regs->gusbcfg, usbcfg.d32);

			/* Reset after a PHY select */
			dwc_otg_core_reset(core_if);
		}

		/* Program DCFG.DevSpd or HCFG.FSLSPclkSel to 48Mhz in FS.      Also
		 * do this on HNP Dev/Host mode switches (done in dev_init and
		 * host_init). */
		if (dwc_otg_is_host_mode(core_if)) {
			init_fslspclksel(core_if);
		}

		if (core_if->core_params->i2c_enable) {
			PDEBUG("FS_PHY Enabling I2c\n");
			/* Program GUSBCFG.OtgUtmifsSel to I2C */
			usbcfg.d32 = dwc_read_reg32(&global_regs->gusbcfg);
			usbcfg.b.otgutmifssel = 1;
			dwc_write_reg32(&global_regs->gusbcfg, usbcfg.d32);

			/* Program GI2CCTL.I2CEn */
			i2cctl.d32 = dwc_read_reg32(&global_regs->gi2cctl);
			i2cctl.b.i2cdevaddr = 1;
			i2cctl.b.i2cen = 0;
			dwc_write_reg32(&global_regs->gi2cctl, i2cctl.d32);
			i2cctl.b.i2cen = 1;
			dwc_write_reg32(&global_regs->gi2cctl, i2cctl.d32);
		}

	} /* endif speed == DWC_SPEED_PARAM_FULL */
	else {
		/* High speed PHY. */
		if (!core_if->phy_init_done) {
			core_if->phy_init_done = 1;
			/* HS PHY parameters.  These parameters are preserved
			 * during soft reset so only program the first time.  Do
			 * a soft reset immediately after setting phyif.  */
			usbcfg.b.ulpi_utmi_sel = core_if->core_params->phy_type;
			if (usbcfg.b.ulpi_utmi_sel == 1) {
				PDEBUG(" ULPI interface \n");
				/* ULPI interface */
				usbcfg.b.phyif = 0;
				usbcfg.b.ddrsel =
				    core_if->core_params->phy_ulpi_ddr;
			} else {
				PDEBUG(" UTMI+ interface \n");
				/* UTMI+ interface */
				if (core_if->core_params->phy_utmi_width == 16) {
					usbcfg.b.phyif = 1;

				} else {
					usbcfg.b.phyif = 0;
				}

			}

			dwc_write_reg32(&global_regs->gusbcfg, usbcfg.d32);
			/* Reset after setting the PHY parameters */
			dwc_otg_core_reset(core_if);
		}
	}

	if ((core_if->hwcfg2.b.hs_phy_type == 2) &&
	    (core_if->hwcfg2.b.fs_phy_type == 1) &&
	    (core_if->core_params->ulpi_fs_ls)) {
		PDEBUG("Setting ULPI FSLS\n");
		usbcfg.d32 = dwc_read_reg32(&global_regs->gusbcfg);
		usbcfg.b.ulpi_fsls = 1;
		usbcfg.b.ulpi_clk_sus_m = 1;
		dwc_write_reg32(&global_regs->gusbcfg, usbcfg.d32);
	} else {
		usbcfg.d32 = dwc_read_reg32(&global_regs->gusbcfg);
		usbcfg.b.ulpi_fsls = 0;
		usbcfg.b.ulpi_clk_sus_m = 0;
		dwc_write_reg32(&global_regs->gusbcfg, usbcfg.d32);
	}

	/* Program the GAHBCFG Register. */
	switch (core_if->hwcfg2.b.architecture) {

	case DWC_SLAVE_ONLY_ARCH:
		PDEBUG("Slave Only Mode\n");
		ahbcfg.b.nptxfemplvl_txfemplvl =
		    DWC_GAHBCFG_TXFEMPTYLVL_HALFEMPTY;
		ahbcfg.b.ptxfemplvl = DWC_GAHBCFG_TXFEMPTYLVL_HALFEMPTY;
		core_if->dma_enable = 0;
		core_if->dma_desc_enable = 0;
		break;

	case DWC_EXT_DMA_ARCH:
		PDEBUG("External DMA Mode\n");
		{
			uint8_t brst_sz = core_if->core_params->dma_burst_size;
			ahbcfg.b.hburstlen = 0;
			while (brst_sz > 1) {
				ahbcfg.b.hburstlen++;
				brst_sz >>= 1;
			}
		}
		core_if->dma_enable = (core_if->core_params->dma_enable != 0);
		core_if->dma_desc_enable =
		    (core_if->core_params->dma_desc_enable != 0);
		break;

	case DWC_INT_DMA_ARCH:
		PDEBUG("Internal DMA Mode\n");
		/*ahbcfg.b.hburstlen = DWC_GAHBCFG_INT_DMA_BURST_INCR; */
		ahbcfg.b.hburstlen = DWC_GAHBCFG_INT_DMA_BURST_INCR4;
		core_if->dma_enable = (core_if->core_params->dma_enable != 0);
		core_if->dma_desc_enable =
		    (core_if->core_params->dma_desc_enable != 0);
		break;

	}
	if (core_if->dma_enable) {
		if (core_if->dma_desc_enable) {
			PDEBUG("Using Descriptor DMA mode\n");
		} else {
			PDEBUG("Using Buffer DMA mode\n");

		}
	} else {
		PDEBUG("Using Slave mode\n");
		core_if->dma_desc_enable = 0;
	}

	ahbcfg.b.dmaenable = core_if->dma_enable;
	PDEBUG("DMA enable: %d(%08x)\n", ahbcfg.b.dmaenable, ahbcfg.d32);
	dwc_write_reg32(&global_regs->gahbcfg, ahbcfg.d32);

	core_if->en_multiple_tx_fifo = core_if->hwcfg4.b.ded_fifo_en;

	core_if->pti_enh_enable = core_if->core_params->pti_enable != 0;
	core_if->multiproc_int_enable = core_if->core_params->mpi_enable;
	PDEBUG("Periodic Transfer Interrupt Enhancement - %s\n",
		   ((core_if->pti_enh_enable) ? "enabled" : "disabled"));
	PDEBUG("Multiprocessor Interrupt Enhancement - %s\n",
		   ((core_if->multiproc_int_enable) ? "enabled" : "disabled"));


	ahbcfg.d32 = dwc_read_reg32(&global_regs->gahbcfg);

	/*
	 * Program the GUSBCFG register.
	 */
	usbcfg.d32 = dwc_read_reg32(&global_regs->gusbcfg);

	switch (core_if->hwcfg2.b.op_mode) {
	case DWC_MODE_HNP_SRP_CAPABLE:
		usbcfg.b.hnpcap = (core_if->core_params->otg_cap ==
				   DWC_OTG_CAP_PARAM_HNP_SRP_CAPABLE);
		usbcfg.b.srpcap = (core_if->core_params->otg_cap !=
				   DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE);
		break;

	case DWC_MODE_SRP_ONLY_CAPABLE:
		usbcfg.b.hnpcap = 0;
		usbcfg.b.srpcap = (core_if->core_params->otg_cap !=
				   DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE);
		break;

	case DWC_MODE_NO_HNP_SRP_CAPABLE:
		usbcfg.b.hnpcap = 0;
		usbcfg.b.srpcap = 0;
		break;

	case DWC_MODE_SRP_CAPABLE_DEVICE:
		usbcfg.b.hnpcap = 0;
		usbcfg.b.srpcap = (core_if->core_params->otg_cap !=
				   DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE);
		break;

	case DWC_MODE_NO_SRP_CAPABLE_DEVICE:
		usbcfg.b.hnpcap = 0;
		usbcfg.b.srpcap = 0;
		break;

	case DWC_MODE_SRP_CAPABLE_HOST:
		usbcfg.b.hnpcap = 0;
		usbcfg.b.srpcap = (core_if->core_params->otg_cap !=
				   DWC_OTG_CAP_PARAM_NO_HNP_SRP_CAPABLE);
		break;

	case DWC_MODE_NO_SRP_CAPABLE_HOST:
		usbcfg.b.hnpcap = 0;
		usbcfg.b.srpcap = 0;
		break;
	}

	dwc_write_reg32(&global_regs->gusbcfg, usbcfg.d32);

	if (core_if->core_params->ic_usb_cap) {
		gusbcfg_data_t gusbcfg = {.d32 = 0 };
		gusbcfg.b.ic_usb_cap = 1;
		dwc_modify_reg32(&core_if->core_global_regs->gusbcfg,
				 0, gusbcfg.d32);
	}

	/* Do device or host intialization based on mode during PCD
	 * and HCD initialization  */
	if (dwc_otg_is_host_mode(core_if)) {
		PDEBUG("Host Mode\n");
		core_if->op_state = A_HOST;
	}
}

/**
 * Prepares a host channel for transferring packets to/from a specific
 * endpoint. The HCCHARn register is set up with the characteristics specified
 * in _hc. Host channel interrupts that may need to be serviced while this
 * transfer is in progress are enabled.
 *
 * @param core_if Programming view of DWC_otg controller
 * @param hc Information needed to initialize the host channel
 */
void dwc_otg_hc_init(dwc_otg_core_if_t *core_if, uint8_t hc_num,
		uint8_t dev_addr, uint8_t ep_num, uint8_t ep_is_in,
		uint8_t ep_type, uint16_t max_packet)
{
	hcintmsk_data_t hc_intr_mask;
	hcchar_data_t hcchar;
	hcsplt_data_t hcsplt;

	dwc_otg_host_if_t *host_if = core_if->host_if;
	dwc_otg_hc_regs_t *hc_regs = host_if->hc_regs[hc_num];

	/* Clear old interrupt conditions for this host channel. */
	hc_intr_mask.d32 = 0xFFFFFFFF;
	hc_intr_mask.b.reserved14_31 = 0;
	dwc_write_reg32(&hc_regs->hcint, hc_intr_mask.d32);

	/*
	 * Program the HCCHARn register with the endpoint characteristics for
	 * the current transfer.
	 */
	hcchar.d32 = 0;
	hcchar.b.devaddr = dev_addr;
	hcchar.b.epnum = ep_num;
	hcchar.b.epdir = ep_is_in;
	hcchar.b.lspddev = 0; /* XXX: low speed handler needed */
	hcchar.b.eptype = ep_type;
	hcchar.b.mps = max_packet;

	dwc_write_reg32(&host_if->hc_regs[hc_num]->hcchar, hcchar.d32);

	PDEBUG("%s: Channel %d\n", __func__, hc_num);
	PDEBUG("	 Dev Addr: %d\n", hcchar.b.devaddr);
	PDEBUG("	 Ep Num: %d\n", hcchar.b.epnum);
	PDEBUG("	 Is In: %d\n", hcchar.b.epdir);
	PDEBUG("	 Is Low Speed: %d\n", hcchar.b.lspddev);
	PDEBUG("	 Ep Type: %d\n", hcchar.b.eptype);
	PDEBUG("	 Max Pkt: %d\n", hcchar.b.mps);
	PDEBUG("	 Multi Cnt: %d\n", hcchar.b.multicnt);

	/*
	 * Program the HCSPLIT register for SPLITs
	 */
	hcsplt.d32 = 0;
	dwc_write_reg32(&host_if->hc_regs[hc_num]->hcsplt, hcsplt.d32);
}
