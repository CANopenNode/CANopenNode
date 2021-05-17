/**
 * CANopen Service Data Object - server protocol.
 *
 * @file        CO_SDOserver.h
 * @ingroup     CO_SDOserver
 * @author      Janez Paternoster
 * @copyright   2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CO_SDO_SERVER_H
#define CO_SDO_SERVER_H

#include "301/CO_driver.h"
#include "301/CO_ODinterface.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_SDO_SRV
#define CO_CONFIG_SDO_SRV (CO_CONFIG_SDO_SRV_SEGMENTED | \
                           CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE | \
                           CO_CONFIG_GLOBAL_FLAG_TIMERNEXT | \
                           CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC)
#endif
#ifndef CO_CONFIG_SDO_SRV_BUFFER_SIZE
#define CO_CONFIG_SDO_SRV_BUFFER_SIZE 32
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_SDOserver SDO server
 * CANopen Service Data Object - server protocol.
 *
 * @ingroup CO_CANopen_301
 * @{
 *
 * Service data objects (SDOs) allow the access to any entry of the CANopen
 * Object dictionary. By SDO a peer-to-peer communication channel between two
 * CANopen devices is established. In addition, the SDO protocol enables to
 * transfer any amount of data in a segmented way. Therefore the SDO protocol is
 * mainly used in order to communicate configuration data.
 *
 * All CANopen devices must have implemented SDO server and first SDO server
 * channel. Servers serves data from Object dictionary. Object dictionary
 * is a collection of variables, arrays or records (structures), which can be
 * used by the stack or by the application. This file (CO_SDOserver.h)
 * implements SDO server.
 *
 * SDO client can be (optionally) implemented on one (or multiple, if multiple
 * SDO channels are used) device in CANopen network. Usually this is master
 * device and provides also some kind of user interface, so configuration of
 * the network is possible. Code for the SDO client is in file CO_SDOclient.h.
 *
 * SDO communication cycle is initiated by the client. Client can upload (read)
 * data from device or can download (write) data to device. If data size is less
 * or equal to 4 bytes, communication is finished by one server response
 * (expedited transfer). If data size is longer, data are split into multiple
 * segments of request/response pairs (normal or segmented transfer). For longer
 * data there is also a block transfer protocol, which transfers larger block of
 * data in secure way with little protocol overhead. If error occurs during SDO
 * transfer #CO_SDO_abortCode_t is send by client or server and transfer is
 * terminated. For more details see #CO_SDO_state_t.
 *
 * Access to Object dictionary is specified in @ref CO_ODinterface.
 */


/**
 * Internal state flags indicate type of transfer
 *
 * These flags correspond to the upper nibble of the SDO state machine states
 * and can be used to determine the type of state an SDO object is in.
 */
#define CO_SDO_ST_FLAG_DOWNLOAD     0x10U
#define CO_SDO_ST_FLAG_UPLOAD       0x20U
#define CO_SDO_ST_FLAG_BLOCK        0x40U

/**
 * Internal states of the SDO state machine.
 *
 * Upper nibble of byte indicates type of state:
 * 0x10: Download
 * 0x20: Upload
 * 0x40: Block Mode
 *
 * Note: CANopen has little endian byte order.
 */
