/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Intel Coporation.
 */

#ifndef _DW_I3C_H_
#define _DW_I3C_H_
#include <clk.h>
#include <reset.h>
#include <dm/device.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linker_lists.h>

#define DEVICE_CTRL                     (0x0)
#define DEV_CTRL_ENABLE                 BIT(31)
#define DEV_CTRL_RESUME                 BIT(30)
#define DEV_CTRL_HOT_JOIN_NACK          BIT(8)
#define DEV_CTRL_I2C_SLAVE_PRESENT      BIT(7)

#define DEVICE_ADDR                     (0x4)
#define DEV_ADDR_DYNAMIC_ADDR_VALID     BIT(31)
#define DEV_ADDR_DYNAMIC(x)             (((x) << 16) & GENMASK(22, 16))

#define I3C_DDR_SPEED                   (0x6)
#define COMMAND_QUEUE_PORT              (0xc)
#define COMMAND_PORT_TOC                BIT(30)
#define COMMAND_PORT_READ_TRANSFER      BIT(28)
#define COMMAND_PORT_ROC                BIT(26)
#define COMMAND_PORT_SPEED(x)           (((x) << 21) & GENMASK(23, 21))
#define COMMAND_PORT_DEV_INDEX(x)       (((x) << 16) & GENMASK(20, 16))
#define COMMAND_PORT_CP                 BIT(15)
#define COMMAND_PORT_CMD(x)             (((x) << 7) & GENMASK(14, 7))
#define COMMAND_PORT_TID(x)             (((x) << 3) & GENMASK(6, 3))

#define COMMAND_PORT_ARG_DATA_LEN(x)    (((x) << 16) & GENMASK(31, 16))
#define COMMAND_PORT_TRANSFER_ARG       (0x01)

#define COMMAND_PORT_DEV_COUNT(x)       (((x) << 21) & GENMASK(25, 21))
#define COMMAND_PORT_ADDR_ASSGN_CMD     (0x03)

#define RESPONSE_QUEUE_PORT             (0x10)
#define RESPONSE_PORT_ERR_STATUS(x)     (((x) & GENMASK(31, 28)) >> 28)
#define RESPONSE_NO_ERROR               (0)
#define RESPONSE_ERROR_CRC              (1)
#define RESPONSE_ERROR_PARITY           (2)
#define RESPONSE_ERROR_FRAME            (3)
#define RESPONSE_ERROR_IBA_NACK         (4)
#define RESPONSE_ERROR_ADDRESS_NACK     (5)
#define RESPONSE_ERROR_OVER_UNDER_FLOW  (6)
#define RESPONSE_ERROR_TRANSF_ABORT     (8)
#define RESPONSE_ERROR_I2C_W_NACK_ERR   (9)
#define RESPONSE_PORT_TID(x)            (((x) & GENMASK(27, 24)) >> 24)
#define RESPONSE_PORT_DATA_LEN(x)       FIELD_PREP(GENMASK(15, 0), (x))

#define RX_TX_BUFFER_DATA_PACKET_SIZE   (0x4)
#define RX_TX_DATA_PORT                 (0x14)
#define IBI_QUEUE_STATUS                (0x18)
#define IBI_QUEUE_IBI_ID(x)             (((x) & GENMASK(15, 8)) >> 8)
#define IBI_QUEUE_IBI_ID_ADDR(x)        ((x) >> 1)

#define QUEUE_THLD_CTRL                 (0x1c)
#define QUEUE_THLD_CTRL_IBI_STS_MASK    GENMASK(31, 24)
#define QUEUE_THLD_CTRL_RESP_BUF_MASK   GENMASK(15, 8)
#define QUEUE_THLD_CTRL_RESP_BUF(x)     (((x)-1) << 8)

#define DATA_BUFFER_THLD_CTRL           (0x20)
#define DATA_BUFFER_THLD_CTRL_RX_BUF    GENMASK(11, 8)

#define IBI_MR_REQ_REJECT               0x2C
#define IBI_SIR_REQ_REJECT              0x30
#define IBI_SIR_REQ_ID(x)               ((((x) & GENMASK(6, 5)) >> 5) +\
                                        ((x) & GENMASK(4, 0)))
#define IBI_REQ_REJECT_ALL              GENMASK(31, 0)

#define RESET_CTRL                      (0x34)
#define RESET_CTRL_IBI_QUEUE            BIT(5)
#define RESET_CTRL_RX_FIFO              BIT(4)
#define RESET_CTRL_TX_FIFO              BIT(3)
#define RESET_CTRL_RESP_QUEUE           BIT(2)
#define RESET_CTRL_CMD_QUEUE            BIT(1)
#define RESET_CTRL_SOFT                 BIT(0)
#define RESET_CTRL_ALL              \
    (RESET_CTRL_IBI_QUEUE | RESET_CTRL_RX_FIFO |    \
    RESET_CTRL_TX_FIFO | RESET_CTRL_RESP_QUEUE|    \
    RESET_CTRL_CMD_QUEUE | RESET_CTRL_SOFT)

