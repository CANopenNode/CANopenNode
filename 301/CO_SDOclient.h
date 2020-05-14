/**
 * CANopen Service Data Object - client protocol.
 *
 * @file        CO_SDOclient.h
 * @ingroup     CO_SDOclient
 * @author      Janez Paternoster
 * @author      Matej Severkar
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


#ifndef CO_SDO_CLIENT_H
#define CO_SDO_CLIENT_H

#include "301/CO_driver.h"
#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL) || defined CO_DOXYGEN
#include "301/CO_SDOserver.h"
#endif
#include "301/CO_fifo.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_SDOclient SDO client
 * @ingroup CO_CANopen_301
 * @{
 *
 * CANopen Service Data Object - client protocol (master functionality).
 *
 * @see @ref CO_SDOserver
 */


/**
 * Return values of SDO client functions.
 */
typedef enum {
    /** Data buffer is full, data must be read before next upload cycle begins*/
    CO_SDOcli_uploadDataBufferFull      = 5,
    /** CAN transmit buffer is full. Waiting. */
    CO_SDOcli_transmittBufferFull       = 4,
    /** Block download is in progress. Sending train of messages. */
    CO_SDOcli_blockDownldInProgress     = 3,
    /** Block upload is in progress. Receiving train of messages. Data must not
     * be read in this state. */
    CO_SDOcli_blockUploadInProgress     = 2,
    /** Waiting server response */
    CO_SDOcli_waitingServerResponse     = 1,
    /** Success, end of communication. Uploaded data must be read. */
    CO_SDOcli_ok_communicationEnd       = 0,
    /** Error in arguments */
    CO_SDOcli_wrongArguments            = -2,
    /** Communication ended with client abort */
    CO_SDOcli_endedWithClientAbort      = -9,
    /** Communication ended with server abort */
    CO_SDOcli_endedWithServerAbort      = -10,
} CO_SDOclient_return_t;


/**
 * SDO Client Parameter. The same as record from Object dictionary
 * (index 0x1280+).
 */
typedef struct {
    /** Equal to 3 */
    uint8_t             maxSubIndex;
    /** Communication object identifier for client transmission.
     * Meaning of the specific bits:
        - Bit 0...10: 11-bit CAN identifier.
        - Bit 11..30: reserved, set to 0.
        - Bit 31: if 1, SDO client object is not used. */
    uint32_t            COB_IDClientToServer;
    /** Communication object identifier for message received from server.
     * Meaning of the specific bits:
        - Bit 0...10: 11-bit CAN identifier.
        - Bit 11..30: reserved, set to 0.
        - Bit 31: if 1, SDO client object is not used. */
    uint32_t            COB_IDServerToClient;
    /** Node-ID of the SDO server */
    uint8_t             nodeIDOfTheSDOServer;
} CO_SDOclientPar_t;


/**
 * SDO client object
 */
