/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Intel Coporation.
 */
#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <dw-i3c.h>
#include <i2c.h>
#include <log.h>
#include <malloc.h>
#include <pci.h>
#include <dm/device_compat.h>
#include <linux/completion.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/iopoll.h>

#define XFER_TIMEOUT (msecs_to_jiffies(1000))
#define struct_size(struct, count)  (sizeof(struct) * (count))

/**
 * @brief Check parity
 *
 * @param p: data
 *
 * @retval 0 if input is odd
 * @retval 1 if input is even
 */
static u8 odd_parity(u8 p)
{
    p ^= p >> 4;
    p &= 0xf;

    return (0x9669 >> p) & 1;
}

ulong msecs_to_jiffies(ulong msec)
{
    ulong hz = CONFIG_SYS_HZ;
    return (msec + (1000 / hz) - 1) / (1000 / hz);
}

/**
 * @brief Terminate the transfer process
 *
 * @param dev: Pointer to the device structure
 */
static void xfer_done(const struct udevice *dev, struct dw_i3c_xfer *xfer)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    struct dw_i3c_cmd *cmd;
    int nresp, resp;
    int i, ret = 0;

    nresp = readl(priv->regs + QUEUE_STATUS_LEVEL);
    nresp = QUEUE_STATUS_LEVEL_RESP(nresp);

    for (i = 0; i < nresp; i++) {
        resp = readl(priv->regs + RESPONSE_QUEUE_PORT);
        cmd = &xfer->cmds[RESPONSE_PORT_TID(resp)];
        cmd->rx_len = RESPONSE_PORT_DATA_LEN(resp);
        cmd->error = RESPONSE_PORT_ERR_STATUS(resp);
    }

    for (i = 0; i < nresp; i++) {
        switch (xfer->cmds[i].error) {
        case RESPONSE_NO_ERROR:
            break;
        case RESPONSE_ERROR_PARITY:        /* FALLTHROUGH */
        case RESPONSE_ERROR_IBA_NACK:        /* FALLTHROUGH */
        case RESPONSE_ERROR_TRANSF_ABORT:    /* FALLTHROUGH */
        case RESPONSE_ERROR_CRC:        /* FALLTHROUGH */
        case RESPONSE_ERROR_FRAME:
            ret = -EIO;
            break;
        case RESPONSE_ERROR_OVER_UNDER_FLOW:
            ret = -ENOSPC;
            break;
        case RESPONSE_ERROR_I2C_W_NACK_ERR:    /* FALLTHROUGH */
        case RESPONSE_ERROR_ADDRESS_NACK:    /* FALLTHROUGH */
        default:
            ret = -EINVAL;
            break;
        }
    }

    xfer->ret = ret;

    if (ret < 0) {
        writel(RESET_CTRL_RX_FIFO | RESET_CTRL_TX_FIFO |
                RESET_CTRL_RESP_QUEUE | RESET_CTRL_CMD_QUEUE,
                priv->regs + RESET_CTRL);
        writel(readl(priv->regs + DEVICE_CTRL) | DEV_CTRL_RESUME,
                       (priv->regs + DEVICE_CTRL));
    }
}

/**
 * @brief DesignWare IRQ handler
 *
 * @param dev: Pointer to the device structure
 *
 */
static void i3c_dw_irq(const struct udevice *dev, struct dw_i3c_xfer *xfer)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    u32 status;

    status = readl(priv->regs + INTR_STATUS);
    if (status & (INTR_TRANSFER_ERR_STAT | INTR_RESP_READY_STAT)) {
        xfer_done(dev,xfer);

        if (status & INTR_TRANSFER_ERR_STAT) {
            writel(INTR_TRANSFER_ERR_STAT,
                    priv->regs + INTR_STATUS);
        }
    }
}

static struct dw_i3c_xfer *
dw_i3c_master_alloc_xfer(struct dw_i3c_priv *priv, unsigned int ncmds)
{
    struct dw_i3c_xfer *xfer;

    xfer = calloc(1, struct_size(struct dw_i3c_xfer, ncmds));
    if (!xfer){
        debug("Memory allocation for xfer failed\n");
        return NULL;
    }

    xfer->ncmds = ncmds;
    xfer->ret = -ETIMEDOUT;
    
    return xfer;
}

static int get_free_pos(int free_pos)
{
    return ffs(free_pos) - 1;
}

/**
 * @brief Initialize the CCC command structure
 *
 * @param cmd: Pointer to the command data structure
 * @param addr: Target Address
 * @param data: Data to be sent
 * @param len: Length of the data
 * @param rnw: read or write value flag
 * @id: Command ID value
 *
 */
static void i3c_ccc_cmd_init(struct i3c_ccc_payload *cmd, u8 addr,
                 void *data, u16 len, u8 rnw, u8 id)
{
    struct i3c_ccc_target_payload target_payload;

    cmd->targets.payloads = &target_payload;
    cmd->targets.payloads->addr = addr;
    cmd->ccc.data = data;
    cmd->ccc.data_len = len;
    cmd->targets.payloads->rnw = (rnw ? 1 : 0);
    cmd->ccc.id = id;
}

/**
 * @brief Get the position of the address
 *
 * @param dev: Pointer to the device structure
 * @param addr: Address of the device
 *
 * @retval position of the device
 * @retval -EINVAL if not found
 */
static int get_addr_pos(const struct udevice *dev, u8 addr)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    int pos = 0;

    for (pos = 0; pos < priv->config.max_slv_devs; pos++) {
        if (addr == priv->data.base.addrs[pos]) {
            return pos;
        }
    }

    return -EINVAL;
}

/**
 * @brief Read data to RX buffer during I3C private transfer
 *
 * @param dev: Pointer to the device structure
 * @param buf: Pointer to buffer in which data to be read
 * @param nbytes: Number of bytes to be read
 */
static void read_rx_data_buffer(const struct udevice *dev,
                u8 *buf, int nbytes)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    int i, packet_size;
    u32 reg_val;

    if (!buf) {
        return;
    }

    packet_size = RX_TX_BUFFER_DATA_PACKET_SIZE;

    /* if bytes received are greater than/equal to data packet size */
    if (nbytes >= packet_size) {
        for (i = 0; i <= nbytes - packet_size; i += packet_size) {
            reg_val = readl(priv->regs + RX_TX_DATA_PORT);
            memcpy(buf + i, &reg_val, packet_size);
        }
    }

    /* if bytes less than data packet size or not multiple of packet size */
    packet_size = RX_TX_BUFFER_DATA_PACKET_SIZE - 1;

    if (nbytes & packet_size) {
        reg_val = readl(priv->regs + RX_TX_DATA_PORT);
        memcpy(buf + (nbytes & ~packet_size),
                  &reg_val, nbytes & packet_size);
    }
}

/**
 * @brief Write data from RX buffer during I3C private transfer
 *
 * @param dev: Pointer to the device structure
 * @param buf: Pointer to buffer in from which data to send
 * @param nbytes: Number bytes to be send
 */
static void write_tx_data_buffer(const struct udevice *dev,
                 const u8 *buf, int nbytes)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    int i, packet_size;
    u32 reg_val;

    if (!buf) {
        return;
    }

    packet_size = RX_TX_BUFFER_DATA_PACKET_SIZE;

    /* if bytes received are greater than/equal to data packet size */
    if (nbytes >= packet_size) {
        for (i = 0; i <= nbytes - packet_size; i += packet_size) {
            memcpy(&reg_val, buf + i, packet_size);
            writel(reg_val, priv->regs + RX_TX_DATA_PORT);
        }
    }

    /* if bytes less than data packet size or not multiple of packet size */
    packet_size = RX_TX_BUFFER_DATA_PACKET_SIZE - 1;

    if (nbytes & packet_size) {
        reg_val = 0;
        memcpy(&reg_val, buf + (nbytes & ~packet_size),
               nbytes & packet_size);
        writel(reg_val, priv->regs + RX_TX_DATA_PORT);
    }
}

/**
 * @brief Initiate the transfer procedure
 *
 * @param dev: Pointer to the device structure
 */