#define INTR_STATUS                     (0x3c)
#define INTR_STATUS_EN                  (0x40)
#define INTR_SIGNAL_EN                  (0x44)
#define INTR_BUSOWNER_UPDATE_STAT       BIT(13)
#define INTR_IBI_UPDATED_STAT           BIT(12)
#define INTR_READ_REQ_RECV_STAT         BIT(11)
#define INTR_TRANSFER_ERR_STAT          BIT(9)
#define INTR_DYN_ADDR_ASSGN_STAT        BIT(8)
#define INTR_CCC_UPDATED_STAT           BIT(6)
#define INTR_TRANSFER_ABORT_STAT        BIT(5)
#define INTR_RESP_READY_STAT            BIT(4)
#define INTR_CMD_QUEUE_READY_STAT       BIT(3)
#define INTR_IBI_THLD_STAT              BIT(2)
#define INTR_RX_THLD_STAT               BIT(1)
#define INTR_TX_THLD_STAT               BIT(0)
#define INTR_ALL                \
    (INTR_BUSOWNER_UPDATE_STAT | INTR_IBI_UPDATED_STAT |    \
    INTR_READ_REQ_RECV_STAT | INTR_TRANSFER_ERR_STAT |      \
    INTR_DYN_ADDR_ASSGN_STAT | INTR_CCC_UPDATED_STAT |      \
    INTR_TRANSFER_ABORT_STAT | INTR_RESP_READY_STAT |       \
    INTR_CMD_QUEUE_READY_STAT | INTR_IBI_THLD_STAT |        \
    INTR_TX_THLD_STAT | INTR_RX_THLD_STAT)

#define INTR_CONTROLLER_MASK    \
    (INTR_TRANSFER_ERR_STAT | INTR_RESP_READY_STAT | INTR_IBI_THLD_STAT)

#define QUEUE_STATUS_LEVEL                0x4c
#define QUEUE_STATUS_IBI_STATUS_CNT(x)    (((x) & GENMASK(28, 24)) >> 24)
#define QUEUE_STATUS_IBI_BUF_BLR(x)       (((x) & GENMASK(23, 16)) >> 16)
#define QUEUE_STATUS_LEVEL_RESP(x)        (((x) & GENMASK(15, 8)) >> 8)
#define QUEUE_STATUS_LEVEL_CMD(x)         FIELD_PREP(GENMASK(7, 0), (x))

#define DATA_BUFFER_STATUS_LEVEL          0x50
#define DATA_BUFFER_STATUS_LEVEL_TX(x)    FIELD_PREP(GENMASK(7, 0), (x))

#define DEVICE_ADDR_TABLE_POINTER         (0x5c)
#define DEVICE_ADDR_TABLE_ADDR(x)         FIELD_PREP(GENMASK(15, 0), (x))

#define DEV_CHAR_TABLE_POINTER            (0x60)
#define DEVICE_CHAR_TABLE_ADDR(x)         FIELD_PREP(GENMASK(11, 0), (x))

#define SCL_I3C_OD_TIMING                 (0xb4)
#define SCL_I3C_PP_TIMING                 (0xb8)
#define SCL_I3C_TIMING_HCNT(x)            (((u32)(x) << 16) & GENMASK(23, 16))
#define SCL_I3C_TIMING_LCNT(x)            FIELD_PREP(GENMASK(7, 0), (u32)(x))
#define SCL_I3C_TIMING_CNT_MIN            (5)

#define SCL_I2C_FM_TIMING                 (0xbc)
#define SCL_I2C_FM_TIMING_HCNT(x)         (((u32)(x) << 16) & GENMASK(31, 16))
#define SCL_I2C_FM_TIMING_LCNT(x)         FIELD_PREP(GENMASK(15, 0), (u32)(x))
#define SCL_I2C_FMP_TIMING                (0xc0)
#define SCL_I2C_FMP_TIMING_HCNT(x)        (((u32)(x) << 16) & GENMASK(23, 16))
#define SCL_I2C_FMP_TIMING_LCNT(x)        FIELD_PREP(GENMASK(15, 0), (u32)(x))