typedef enum {
/**
 * - SDO client may start new download to or upload from specified node,
 * specified index and specified subindex. It can start normal or block
 * communication.
 * - SDO server is waiting for client request. */
CO_SDO_ST_IDLE = 0x00U,
/**
 * - SDO client or server may send SDO abort message in case of error:
 *  - byte 0: @b 10000000 binary.
 *  - byte 1..3: Object index and subIndex.
 *  - byte 4..7: #CO_SDO_abortCode_t. */
CO_SDO_ST_ABORT = 0x01U,

/**
 * - SDO client: Node-ID of the SDO server is the same as node-ID of this node,
 *   SDO client is the same device as SDO server. Transfer data directly without
 *   communication on CAN.
 * - SDO server does not use this state. */
CO_SDO_ST_DOWNLOAD_LOCAL_TRANSFER = 0x10U,
/**
 * - SDO client initiates SDO download:
 *  - byte 0: @b 0010nnes binary: (nn: if e=s=1, number of data bytes, that do
 *    @b not contain data; e=1 for expedited transfer; s=1 if data size is
 *    indicated.)
 *  - byte 1..3: Object index and subIndex.
 *  - byte 4..7: If e=1, expedited data are here. If e=0 s=1, size of data for
 *    segmented transfer is indicated here.
 * - SDO server is in #CO_SDO_ST_IDLE state and waits for client request. */
CO_SDO_ST_DOWNLOAD_INITIATE_REQ = 0x11U,
/**
 * - SDO client waits for response.
 * - SDO server responses:
 *  - byte 0: @b 01100000 binary.
 *  - byte 1..3: Object index and subIndex.
 *  - byte 4..7: Reserved.
 * - In case of expedited transfer communication ends here. */
CO_SDO_ST_DOWNLOAD_INITIATE_RSP = 0x12U,
/**
 * - SDO client sends SDO segment:
 *  - byte 0: @b 000tnnnc binary: (t: toggle bit, set to 0 in first segment;
 *    nnn: number of data bytes, that do @b not contain data; c=1 if this is the
 *    last segment).
 *  - byte 1..7: Data segment.
 * - SDO server waits for segment. */
CO_SDO_ST_DOWNLOAD_SEGMENT_REQ = 0x13U,
/**
 * - SDO client waits for response.
 * - SDO server responses:
 *  - byte 0: @b 001t0000 binary: (t: toggle bit, set to 0 in first segment).
 *  - byte 1..7: Reserved.
 * - If c was set to 1, then communication ends here. */
CO_SDO_ST_DOWNLOAD_SEGMENT_RSP = 0x14U,

/**
 * - SDO client: Node-ID of the SDO server is the same as node-ID of this node,
 *   SDO client is the same device as SDO server. Transfer data directly without
 *   communication on CAN.
 * - SDO server does not use this state. */
CO_SDO_ST_UPLOAD_LOCAL_TRANSFER = 0x20U,
/**
 * - SDO client initiates SDO upload:
 *  - byte 0: @b 01000000 binary.
 *  - byte 1..3: Object index and subIndex.
 *  - byte 4..7: Reserved.
 * - SDO server is in #CO_SDO_ST_IDLE state and waits for client request. */
CO_SDO_ST_UPLOAD_INITIATE_REQ = 0x21U,
/**
 * - SDO client waits for response.
 * - SDO server responses:
 *  - byte 0: @b 0100nnes binary: (nn: if e=s=1, number of data bytes, that do
 *    @b not contain data; e=1 for expedited transfer; s=1 if data size is
 *    indicated).
 *  - byte 1..3: Object index and subIndex.
 *  - byte 4..7: If e=1, expedited data are here. If e=0 s=1, size of data for
 *    segmented transfer is indicated here.
 * - In case of expedited transfer communication ends here. */
CO_SDO_ST_UPLOAD_INITIATE_RSP = 0x22U,
/**
 * - SDO client requests SDO segment:
 *  - byte 0: @b 011t0000 binary: (t: toggle bit, set to 0 in first segment).
 *  - byte 1..7: Reserved.
 * - SDO server waits for segment request. */
CO_SDO_ST_UPLOAD_SEGMENT_REQ = 0x23U,
/**
 * - SDO client waits for response.
 * - SDO server responses with data:
 *  - byte 0: @b 000tnnnc binary: (t: toggle bit, set to 0 in first segment;
 *    nnn: number of data bytes, that do @b not contain data; c=1 if this is the
 *    last segment).
 *  - byte 1..7: Data segment.
 * - If c is set to 1, then communication ends here. */
CO_SDO_ST_UPLOAD_SEGMENT_RSP = 0x24U,

/**
 * - SDO client initiates SDO block download:
 *  - byte 0: @b 11000rs0 binary: (r=1 if client supports generating CRC on
 *    data; s=1 if data size is indicated.)
 *  - byte 1..3: Object index and subIndex.
 *  - byte 4..7: If s=1, then size of data for block download is indicated here.
 * - SDO server is in #CO_SDO_ST_IDLE state and waits for client request. */
CO_SDO_ST_DOWNLOAD_BLK_INITIATE_REQ = 0x51U,
/**
 * - SDO client waits for response.
 * - SDO server responses:
 *  - byte 0: @b 10100r00 binary: (r=1 if server supports generating CRC on
 *    data.)
 *  - byte 1..3: Object index and subIndex.
 *  - byte 4: blksize: Number of segments per block that shall be used by the
 *    client for the following block download with 0 < blksize < 128.
 *  - byte 5..7: Reserved. */
CO_SDO_ST_DOWNLOAD_BLK_INITIATE_RSP = 0x52U,
/**
 * - SDO client sends 'blksize' segments of data in sequence:
 *  - byte 0: @b cnnnnnnn binary: (c=1 if no more segments to be downloaded,
 *    enter SDO block download end phase; nnnnnnn is sequence number of segment,
 *    1..127.
 *  - byte 1..7: At most 7 bytes of segment data to be downloaded.
 * - SDO server reads sequence of 'blksize' blocks. */
CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ = 0x53U,
/**
 * - SDO client waits for response.
 * - SDO server responses:
 *  - byte 0: @b 10100010 binary.
 *  - byte 1: ackseq: sequence number of last segment that was received
 *    successfully during the last block download. If ackseq is set to 0 the
 *    server indicates the client that the segment with the sequence number 1
 *    was not received correctly and all segments shall be retransmitted by the
 *    client.
 *  - byte 2: Number of segments per block that shall be used by the client for
 *    the following block download with 0 < blksize < 128.
 *  - byte 3..7: Reserved.
 * - If c was set to 1, then communication enters SDO block download end phase.
 */
CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP = 0x54U,
/**
 * - SDO client sends SDO block download end:
 *  - byte 0: @b 110nnn01 binary: (nnn: number of data bytes, that do @b not
 *    contain data)
 *  - byte 1..2: 16 bit CRC for the data set, if enabled by client and server.
 *  - byte 3..7: Reserved.
 * - SDO server waits for client request. */
CO_SDO_ST_DOWNLOAD_BLK_END_REQ = 0x55U,
/**
 * - SDO client waits for response.
 * - SDO server responses:
 *  - byte 0: @b 10100001 binary.
 *  - byte 1..7: Reserved.
 * - Block download successfully ends here.
 */
CO_SDO_ST_DOWNLOAD_BLK_END_RSP = 0x56U,

/**
 * - SDO client initiates SDO block upload:
 *  - byte 0: @b 10100r00 binary: (r=1 if client supports generating CRC on
 *    data.)
 *  - byte 1..3: Object index and subIndex.
 *  - byte 4: blksize: Number of segments per block with 0 < blksize < 128.
 *  - byte 5: pst - protocol switch threshold. If pst > 0 and size of the data
 *    in bytes is less or equal pst, then the server may switch to the SDO
 *    upload protocol #CO_SDO_ST_UPLOAD_INITIATE_RSP.
 *  - byte 6..7: Reserved.
 * - SDO server is in #CO_SDO_ST_IDLE state and waits for client request. */
CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ = 0x61U,
/**
 * - SDO client waits for response.
 * - SDO server responses:
 *  - byte 0: @b 11000rs0 binary: (r=1 if server supports generating CRC on
 *    data; s=1 if data size is indicated. )
 *  - byte 1..3: Object index and subIndex.
 *  - byte 4..7: If s=1, then size of data for block upload is indicated here.
 * - If enabled by pst, then server may alternatively response with
 *   #CO_SDO_ST_UPLOAD_INITIATE_RSP */
CO_SDO_ST_UPLOAD_BLK_INITIATE_RSP = 0x62U,
/**
 * - SDO client sends second initiate for SDO block upload:
 *  - byte 0: @b 10100011 binary.
 *  - byte 1..7: Reserved.
 * - SDO server waits for client request. */
CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ2 = 0x63U,
/**
 * - SDO client reads sequence of 'blksize' blocks.
 * - SDO server sends 'blksize' segments of data in sequence:
 *  - byte 0: @b cnnnnnnn binary: (c=1 if no more segments to be uploaded,
 *    enter SDO block upload end phase; nnnnnnn is sequence number of segment,
 *    1..127.
 *  - byte 1..7: At most 7 bytes of segment data to be uploaded. */
CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ = 0x64U,
/**
 * - SDO client responses:
 *  - byte 0: @b 10100010 binary.
 *  - byte 1: ackseq: sequence number of last segment that was received
 *    successfully during the last block upload. If ackseq is set to 0 the
 *    client indicates the server that the segment with the sequence number 1
 *    was not received correctly and all segments shall be retransmitted by the
 *    server.
 *  - byte 2: Number of segments per block that shall be used by the server for
 *    the following block upload with 0 < blksize < 128.
 *  - byte 3..7: Reserved.
 * - SDO server waits for response.
 * - If c was set to 1 and all segments were successfull received, then
 *   communication enters SDO block upload end phase. */
CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP = 0x65U,
/**
 * - SDO client waits for server request.
 * - SDO server sends SDO block upload end:
 *  - byte 0: @b 110nnn01 binary: (nnn: number of data bytes, that do @b not
 *    contain data)
 *  - byte 1..2: 16 bit CRC for the data set, if enabled by client and server.
 *  - byte 3..7: Reserved. */
CO_SDO_ST_UPLOAD_BLK_END_SREQ = 0x66U,
/**
 * - SDO client responses:
 *  - byte 0: @b 10100001 binary.
 *  - byte 1..7: Reserved.
 * - SDO server waits for response.
 * - Block download successfully ends here. Note that this communication ends
 *   with client response. Client may then start next SDO communication
 *   immediately.
 */
CO_SDO_ST_UPLOAD_BLK_END_CRSP = 0x67U,
} CO_SDO_state_t;