static void init_xfer(const struct udevice *dev, struct dw_i3c_xfer *xfer)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    struct dw_i3c_cmd *cmd;
    u32 thld_ctrl;
    int i;

    for (i = 0; i < xfer->ncmds; i++) {
        cmd = &xfer->cmds[i];
        /* Push data to TXFIFO */
        write_tx_data_buffer(dev, cmd->buf, cmd->tx_len);
    }

    /* Setting Response Buffer Threshold */
    thld_ctrl = readl(priv->regs + QUEUE_THLD_CTRL);
    thld_ctrl &= ~QUEUE_THLD_CTRL_RESP_BUF_MASK;
    thld_ctrl |= QUEUE_THLD_CTRL_RESP_BUF(xfer->ncmds);
    writel(thld_ctrl, priv->regs + QUEUE_THLD_CTRL);

    /* Enqueue CMD */
    for (i = 0; i < xfer->ncmds; i++) {
        cmd = &xfer->cmds[i];
        writel(cmd->cmd_hi, priv->regs + COMMAND_QUEUE_PORT);
        writel(cmd->cmd_lo, priv->regs + COMMAND_QUEUE_PORT);
    }
}

/**
 * @brief Send a CCC command
 *
 * @param dev: Pointer to the device structure
 * @param ccc: Pointer to command structure with command which needs to be send
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure is found NULL
 * @retval  Otherwise a value linked to transfer errors as per I3C specs
 */
static int dw_i3c_send_ccc_cmd(const struct udevice *dev,
                   struct i3c_ccc_payload *ccc)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    struct dw_i3c_xfer *xfer;
    struct dw_i3c_cmd *cmd;
    int ret, pos = 0;
    u8 i;

    if (!ccc) {
        return -EINVAL;
    }

    if (ccc->ccc.id & I3C_CCC_DIRECT) {
        pos = get_addr_pos(dev, ccc->targets.payloads->addr);
        if (pos < 0) {
            debug("%s Invalid Slave address\n", __func__);
            return pos;
        }
    }

    xfer = dw_i3c_master_alloc_xfer(priv, 1);
    if (!xfer){
        debug("Memory allocation for xfer failed\n");
        return -ENOMEM;
    }

    cmd = &xfer->cmds[0];
    cmd->buf = ccc->ccc.data;

    cmd->cmd_hi = COMMAND_PORT_ARG_DATA_LEN(ccc->ccc.data_len) |
        COMMAND_PORT_TRANSFER_ARG;
    cmd->cmd_lo = COMMAND_PORT_CP | COMMAND_PORT_TOC | COMMAND_PORT_ROC |
              COMMAND_PORT_DEV_INDEX(pos) |
              COMMAND_PORT_CMD(ccc->ccc.id);

    if ((ccc->targets.payloads) && (ccc->targets.payloads->rnw)) {
        cmd->cmd_lo |= COMMAND_PORT_READ_TRANSFER;
        cmd->rx_len = ccc->ccc.data_len;
    } else {
        cmd->tx_len = ccc->ccc.data_len;
    }

    init_xfer(dev,xfer);

    for (i = 0; i < xfer->ncmds; i++) {
        if (xfer->cmds[i].rx_len && !xfer->cmds[i].error) {
            read_rx_data_buffer(dev, xfer->cmds[i].buf,
                        xfer->cmds[i].rx_len);
        }
    }

    if (xfer->cmds[0].error == RESPONSE_ERROR_IBA_NACK) {
        cmd->error = I3C_ERROR_M2;
    }

    i3c_dw_irq(dev,xfer);
    ret = xfer->ret;
    free(xfer);

    return ret;
}

int i3c_ccc_do_setmrl(const struct i3c_device_desc *target,
              const struct i3c_ccc_mrl *mrl)
{
    struct i3c_ccc_payload ccc_payload;
    struct i3c_ccc_target_payload ccc_tgt_payload;
    u8 data[3];

    memset(&ccc_payload, 0, sizeof(ccc_payload));

    ccc_tgt_payload.addr = target->static_addr;
    ccc_tgt_payload.rnw = 0;
    ccc_tgt_payload.data = &data[0];

    ccc_payload.ccc.id = I3C_CCC_SETMRL(false);
    ccc_payload.targets.payloads = &ccc_tgt_payload;
    ccc_payload.targets.num_targets = 1;

    /* The actual length is MSB first. So order the data. */
    data[0] = (u8)((mrl->len & 0xFF00U) >> 8);
    data[1] = (u8)(mrl->len & 0xFFU);

    if ((target->bcr & I3C_BCR_IBI_PAYLOAD_HAS_DATA_BYTE) == I3C_BCR_IBI_PAYLOAD_HAS_DATA_BYTE) {
        ccc_tgt_payload.data_len = 3;
        data[2] = mrl->ibi_len;
    } else {
        ccc_tgt_payload.data_len = 2;
    }

    return dw_i3c_send_ccc_cmd(target->bus, &ccc_payload);
}

int i3c_ccc_do_getmwl(const struct i3c_device_desc *target,
              struct i3c_ccc_mwl *mwl)
{
    struct i3c_ccc_payload ccc_payload;
    struct i3c_ccc_target_payload ccc_tgt_payload;
    u8 data[2];
    int ret;

    ccc_tgt_payload.addr = target->dynamic_addr;
    ccc_tgt_payload.rnw = 1;
    ccc_tgt_payload.data = &data[0];
    ccc_tgt_payload.data_len = sizeof(data);

    memset(&ccc_payload, 0, sizeof(ccc_payload));
    ccc_payload.ccc.id = I3C_CCC_GETMWL;
    ccc_payload.targets.payloads = &ccc_tgt_payload;
    ccc_payload.targets.num_targets = 1;

    ret = dw_i3c_send_ccc_cmd(target->bus, &ccc_payload);

    if (ret == 0) {
        /* The actual length is MSB first. So order the data. */
        mwl->len = (data[0] << 8) | data[1];
    }

    return ret;
}

int i3c_ccc_do_getmrl(const struct i3c_device_desc *target,
              struct i3c_ccc_mrl *mrl)
{
    struct i3c_ccc_payload ccc_payload;
    struct i3c_ccc_target_payload ccc_tgt_payload;
    u8 data[3];
    bool has_ibi_sz;
    int ret;

    has_ibi_sz = (target->bcr & I3C_BCR_IBI_PAYLOAD_HAS_DATA_BYTE)
             == I3C_BCR_IBI_PAYLOAD_HAS_DATA_BYTE;

    ccc_tgt_payload.addr = target->dynamic_addr;
    ccc_tgt_payload.rnw = 1;
    ccc_tgt_payload.data = &data[0];
    ccc_tgt_payload.data_len = has_ibi_sz ? 3 : 2;

    memset(&ccc_payload, 0, sizeof(ccc_payload));
    ccc_payload.ccc.id = I3C_CCC_GETMRL;
    ccc_payload.targets.payloads = &ccc_tgt_payload;
    ccc_payload.targets.num_targets = 1;

    ret = dw_i3c_send_ccc_cmd(target->bus, &ccc_payload);

    if (ret == 0) {
        /* The actual length is MSB first. So order the data. */
        mrl->len = (data[0] << 8) | data[1];

        if (has_ibi_sz) {
            mrl->ibi_len = data[2];
        }
    }

    return ret;
}


int i3c_ccc_do_getbcr(struct i3c_device_desc *target,
              struct i3c_ccc_getbcr *bcr)
{
    struct i3c_ccc_payload ccc_payload;
    struct i3c_ccc_target_payload ccc_tgt_payload;

    ccc_tgt_payload.addr = target->dynamic_addr;
    ccc_tgt_payload.rnw = 1;
    ccc_tgt_payload.data = &bcr->bcr;
    ccc_tgt_payload.data_len = sizeof(bcr->bcr);

    memset(&ccc_payload, 0, sizeof(ccc_payload));
    ccc_payload.ccc.id = I3C_CCC_GETBCR;
    ccc_payload.targets.payloads = &ccc_tgt_payload;
    ccc_payload.targets.num_targets = 1;

    return dw_i3c_send_ccc_cmd(target->bus, &ccc_payload);
}

int i3c_ccc_do_getdcr(struct i3c_device_desc *target,
              struct i3c_ccc_getdcr *dcr)
{
    struct i3c_ccc_payload ccc_payload;
    struct i3c_ccc_target_payload ccc_tgt_payload;

    ccc_tgt_payload.addr = target->dynamic_addr;
    ccc_tgt_payload.rnw = 1;
    ccc_tgt_payload.data = &dcr->dcr;
    ccc_tgt_payload.data_len = sizeof(dcr->dcr);