#define SCL_EXT_LCNT_TIMING               (0xc8)
#define SCL_EXT_LCNT_4(x)                 (((u32)(x) << 24) & GENMASK(31, 24))
#define SCL_EXT_LCNT_3(x)                 (((u32)(x) << 16) & GENMASK(23, 16))
#define SCL_EXT_LCNT_2(x)                 (((u32)(x) << 8) & GENMASK(15, 8))
#define SCL_EXT_LCNT_1(x)                 FIELD_PREP(GENMASK(7, 0), (u64)(x))

#define BUS_FREE_TIMING                   (0xd4)
#define BUS_I3C_MST_FREE(x)               FIELD_PREP(GENMASK(15, 0), (x))

#define DEV_ADDR_TABLE_DYNAMIC_ADDR(x)    (((x) << 16) & GENMASK(23, 16))
#define DEV_ADDR_TABLE_LOC(start, idx)    ((start) + ((idx) << 2))

#define DEV_CHAR_TABLE_LOC1(start, idx)   ((start) + ((idx) << 4))
#define DEV_CHAR_TABLE_MSB_PID(x)         FIELD_PREP(GENMASK(31, 16), (x))
#define DEV_CHAR_TABLE_LSB_PID(x)         FIELD_PREP(GENMASK(15, 0), (x))
#define DEV_CHAR_TABLE_LOC2(start, idx)   ((DEV_CHAR_TABLE_LOC1(start,\
                        idx)) + 4)
#define DEV_CHAR_TABLE_LOC3(start, idx)   ((DEV_CHAR_TABLE_LOC1(start,\
                        idx)) + 8)
#define DEV_CHAR_TABLE_DCR(x)             FIELD_PREP(GENMASK(15, 0), (x))
#define DEV_CHAR_TABLE_BCR(x)             (((x) & GENMASK(15, 8)) >> 8)

#define I3C_BUS_SDR1_SCL_RATE             (8000000)
#define I3C_BUS_SDR2_SCL_RATE             (6000000)
#define I3C_BUS_SDR3_SCL_RATE             (4000000)
#define I3C_BUS_SDR4_SCL_RATE             (2000000)
#define I3C_BUS_I2C_FM_TLOW_MIN_NS        (1300)
#define I3C_BUS_I2C_FMP_TLOW_MIN_NS       (500)
#define I3C_BUS_THIGH_MAX_NS              (41)
#define I3C_PERIOD_NS                     (1000000000)

#define I3C_SDR_MODE                      (0x0)
#define I3C_HDR_MODE                      (0x1)
#define I2C_SLAVE                         (2)
#define I3C_SLAVE                         (3)
#define I3C_GETMXDS_FORMAT_1_LEN          (2)
#define I3C_GETMXDS_FORMAT_2_LEN          (5)

/*
 * CCC event bits.
 */
#define I3C_CCC_EVENT_SIR                 BIT(0)
#define I3C_CCC_EVENT_MR                  BIT(1)
#define I3C_CCC_EVENT_HJ                  BIT(3)

#define I3C_BUS_TYP_I3C_SCL_RATE          (12500000)
#define I3C_BUS_I2C_FM_PLUS_SCL_RATE      (1000000)
#define I3C_BUS_I2C_FM_SCL_RATE           (400000)
#define I3C_BUS_TLOW_OD_MIN_NS            (200)

#define I3C_LVR_I2C_INDEX_MASK            GENMASK(7, 5)
#define I3C_LVR_I2C_INDEX(x)              ((x) << 5)

/*
 * addresses.
 */
#define I3C_BROADCAST_ADDR                0x7E
#define I3C_MAX_ADDR                      0x7F

/*
 * ccc.
 */
#define I3C_CCC_BROADCAST_MAX_ID          0x7FU
#define I3C_CCC_DIRECT                    BIT(7)

/**
 * Enable Events Command
 *
 * @param broadcast True if broadcast, false if direct.
 */
#define I3C_CCC_ENEC(broadcast)         ((broadcast) ? 0x00U : 0x80U)

/**
 * Disable Events Command
 *
 * @param broadcast True if broadcast, false if direct.
 */
#define I3C_CCC_DISEC(broadcast)        ((broadcast) ? 0x01U : 0x81U)

/** Reset Dynamic Address Assignment (Broadcast) */
#define I3C_CCC_RSTDAA                  0x06U

/** Enter Dynamic Address Assignment (Broadcast) */
#define I3C_CCC_ENTDAA                  0x07U

/**
 * Set Max Read Length (Broadcast or Direct)
 *
 * @param broadcast True if broadcast, false if direct.
 */
#define I3C_CCC_SETMRL(broadcast)        ((broadcast) ? 0x0AU : 0x8AU)

/**
 * Target Reset Action
 *
 * @param broadcast True if broadcast, false if direct.
 */
#define I3C_CCC_RSTACT(broadcast)          ((broadcast) ? 0x2AU : 0x9AU)