typedef struct {
#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL) || defined CO_DOXYGEN
    /** From CO_SDOclient_init() */
    CO_SDO_t *SDO;
#endif
    /** From CO_SDOclient_init() */
    CO_SDOclientPar_t *SDOClientPar;
    /** From CO_SDOclient_init() */
    CO_CANmodule_t *CANdevRx;
    /** From CO_SDOclient_init() */
    uint16_t CANdevRxIdx;
    /** From CO_SDOclient_init() */
    CO_CANmodule_t *CANdevTx;
    /** From CO_SDOclient_init() */
    uint16_t CANdevTxIdx;
    /** CAN transmit buffer inside CANdevTx for CAN tx message */
    CO_CANtx_t *CANtxBuff;
    /** Index of current object in Object Dictionary */
    uint16_t index;
    /** Subindex of current object in Object Dictionary */
    uint8_t subIndex;
    /* If true, then data transfer is finished */
    bool_t finished;
    /** Size of data, which will be transferred. It is optionally indicated by
     * client in case of download or by server in case of upload. */
    size_t sizeInd;
    /** Size of data which is actually transferred. */
    size_t sizeTran;
    /** Internal state of the SDO client */
    volatile CO_SDO_state_t state;
    /** Maximum timeout time between request and response in microseconds */
    uint32_t SDOtimeoutTime_us;
    /** Timeout timer for SDO communication */
    uint32_t timeoutTimer;
    /** CO_fifo_t object for data buffer (not pointer) */
    CO_fifo_t bufFifo;
    /** Data buffer of usable size @ref CO_CONFIG_SDO_CLI_BUFFER_SIZE, used
     * inside bufFifo. Must be one byte larger for fifo usage. */
    char buf[CO_CONFIG_SDO_CLI_BUFFER_SIZE + 1];
    /** Indicates, if new SDO message received from CAN bus. It is not cleared,
     * until received message is completely processed. */
    volatile void *CANrxNew;
    /** 8 data bytes of the received message */
    uint8_t CANrxData[8];
    /** Previous value of the COB_IDClientToServer */
    uint32_t COB_IDClientToServerPrev;
    /** Previous value of the COB_IDServerToClient */
    uint32_t COB_IDServerToClientPrev;
#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_SDOclient_initCallbackPre() or NULL */
    void (*pFunctSignal)(void *object);
    /** From CO_SDOclient_initCallbackPre() or NULL */
    void *functSignalObject;
#endif
#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED) || defined CO_DOXYGEN
    /** Toggle bit toggled in each segment in segmented transfer */
    uint8_t toggle;
#endif
#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK) || defined CO_DOXYGEN
    /** Timeout time for SDO sub-block upload, half of #SDOtimeoutTime_us */
    uint32_t block_SDOtimeoutTime_us;
    /** Timeout timer for SDO sub-block upload */
    uint32_t block_timeoutTimer;
    /** Sequence number of segment in block, 1..127 */
    uint8_t block_seqno;
    /** Number of segments per block, 1..127 */
    uint8_t block_blksize;
    /** Number of bytes in last segment that do not contain data */
    uint8_t block_noData;
    /** Server CRC support in block transfer */
    bool_t block_crcEnabled;
    /** Last 7 bytes of data at block download */
    uint8_t block_dataUploadLast[7];
    /** Calculated CRC checksum */
    uint16_t block_crc;
#endif
} CO_SDOclient_t;


/**
 * Initialize SDO client object.
 *
 * Function must be called in the communication reset section.
 *
 * @param SDO_C This object will be initialized.
 * @param SDO SDO server object. It is used in case, if client is accessing
 * object dictionary from its own device. If NULL, it will be ignored.
 * @param SDOClientPar Pointer to _SDO Client Parameter_ record from Object
 * dictionary (index 0x1280+). Will be written.
 * @param CANdevRx CAN device for SDO client reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANdevTx CAN device for SDO client transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_SDOclient_init(CO_SDOclient_t *SDO_C,
                                   void *SDO,
                                   CO_SDOclientPar_t *SDOClientPar,
                                   CO_CANmodule_t *CANdevRx,
                                   uint16_t CANdevRxIdx,
                                   CO_CANmodule_t *CANdevTx,
                                   uint16_t CANdevTxIdx);


#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize SDOclient callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_SDOclientDownload() or CO_SDOclientUpload() function.
 * Callback is called after SDOclient message is received from the CAN bus or
 * when new call without delay is necessary (exchange data with own SDO server
 * or SDO block transfer is in progress).
 *
 * @param SDOclient This object.
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can
 * be NULL.
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_SDOclient_initCallbackPre(CO_SDOclient_t *SDOclient,
                                  void *object,
                                  void (*pFunctSignal)(void *object));
#endif


/**
 * Setup SDO client object.
 *
 * Function must be called before new SDO communication. If previous SDO
 * communication was with the same node, function does not need to be called.
 *
 * @remark If configuring SDO client from network is required, this function
 * should be set as callback for the corresponding SDO client parameter OD
 * entry.
 *
 * @param SDO_C This object.
 * @param COB_IDClientToServer See CO_SDOclientPar_t. If zero, then
 * nodeIDOfTheSDOServer is used with default COB-ID.
 * @param COB_IDServerToClient See CO_SDOclientPar_t. If zero, then
 * nodeIDOfTheSDOServer is used with default COB-ID.
 * @param nodeIDOfTheSDOServer Node-ID of the SDO server. If zero, SDO client
 * object is not used. If it is the same as node-ID of this node, then data will
 * be exchanged with this node (without CAN communication).
 *
 * @return #CO_SDOclient_return_t
 */