    memset(&ccc_payload, 0, sizeof(ccc_payload));
    ccc_payload.ccc.id = I3C_CCC_GETDCR;
    ccc_payload.targets.payloads = &ccc_tgt_payload;
    ccc_payload.targets.num_targets = 1;

    return dw_i3c_send_ccc_cmd(target->bus, &ccc_payload);
}

int i3c_ccc_do_getpid(struct i3c_device_desc *target,
              struct i3c_ccc_getpid *pid)
{
    struct i3c_ccc_payload ccc_payload;
    struct i3c_ccc_target_payload ccc_tgt_payload;

    ccc_tgt_payload.addr = target->dynamic_addr;
    ccc_tgt_payload.rnw = 1;
    ccc_tgt_payload.data = &pid->pid[0];
    ccc_tgt_payload.data_len = sizeof(pid->pid);

    memset(&ccc_payload, 0, sizeof(ccc_payload));
    ccc_payload.ccc.id = I3C_CCC_GETPID;
    ccc_payload.targets.payloads = &ccc_tgt_payload;
    ccc_payload.targets.num_targets = 1;

    return dw_i3c_send_ccc_cmd(target->bus, &ccc_payload);
}

/**
 * @brief Reset the DAA
 *
 * @param controller: Pointer to the controller device
 * @param addr: Target broadcast address
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure/pointer is found NULL
 * @retval  Otherwise a value linked to cmd transfer errors as per I3C specs
 */
int i3c_controller_rstdaa(const struct udevice *dev, u8 addr)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    enum i3c_addr_slot_status addrstat;
    struct i3c_ccc_payload cmd;

    if (!dev) {
        return -EINVAL;
    }

    addrstat = i3c_addr_slots_status(&priv->data.addr_slots, addr);
    if (addr != I3C_BROADCAST_ADDR
        && addrstat != I3C_ADDR_SLOT_STATUS_I3C_DEV) {
        return -EINVAL;
    }

    i3c_ccc_cmd_init(&cmd, addr, NULL, 0, 0, I3C_CCC_RSTDAA);
    return dw_i3c_send_ccc_cmd(dev, &cmd);
}

/** 
 * @brief Send CCC command to enable/disable events
 *
 * @param controller: Pointer to the controller device
 * @param addr: Address of the target device
 * @param evts: Events
 * @param enable: Flag to enable/disable
 *
 * @retval 0 if successful or error code
 */
static int i3c_controller_en_dis_evt(const struct udevice *dev,
                     u8 addr, u8 evts, bool enable)
{
    struct i3c_ccc_payload cmd;
    struct i3c_ccc_events events;
    u8 id = (enable ? I3C_CCC_ENEC(addr == I3C_BROADCAST_ADDR) :
            I3C_CCC_DISEC(addr == I3C_BROADCAST_ADDR));

    events.events = evts;

    i3c_ccc_cmd_init(&cmd, addr, &events, sizeof(events), 0, id);
    return dw_i3c_send_ccc_cmd(dev, &cmd);
}

/**
 * @brief Detaches the target devices from device addr table
 *
 * @param dev: Pointer to the device structure
 * @param target_dev: Pointer to the target device structure
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure/pointer is found NULL
 *        or if address status is not as expected
 */
static int detach_i3c_target(const struct udevice *dev,
                struct dw_target_info *target_dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    u32 pos;
    u32 status;

    if (!dev || !target_dev) {
        return -EINVAL;
    }

    if (!target_dev->dev.static_addr && !target_dev->dev.dynamic_addr) {
        return 0;
    }

    if (target_dev->dev.static_addr) {
        status = i3c_addr_slots_status(&priv->data.addr_slots,
                target_dev->dev.static_addr);
        if (status != I3C_ADDR_SLOT_STATUS_I3C_DEV) {
            return -EINVAL;
        }
        i3c_addr_slots_set(&priv->data.addr_slots,
                target_dev->dev.static_addr,
                I3C_ADDR_SLOT_STATUS_FREE);
    }

    if (target_dev->dev.dynamic_addr) {
        status = i3c_addr_slots_status(&priv->data.addr_slots,
                target_dev->dev.dynamic_addr);
        if (status != I3C_ADDR_SLOT_STATUS_I3C_DEV) {
            return -EINVAL;
        }
        i3c_addr_slots_set(&priv->data.addr_slots,
                target_dev->dev.dynamic_addr,
                I3C_ADDR_SLOT_STATUS_FREE);
        target_dev->dev.dynamic_addr = 0;
    }

    pos = target_dev->index;
    priv->data.free_pos |= BIT(pos);
    writel(0, priv->regs + DEV_ADDR_TABLE_LOC(priv->data.datstartaddr, pos));

    return 0;
}

/**
 * @brief Attach the I3C target device to bus
 *
 * @param dev: Pointer to the device structure
 * @param target_dev: Pointer to the target device to be attached
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure is found NULL
 * @retval -EBUSY if no I3C address slot is free
 */
static int attach_i3c_target(const struct udevice *dev,
                struct dw_target_info *target_dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    int status, pos;

    if (!dev || !target_dev) {
        return -EINVAL;
    }

    if (!target_dev->dev.static_addr && !target_dev->dev.dynamic_addr) {
        return 0;
    }

    if (target_dev->dev.dynamic_addr) {
        status = i3c_addr_slots_status(&priv->data.addr_slots,
                              target_dev->dev.dynamic_addr);
        if (status != I3C_ADDR_SLOT_STATUS_I3C_DEV) {
            return -EBUSY;
        }
    }

    pos = get_free_pos(priv->data.free_pos);
    if (pos < 0) {
        return pos;
    }

    target_dev->index = pos;
    priv->data.free_pos &= ~BIT(pos);
    priv->data.base.addrs[pos] = target_dev->dev.dynamic_addr ?
                   : target_dev->dev.static_addr;
    writel(DEV_ADDR_TABLE_DYNAMIC_ADDR(priv->data.base.addrs[pos]),
            priv->regs + DEV_ADDR_TABLE_LOC(priv->data.datstartaddr, pos));

    return 0;
}

/**
 * @brief Send CCC command to get maximum data speed
 *
 * @param dev: Pointer to the device structure
 * @param info: Pointer to structure containing target device info
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure is found NULL
 * @retval  Otherwise a value linked to transfer errors as per I3C specs
 */
static int i3c_controller_getmxds(const struct udevice *dev,
                  struct i3c_device_desc *info)
{
    union i3c_ccc_getmxds *getmaxds;
    struct i3c_ccc_payload cmd;
    int ret;

    i3c_ccc_cmd_init(&cmd, info->dynamic_addr, &getmaxds,
             sizeof(getmaxds), true, I3C_CCC_GETMXDS);

    ret = dw_i3c_send_ccc_cmd(dev, &cmd);
    if (ret) {
        pr_err("failed to send CCC command: %d\n", ret);
        return ret;
    }

    if (cmd.ccc.data_len != I3C_GETMXDS_FORMAT_1_LEN
        && cmd.ccc.data_len != I3C_GETMXDS_FORMAT_2_LEN) {
        return -EINVAL;
    }

    info->data_speed.maxrd = getmaxds->fmt2.maxrd;
    info->data_speed.maxwr = getmaxds->fmt2.maxwr;
    if (cmd.ccc.data_len == I3C_GETMXDS_FORMAT_2_LEN) {
        info->data_speed.max_read_turnaround =
                (getmaxds->fmt2.maxrdturn[0] |
                ((int)getmaxds->fmt2.maxrdturn[1] << 8) |
                ((int)getmaxds->fmt2.maxrdturn[2] << 16));
    }

    return 0;
}

/**
 * @brief Retrieve the device information
 *
 * @param dev: Pointer to the device structure
 * @param info: Pointer to structure containing target device info
 * @param flag: Flag to check if BCR and DCR needs to be checked
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure/pointer is found NULL
 * @retval  Otherwise a value linked to cmd transfer errors as per I3C specs
 */