/** Set Dynamic Address from Static Address (Direct) */
#define I3C_CCC_SETDASA                   0x87U

/** Get Max Write Length (Direct) */
#define I3C_CCC_GETMWL                    0x8BU

/** Get Max Read Length (Direct) */
#define I3C_CCC_GETMRL                    0x8CU

/** Get Provisioned ID (Direct) */
#define I3C_CCC_GETPID                    0x8DU

/** Get Bus Characteristics Register (Direct) */
#define I3C_CCC_GETBCR                    0x8EU

/** Get Device Characteristics Register (Direct) */
#define I3C_CCC_GETDCR                    0x8FU

/** Get Max Data Speed (Direct) */
#define I3C_CCC_GETMXDS                   0x94U

/*
 * Events for both enabling and disabling since
 * they have the same bits.
 */
#define I3C_CCC_EVT_INTR                  BIT(0)
#define I3C_CCC_EVT_CR                    BIT(1)
#define I3C_CCC_EVT_HJ                    BIT(3)

#define I3C_CCC_EVT_ALL            \
    (I3C_CCC_EVT_INTR | I3C_CCC_EVT_CR | I3C_CCC_EVT_HJ)

/*
 * Bus Characteristic Register (BCR)
 * - BCR[7:6]: Device Role
 *   - 0: I3C Target
 *   - 1: I3C Controller capable
 *   - 2: Reserved
 *   - 3: Reserved
 * - BCR[5]: Advanced Capabilities
 *   - 0: Does not support optional advanced capabilities.
 *   - 1: Supports optional advanced capabilities which
 *        can be viewed via GETCAPS CCC.
 * - BCR[4}: Virtual Target Support
 *   - 0: Is not a virtual target.
 *   - 1: Is a virtual target.
 * - BCR[3]: Offline Capable
 *   - 0: Will always response to I3C commands.
 *   - 1: Will not always response to I3C commands.
 * - BCR[2]: IBI Payload
 *   - 0: No data bytes following the accepted IBI.
 *   - 1: One data byte (MDB, Mandatory Data Byte) follows
 *        the accepted IBI. Additional data bytes may also
 *        follows.
 * - BCR[1]: IBI Request Capable
 *   - 0: Not capable
 *   - 1: Capable
 * - BCR[0]: Max Data Speed Limitation
 *   - 0: No Limitation
 *   - 1: Limitation obtained via GETMXDS CCC.
 */
#define I3C_BCR_MAX_DATA_SPEED_LIMIT           BIT(0)
#define I3C_BCR_IBI_PAYLOAD_HAS_DATA_BYTE      BIT(2)

/*
 * I3C_MSG_* are I3C Message flags.
 */

/** Write message to I3C bus. */
#define I3C_MSG_WRITE            (0U << 0U)

/** Read message from I2C bus. */
#define I3C_MSG_READ             BIT(0)

/** Send STOP after this message. */
#define I3C_MSG_STOP             BIT(1)

struct i3c_msg {
    u8 addr;
    u8 flags;
    u8 len;
    u8 *buf;
    u8 hdr_mode;
};

/**
 * @brief Structure describing a I3C target device.
 *
 * Instances of this are passed to the I3C controller device APIs,
 * for example:
 * - i3c_device_register() to tell the controller of a target device.
 * - i3c_transfers() to initiate data transfers between controller and
 *   target device.
 *
 * Fields @c bus, @c pid and @c static_addr must be initialized by
 * the module that implements the target device behavior prior to
 * passing the object reference to I3C controller device APIs.
 * @c static_addr can be zero if target device does not have static
 * address.
 *
 * Field @c node should not be initialized or modified manually.
 */
struct i3c_device_desc {
    ofnode node;
    struct udevice * bus;
    const struct udevice * const dev;
    const uint64_t pid:48;
    /** Pid[47:33](Manufacture ID) + Pid[32](Provisional ID Type Selector) */
    int pid_h;
    /** Pid[31:16](Part ID) + Pid[15:12](Instance ID) + Pid[11:0](extra info) */
    int pid_l;

    /**
    * Static address for this target device.
    *
    * 0 if static address is not being used, and only dynamic
    * address is used. This means that the target device must
    * go through ENTDAA (Dynamic Address Assignment) to get
    * a dynamic address before it can communicate with
    * the controller. This means SETAASA and SETDASA CCC
    * cannot be used to set dynamic address on the target
    * device (as both are to tell target device to use static
    * address as dynamic address).
    */
    const u8 static_addr;

    /**
    * Initial dynamic address.
    *
    * This is specified in the device tree property "assigned-address"
    * to indicate the desired dynamic address during address
    * assignment (SETDASA and ENTDAA).
    *
    * 0 if there is no preference.
    */
    const u8 init_dynamic_addr;

