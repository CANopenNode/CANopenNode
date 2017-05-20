/**
 * CANopen Service Data Object - client protocol.
 *
 * @file        CO_SDOmaster.h
 * @ingroup     CO_SDOmaster
 * @author      Janez Paternoster
 * @author      Matej Severkar
 * @copyright   2004 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
 */


#ifndef CO_SDO_CLIENT_H
#define CO_SDO_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_SDOmaster SDO client
 * @ingroup CO_CANopen
 * @{
 *
 * CANopen Service Data Object - client protocol.
 *
 * @see @ref CO_SDO
 */


/**
 * Return values of SDO client functions.
 */
typedef enum{
    /** Transmit buffer is full. Waiting */
    CO_SDOcli_transmittBufferFull       = 4,
    /** Block download is in progress. Sending train of messages */
    CO_SDOcli_blockDownldInProgress     = 3,
    /** Block upload in progress. Receiving train of messages */
    CO_SDOcli_blockUploadInProgress     = 2,
    /** Waiting server response */
    CO_SDOcli_waitingServerResponse     = 1,
    /** Success, end of communication */
    CO_SDOcli_ok_communicationEnd       = 0,
    /** Error in arguments */
    CO_SDOcli_wrongArguments            = -2,
    /** Communication ended with client abort */
    CO_SDOcli_endedWithClientAbort      = -9,
    /** Communication ended with server abort */
    CO_SDOcli_endedWithServerAbort      = -10,
    /** Communication ended with timeout */
    CO_SDOcli_endedWithTimeout          = -11
}CO_SDOclient_return_t;


/**
 * SDO Client Parameter. The same as record from Object dictionary (index 0x1280+).
 */
typedef struct{
    /** Equal to 3 */
    uint8_t             maxSubIndex;
    /** Communication object identifier for client transmission. Meaning of the specific bits:
        - Bit 0...10: 11-bit CAN identifier.
        - Bit 11..30: reserved, set to 0.
        - Bit 31: if 1, SDO client object is not used. */
    uint32_t            COB_IDClientToServer;
    /** Communication object identifier for message received from server. Meaning of the specific bits:
        - Bit 0...10: 11-bit CAN identifier.
        - Bit 11..30: reserved, set to 0.
        - Bit 31: if 1, SDO client object is not used. */
    uint32_t            COB_IDServerToClient;
    /** Node-ID of the SDO server */
    uint8_t             nodeIDOfTheSDOServer;
}CO_SDOclientPar_t;


/**
 * SDO client object
 */
typedef struct{
    /** From CO_SDOclient_init() */
    CO_SDOclientPar_t  *SDOClientPar;
    /** From CO_SDOclient_init() */
    CO_SDO_t           *SDO;
    /** Internal state of the SDO client */
    uint8_t             state;
    /** Pointer to data buffer supplied by user */
    uint8_t            *buffer;
    /** By download application indicates data size in buffer.
    By upload application indicates buffer size */
    uint32_t            bufferSize;
    /** Offset in buffer of next data segment being read/written */
    uint32_t            bufferOffset;
    /** Acknowledgement */
    uint32_t            bufferOffsetACK;
    /** data length to be uploaded in block transfer */
    uint32_t            dataSize;
    /** Data length transferred in block transfer */
    uint32_t            dataSizeTransfered;
    /** Timeout timer for SDO communication */
    uint16_t            timeoutTimer;
    /** Timeout timer for SDO block transfer */
    uint16_t            timeoutTimerBLOCK;
    /** Index of current object in Object Dictionary */
    uint16_t            index;
    /** Subindex of current object in Object Dictionary */
    uint8_t             subIndex;
    /** From CO_SDOclient_init() */
    CO_CANmodule_t     *CANdevRx;
    /** From CO_SDOclient_init() */
    uint16_t            CANdevRxIdx;
    /** Flag indicates, if new SDO message received from CAN bus.
    It is not cleared, until received message is completely processed. */
    bool_t              CANrxNew;
    /** 8 data bytes of the received message */
    uint8_t             CANrxData[8];
    /** From CO_SDOclient_initCallback() or NULL */
    void              (*pFunctSignal)(void);
    /** From CO_SDOclient_init() */
    CO_CANmodule_t     *CANdevTx;
    /** CAN transmit buffer inside CANdevTx for CAN tx message */
    CO_CANtx_t         *CANtxBuff;
    /** From CO_SDOclient_init() */
    uint16_t            CANdevTxIdx;
    /** Toggle bit toggled with each subsequent in segmented transfer */
    uint8_t             toggle;
    /** Server threshold for switch back to segmented transfer, if data size is small.
    Set in CO_SDOclient_init(). Can be changed by application. 0 Disables switching. */
    uint8_t             pst;
    /** Maximum number of segments in one block. Set in CO_SDOclient_init(). Can
    be changed by application to 2 .. 127. */
    uint8_t             block_size_max;
    /** Last sector number */
    uint8_t             block_seqno;
    /** Block size in current transfer */
    uint8_t             block_blksize;
    /** Number of bytes in last segment that do not contain data */
    uint8_t             block_noData;
    /** Server CRC support in block transfer */
    uint8_t             crcEnabled;
    /** Previous value of the COB_IDClientToServer */
    uint32_t            COB_IDClientToServerPrev;
    /** Previous value of the COB_IDServerToClient */
    uint32_t            COB_IDServerToClientPrev;

}CO_SDOclient_t;


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
CO_ReturnError_t CO_SDOclient_init(
        CO_SDOclient_t         *SDO_C,
        CO_SDO_t               *SDO,
        CO_SDOclientPar_t      *SDOClientPar,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx);


/**
 * Initialize SDOclientRx callback function.
 *
 * Function initializes optional callback function, which is called after new
 * message is received from the CAN bus. Function may wake up external task,
 * which processes mainline CANopen functions.
 *
 * @param SDOclient This object.
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_SDOclient_initCallback(
        CO_SDOclient_t         *SDOclient,
        void                  (*pFunctSignal)(void));


/**
 * Setup SDO client object.
 *
 * Function must be called before new SDO communication. If previous SDO
 * communication was with the same node, function does not need to be called.
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
CO_SDOclient_return_t CO_SDOclient_setup(
        CO_SDOclient_t         *SDO_C,
        uint32_t                COB_IDClientToServer,
        uint32_t                COB_IDServerToClient,
        uint8_t                 nodeIDOfTheSDOServer);


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
 * @param dataTx Pointer to data to be written. Data must be valid until end
 * of communication. Note that data are aligned in little-endian
 * format, because CANopen itself uses little-endian. Take care,
 * when using processors with big-endian.
 * @param dataSize Size of data in dataTx.
 * @param blockEnable Try to initiate block transfer.
 *
 * @return #CO_SDOclient_return_t
 */
CO_SDOclient_return_t CO_SDOclientDownloadInitiate(
        CO_SDOclient_t         *SDO_C,
        uint16_t                index,
        uint8_t                 subIndex,
        uint8_t                *dataTx,
        uint32_t                dataSize,
        uint8_t                 blockEnable);


/**
 * Process SDO download communication.
 *
 * Function must be called cyclically until it returns <=0. It Proceeds SDO
 * download communication initiated with CO_SDOclientDownloadInitiate().
 * Function is non-blocking.
 *
 * @param SDO_C This object.
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 * @param SDOtimeoutTime Timeout time for SDO communication in milliseconds.
 * @param pSDOabortCode Pointer to external variable written by this function
 * in case of error in communication.
 *
 * @return #CO_SDOclient_return_t
 */
CO_SDOclient_return_t CO_SDOclientDownload(
        CO_SDOclient_t         *SDO_C,
        uint16_t                timeDifference_ms,
        uint16_t                SDOtimeoutTime,
        uint32_t               *pSDOabortCode);


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
 * @param dataRx Pointer to data buffer, into which received data will be written.
 * Buffer must be valid until end of communication. Note that data are aligned
 * in little-endian format, because CANopen itself uses
 * little-endian. Take care, when using processors with big-endian.
 * @param dataRxSize Size of dataRx.
 * @param blockEnable Try to initiate block transfer.
 *
 * @return #CO_SDOclient_return_t
 */
CO_SDOclient_return_t CO_SDOclientUploadInitiate(
        CO_SDOclient_t         *SDO_C,
        uint16_t                index,
        uint8_t                 subIndex,
        uint8_t                *dataRx,
        uint32_t                dataRxSize,
        uint8_t                 blockEnable);


/**
 * Process SDO upload communication.
 *
 * Function must be called cyclically until it returns <=0. It Proceeds SDO
 * upload communication initiated with CO_SDOclientUploadInitiate().
 * Function is non-blocking.
 *
 * @param SDO_C This object.
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 * @param SDOtimeoutTime Timeout time for SDO communication in milliseconds.
 * @param pDataSize pointer to external variable, where size of received
 * data will be written.
 * @param pSDOabortCode Pointer to external variable written by this function
 * in case of error in communication.
 *
 * @return #CO_SDOclient_return_t
 */
CO_SDOclient_return_t CO_SDOclientUpload(
        CO_SDOclient_t         *SDO_C,
        uint16_t                timeDifference_ms,
        uint16_t                SDOtimeoutTime,
        uint32_t               *pDataSize,
        uint32_t               *pSDOabortCode);


/**
 * Close SDO communication temporary.
 *
 * Function must be called after finish of each SDO client communication cycle.
 * It disables reception of SDO client CAN messages. It is necessary, because
 * CO_SDOclient_receive function may otherwise write into undefined SDO buffer.
 */
void CO_SDOclientClose(CO_SDOclient_t *SDO_C);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