int i3c_controller_retrieve_dev_info(const struct udevice *dev,
                     struct i3c_device_desc *info,
                     u8 flag)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    u8 ret;
    enum i3c_addr_slot_status slot_status;
    struct i3c_ccc_getpid getpid;
    struct i3c_ccc_getbcr getbcr;
    struct i3c_ccc_getdcr getdcr;

    if (!info->dynamic_addr) {
        return -EINVAL;
    }

    slot_status = i3c_addr_slots_status(&priv->data.addr_slots,
            info->dynamic_addr);
    if (slot_status == I3C_ADDR_SLOT_STATUS_RSVD ||
            slot_status == I3C_ADDR_SLOT_STATUS_I2C_DEV) {
        return -EINVAL;
    }

    if (flag) {
        ret = i3c_ccc_do_getpid(info, &getpid);
        if (ret) {
            pr_err("failed to get pid from target device: %d\n", ret);
            return ret;
        }

        info->pid_h = (getpid.pid[5] << 8) | getpid.pid[4];
        info->pid_l = ((getpid.pid[3] << 24) | (getpid.pid[2] << 16) |
                (getpid.pid[1] << 8) | getpid.pid[0]);

        ret = i3c_ccc_do_getbcr(info, &getbcr);
        if (ret) {
            pr_err("failed to get bcr from target device: %d\n", ret);
            return ret;
        }

        ret = i3c_ccc_do_getdcr(info, &getdcr);
        if (ret) {
            pr_err("failed to get dcr from target device: %d\n", ret);
            return ret;
        }
    }

    if (info->bcr & I3C_BCR_MAX_DATA_SPEED_LIMIT) {
        ret = i3c_controller_getmxds(dev, info);
        if (ret) {
            pr_err("failed to set data rate for the target device: %d\n", ret);
            return ret;
        }
    }

    return 0;
}

/**
 * @brief Function to handle the I3C private transfers
 *
 * @param dev: Pointer to the device structure
 * @target: Pointer to structure containing info of target
 * @param msgs: Pointer to the I3C message lists
 * @param num_msgs: Number of messages to be send
 *
 * @retval  0 if successful
 * @retval -EINVAL if msgs structure is NULL
 * @retval -ENOMEM if no. of messages over runs fifo
 */
static int dw_master_i3c_xfers(struct udevice *dev,
                struct i3c_device_desc *target,
                struct i3c_msg *msgs, u8 num_msgs)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    struct dw_i3c_xfer *xfer;
    int ret, pos, nrxwords = 0, ntxwords = 0;
    u8 i;

    if (!msgs || !num_msgs) {
        return -EINVAL;
    }

    if (num_msgs > priv->config.cmdfifodepth) {
        return -ENOMEM;
    }

    pos = get_addr_pos(dev, target->dynamic_addr);
    if (pos < 0) {
        debug("%s Invalid Slave address\n", __func__);
        return pos;
    }

    for (i = 0; i < num_msgs; i++) {
        if (msgs[i].flags & I3C_MSG_READ) {
            nrxwords += DIV_ROUND_UP(msgs[i].len, 4);
        } else {
            ntxwords += DIV_ROUND_UP(msgs[i].len, 4);
        }
    }

    if (ntxwords > priv->config.datafifodepth ||
        nrxwords > priv->config.datafifodepth) {
        return -ENOMEM;
    }

    xfer = dw_i3c_master_alloc_xfer(priv, num_msgs);
    if (!xfer){
        debug("Memory allocation for xfer failed\n");
        return -ENOMEM;
    }

    for (i = 0; i < num_msgs; i++) {
        struct dw_i3c_cmd *cmd = &xfer->cmds[i];

        cmd->cmd_hi = COMMAND_PORT_ARG_DATA_LEN(msgs[i].len) |
                  COMMAND_PORT_TRANSFER_ARG;
        cmd->cmd_lo = COMMAND_PORT_TID(i) | COMMAND_PORT_ROC |
                  COMMAND_PORT_DEV_INDEX(pos);
        if (msgs[i].hdr_mode) {
            cmd->cmd_lo |= COMMAND_PORT_CP;
        }

        cmd->buf = msgs[i].buf;
        if (msgs[i].flags & I3C_MSG_READ) {
            u8 rd_speed = (msgs[i].hdr_mode) ? (I3C_DDR_SPEED)
                       : (priv->data.target[pos].dev.data_speed.maxrd);
            cmd->cmd_lo |= (COMMAND_PORT_READ_TRANSFER |
                    COMMAND_PORT_SPEED(rd_speed));
            cmd->rx_len = msgs[i].len;
        } else {
            u8 wr_speed = (msgs[i].hdr_mode) ? (I3C_DDR_SPEED)
                       : (priv->data.target[pos].dev.data_speed.maxwr);
            cmd->cmd_lo |= COMMAND_PORT_SPEED(wr_speed);
            cmd->tx_len = msgs[i].len;
        }

        if (i == (num_msgs - 1)) {
            cmd->cmd_lo |= COMMAND_PORT_TOC;
        }
    }

    init_xfer(dev,xfer);

    /* Reading back from RX FIFO to user buffer */
    for (i = 0; i < num_msgs; i++) {
        struct dw_i3c_cmd *cmd = &xfer->cmds[i];

        if ((cmd->rx_len > 0) && (cmd->error == 0)) {
            read_rx_data_buffer(dev, cmd->buf, cmd->rx_len);
        }
    }

    i3c_dw_irq(dev,xfer);
    ret = xfer->ret;
    free(xfer);

    return ret;
}

/**
 * @brief Write a set amount of data to an I3C target device.
 *
 * This routine writes a set amount of data synchronously.
 *
 * @param target I3C target device descriptor.
 * @param buf Memory pool from which the data is transferred.
 * @param num_bytes Number of bytes to write.
 *
 * @retval 0 If successful.
 * @retval -EBUSY Bus is busy.
 * @retval -EIO General input / output error.
 */
int i3c_write(struct udevice *dev,struct i3c_device_desc *target,
              const u8 *buf, int num_bytes)
{
    struct i3c_msg msg;

    msg.buf = (u8 *)buf;
    msg.len = num_bytes;
    msg.flags = I3C_MSG_WRITE | I3C_MSG_STOP;
    msg.hdr_mode = 0;

    return dw_master_i3c_xfers(dev, target, &msg, 1);
}

/**
 * @brief Read a set amount of data from an I3C target device.
 *
 * This routine reads a set amount of data synchronously.
 *
 * @param target I3C target device descriptor.
 * @param buf Memory pool that stores the retrieved data.
 * @param num_bytes Number of bytes to read.
 *
 * @retval 0 If successful.
 * @retval -EBUSY Bus is busy.
 * @retval -EIO General input / output error.
 */
int i3c_read(struct udevice *dev, struct i3c_device_desc *target,
             u8 *buf, int num_bytes)
{
    struct i3c_msg msg;

    msg.buf = buf;
    msg.len = num_bytes;
    msg.flags = I3C_MSG_READ | I3C_MSG_STOP;
    msg.hdr_mode = 0;

    return dw_master_i3c_xfers(dev, target, &msg, 1);
}

/**
 * @brief Add a target device to list of devices
 *
 * @param dev: Pointer to the device structure
 * @param pos: Position of the target to be added
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure/pointer is found NULL
 * @retval  Otherwise a value linked to cmd transfer errors as per I3C specs
 */
static int add_to_dev_tbl(const struct udevice *dev, int pos)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    struct dw_target_info *target_dev = NULL;
    int reg_val;
    u8 i, ret;

    for (i = 0; i < priv->config.max_slv_devs; i++) {
        if (priv->data.target[i].dev_type == 0) {
            priv->data.target[i].dev_type = I3C_SLAVE;
            target_dev = &priv->data.target[i];
            break;
        }
    }

    if (!target_dev) {
        debug("%s No targets\n", __func__);
        return -EINVAL;
    }

    target_dev->dev.dynamic_addr = priv->data.base.addrs[pos];

    ret = attach_i3c_target(dev, target_dev);
    if (ret) {
        pr_err("Attach I3C target failed :%d\n", ret);
        return -EINVAL;
    }

    reg_val = (u32)readl(priv->regs +
                 DEV_CHAR_TABLE_LOC1(priv->data.dctstartaddr, pos));
    target_dev->dev.pid_l = reg_val;

    reg_val = (u32)readl(priv->regs +
                 DEV_CHAR_TABLE_LOC2(priv->data.dctstartaddr, pos));
    target_dev->dev.pid_h = DEV_CHAR_TABLE_LSB_PID(reg_val);

    reg_val = (u32)readl(priv->regs +
                 DEV_CHAR_TABLE_LOC3(priv->data.dctstartaddr, pos));
    target_dev->dev.bcr = DEV_CHAR_TABLE_BCR(reg_val);
    target_dev->dev.dcr = DEV_CHAR_TABLE_DCR(reg_val);

    ret = i3c_controller_retrieve_dev_info(dev, &target_dev->dev, 0);
    if (ret) {
        pr_err("Get target info error :%d\n", ret);
        return ret;
    }

    target_dev->dev.bus = priv->config.device_list.i3c[0].bus;
    return 0;
}