    /**
    * Dynamic Address for this target device used for communication.
    *
    * This is to be set by the controller driver in one of
    * the following situations:
    * - During Dynamic Address Assignment (during ENTDAA)
    * - Reset Dynamic Address Assignment (RSTDAA)
    * - Set All Addresses to Static Addresses (SETAASA)
    * - Set New Dynamic Address (SETNEWDA)
    * - Set Dynamic Address from Static Address (SETDASA)
    *
    * 0 if address has not been assigned.
    */
    u8 dynamic_addr;

    /**
    * Bus Characteristic Register (BCR)
    * - BCR[7:6]: Device Role
    *   - 0: I3C Target
    *   - 1: I3C Controller capable
    *   - 2: Reserved
    *   - 3: Reserved
    * - BCR[5]: Advanced Capabilities
    *   - 0: Does not support optional advanced capabilities.
    *   - 1: Supports optional advanced capabilities which
    *        can be viewed via GETCAPS CCC.
    * - BCR[4}: Virtual Target Support
    *   - 0: Is not a virtual target.
    *   - 1: Is a virtual target.
    * - BCR[3]: Offline Capable
    *   - 0: Will always response to I3C commands.
    *   - 1: Will not always response to I3C commands.
    * - BCR[2]: IBI Payload
    *   - 0: No data bytes following the accepted IBI.
    *   - 1: One data byte (MDB, Mandatory Data Byte) follows
    *        the accepted IBI. Additional data bytes may also
    *        follows.
    * - BCR[1]: IBI Request Capable
    *   - 0: Not capable
    *   - 1: Capable
    * - BCR[0]: Max Data Speed Limitation
    *   - 0: No Limitation
    *   - 1: Limitation obtained via GETMXDS CCC.
    */
    u8 bcr;

    /**
    * Device Characteristic Register (DCR)
    *
    * Describes the type of device. Refer to official documentation
    * on what this number means.
    */
    u8 dcr;

    struct {
        /** Maximum Read Speed */
        u8 maxrd;

        /** Maximum Write Speed */
        u8 maxwr;

        /** Maximum Read turnaround time in microseconds. */
        int max_read_turnaround;
    } data_speed;

    struct {
        /** Maximum Read Length */
        uint16_t mrl;

        /** Maximum Write Length */
        uint16_t mwl;

        /** Maximum IBI Payload Size. Valid only if BCR[2] is 1. */
        u8 max_ibi;
    } data_length;

    /** Private data by the controller to aid in transactions. Do not modify. */
    void *controller_priv;
};

/**
 * struct dm_i3c_ops - driver operations for I2C uclass
 *
 * Drivers should support these operations unless otherwise noted. These
 * operations are intended to be used by uclass code, not directly from
 * other code.
 */
struct dm_i3c_ops {
    /**
    * Transfer messages in I3C mode.
    *
    * @see i3c_transfer
    *
    * @param dev Pointer to controller device driver instance.
    * @param target Pointer to target device descriptor.
    * @param msg Pointer to I3C messages.
    * @param num_msgs Number of messages to transfer.
    *
    * @return @see i3c_transfer
    */
    int (*i3c_xfers)(struct udevice *dev,
            struct i3c_device_desc *target,
            struct i3c_msg *msgs,
            u8 num_msgs);
    int (*read)(struct udevice *dev,u8 dev_number,
               u8 *buf, int num_bytes);
    int (*write)(struct udevice *dev,u8 dev_number,
               u8 *buf, int num_bytes);
};

#define i3c_get_ops(dev)    ((struct dm_i3c_ops *)(dev)->driver->ops)

enum i3c_addr_slot_status {
    I3C_ADDR_SLOT_STATUS_FREE = 0ULL,
    I3C_ADDR_SLOT_STATUS_RSVD,
    I3C_ADDR_SLOT_STATUS_I3C_DEV,
    I3C_ADDR_SLOT_STATUS_I2C_DEV,
    I3C_ADDR_SLOT_STATUS_MASK = 0x03ULL,
};

/**
 * enum i3c_bus_mode - I3C bus mode
 * @I3C_BUS_MODE_PURE: only I3C devices are connected to the bus. No limitation
 *              expected
 * @I3C_BUS_MODE_MIXED_FAST: I2C devices with 50ns spike filter are present on
 *                the bus. The only impact in this mode is that the
 *                high SCL pulse has to stay below 50ns to trick I2C
 *                devices when transmitting I3C frames
 * @I3C_BUS_MODE_MIXED_LIMITED: I2C devices without 50ns spike filter are
 *                present on the bus. However they allow
 *                compliance up to the maximum SDR SCL clock
 *                frequency.
 * @I3C_BUS_MODE_MIXED_SLOW: I2C devices without 50ns spike filter are present
 *                on the bus
 */