/**
 * SDO abort codes.
 *
 * Send with Abort SDO transfer message.
 *
 * The abort codes not listed here are reserved.
 */
typedef enum {
    /** 0x00000000, No abort */
    CO_SDO_AB_NONE                  = 0x00000000UL,
    /** 0x05030000, Toggle bit not altered */
    CO_SDO_AB_TOGGLE_BIT            = 0x05030000UL,
    /** 0x05040000, SDO protocol timed out */
    CO_SDO_AB_TIMEOUT               = 0x05040000UL,
    /** 0x05040001, Command specifier not valid or unknown */
    CO_SDO_AB_CMD                   = 0x05040001UL,
    /** 0x05040002, Invalid block size in block mode */
    CO_SDO_AB_BLOCK_SIZE            = 0x05040002UL,
    /** 0x05040003, Invalid sequence number in block mode */
    CO_SDO_AB_SEQ_NUM               = 0x05040003UL,
    /** 0x05040004, CRC error (block mode only) */
    CO_SDO_AB_CRC                   = 0x05040004UL,
    /** 0x05040005, Out of memory */
    CO_SDO_AB_OUT_OF_MEM            = 0x05040005UL,
    /** 0x06010000, Unsupported access to an object */
    CO_SDO_AB_UNSUPPORTED_ACCESS    = 0x06010000UL,
    /** 0x06010001, Attempt to read a write only object */
    CO_SDO_AB_WRITEONLY             = 0x06010001UL,
    /** 0x06010002, Attempt to write a read only object */
    CO_SDO_AB_READONLY              = 0x06010002UL,
    /** 0x06020000, Object does not exist in the object dictionary */
    CO_SDO_AB_NOT_EXIST             = 0x06020000UL,
    /** 0x06040041, Object cannot be mapped to the PDO */
    CO_SDO_AB_NO_MAP                = 0x06040041UL,
    /** 0x06040042, Number and length of object to be mapped exceeds PDO
     * length */
    CO_SDO_AB_MAP_LEN               = 0x06040042UL,
    /** 0x06040043, General parameter incompatibility reasons */
    CO_SDO_AB_PRAM_INCOMPAT         = 0x06040043UL,
    /** 0x06040047, General internal incompatibility in device */
    CO_SDO_AB_DEVICE_INCOMPAT       = 0x06040047UL,
    /** 0x06060000, Access failed due to hardware error */
    CO_SDO_AB_HW                    = 0x06060000UL,
    /** 0x06070010, Data type does not match, length of service parameter does
     * not match */
    CO_SDO_AB_TYPE_MISMATCH         = 0x06070010UL,
    /** 0x06070012, Data type does not match, length of service parameter too
     * high */
    CO_SDO_AB_DATA_LONG             = 0x06070012UL,
    /** 0x06070013, Data type does not match, length of service parameter too
     * short */
    CO_SDO_AB_DATA_SHORT            = 0x06070013UL,
    /** 0x06090011, Sub index does not exist */
    CO_SDO_AB_SUB_UNKNOWN           = 0x06090011UL,
    /** 0x06090030, Invalid value for parameter (download only). */
    CO_SDO_AB_INVALID_VALUE         = 0x06090030UL,
    /** 0x06090031, Value range of parameter written too high */
    CO_SDO_AB_VALUE_HIGH            = 0x06090031UL,
    /** 0x06090032, Value range of parameter written too low */
    CO_SDO_AB_VALUE_LOW             = 0x06090032UL,
    /** 0x06090036, Maximum value is less than minimum value. */
    CO_SDO_AB_MAX_LESS_MIN          = 0x06090036UL,
    /** 0x060A0023, Resource not available: SDO connection */
    CO_SDO_AB_NO_RESOURCE           = 0x060A0023UL,
    /** 0x08000000, General error */
    CO_SDO_AB_GENERAL               = 0x08000000UL,
    /** 0x08000020, Data cannot be transferred or stored to application */
    CO_SDO_AB_DATA_TRANSF           = 0x08000020UL,
    /** 0x08000021, Data cannot be transferred or stored to application because
     * of local control */
    CO_SDO_AB_DATA_LOC_CTRL         = 0x08000021UL,
    /** 0x08000022, Data cannot be transferred or stored to application because
     * of present device state */
    CO_SDO_AB_DATA_DEV_STATE        = 0x08000022UL,
    /** 0x08000023, Object dictionary not present or dynamic generation fails */
    CO_SDO_AB_DATA_OD               = 0x08000023UL,
    /** 0x08000024, No data available */
    CO_SDO_AB_NO_DATA               = 0x08000024UL
} CO_SDO_abortCode_t;