static int write_to_target(struct udevice *dev,u8 dev_number,
                u8 *buf, int num_bytes)
{
    u8 ret = 0;
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    struct dw_target_info *target_dev = &priv->data.target[dev_number];

    i3c_write(dev, &target_dev->dev, buf, num_bytes);

    if (ret) {
        pr_err("write unsuccesfull :%d\n", ret);
        return ret;
    }

    return 0;
}

static int read_from_target(struct udevice *dev,u8 dev_number,
                u8 *buf, int num_bytes)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    struct dw_target_info *target_dev = &priv->data.target[dev_number];
    u8 ret = 0;

    ret = i3c_read(dev, &target_dev->dev, buf, num_bytes);
    if (ret) {
        pr_err("read unsuccesfull :%d\n", ret);
        return ret;
    }

    return ret;
}

/**
 * @brief Mark the address as I3C device in device list.
 *
 * @param addr_slots Pointer to the address slots struct.
 * @param addr Device address.
 */
static inline void i3c_addr_slots_mark_i3c(struct i3c_addr_slots *addr_slots,
                       u8 addr)
{
    i3c_addr_slots_set(addr_slots, addr,
               I3C_ADDR_SLOT_STATUS_I3C_DEV);
}

/**
 * @brief Do the DAA procedure
 *
 * @param dev: Pointer to the device structure
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure/pointer is found NULL
 * @retval -ENOMEM if free address is not found
 */
static int do_daa(const struct udevice *dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    struct dw_i3c_xfer *xfer;
    struct dw_i3c_cmd *cmd;
    int prev_devs, new_devs;
    u8 p, last_addr = 0;
    int pos, addr;

    prev_devs = ~(priv->data.free_pos);

    /* Prepare DAT before launching DAA */
    for (pos = 0; pos < priv->config.max_slv_devs; pos++) {
        if (prev_devs & BIT(pos)) {
            continue;
        }

        addr = i3c_addr_slots_next_free_find(&priv->data.addr_slots);
        if (addr < 0) {
            return addr;
        }

        priv->data.base.addrs[pos] = addr;
        i3c_addr_slots_mark_i3c(&priv->data.addr_slots, addr);
        p = odd_parity(addr);
        last_addr = addr;
        addr |= (p << 7);
        writel(DEV_ADDR_TABLE_DYNAMIC_ADDR(addr), priv->regs +
                DEV_ADDR_TABLE_LOC(priv->data.datstartaddr, pos));
    }

    pos = get_free_pos(priv->data.free_pos);

    xfer = dw_i3c_master_alloc_xfer(priv, 1);
    if (!xfer){
        debug("Memory allocation for xfer failed\n");
        return -ENOMEM;
    }

    xfer->ncmds = 1;
    xfer->ret = -1;

    cmd = &xfer->cmds[0];
    cmd->cmd_hi = COMMAND_PORT_TRANSFER_ARG;
    cmd->cmd_lo = COMMAND_PORT_TOC | COMMAND_PORT_ROC |
                COMMAND_PORT_DEV_COUNT(priv->config.max_slv_devs - pos) |
                COMMAND_PORT_DEV_INDEX(pos) |
                COMMAND_PORT_CMD(I3C_CCC_ENTDAA) |
                COMMAND_PORT_ADDR_ASSGN_CMD;

    init_xfer(dev,xfer);

    if (priv->config.max_slv_devs == cmd->rx_len) {
        new_devs = 0;
    } else {
        new_devs = GENMASK(priv->config.max_slv_devs - cmd->rx_len - 1, 0);
    }
    new_devs &= ~prev_devs;

    for (pos = 0; pos < priv->config.max_slv_devs; pos++) {
        if (new_devs & BIT(pos)) {
            add_to_dev_tbl(dev, pos);
        }
    }

    /* send Disable event CCC : disable all target event */
    u8 events = I3C_CCC_EVENT_SIR | I3C_CCC_EVENT_MR | I3C_CCC_EVENT_HJ;

    i3c_controller_en_dis_evt(dev, I3C_BROADCAST_ADDR, events, false);

    return 0;
}

/**
 * @brief Does Dynamic addressing of targets
 *
 * @param dev: Pointer to the device structure
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure/pointer is found NULL
 *        or if address status is not as expected
 * @retval -ENOMEM if free address is not found
 * @retval  Otherwise semaphore/mutex acquiring failed
 */
static int dw_i3c_master_daa(struct udevice *dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    int ret = -1;

    /* send RSTDAA : reset all dynamic address */
    i3c_controller_rstdaa(dev, I3C_BROADCAST_ADDR);

    /* send Disbale Event Cmd : disable all target event before starting DAA */
    i3c_controller_en_dis_evt(dev, I3C_BROADCAST_ADDR, I3C_CCC_EVENT_SIR |
            I3C_CCC_EVENT_MR | I3C_CCC_EVENT_HJ, false);

    /* Detach if any, before re-addressing */
    for (u8 i = 0; i < priv->config.max_slv_devs; i++) {
        ret = detach_i3c_target(dev, &priv->data.target[i]);
        if (ret) {
            pr_err("detach i3c target failed :%d\n", ret);
            return ret;
        }
        priv->data.target[i].dev_type = 0;
    }

    /* start DAA */
    ret = do_daa(dev);
    if (ret) {
        pr_err("dynamic addressing failed :%d\n", ret);
        return ret;
    }

    return 0;
}

void i3c_addr_slots_set(struct i3c_addr_slots *slots,
            u8 dev_addr,
            enum i3c_addr_slot_status status)
{
    u32 bitpos;
    u32 idx;

    if (dev_addr > I3C_MAX_ADDR) {
        /* Invalid address. Do nothing. */
        return;
    }

    bitpos = dev_addr * 2;
    idx = bitpos / BITS_PER_LONG;

    slots->slots[idx] &= ~((u64)I3C_ADDR_SLOT_STATUS_MASK <<
                    (bitpos % BITS_PER_LONG));
    slots->slots[idx] |= (u64)status << (bitpos % BITS_PER_LONG);
}

enum i3c_addr_slot_status
i3c_addr_slots_status(struct i3c_addr_slots *slots,
              u8 dev_addr)
{
    unsigned long status;
    int bitpos;
    int idx;

    if (dev_addr > I3C_MAX_ADDR) {
        /* Invalid address.
         * Simply says it's reserved so it will not be
         * used for anything.
         */
        return I3C_ADDR_SLOT_STATUS_RSVD;
    }

    bitpos = dev_addr * 2;
    idx = bitpos / BITS_PER_LONG;

    status = slots->slots[idx] >> (bitpos % BITS_PER_LONG);
    status &= I3C_ADDR_SLOT_STATUS_MASK;

    return status;
}

bool i3c_addr_slots_is_free(struct i3c_addr_slots *slots,
                u8 dev_addr)
{
    enum i3c_addr_slot_status status;
    status = i3c_addr_slots_status(slots, dev_addr);

    return (status == I3C_ADDR_SLOT_STATUS_FREE);
}

u8 i3c_addr_slots_next_free_find(struct i3c_addr_slots *slots)
{
    u8 addr;
    enum i3c_addr_slot_status status;

    /* Addresses 0 to 7 are reserved. So start at 8. */
    for (addr = 8; addr < I3C_MAX_ADDR; addr++) {
        status = i3c_addr_slots_status(slots, addr);
        if (status == I3C_ADDR_SLOT_STATUS_FREE) {
            return addr;
        }
    }

    return 0;
}

/**
 * @brief Mark the address as I2C device in device list.
 *
 * @param addr_slots Pointer to the address slots struct.
 * @param addr Device address.
 */
static inline void i3c_addr_slots_mark_i2c(struct i3c_addr_slots *addr_slots,
                       u8 addr)
{
    i3c_addr_slots_set(addr_slots, addr,
               I3C_ADDR_SLOT_STATUS_I2C_DEV);
}