enum i3c_bus_mode {
    I3C_BUS_MODE_PURE,
    I3C_BUS_MODE_MIXED_FAST,
    I3C_BUS_MODE_MIXED_LIMITED,
    I3C_BUS_MODE_MIXED_SLOW,
};

enum i3c_ccc_rstact_defining_byte {
    I3C_CCC_RSTACT_NO_RESET = 0x00U,
    I3C_CCC_RSTACT_PERIPHERAL_ONLY = 0x01U,
    I3C_CCC_RSTACT_RESET_WHOLE_TARGET = 0x02U,
    I3C_CCC_RSTACT_DEBUG_NETWORK_ADAPTER = 0x03U,
    I3C_CCC_RSTACT_VIRTUAL_TARGET_DETECT = 0x04U,
};

/**
 * enum i3c_error_code - I3C error codes
 *
 * These are the standard error codes as defined by the I3C specification.
 * When -EIO is returned by the i3c_device_do_priv_xfers() or
 * i3c_device_send_hdr_cmds() one can check the error code in
 * &struct_i3c_priv_xfer.err or &struct i3c_hdr_cmd.err to get a better idea of
 * what went wrong.
 *
 * @I3C_ERROR_UNKNOWN: unknown error, usually means the error is not I3C
 *              related
 * @I3C_ERROR_M0: M0 error
 * @I3C_ERROR_M1: M1 error
 * @I3C_ERROR_M2: M2 error
 */
enum i3c_error_code {
    I3C_ERROR_UNKNOWN = 0,
    I3C_ERROR_M0 = 1,
    I3C_ERROR_M1,
    I3C_ERROR_M2,
};

/**
 * @brief Payload for SETMWL/GETMWL CCC (Set/Get Maximum Write Length).
 *
 * @note For drivers and help functions, the raw data coming
 * back from target device is in big endian. This needs to be
 * translated back to CPU endianness before passing back to
 * function caller.
 */
struct i3c_ccc_mwl {
    /** Maximum Write Length */
    uint16_t len;
} __packed;

/**
 * @brief Payload for SETMRL/GETMRL CCC (Set/Get Maximum Read Length).
 *
 * @note For drivers and help functions, the raw data coming
 * back from target device is in big endian. This needs to be
 * translated back to CPU endianness before passing back to
 * function caller.
 */
struct i3c_ccc_mrl {
    /** Maximum Read Length */
    uint16_t len;

    /** Optional IBI Payload Size */
    u8 ibi_len;
} __packed;

/**
 * @brief Payload for GETMXDS CCC (Get Max Data Speed).
 *
 * @note This is only for GETMXDS Format 1 and Format 2.
 */
union i3c_ccc_getmxds {
    struct {
        /** maxWr */
        u8 maxwr;

        /** maxRd */
        u8 maxrd;
    } fmt1;

    struct {
        /** maxWr */
        u8 maxwr;

        /** maxRd */
        u8 maxrd;

        /**
        * Maximum Read Turnaround Time in microsecond.
        *
        * This is in little-endian where first byte is LSB.
        */
        u8 maxrdturn[3];
    } fmt2;

    struct {
        /**
        * Defining Byte 0x00: WRRDTURN
        *
        * @see i3c_ccc_getmxds::fmt2
        */
        u8 wrrdturn;

        /**
        * Defining Byte 0x91: CRHDLY
        * - Bit[2]: Set Bus Actibity State
        * - Bit[1:0]: Controller Handoff Activity State
        */
        u8 crhdly1;
    } fmt3;
} __packed;

/**
 * @brief Payload for ENEC/DISEC CCC (Target Events Command).
 */
struct i3c_ccc_events {
    /**
    * Event byte:
    * - Bit[0]: ENINT/DISINT:
    *   - Target Interrupt Requests
    * - Bit[1]: ENCR/DISCR:
    *   - Controller Role Requests
    * - Bit[3]: ENHJ/DISHJ:
    *   - Hot-Join Event
    */
    u8 events;
} __packed;

/**
 * @brief Structure describing a I2C device on I3C bus.
 *
 * Instances of this are passed to the I3C controller device APIs,
 * for example:
 * () i3c_i2c_device_register() to tell the controller of an I2C device.
 * () i3c_i2c_transfers() to initiate data transfers between controller
 *    and I2C device.
 *
 * Fields other than @c node must be initialized by the module that
 * implements the device behavior prior to passing the object
 * reference to I3C controller device APIs.
 */