/**
 * Return values from SDO server or client functions.
 */
typedef enum {
    /** Waiting in client local transfer. */
    CO_SDO_RT_waitingLocalTransfer = 6,
    /** Data buffer is full.
     * SDO client: data must be read before next upload cycle begins. */
    CO_SDO_RT_uploadDataBufferFull = 5,
    /** CAN transmit buffer is full. Waiting. */
    CO_SDO_RT_transmittBufferFull = 4,
    /** Block download is in progress. Sending train of messages. */
    CO_SDO_RT_blockDownldInProgress = 3,
    /** Block upload is in progress. Receiving train of messages.
     * SDO client: Data must not be read in this state. */
    CO_SDO_RT_blockUploadInProgress = 2,
    /** Waiting server or client response. */
    CO_SDO_RT_waitingResponse = 1,
    /** Success, end of communication. SDO client: uploaded data must be read.*/
    CO_SDO_RT_ok_communicationEnd = 0,
    /** Error in arguments */
    CO_SDO_RT_wrongArguments = -2,
    /** Communication ended with client abort */
    CO_SDO_RT_endedWithClientAbort = -9,
    /** Communication ended with server abort */
    CO_SDO_RT_endedWithServerAbort = -10,
} CO_SDO_return_t;


/**
 * SDO server object.
 */