int i3c_addr_slots_init(struct i3c_addr_slots *slots,
            const struct i3c_dev_list *dev_list)
{
    int i, ret = 0;
    struct i3c_device_desc *i3c_dev;
    struct i3c_i2c_device_desc *i2c_dev;

    memset(slots, 0, sizeof(*slots));

    for (i = 0; i <= 7; i++) {
        /* Addresses 0 to 7 are reserved */
        i3c_addr_slots_set(slots, i, I3C_ADDR_SLOT_STATUS_RSVD);

        /*
         * Addresses within a single bit error of broadcast address
         * are also reserved.
         */
        i3c_addr_slots_set(slots, I3C_BROADCAST_ADDR ^ BIT(i),
                   I3C_ADDR_SLOT_STATUS_RSVD);
    }

    /* The broadcast address is reserved */
    i3c_addr_slots_set(slots, I3C_BROADCAST_ADDR,
               I3C_ADDR_SLOT_STATUS_RSVD);

    /*
     * If there is a static address for the I3C devices, check
     * if this address is free, and there is no other devices of
     * the same (pre-assigned) address on the bus.
     */
    for (i = 0; i < dev_list->num_i3c; i++) {
        i3c_dev = &dev_list->i3c[i];

        if (i3c_dev->static_addr != 0U) {
            if (i3c_addr_slots_is_free(slots, i3c_dev->static_addr)) {
                /*
                 * Mark address slot as I3C device for now to
                 * detect address collisons. This marking may be
                 * released during address assignment.
                 */
                i3c_addr_slots_mark_i3c(slots, i3c_dev->static_addr);
            } else {
                /* Address slot is not free */
                ret = -EINVAL;
                goto out;
            }
        }
    }

    /*
     * Mark all I2C addresses.
     */
    for (i = 0; i < dev_list->num_i2c; i++) {
        i2c_dev = &dev_list->i2c[i];
        if (i3c_addr_slots_is_free(slots, i2c_dev->addr)) {
            i3c_addr_slots_mark_i2c(slots, i2c_dev->addr);
        } else {
            /* Address slot is not free */
            ret = -EINVAL;
            goto out;
        }
    }

out:
    return ret;
}

/**
 * @brief Define the mode of I3C bus
 *
 * @param dev: Pointer to the device structure
 *
 * @retval  0 if successful
 * @retval -EINVAL if there exist LVR related error
 */
static int define_i3c_bus_mode(const struct udevice *dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    u8 i;

    priv->data.base.mode = I3C_BUS_MODE_PURE;

    for (i = 0; i < priv->config.max_slv_devs; i++) {
        if (priv->data.target[i].dev_type != I2C_SLAVE) {
            continue;
        }

        switch (priv->data.target[i].lvr & I3C_LVR_I2C_INDEX_MASK) {
        case I3C_LVR_I2C_INDEX(0):
            if (priv->data.base.mode < I3C_BUS_MODE_MIXED_FAST) {
                priv->data.base.mode = I3C_BUS_MODE_MIXED_FAST;
            }
            break;
        case I3C_LVR_I2C_INDEX(1):
            if (priv->data.base.mode < I3C_BUS_MODE_MIXED_LIMITED) {
                priv->data.base.mode = I3C_BUS_MODE_MIXED_LIMITED;
            }
            break;
        case I3C_LVR_I2C_INDEX(2):
            if (priv->data.base.mode < I3C_BUS_MODE_MIXED_SLOW) {
                priv->data.base.mode = I3C_BUS_MODE_MIXED_SLOW;
            }
            break;
        default:
            debug("%s I2C Slave device(%d) LVR error\n",__func__, i);
            return -EINVAL;
        }
    }

    return 0;
}

/**
 * @brief Initialize the SCL timing for I3C
 *
 * @param dev: Pointer to the device structure
 */
static int dw_i3c_init_scl_timing(const struct udevice *dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    u32 scl_timing, core_rate, core_period;
    u8 hcnt, lcnt;

    core_rate = priv->config.core_clk;
    if (!core_rate)
        return -EINVAL;

    core_period = DIV_ROUND_UP(I3C_PERIOD_NS, core_rate);

    /* I3C_PP */
    hcnt = DIV_ROUND_UP(I3C_BUS_THIGH_MAX_NS, core_period) - 1;
    if (hcnt < SCL_I3C_TIMING_CNT_MIN) {
        hcnt = SCL_I3C_TIMING_CNT_MIN;
    }

    lcnt = DIV_ROUND_UP(core_rate, I3C_BUS_TYP_I3C_SCL_RATE) - hcnt;
    if (lcnt < SCL_I3C_TIMING_CNT_MIN) {
        lcnt = SCL_I3C_TIMING_CNT_MIN;
    }

    scl_timing = SCL_I3C_TIMING_HCNT(hcnt) | SCL_I3C_TIMING_LCNT(lcnt);
    writel(scl_timing, priv->regs + SCL_I3C_PP_TIMING);

    writel(BUS_I3C_MST_FREE(lcnt), priv->regs + BUS_FREE_TIMING);

    /* I3C_OD */
    lcnt = DIV_ROUND_UP(I3C_BUS_TLOW_OD_MIN_NS, core_period);
    scl_timing = SCL_I3C_TIMING_HCNT(hcnt) | SCL_I3C_TIMING_LCNT(lcnt);
    writel(scl_timing, priv->regs + SCL_I3C_OD_TIMING);

    /* I3C */
    lcnt = DIV_ROUND_UP(core_rate, I3C_BUS_SDR1_SCL_RATE) - hcnt;
    scl_timing = SCL_EXT_LCNT_1(lcnt);
    lcnt = DIV_ROUND_UP(core_rate, I3C_BUS_SDR2_SCL_RATE) - hcnt;
    scl_timing |= SCL_EXT_LCNT_2(lcnt);
    lcnt = DIV_ROUND_UP(core_rate, I3C_BUS_SDR3_SCL_RATE) - hcnt;
    scl_timing |= SCL_EXT_LCNT_3(lcnt);
    lcnt = DIV_ROUND_UP(core_rate, I3C_BUS_SDR4_SCL_RATE) - hcnt;
    scl_timing |= SCL_EXT_LCNT_4(lcnt);
    writel(scl_timing, priv->regs + SCL_EXT_LCNT_TIMING);

    return 0;
}

static int dw_i2c_init_scl_timing(const struct udevice *dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    u32 scl_timing, core_rate, core_period;
    u16 hcnt, lcnt;

    core_rate = priv->config.core_clk;
    if (!core_rate)
        return -EINVAL;

    core_period = DIV_ROUND_UP(I3C_PERIOD_NS, core_rate);

    /* I2C FM+ */
    lcnt = DIV_ROUND_UP(I3C_BUS_I2C_FMP_TLOW_MIN_NS, core_period);
    hcnt = DIV_ROUND_UP(core_rate, I3C_BUS_I2C_FM_PLUS_SCL_RATE) - lcnt;
    scl_timing = SCL_I2C_FMP_TIMING_HCNT(hcnt) | SCL_I2C_FMP_TIMING_LCNT(lcnt);
    writel(scl_timing, priv->regs + SCL_I2C_FMP_TIMING);

    /* I2C FM */
    lcnt = DIV_ROUND_UP(I3C_BUS_I2C_FM_TLOW_MIN_NS, core_period);
    hcnt = DIV_ROUND_UP(core_rate, I3C_BUS_I2C_FM_SCL_RATE) - lcnt;
    scl_timing = (u32)(SCL_I2C_FM_TIMING_HCNT(hcnt) | SCL_I2C_FM_TIMING_LCNT(lcnt));
    writel(scl_timing, priv->regs + SCL_I2C_FM_TIMING);

    writel(BUS_I3C_MST_FREE(lcnt), priv->regs + BUS_FREE_TIMING);
    writel(readl(priv->regs + DEVICE_CTRL) |
                DEV_CTRL_I2C_SLAVE_PRESENT, (priv->regs + DEVICE_CTRL));

    return 0;
}

/**
 * @brief Configure the controller address
 *
 * @param dev: Pointer to the device structure
 *
 * @retval  0 if successful
 * @retval -EINVAL if a structure/pointer is found NULL
 * @retval -ENOMEM if no such free address is found
 * @retval -EBUSY if no I3C address slot is free
 */