struct i3c_i2c_device_desc {
    /** Private, do not modify */
    ofnode node;

    /** I3C bus to which this I2C device is attached */
    const struct udevice *bus;

    /** Static address for this I2C device. */
    const uint16_t addr;

    /**
    * Legacy Virtual Register (LVR)
    * - LVR[7:5]: I2C device index:
    *   - 0: I2C device has a 50 ns spike filter where
    *        it is not affected by high frequency on SCL.
    *   - 1: I2C device does not have a 50 ns spike filter
    *        but can work with high frequency on SCL.
    *   - 2: I2C device does not have a 50 ns spike filter
    *        and cannot work with high frequency on SCL.
    * - LVR[4]: I2C mode indicator:
    *   - 0: FM+ mode
    *   - 1: FM mode
    * - LVR[3:0]: Reserved.
    */
    const u8 lvr;

    /** Private data by the controller to aid in transactions. Do not modify. */
    void *controller_priv;
};

/**
 * @brief Structure for describing attached devices for a controller.
 *
 * This contains arrays of attached I3C and I2C devices.
 *
 * This is a helper struct that can be used by controller device
 * driver to aid in device management.
 */
struct i3c_dev_list {
    /**
    * Pointer to array of attached I3C devices.
    */
    struct i3c_device_desc * const i3c;

    /**
    * Pointer to array of attached I2C devices.
    */
    struct i3c_i2c_device_desc * const i2c;

    /**
    * Number of I3C devices in array.
    */
    u8 num_i3c;

    /**
    * Number of I2C devices in array.
    */
    const u8 num_i2c;
};

/**
 * @brief Structure to keep track of addresses on I3C bus.
 */
struct i3c_addr_slots {
    /* 2 bits per slot */
    unsigned long slots[((I3C_MAX_ADDR + 1) * 2) / BITS_PER_LONG];
};

/**
 * @brief I3C controller object
 *
 * @param dev: associated controller device
 * @param ops: controller operations
 * @param addrslots: structure to keep track of addresses
 * status and ease the DAA (Dynamic Address Assignment) procedure
 * @param addrs: 2 lists containing all I3C/I2C devices connected to the bus
 * @param mode: bus mode
 */
struct i3c_controller {
    const struct udevice *dev;
    const struct dm_i3c_ops *ops;
    struct i3c_addr_slots addrslots;
    u8 *addrs;
    enum i3c_bus_mode mode;
};

/**
 * @brief DesignWare I3C target information
 *
 * @param dev: Common I3C device data
 * @param controller: Pointer to the controller device
 * @param work: Work structure for queue
 * @param ibi_handle: Function to handle In-Bound Interrupt
 * @param dev_type: Type of the device
 * @param index: Index of the device
 * @param assigned_dyn_addr: Assigned dynamic address during DAA
 * @param lvr: Legacy Virtual Register value
 */
struct dw_target_info {
    struct i3c_device_desc dev;
    const struct udevice *controller;
    int dev_type;
    int index;
    u8 assigned_dyn_addr;
    u8 lvr;
};

/**
 * @brief Payload structure for Direct CCC to one target.
 */
struct i3c_ccc_target_payload {
    /** Target address */
    u8 addr;

    /** @c 0 for Write, @c 1 for Read */
    u8 rnw:1;

    /**
    * - For Write CCC, pointer to the byte array of data
    *   to be sent, which may contain the Sub-Command Byte
    *   and additional data.
    * - For Read CCC, pointer to the byte buffer for data
    *   to be read into.
    */
    u8 *data;

    /** Length in bytes for @p data. */
    size_t data_len;
};

/**
 * @brief Payload structure for one CCC transaction.
 */
struct i3c_ccc_payload {
    struct {
        /**
        * The CCC ID (@c I3C_CCC_*).
        */
        u8 id;

        /**
        * Pointer to byte array of data for this CCC.
        *
        * This is the bytes following the CCC command in CCC frame.
        * Set to @c NULL if no associated data.
        */
        u8 *data;

        /** Length in bytes for optional data array. */
        size_t data_len;
    } ccc;

    struct {
        /**
        * Array of struct i3c_ccc_target_payload.
        *
        * Each element describes the target and associated
        * payloads for this CCC.
        *
        * Use with Direct CCC.
        */
        struct i3c_ccc_target_payload *payloads;

        /** Number of targets */
        size_t num_targets;
    } targets;
};

/**
 * @brief Find the next free address.
 *
 * This can be used to find the next free address that can be
 * assigned to a new device.
 *
 * @param slots Pointer to the address slots structure.
 *
 * @return The next free address, or 0 if none found.
 */
u8 i3c_addr_slots_next_free_find(struct i3c_addr_slots *slots);