CO_SDOclient_return_t CO_SDOclient_setup(CO_SDOclient_t *SDO_C,
                                         uint32_t COB_IDClientToServer,
                                         uint32_t COB_IDServerToClient,
                                         uint8_t nodeIDOfTheSDOServer);


/**
 * Initiate SDO download communication.
 *
 * Function initiates SDO download communication with server specified in
 * CO_SDOclient_init() function. Data will be written to remote node.
 * Function is non-blocking.
 *
 * @param SDO_C This object.
 * @param index Index of object in object dictionary in remote node.
 * @param subIndex Subindex of object in object dictionary in remote node.
 * @param sizeIndicated Optionally indicate size of data to be downloaded.
 * Actual data are written with one or multiple CO_SDOclientDownloadBufWrite()
 * calls.
 * - If sizeIndicated is different than 0, then total number of data
 * written by CO_SDOclientDownloadBufWrite() will be compared against
 * sizeIndicated. Also sizeIndicated info will be passed to the server, which
 * will compare actual data size downloaded. In case of mismatch, SDO abort
 * message will be generated.
 * - If sizeIndicated is 0, then actual data size will not be verified.
 * @param SDOtimeoutTime_ms Timeout time for SDO communication in milliseconds.
 * @param blockEnable Try to initiate block transfer.
 *
 * @return #CO_SDOclient_return_t
 */
CO_SDOclient_return_t CO_SDOclientDownloadInitiate(CO_SDOclient_t *SDO_C,
                                                   uint16_t index,
                                                   uint8_t subIndex,
                                                   size_t sizeIndicated,
                                                   uint16_t SDOtimeoutTime_ms,
                                                   bool_t blockEnable);


/**
 * Initiate SDO download communication - update size.
 *
 * This is optional function, which updates sizeIndicated, if it was not known
 * in the CO_SDOclientDownloadInitiate() function call. This function can be
 * used after CO_SDOclientDownloadBufWrite(), but must be used before
 * CO_SDOclientDownload().
 *
 * @param SDO_C This object.
 * @param sizeIndicated Same as in CO_SDOclientDownloadInitiate().
 */
void CO_SDOclientDownloadInitiateSize(CO_SDOclient_t *SDO_C,
                                      size_t sizeIndicated);


/**
 * Write data into SDO client buffer
 *
 * This function copies data from buf into internal SDO client fifo buffer.
 * Function returns number of bytes successfully copied. If there is not enough
 * space in destination, not all bytes will be copied. Additional data can be
 * copied in next cycles. If there is enough space in destination and
 * sizeIndicated is different than zero, then all data must be written at once.
 *
 * This function is basically a wrapper for CO_fifo_write() function. As
 * alternative, other functions from CO_fifo can be used directly, for example
 * CO_fifo_cpyTok2U8() or similar.
 *
 * @param SDO_C This object.
 * @param buf Buffer which will be copied
 * @param count Number of bytes in buf
 *
 * @return number of bytes actually written.
 */
size_t CO_SDOclientDownloadBufWrite(CO_SDOclient_t *SDO_C,
                                     const char *buf,
                                     size_t count);


/**
 * Process SDO download communication.
 *
 * Function must be called cyclically until it returns <=0. It Proceeds SDO
 * download communication initiated with CO_SDOclientDownloadInitiate().
 * Function is non-blocking.
 *
 * If function returns #CO_SDOcli_blockDownldInProgress and OS has buffer for
 * CAN tx messages, then this function may be called multiple times within own
 * loop. This can speed-up SDO block transfer.
 *
 * @param SDO_C This object.
 * @param timeDifference_us Time difference from previous function call in
 * [microseconds].
 * @param abort If true, SDO client will send abort message from SDOabortCode
 * and transmission will be aborted.
 * @param [out] SDOabortCode In case of error in communication, SDO abort code
 * contains reason of error. Ignored if NULL.
 * @param [out] sizeTransferred Actual size of data transferred. Ignored if NULL
 * @param [out] timerNext_us info to OS - see CO_process(). Ignored if NULL.
 *
 * @return #CO_SDOclient_return_t. If less than 0, then error occurred,
 * SDOabortCode contains reason and state becomes idle. If 0, communication
 * ends successfully and state becomes idle. If greater than 0, then
 * communication is in progress.
 */