static int controller_addr_config(const struct udevice *dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    int addr;

    addr = i3c_addr_slots_next_free_find(&priv->data.addr_slots);
    if (addr < 0) {
        return addr;
    }

    if (i3c_addr_slots_status(&priv->data.addr_slots, addr)
        != I3C_ADDR_SLOT_STATUS_FREE) {
        return -EBUSY;
    }
    i3c_addr_slots_set(&priv->data.addr_slots, addr,
                     I3C_ADDR_SLOT_STATUS_I3C_DEV);

    writel(DEV_ADDR_DYNAMIC_ADDR_VALID | DEV_ADDR_DYNAMIC(addr),
            (priv->regs + DEVICE_ADDR));

    return 0;
}

/**-
 * @brief Enable interrupt signal
 *
 * @param dev: Pointer to the device structure
 */
static void en_inter_sig_stat(const struct udevice *dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    int thld_ctrl;

    thld_ctrl = readl(priv->regs + QUEUE_THLD_CTRL);
    thld_ctrl &= (~QUEUE_THLD_CTRL_RESP_BUF_MASK
              & ~QUEUE_THLD_CTRL_IBI_STS_MASK);
    writel(thld_ctrl, priv->regs + QUEUE_THLD_CTRL);

    /* Set Data Threshold level */
    thld_ctrl = readl(priv->regs + DATA_BUFFER_THLD_CTRL);
    thld_ctrl &= ~DATA_BUFFER_THLD_CTRL_RX_BUF;
    writel(thld_ctrl, priv->regs + DATA_BUFFER_THLD_CTRL);

    /* Clearing any previous interrupts, if any */
    writel(INTR_ALL, priv->regs + INTR_STATUS);

    writel(INTR_CONTROLLER_MASK, priv->regs + INTR_STATUS_EN);
    writel(INTR_CONTROLLER_MASK, priv->regs + INTR_SIGNAL_EN);
}

int i3c_ccc_do_rstact_all(const struct udevice *controller,
              enum i3c_ccc_rstact_defining_byte action)
{
    struct i3c_ccc_payload ccc_payload;
    u8 def_byte;

    memset(&ccc_payload, 0, sizeof(ccc_payload));
    ccc_payload.ccc.id = I3C_CCC_RSTACT(true);

    def_byte = (u8)action;
    ccc_payload.ccc.data = &def_byte;
    ccc_payload.ccc.data_len = 1U;

    return dw_i3c_send_ccc_cmd(controller, &ccc_payload);
}

int i3c_ccc_do_rstdaa_all(const struct udevice *controller)
{
    struct i3c_ccc_payload ccc_payload;

    memset(&ccc_payload, 0, sizeof(ccc_payload));
    ccc_payload.ccc.id = I3C_CCC_RSTDAA;

    return dw_i3c_send_ccc_cmd(controller, &ccc_payload);
}

int i3c_ccc_do_setdasa(const struct i3c_device_desc *target)
{
    struct i3c_ccc_payload ccc_payload;
    struct i3c_ccc_target_payload ccc_tgt_payload;
    u8 dyn_addr;

    if ((target->static_addr == 0U) || (target->dynamic_addr != 0U)) {
        return -EINVAL;
    }

    /*
     * Note that the 7-bit address needs to start at bit 1
     * (aka left-justified). So shift left by 1;
     */
    dyn_addr = target->static_addr << 1;

    ccc_tgt_payload.addr = target->static_addr;
    ccc_tgt_payload.rnw = 0;
    ccc_tgt_payload.data = &dyn_addr;
    ccc_tgt_payload.data_len = 1;

    memset(&ccc_payload, 0, sizeof(ccc_payload));
    ccc_payload.ccc.id = I3C_CCC_SETDASA;
    ccc_payload.targets.payloads = &ccc_tgt_payload;
    ccc_payload.targets.num_targets = 1;

    return dw_i3c_send_ccc_cmd(target->bus, &ccc_payload);
}

int i3c_ccc_do_events_all_set(const struct udevice *controller,
                  bool enable, struct i3c_ccc_events *events)
{
    struct i3c_ccc_payload ccc_payload;
    memset(&ccc_payload, 0, sizeof(ccc_payload));
    ccc_payload.ccc.id = enable ? I3C_CCC_ENEC(true) : I3C_CCC_DISEC(true);
    ccc_payload.ccc.data = &events->events;
    ccc_payload.ccc.data_len = sizeof(events->events);

    return dw_i3c_send_ccc_cmd(controller, &ccc_payload);
}

int i3c_device_basic_info_get(struct i3c_device_desc *target)
{
    int ret;
    u8 tmp_bcr;

    struct i3c_ccc_getbcr bcr = {0};
    struct i3c_ccc_getdcr dcr = {0};
    struct i3c_ccc_mrl mrl = {0};
    struct i3c_ccc_mwl mwl = {0};

    /*
     * Since some CCC functions requires BCR to function
     * correctly, we save the BCR here and update the BCR
     * in the descriptor. If any following operations fails,
     * we can restore the BCR.
     */
    tmp_bcr = target->bcr;

    /* GETBCR */
    ret = i3c_ccc_do_getbcr(target, &bcr);
    if (ret != 0) {
        goto out;
    }

    target->bcr = bcr.bcr;

    /* GETDCR */
    ret = i3c_ccc_do_getdcr(target, &dcr);
    if (ret != 0) {
        goto out;
    }

    /* GETMRL */
    ret = i3c_ccc_do_getmrl(target, &mrl);
    if (ret != 0) {
        goto out;
    }

    /* GETMWL */
    ret = i3c_ccc_do_getmwl(target, &mwl);
    if (ret != 0) {
        goto out;
    }

    target->dcr = dcr.dcr;
    target->data_length.mrl = mrl.len;
    target->data_length.mwl = mwl.len;
    target->data_length.max_ibi = mrl.ibi_len;

out:
    if (ret != 0) {
        /* Restore BCR is any CCC fails. */
        target->bcr = tmp_bcr;
    }
    return ret;
}

/**
 * @brief Do SETDASA to set static address as dynamic address.
 *
 * @param dev Pointer to the device driver instance.
 * @param[out] True if DAA is still needed. False if all registered
 *             devices have static addresses.
 *
 * @retval 0 if successful.
 */
static int i3c_bus_setdasa(const struct udevice *dev,
               const struct i3c_dev_list *dev_list,
               bool *need_daa)
{
    int i, ret;

    if(dev_list->num_i3c > 0)
        *need_daa = false;

    /* Loop through the registered I3C devices */
    for (i = 0; i < dev_list->num_i3c; i++) {
        struct i3c_device_desc *desc = &dev_list->i3c[i];

        /*
         * A device without static address => need to do
         * dynamic address assignment.
         */
        if (desc->static_addr == 0U) {
            *need_daa = true;
            continue;
        }

        /*
         * If there is a desired dynamic address and it is
         * not the same as the static address, wait till
         * ENTDAA to do address assignment as this is
         * no longer SETDASA.
         */
        if ((desc->init_dynamic_addr != 0U) &&
            (desc->init_dynamic_addr != desc->static_addr)) {
            *need_daa = true;
            continue;
        }

        ret = i3c_ccc_do_setdasa(desc);
        if (ret == 0) {
            desc->dynamic_addr = desc->static_addr;
        } else {
            debug("SETDASA error on address 0x%x (%d)",
                desc->static_addr, ret);
            continue;
        }
    }

    return 0;
}