/**
 * @brief Set the address status of a device.
 *
 * @param slots Pointer to the address slots structure.
 * @param dev_addr Device address.
 * @param status New status for the address @p dev_addr.
 */
void i3c_addr_slots_set(struct i3c_addr_slots *slots,
            u8 dev_addr,
            enum i3c_addr_slot_status status);

/**
 * @brief Get the address status of a device.
 *
 * @param slots Pointer to the address slots structure.
 * @param dev_addr Device address.
 *
 * @return Address status for the address @p dev_addr.
 */
enum i3c_addr_slot_status i3c_addr_slots_status(struct i3c_addr_slots *slots,
                        u8 dev_addr);

/**
 * @brief Payload for GETPID CCC (Get Provisioned ID).
 */
struct i3c_ccc_getpid {
    /**
    * 48-bit Provisioned ID.
    *
    * @note Data is big-endian where first byte is MSB.
    */
    u8 pid[6];
} __packed;

/**
 * @brief Payload for GETBCR CCC (Get Bus Characteristics Register).
 */
struct i3c_ccc_getbcr {
    /** Bus Characteristics Register */
    u8 bcr;
} __packed;

/**
 * @brief Payload for GETDCR CCC (Get Device Characteristics Register).
 */
struct i3c_ccc_getdcr {
    /** Device Characteristics Register */
    u8 dcr;
} __packed;

struct dw_i3c_cmd {
    int cmd_lo;
    int cmd_hi;
    u16 tx_len;
    u16 rx_len;
    u8 error;
    void *buf;
};

struct dw_i3c_xfer {
    int ret;
    int ncmds;
    struct dw_i3c_cmd cmds[16];
};

struct dw_i3c_priv {
    void __iomem *regs;
    struct reset_ctl_bulk resets;
    struct clk clk;
    struct {
        unsigned long core_clk;
        int max_slv_devs;
        u8 cmdfifodepth;
        u8 datafifodepth;
        struct i3c_dev_list device_list;
    } config;
    struct {
        struct i3c_controller base;
        int free_pos;
        u16 datstartaddr;
        u16 dctstartaddr;
        struct dw_target_info *target;
        struct dw_i3c_xfer *xfer;
        /** Address slots */
        struct i3c_addr_slots addr_slots;
    } data;
};

/**
 * @brief Get PID from a target
 *
 * Helper function to get PID (Provisioned ID) from
 * target device.
 *
 * @param[in] target Pointer to the target device descriptor.
 * @param[out] pid Pointer to the PID payload structure.
 *
 * @return @see i3c_do_ccc
 */
int i3c_ccc_do_getpid(struct i3c_device_desc *target,
             struct i3c_ccc_getpid *pid);

/**
 * @brief Get BCR from a target
 *
 * Helper function to get BCR (Bus Characteristic Register) from
 * target device.
 *
 * @param[in] target Pointer to the target device descriptor.
 * @param[out] bcr Pointer to the BCR payload structure.
 *
 * @return @see i3c_do_ccc
 */
int i3c_ccc_do_getbcr(struct i3c_device_desc *target,
             struct i3c_ccc_getbcr *bcr);

/**
 * @brief Get DCR from a target
 *
 * Helper function to get DCR (Device Characteristic Register) from
 * target device.
 *
 * @param[in] target Pointer to the target device descriptor.
 * @param[out] dcr Pointer to the DCR payload structure.
 *
 * @return @see i3c_do_ccc
 */
int i3c_ccc_do_getdcr(struct i3c_device_desc *target,
             struct i3c_ccc_getdcr *dcr);

/**
 * @brief Do i3c write
 *
 * Uclass general function to start write to i3c target
 *
 * @udevice pointer to i3c controller.
 * @dev_number target device number.
 * @buf target Buffer to write.
 * @num_bytes length of bytes to write.
 *
 * @return 0 for success 
 */
int dm_i3c_write(struct udevice *dev, u8 dev_number,
               u8 *buf, int num_bytes);

/**
 * @brief Do i3c read
 *
 * Uclass general function to start read from i3c target
 *
 * @udevice pointer to i3c controller.
 * @dev_number target device number.
 * @buf target Buffer to read.
 * @num_bytes length of bytes to read.
 *
 * @return 0 for success 
 */
int dm_i3c_read(struct udevice *dev,u8 dev_number,
               u8 *buf, int num_bytes);

/**
 * @brief get device target list
 *
 * Uclass general function to get and list i3c targets
 * binded to the controller
 *
 * @udevice pointer to i3c controller.
 *
 * @return 0 for success 
 */
int dm_get_target_device_list(struct udevice *dev);

#endif /*_DW_I3C_H_*/