typedef struct {
    /** From CO_SDOserver_init() */
    CO_CANmodule_t *CANdevTx;
    /** CAN transmit buffer inside CANdevTx for CAN tx message */
    CO_CANtx_t *CANtxBuff;
    /** From CO_SDOserver_init() */
    OD_t *OD;
    /** From CO_SDOserver_init() */
    uint8_t nodeId;
    /* If true, SDO channel is valid */
    bool_t valid;
    /** Internal state of the SDO server */
    volatile CO_SDO_state_t state;
    /** Object dictionary interface for current object. */
    OD_IO_t OD_IO;
    /** Index of the current object in Object Dictionary */
    uint16_t index;
    /** Subindex of the current object in Object Dictionary */
    uint8_t subIndex;
    /** Indicates, if new SDO message received from CAN bus. It is not cleared,
     * until received message is completely processed. */
    volatile void *CANrxNew;
    /** 8 data bytes of the received message */
    uint8_t CANrxData[8];
#if ((CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_OD_DYNAMIC) || defined CO_DOXYGEN
    /** From CO_SDOserver_init() */
    CO_CANmodule_t *CANdevRx;
    /** From CO_SDOserver_init() */
    uint16_t CANdevRxIdx;
    /** From CO_SDOserver_init() */
    uint16_t CANdevTxIdx;
    /** Copy of CANopen COB_ID Client -> Server, meaning of the specific bits:
        - Bit 0...10: 11-bit CAN identifier.
        - Bit 11..30: reserved, must be 0.
        - Bit 31: if 1, SDO client object is not used. */
    uint32_t COB_IDClientToServer;
    /** Copy of CANopen COB_ID Server -> Client, similar as above */
    uint32_t COB_IDServerToClient;
    /** Extension for OD object */
    OD_extension_t OD_1200_extension;
#endif
#if ((CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED) || defined CO_DOXYGEN
    /** Size of data, which will be transferred. It is optionally indicated by
     * client in case of download or by server in case of upload. */
    OD_size_t sizeInd;
    /** Size of data which is actually transferred. */
    OD_size_t sizeTran;
    /** Toggle bit toggled in each segment in segmented transfer */
    uint8_t toggle;
    /** If true, then: data transfer is finished (by download) or read from OD
     * variable is finished (by upload) */
    bool_t finished;
    /** Maximum timeout time between request and response in microseconds */
    uint32_t SDOtimeoutTime_us;
    /** Timeout timer for SDO communication */
    uint32_t timeoutTimer;
    /** Interim data buffer for segmented or block transfer + byte for '\0' */
    uint8_t buf[CO_CONFIG_SDO_SRV_BUFFER_SIZE + 1];
    /** Offset of next free data byte available for write in the buffer. */
    OD_size_t bufOffsetWr;
    /** Offset of first data available for read in the buffer */
    OD_size_t bufOffsetRd;
#endif
#if ((CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK) || defined CO_DOXYGEN
    /** Timeout time for SDO sub-block download, half of #SDOtimeoutTime_us */
    uint32_t block_SDOtimeoutTime_us;
    /** Timeout timer for SDO sub-block download */
    uint32_t block_timeoutTimer;
    /** Sequence number of segment in block, 1..127 */
    uint8_t block_seqno;
    /** Number of segments per block, 1..127 */
    uint8_t block_blksize;
    /** Number of bytes in last segment that do not contain data */
    uint8_t block_noData;
    /** Client CRC support in block transfer */
    bool_t block_crcEnabled;
    /** Calculated CRC checksum */
    uint16_t block_crc;
#endif
#if ((CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_SDOserver_initCallbackPre() or NULL */
    void (*pFunctSignalPre)(void *object);
    /** From CO_SDOserver_initCallbackPre() or NULL */
    void *functSignalObjectPre;
#endif
} CO_SDOserver_t;