int i3c_bus_init(struct udevice *dev, const struct i3c_dev_list *dev_list)
{
    int i, ret = 0;
    bool need_daa = true;
    struct i3c_ccc_events i3c_events;

    /*
     * Reset all connected targets. Also reset dynamic
     * addresses for all devices as we have no idea what
     * dynamic addresses the connected devices have
     * (e.g. assigned during previous power cycle).
     *
     * Note that we ignore error for both RSTACT and RSTDAA
     * as there may not be any connected devices responding
     * to these CCCs.
     */
    if (i3c_ccc_do_rstact_all(dev, I3C_CCC_RSTACT_RESET_WHOLE_TARGET) != 0) {
        /*
         * Reset Whole Target support is not required so
         * if there is any NACK, we want to at least reset
         * the I3C peripheral of targets.
         */
        debug("Broadcast RSTACT (whole target) was NACK.\n");

        if (i3c_ccc_do_rstact_all(dev, I3C_CCC_RSTACT_PERIPHERAL_ONLY) != 0) {
            debug("Broadcast RSTACT (peripehral) was NACK.\n");
        }
    }

    if (i3c_ccc_do_rstdaa_all(dev) != 0) {
        debug("Broadcast RSTDAA was NACK.\n");
    }

    /*
     * Disable all events from targets to avoid them
     * interfering with bus initialization,
     * especially during DAA.
     */
    i3c_events.events = I3C_CCC_EVT_ALL;
    ret = i3c_ccc_do_events_all_set(dev, false, &i3c_events);
    if (ret != 0) {
        debug("Broadcast DISEC was NACK.\n");
    }

    /*
     * Set static addresses as dynamic addresses.
     */
    ret = i3c_bus_setdasa(dev, dev_list, &need_daa);
    if (ret != 0) {
        dev_err(dev, "Setdasa failed.\n");
        goto err_out;
    }

    /*
     * Perform Dynamic Address Assignment if needed.
     */
    if (need_daa) {
        ret = dw_i3c_master_daa(dev);
        if (ret != 0) {
            /*
             * Spec says to try once more
             * if DAA fails the first time.
             */
            ret = dw_i3c_master_daa(dev);
            if (ret != 0) {
                /*
                 * Failure to finish dynamic address assignment
                 * is not the end of world... hopefully.
                 * Continue on so the devices already have
                 * addresses can still function.
                 */
                debug("DAA was not successful.\n");
            }
        }
    }

    /*
     * Loop through the registered I3C devices to retrieve
     * basic target information.
     */
    for (i = 0; i < dev_list->num_i3c; i++) {
        struct i3c_device_desc *desc = &dev_list->i3c[i];

        if (desc->dynamic_addr == 0U) {
            continue;
        }

        ret = i3c_device_basic_info_get(desc);
        if (ret != 0) {
            debug("Error getting basic device info for 0x%02x",
                desc->static_addr);
        } else {
            debug("Target 0x%02x, BCR 0x%02x, DCR 0x%02x, MRL %d, MWL %d, IBI %d",
                desc->dynamic_addr, desc->bcr, desc->dcr,
                desc->data_length.mrl, desc->data_length.mwl,
                desc->data_length.max_ibi);
        }
    }

    /*
     * Only re-enable Hot-Join from targets.
     * Target interrupts will be enabled when IBI is enabled.
     * And transferring controller role is not supported so not need to
     * enable the event.
     */
    i3c_events.events = I3C_CCC_EVT_HJ;
    ret = i3c_ccc_do_events_all_set(dev, true, &i3c_events);
    if (ret != 0) {
        debug("Broadcast ENEC was NACK.\n");
    }

err_out:
    return ret;
}

int dw_i3c_of_to_plat(struct udevice *dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);

    if (!priv->regs)
        priv->regs = dev_read_addr_ptr(dev);

    dev_read_u32(dev, "max_devices", &priv->config.max_slv_devs);

    /*No registered devices. all through daa*/
    priv->config.device_list.num_i3c = 0;

    return 0;
}

int dw_i3c_probe(struct udevice *dev)
{
    struct dw_i3c_priv *priv = dev_get_priv(dev);
    int ret;

    if (dev == NULL) {
        dev_err(dev, "%s Device ptr null !!\n", __func__);
        return -EINVAL;
    }

    if (priv->config.max_slv_devs > 11) {
        dev_err(dev, "%s Max target dev count exceeded !\n", __func__);
        return -ENOMEM;
    }

    /* Allocate Memory */
    priv->data.base.addrs = (u8 *)calloc(1,priv->config.max_slv_devs *
               sizeof(u8));
    if(!priv->data.base.addrs){
        debug("Memory allocation for priv->data.base.addrs failed\n");
        ret = -ENOMEM;
        goto out;
    }

    priv->data.target = (struct dw_target_info *)calloc(1,priv->config.max_slv_devs
              * sizeof(struct dw_target_info));
    if(!priv->data.target){
        debug("Memory allocation for priv->data.target failed\n");
        ret = -ENOMEM;
        goto out;
    }
    memset(priv->data.target, 0,(priv->config.max_slv_devs * sizeof(struct dw_target_info)));

    priv->data.base.dev = dev;

    /* Initializing feasible address slots for DAA */
    ret = i3c_addr_slots_init(&priv->data.addr_slots, &priv->config.device_list);
    if (ret != 0) {
        goto out;
    }

    /* I3C pure, I2C pure or Mix */
    ret = define_i3c_bus_mode(dev);
    if (ret) {
        pr_err("Can't set i3c bus mode :%d\n", ret);
        goto out;
    }

    ret = clk_get_by_index(dev, 0, &priv->clk);
    if (ret){
        dev_err(dev, "Can't get clock: %d\n", ret);
        goto out;
    }
    priv->config.core_clk = clk_get_rate(&priv->clk);
    clk_free(&priv->clk);

    /* Reset the Designware I3C hardware */
    ret = reset_get_bulk(dev, &priv->resets);
    if (ret) {
        if (ret != -ENOMEM){
            dev_err(dev, "Can't get reset: %d\n", ret);
            goto out;
        }
    } else {
        reset_deassert_bulk(&priv->resets);
    }

    /* get DAT, DCT pointer */
    priv->data.datstartaddr = DEVICE_ADDR_TABLE_ADDR(readl(priv->regs +
                DEVICE_ADDR_TABLE_POINTER));
    priv->data.dctstartaddr = DEVICE_CHAR_TABLE_ADDR(readl(priv->regs +
                DEV_CHAR_TABLE_POINTER));

    /* Config timing registers */
    switch (priv->data.base.mode) {
    case I3C_BUS_MODE_MIXED_FAST:
    case I3C_BUS_MODE_MIXED_LIMITED:
        ret = dw_i2c_init_scl_timing(dev);
        if (ret)
            goto out;
        fallthrough;
    case I3C_BUS_MODE_PURE:
        ret = dw_i3c_init_scl_timing(dev);
        if (ret)
            goto out;
        break;
    default:
        ret = -EINVAL;
        debug("Bus mode is undefined, Serial clock timing failed\n");
        goto out;
    }

    /* Self-assigning a dynamic address */
    ret = controller_addr_config(dev);
    if (ret) {
        pr_err("DAA address configuration failed :%d\n", ret);
        goto out;
    }

    /* Enable Interrupt signals */
    en_inter_sig_stat(dev);

    /* disable IBI */
    writel(IBI_REQ_REJECT_ALL, priv->regs + IBI_SIR_REQ_REJECT);
    writel(IBI_REQ_REJECT_ALL, priv->regs + IBI_MR_REQ_REJECT);

    /* disable hot-join */
    writel(readl(priv->regs + DEVICE_CTRL) | DEV_CTRL_HOT_JOIN_NACK,
            (priv->regs + DEVICE_CTRL));

    /* enable I3C controller */
    writel(readl(priv->regs + DEVICE_CTRL) | DEV_CTRL_ENABLE,
            (priv->regs + DEVICE_CTRL));

    /* Information regarding the FIFOs/QUEUEs depth */
    ret = readl(priv->regs + QUEUE_STATUS_LEVEL);
    priv->config.cmdfifodepth = QUEUE_STATUS_LEVEL_CMD(ret);

    ret = readl(priv->regs + DATA_BUFFER_STATUS_LEVEL);
    priv->config.datafifodepth = DATA_BUFFER_STATUS_LEVEL_TX(ret);

    /* free positions available among available slots */
    priv->data.free_pos = GENMASK(priv->config.max_slv_devs - 1, 0);

    ret = i3c_bus_init(dev, &priv->config.device_list);
    if (ret) {
        pr_err("Bus Init was unsuccessful :%d\n", ret);
        goto out;
    }
    return 0;

out:
    free(priv->data.base.addrs);
    free(priv->data.target);
    return ret;
}

static const struct dm_i3c_ops dw_i3c_ops = {
    .i3c_xfers = dw_master_i3c_xfers,
    .read = read_from_target,
    .write = write_to_target,
};

static const struct udevice_id dw_i3c_ids[] = {
    { .compatible = "snps,dw-i3c-master-1.00a" },
    { }
};

U_BOOT_DRIVER(dw_i3c_driver) = {
    .name = "dw-i3c-master",
    .id = UCLASS_I3C,
    .of_match = dw_i3c_ids,
    .probe = dw_i3c_probe,
    .of_to_plat = dw_i3c_of_to_plat,
    .priv_auto = sizeof(struct dw_i3c_priv),
    .ops    = &dw_i3c_ops,
};