CO_SDOclient_return_t CO_SDOclientDownload(CO_SDOclient_t *SDO_C,
                                           uint32_t timeDifference_us,
                                           bool_t abort,
                                           CO_SDO_abortCode_t *SDOabortCode,
                                           size_t *sizeTransferred,
                                           uint32_t *timerNext_us);


/**
 * Initiate SDO upload communication.
 *
 * Function initiates SDO upload communication with server specified in
 * CO_SDOclient_init() function. Data will be read from remote node.
 * Function is non-blocking.
 *
 * @param SDO_C This object.
 * @param index Index of object in object dictionary in remote node.
 * @param subIndex Subindex of object in object dictionary in remote node.
 * @param SDOtimeoutTime_ms Timeout time for SDO communication in milliseconds.
 * @param blockEnable Try to initiate block transfer.
 *
 * @return #CO_SDOclient_return_t
 */
CO_SDOclient_return_t CO_SDOclientUploadInitiate(CO_SDOclient_t *SDO_C,
                                                 uint16_t index,
                                                 uint8_t subIndex,
                                                 uint16_t SDOtimeoutTime_ms,
                                                 bool_t blockEnable);


/**
 * Process SDO upload communication.
 *
 * Function must be called cyclically until it returns <=0. It Proceeds SDO
 * upload communication initiated with CO_SDOclientUploadInitiate().
 * Function is non-blocking.
 *
 * If this function returns #CO_SDOcli_uploadDataBufferFull, then data must be
 * read from fifo buffer to make it empty. This function can then be called
 * once again immediately to speed-up block transfer. Note also, that remaining
 * data must be read after function returns #CO_SDOcli_ok_communicationEnd.
 * Data must not be read, if function returns #CO_SDOcli_blockUploadInProgress.
 *
 * @param SDO_C This object.
 * @param timeDifference_us Time difference from previous function call in
 * [microseconds].
 * @param [out] SDOabortCode In case of error in communication, SDO abort code
 * contains reason of error. Ignored if NULL.
 * @param [out] sizeIndicated If larger than 0, then SDO server has indicated
 * size of data transfer. Ignored if NULL.
 * @param [out] sizeTransferred Actual size of data transferred. Ignored if NULL
 * @param [out] timerNext_us info to OS - see CO_process(). Ignored if NULL.
 *
 * @return #CO_SDOclient_return_t. If less than 0, then error occurred,
 * SDOabortCode contains reason and state becomes idle. If 0, communication
 * ends successfully and state becomes idle. If greater than 0, then
 * communication is in progress.
 */
CO_SDOclient_return_t CO_SDOclientUpload(CO_SDOclient_t *SDO_C,
                                         uint32_t timeDifference_us,
                                         CO_SDO_abortCode_t *SDOabortCode,
                                         size_t *sizeIndicated,
                                         size_t *sizeTransferred,
                                         uint32_t *timerNext_us);


/**
 * Read data from SDO client buffer.
 *
 * This function copies data from internal fifo buffer of SDO client into buf.
 * Function returns number of bytes successfully copied. It can be called in
 * multiple cycles, if data length is large.
 *
 * This function is basically a wrapper for CO_fifo_read() function. As
 * alternative, other functions from CO_fifo can be used directly, for example
 * CO_fifo_readU82a() or similar.
 *
 * @warning This function (or similar) must NOT be called when
 * CO_SDOclientUpload() returns #CO_SDOcli_blockUploadInProgress!
 *
 * @param SDO_C This object.
 * @param buf Buffer into which data will be copied
 * @param count Copy up to count bytes into buffer
 *
 * @return number of bytes actually read.
 */
size_t CO_SDOclientUploadBufRead(CO_SDOclient_t *SDO_C,
                                 char *buf,
                                 size_t count);


/**
 * Close SDO communication temporary.
 *
 * Function must be called after finish of each SDO client communication cycle.
 * It disables reception of SDO client CAN messages. It is necessary, because
 * CO_SDOclient_receive function may otherwise write into undefined SDO buffer.
 *
 * @param SDO_C This object.
 */
void CO_SDOclientClose(CO_SDOclient_t *SDO_C);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