/**
 * Initialize SDO object.
 *
 * Function must be called in the communication reset section.
 *
 * @param SDO This object will be initialized.
 * @param OD Object Dictionary.
 * @param OD_1200_SDOsrvPar OD entry for SDO server parameter (0x1200+), can be
 * NULL for default single SDO server and must not be NULL for additional SDO
 * servers. With additional SDO servers it may also have IO extension enabled,
 * to allow dynamic configuration (see also @ref CO_CONFIG_FLAG_OD_DYNAMIC).
 * @param nodeId If this is first SDO channel, then "nodeId" is CANopen Node ID
 * of this device. In all additional channels "nodeId" is ignored.
 * @param SDOtimeoutTime_ms Timeout time for SDO communication in milliseconds.
 * @param CANdevRx CAN device for SDO server reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device for SDO server transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return @ref CO_ReturnError_t CO_ERROR_NO in case of success.
 */
CO_ReturnError_t CO_SDOserver_init(CO_SDOserver_t *SDO,
                                   OD_t *OD,
                                   OD_entry_t *OD_1200_SDOsrvPar,
                                   uint8_t nodeId,
                                   uint16_t SDOtimeoutTime_ms,
                                   CO_CANmodule_t *CANdevRx,
                                   uint16_t CANdevRxIdx,
                                   CO_CANmodule_t *CANdevTx,
                                   uint16_t CANdevTxIdx,
                                   uint32_t *errInfo);


#if ((CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize SDOrx callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_SDOserver_process() function.
 * Callback is called after SDOserver message is received from the CAN bus or
 * when new call without delay is necessary (SDO block transfer is in progress).
 *
 * @param SDO This object.
 * @param object Pointer to object, which will be passed to pFunctSignalPre().
 * Can be NULL
 * @param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_SDOserver_initCallbackPre(CO_SDOserver_t *SDO,
                                  void *object,
                                  void (*pFunctSignalPre)(void *object));
#endif


/**
 * Process SDO communication.
 *
 * Function must be called cyclically.
 *
 * @param SDO This object.
 * @param NMTisPreOrOperational True if #CO_NMT_internalState_t is
 * NMT_PRE_OPERATIONAL or NMT_OPERATIONAL.
 * @param timeDifference_us Time difference from previous function call in
 * [microseconds].
 * @param [out] timerNext_us info to OS - see CO_process().
 *
 * @return #CO_SDO_return_t
 */
CO_SDO_return_t CO_SDOserver_process(CO_SDOserver_t *SDO,
                                     bool_t NMTisPreOrOperational,
                                     uint32_t timeDifference_us,
                                     uint32_t *timerNext_us);

/** @} */ /* CO_SDOserver */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_SDO_SERVER_H */
