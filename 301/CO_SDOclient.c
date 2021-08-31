/*
 * CANopen Service Data Object - client.
 *
 * @file        CO_SDOclient.c
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

#include "301/CO_SDOclient.h"

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE

/* verify configuration */
#if CO_CONFIG_SDO_CLI_BUFFER_SIZE < 7
 #error CO_CONFIG_SDO_CLI_BUFFER_SIZE must be set to 7 or more.
#endif
#if !((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ENABLE)
 #error CO_CONFIG_FIFO_ENABLE must be enabled.
#endif
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
 #if !((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED)
  #error CO_CONFIG_SDO_CLI_SEGMENTED must be enabled.
 #endif
 #if !((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ALT_READ)
  #error CO_CONFIG_FIFO_ALT_READ must be enabled.
 #endif
 #if !((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_CRC16_CCITT)
  #error CO_CONFIG_FIFO_CRC16_CCITT must be enabled.
 #endif
#endif

/* default 'protocol switch threshold' size for block transfer */
#ifndef CO_CONFIG_SDO_CLI_PST
#define CO_CONFIG_SDO_CLI_PST 21
#endif


/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_SDOclient_receive(void *object, void *msg) {
    CO_SDOclient_t *SDO_C = (CO_SDOclient_t*)object;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    /* Ignore messages in idle state and messages with wrong length. Ignore
     * message also if previous message was not processed yet and not abort */
    if (SDO_C->state != CO_SDO_ST_IDLE && DLC == 8U
        && (!CO_FLAG_READ(SDO_C->CANrxNew) || data[0] == 0x80)
    ) {
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
        if (data[0] == 0x80 /* abort from server */
            || (SDO_C->state != CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ
                && SDO_C->state != CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP)
        ) {
#endif
            /* copy data and set 'new message' flag */
            memcpy((void *)&SDO_C->CANrxData[0], (const void *)&data[0], 8);
            CO_FLAG_SET(SDO_C->CANrxNew);
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE
            /* Optional signal to RTOS, which can resume task, which handles
            * SDO client processing. */
            if (SDO_C->pFunctSignal != NULL) {
                SDO_C->pFunctSignal(SDO_C->functSignalObject);
            }
#endif

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
        }
        else if (SDO_C->state == CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ) {
            /* block upload, copy data directly */
            CO_SDO_state_t state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ;
            uint8_t seqno = data[0] & 0x7F;
            SDO_C->timeoutTimer = 0;
            SDO_C->block_timeoutTimer = 0;

            /* verify if sequence number is correct */
            if (seqno <= SDO_C->block_blksize
                && seqno == (SDO_C->block_seqno + 1)
            ) {
                SDO_C->block_seqno = seqno;

                /* is this the last segment? */
                if ((data[0] & 0x80) != 0) {
                    /* copy data to temporary buffer, because we don't know the
                     * number of bytes not containing data */
                    memcpy((void *)&SDO_C->block_dataUploadLast[0],
                           (const void *)&data[1], 7);
                    SDO_C->finished = true;
                    state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP;
                }
                else {
                    /* Copy data. There is always enough space in fifo buffer,
                     * because block_blksize was calculated before */
                    CO_fifo_write(&SDO_C->bufFifo,
                                  &data[1],
                                  7, &SDO_C->block_crc);
                    SDO_C->sizeTran += 7;
                    /* all segments in sub-block has been transferred */
                    if (seqno == SDO_C->block_blksize) {
                        state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP;
                    }
                }
            }
            /* If message is duplicate or sequence didn't start yet, ignore
             * it. Otherwise seqno is wrong, so break sub-block. Data after
             * last good seqno will be re-transmitted. */
            else if (seqno != SDO_C->block_seqno && SDO_C->block_seqno != 0U) {
                state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP;
#ifdef CO_DEBUG_SDO_CLIENT
                char msg[80];
                sprintf(msg,
                        "sub-block, rx WRONG: sequno=%02X, previous=%02X",
                        seqno, SDO_C->block_seqno);
                CO_DEBUG_SDO_CLIENT(msg);
#endif
            }
#ifdef CO_DEBUG_SDO_CLIENT
            else {
                char msg[80];
                sprintf(msg,
                        "sub-block, rx ignored: sequno=%02X, expected=%02X",
                        seqno, SDO_C->block_seqno + 1);
                CO_DEBUG_SDO_CLIENT(msg);
            }
#endif

            /* Is exit from sub-block receive state? */
            if (state != CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ) {
                /* Processing will continue in another thread, so make memory
                 * barrier here with CO_FLAG_CLEAR() call. */
                CO_FLAG_CLEAR(SDO_C->CANrxNew);
                SDO_C->state = state;
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE
                /* Optional signal to RTOS, which can resume task, which handles
                * SDO client processing. */
                if (SDO_C->pFunctSignal != NULL) {
                    SDO_C->pFunctSignal(SDO_C->functSignalObject);
                }
#endif
            }
        }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK */
    }
}


#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_OD_DYNAMIC
/*
 * Custom function for writing OD object _SDO client parameter_
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_1280(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    /* "count" is already verified in *_init() function */
    if (stream == NULL || buf == NULL || countWritten == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    CO_SDOclient_t *SDO_C = (CO_SDOclient_t *)stream->object;

    switch (stream->subIndex) {
        case 0: /* Highest sub-index supported */
            return ODR_READONLY;

        case 1: { /* COB-ID client -> server */
            uint32_t COB_ID = CO_getUint32(buf);
            uint16_t CAN_ID = (uint16_t)(COB_ID & 0x7FF);
            uint16_t CAN_ID_cur = (uint16_t)(SDO_C->COB_IDClientToServer&0x7FF);
            bool_t valid = (COB_ID & 0x80000000) == 0;

            /* SDO client must not be valid when changing COB_ID */
            if ((COB_ID & 0x3FFFF800) != 0
                || (valid && SDO_C->valid && CAN_ID != CAN_ID_cur)
                || (valid && CO_IS_RESTRICTED_CAN_ID(CAN_ID))
            ) {
                return ODR_INVALID_VALUE;
            }
            CO_SDOclient_setup(SDO_C,
                               COB_ID,
                               SDO_C->COB_IDServerToClient,
                               SDO_C->nodeIDOfTheSDOServer);
            break;
        }

        case 2: { /* COB-ID server -> client */
            uint32_t COB_ID = CO_getUint32(buf);
            uint16_t CAN_ID = (uint16_t)(COB_ID & 0x7FF);
            uint16_t CAN_ID_cur = (uint16_t)(SDO_C->COB_IDServerToClient&0x7FF);
            bool_t valid = (COB_ID & 0x80000000) == 0;

            /* SDO client must not be valid when changing COB_ID */
            if ((COB_ID & 0x3FFFF800) != 0
                || (valid && SDO_C->valid && CAN_ID != CAN_ID_cur)
                || (valid && CO_IS_RESTRICTED_CAN_ID(CAN_ID))
            ) {
                return ODR_INVALID_VALUE;
            }
            CO_SDOclient_setup(SDO_C,
                               SDO_C->COB_IDClientToServer,
                               COB_ID,
                               SDO_C->nodeIDOfTheSDOServer);
            break;
        }

        case 3: { /* Node-ID of the SDO server */
            uint8_t nodeId = CO_getUint8(buf);
            if (nodeId > 127) {
                return ODR_INVALID_VALUE;
            }
            SDO_C->nodeIDOfTheSDOServer = nodeId;
            break;
        }

        default:
            return ODR_SUB_NOT_EXIST;
    }

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_OD_DYNAMIC */


/******************************************************************************/
CO_ReturnError_t CO_SDOclient_init(CO_SDOclient_t *SDO_C,
                                   OD_t *OD,
                                   OD_entry_t *OD_1280_SDOcliPar,
                                   uint8_t nodeId,
                                   CO_CANmodule_t *CANdevRx,
                                   uint16_t CANdevRxIdx,
                                   CO_CANmodule_t *CANdevTx,
                                   uint16_t CANdevTxIdx,
                                   uint32_t *errInfo)
{
    /* verify arguments */
    if (SDO_C == NULL || OD_1280_SDOcliPar == NULL
        || OD_getIndex(OD_1280_SDOcliPar) < OD_H1280_SDO_CLIENT_1_PARAM
        || OD_getIndex(OD_1280_SDOcliPar) > (OD_H1280_SDO_CLIENT_1_PARAM + 0x7F)
        || CANdevRx==NULL || CANdevTx==NULL
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL
    SDO_C->OD = OD;
    SDO_C->nodeId = nodeId;
#endif
    SDO_C->CANdevRx = CANdevRx;
    SDO_C->CANdevRxIdx = CANdevRxIdx;
    SDO_C->CANdevTx = CANdevTx;
    SDO_C->CANdevTxIdx = CANdevTxIdx;
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE
    SDO_C->pFunctSignal = NULL;
    SDO_C->functSignalObject = NULL;
#endif

    /* prepare circular fifo buffer */
    CO_fifo_init(&SDO_C->bufFifo, SDO_C->buf,
                 CO_CONFIG_SDO_CLI_BUFFER_SIZE + 1);

    /* Get parameters from Object Dictionary (initial values) */
    uint8_t maxSubIndex, nodeIDOfTheSDOServer;
    uint32_t COB_IDClientToServer, COB_IDServerToClient;
    ODR_t odRet0 = OD_get_u8(OD_1280_SDOcliPar, 0, &maxSubIndex, true);
    ODR_t odRet1 = OD_get_u32(OD_1280_SDOcliPar, 1, &COB_IDClientToServer, true);
    ODR_t odRet2 = OD_get_u32(OD_1280_SDOcliPar, 2, &COB_IDServerToClient, true);
    ODR_t odRet3 = OD_get_u8(OD_1280_SDOcliPar, 3, &nodeIDOfTheSDOServer, true);

    if (odRet0 != ODR_OK || maxSubIndex != 3
        || odRet1 != ODR_OK || odRet2 != ODR_OK || odRet3 != ODR_OK
    ) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1280_SDOcliPar);
        return CO_ERROR_OD_PARAMETERS;
    }

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_OD_DYNAMIC
    SDO_C->OD_1280_extension.object = SDO_C;
    SDO_C->OD_1280_extension.read = OD_readOriginal;
    SDO_C->OD_1280_extension.write = OD_write_1280;
    ODR_t odRetE = OD_extension_init(OD_1280_SDOcliPar,
                                     &SDO_C->OD_1280_extension);
    if (odRetE != ODR_OK) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1280_SDOcliPar);
        return CO_ERROR_OD_PARAMETERS;
    }

    /* set to zero to make sure CO_SDOclient_setup() will reconfigure CAN */
    SDO_C->COB_IDClientToServer = 0;
    SDO_C->COB_IDServerToClient = 0;
#endif

    CO_SDO_return_t cliSetupRet = CO_SDOclient_setup(SDO_C,
                                                     COB_IDClientToServer,
                                                     COB_IDServerToClient,
                                                     nodeIDOfTheSDOServer);

    if (cliSetupRet != CO_SDO_RT_ok_communicationEnd) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return CO_ERROR_NO;
}


#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_SDOclient_initCallbackPre(CO_SDOclient_t *SDOclient,
                                  void *object,
                                  void (*pFunctSignal)(void *object))
{
    if (SDOclient != NULL) {
        SDOclient->functSignalObject = object;
        SDOclient->pFunctSignal = pFunctSignal;
    }
}
#endif

#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL) && defined CO_BIG_ENDIAN
static inline void reverseBytes(void *start, OD_size_t size) {
    uint8_t *lo = (uint8_t *)start;
    uint8_t *hi = (uint8_t *)start + size - 1;
    uint8_t swap;
    while (lo < hi) {
        swap = *lo;
        *lo++ = *hi;
        *hi-- = swap;
    }
}
#endif


/******************************************************************************/
CO_SDO_return_t CO_SDOclient_setup(CO_SDOclient_t *SDO_C,
                                   uint32_t COB_IDClientToServer,
                                   uint32_t COB_IDServerToClient,
                                   uint8_t nodeIDOfTheSDOServer)
{
    /* verify parameters */
    if (SDO_C == NULL) {
        return CO_SDO_RT_wrongArguments;
    }

    /* Configure object variables */
    SDO_C->state = CO_SDO_ST_IDLE;
    CO_FLAG_CLEAR(SDO_C->CANrxNew);
    SDO_C->nodeIDOfTheSDOServer = nodeIDOfTheSDOServer;

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_OD_DYNAMIC
    /* proceed only, if parameters change */
    if (COB_IDClientToServer == SDO_C->COB_IDClientToServer
        && COB_IDServerToClient == SDO_C->COB_IDServerToClient
    ) {
        return CO_SDO_RT_ok_communicationEnd;
    }
    /* store variables */
    SDO_C->COB_IDClientToServer = COB_IDClientToServer;
    SDO_C->COB_IDServerToClient = COB_IDServerToClient;
#endif

    /* verify valid bit */
    uint16_t CanIdC2S = ((COB_IDClientToServer & 0x80000000L) == 0) ?
                        (uint16_t)(COB_IDClientToServer & 0x7FF) : 0;
    uint16_t CanIdS2C = ((COB_IDServerToClient & 0x80000000L) == 0) ?
                        (uint16_t)(COB_IDServerToClient & 0x7FF) : 0;
    if (CanIdC2S != 0 && CanIdS2C != 0) {
        SDO_C->valid = true;
    }
    else {
        CanIdC2S = 0;
        CanIdS2C = 0;
        SDO_C->valid = false;
    }

    /* configure SDO client CAN reception */
    CO_ReturnError_t ret = CO_CANrxBufferInit(
            SDO_C->CANdevRx,        /* CAN device */
            SDO_C->CANdevRxIdx,     /* rx buffer index */
            CanIdS2C,               /* CAN identifier */
            0x7FF,                  /* mask */
            0,                      /* rtr */
            (void*)SDO_C,           /* object passed to receive function */
            CO_SDOclient_receive);  /* this function will process rx msg */

    /* configure SDO client CAN transmission */
    SDO_C->CANtxBuff = CO_CANtxBufferInit(
            SDO_C->CANdevTx,        /* CAN device */
            SDO_C->CANdevTxIdx,     /* index of buffer inside CAN module */
            CanIdC2S,               /* CAN identifier */
            0,                      /* rtr */
            8,                      /* number of data bytes */
            0);                     /* synchronous message flag bit */


    if (ret != CO_ERROR_NO || SDO_C->CANtxBuff == NULL) {
        SDO_C->valid = false;
        return CO_SDO_RT_wrongArguments;
    }

    return CO_SDO_RT_ok_communicationEnd;
}


/******************************************************************************
 * DOWNLOAD                                                                   *
 ******************************************************************************/
CO_SDO_return_t CO_SDOclientDownloadInitiate(CO_SDOclient_t *SDO_C,
                                             uint16_t index,
                                             uint8_t subIndex,
                                             size_t sizeIndicated,
                                             uint16_t SDOtimeoutTime_ms,
                                             bool_t blockEnable)
{
    /* verify parameters */
    if (SDO_C == NULL || !SDO_C->valid) {
        return CO_SDO_RT_wrongArguments;
    }

    /* save parameters */
    SDO_C->index = index;
    SDO_C->subIndex = subIndex;
    SDO_C->sizeInd = sizeIndicated;
    SDO_C->sizeTran = 0;
    SDO_C->finished = false;
    SDO_C->SDOtimeoutTime_us = (uint32_t)SDOtimeoutTime_ms * 1000;
    SDO_C->timeoutTimer = 0;
    CO_fifo_reset(&SDO_C->bufFifo);

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL
    /* if node-ID of the SDO server is the same as node-ID of this node, then
     * transfer data within this node */
    if (SDO_C->OD != NULL && SDO_C->nodeId != 0
        && SDO_C->nodeIDOfTheSDOServer == SDO_C->nodeId
    ) {
        SDO_C->OD_IO.write = NULL;
        SDO_C->state = CO_SDO_ST_DOWNLOAD_LOCAL_TRANSFER;
    }
    else
#endif
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
    if (blockEnable && (sizeIndicated == 0 ||
                        sizeIndicated > CO_CONFIG_SDO_CLI_PST)
    ) {
        SDO_C->state = CO_SDO_ST_DOWNLOAD_BLK_INITIATE_REQ;
    }
    else
#endif
    {
        SDO_C->state = CO_SDO_ST_DOWNLOAD_INITIATE_REQ;
    }

    CO_FLAG_CLEAR(SDO_C->CANrxNew);

    return CO_SDO_RT_ok_communicationEnd;
}


void CO_SDOclientDownloadInitiateSize(CO_SDOclient_t *SDO_C,
                                      size_t sizeIndicated)
{
    if (SDO_C != NULL) {
        SDO_C->sizeInd = sizeIndicated;
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
        if (SDO_C->state == CO_SDO_ST_DOWNLOAD_BLK_INITIATE_REQ
            && sizeIndicated > 0 && sizeIndicated <= CO_CONFIG_SDO_CLI_PST
        ) {
            SDO_C->state = CO_SDO_ST_DOWNLOAD_INITIATE_REQ;
        }
#endif
    }
}


/******************************************************************************/
size_t CO_SDOclientDownloadBufWrite(CO_SDOclient_t *SDO_C,
                                    const uint8_t *buf,
                                    size_t count)
{
    size_t ret = 0;
    if (SDO_C != NULL && buf != NULL) {
        ret = CO_fifo_write(&SDO_C->bufFifo, buf, count, NULL);
    }
    return ret;
}


/******************************************************************************/
CO_SDO_return_t CO_SDOclientDownload(CO_SDOclient_t *SDO_C,
                                     uint32_t timeDifference_us,
                                     bool_t abort,
                                     bool_t bufferPartial,
                                     CO_SDO_abortCode_t *SDOabortCode,
                                     size_t *sizeTransferred,
                                     uint32_t *timerNext_us)
{
    (void)timerNext_us; (void) bufferPartial; /* may be unused */

    CO_SDO_return_t ret = CO_SDO_RT_waitingResponse;
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;

    if (SDO_C == NULL || !SDO_C->valid) {
        abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
        ret = CO_SDO_RT_wrongArguments;
    }
    else if (SDO_C->state == CO_SDO_ST_IDLE) {
        ret = CO_SDO_RT_ok_communicationEnd;
    }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL
    /* Transfer data locally **************************************************/
    else if (SDO_C->state == CO_SDO_ST_DOWNLOAD_LOCAL_TRANSFER && !abort) {
        /* search object dictionary in first pass */
        if (SDO_C->OD_IO.write == NULL) {
            ODR_t odRet;

            odRet = OD_getSub(OD_find(SDO_C->OD, SDO_C->index), SDO_C->subIndex,
                              &SDO_C->OD_IO, false);

            if (odRet != ODR_OK) {
                abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                ret = CO_SDO_RT_endedWithClientAbort;
            }
            else if ((SDO_C->OD_IO.stream.attribute & ODA_SDO_RW) == 0) {
                abortCode = CO_SDO_AB_UNSUPPORTED_ACCESS;
                ret = CO_SDO_RT_endedWithClientAbort;
            }
            else if ((SDO_C->OD_IO.stream.attribute & ODA_SDO_W) == 0) {
                abortCode = CO_SDO_AB_READONLY;
                ret = CO_SDO_RT_endedWithClientAbort;
            }
            else if (SDO_C->OD_IO.write == NULL) {
                abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                ret = CO_SDO_RT_endedWithClientAbort;
            }
        }
        /* write data, in several passes if necessary */
        if (SDO_C->OD_IO.write != NULL) {
            size_t count = CO_fifo_getOccupied(&SDO_C->bufFifo);
            uint8_t buf[CO_CONFIG_SDO_CLI_BUFFER_SIZE + 2];

            CO_fifo_read(&SDO_C->bufFifo, buf, count, NULL);
            SDO_C->sizeTran += count;

            /* error: no data */
            if (count == 0) {
                abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                ret = CO_SDO_RT_endedWithClientAbort;
            }
            /* verify if sizeTran is too large */
            else if (SDO_C->sizeInd > 0 && SDO_C->sizeTran > SDO_C->sizeInd) {
                SDO_C->sizeTran -= count;
                abortCode = CO_SDO_AB_DATA_LONG;
                ret = CO_SDO_RT_endedWithClientAbort;
            }
            /* Verify sizeTran is too small in last segment of data */
            else if (!bufferPartial
                     && SDO_C->sizeInd > 0 && SDO_C->sizeTran < SDO_C->sizeInd
            ) {
                abortCode = CO_SDO_AB_DATA_SHORT;
                ret = CO_SDO_RT_endedWithClientAbort;
            }
            /* is the last segment of data? */
            else if (!bufferPartial) {
#ifdef CO_BIG_ENDIAN
                /* swap int16_t .. uint64_t data if necessary */
                if ((SDO_C->OD_IO.stream.attribute & ODA_MB) != 0) {
                    reverseBytes(buf, count);
                }
#endif
                OD_size_t sizeInOd = SDO_C->OD_IO.stream.dataLength;

                /* If dataType is string, then size of data downloaded may be
                 * shorter than size of OD data buffer. If so, add two zero
                 * bytes to terminate (unicode) string. Shorten also OD data
                 * size, (temporary, send info about EOF into OD_IO.write) */
                if ((SDO_C->OD_IO.stream.attribute & ODA_STR) != 0
                    && (sizeInOd == 0 || SDO_C->sizeTran < sizeInOd)
                ) {
                    buf[count++] = 0;
                    SDO_C->sizeTran++;
                    if (sizeInOd == 0 || sizeInOd > SDO_C->sizeTran) {
                        buf[count++] = 0;
                        SDO_C->sizeTran++;
                    }
                    SDO_C->OD_IO.stream.dataLength = (OD_size_t)SDO_C->sizeTran;
                }
                /* Indicate OD data size, if necessary. Used for EOF check. */
                else if (sizeInOd == 0) {
                    SDO_C->OD_IO.stream.dataLength = (OD_size_t)SDO_C->sizeTran;
                }
                /* Verify if size of data downloaded matches data size in OD. */
                else if (SDO_C->sizeTran != sizeInOd) {
                    abortCode = (SDO_C->sizeTran > sizeInOd) ?
                                CO_SDO_AB_DATA_LONG : CO_SDO_AB_DATA_SHORT;
                    ret = CO_SDO_RT_endedWithClientAbort;
                }
            }
            if (abortCode == CO_SDO_AB_NONE) {
                OD_size_t countWritten = 0;
                bool_t lock = OD_mappable(&SDO_C->OD_IO.stream);

                /* write data to Object Dictionary */
                if (lock) { CO_LOCK_OD(SDO_C->CANdevTx); }
                ODR_t odRet = SDO_C->OD_IO.write(&SDO_C->OD_IO.stream, buf,
                                                 (OD_size_t)count, &countWritten);
                if (lock) { CO_UNLOCK_OD(SDO_C->CANdevTx); }

                /* verify for errors in write */
                if (odRet != ODR_OK && odRet != ODR_PARTIAL) {
                    abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                    ret = CO_SDO_RT_endedWithServerAbort;
                }
                /* error if OD variable was written completely,
                 * but SDO download still has data */
                else if (bufferPartial && odRet == ODR_OK) {
                    abortCode = CO_SDO_AB_DATA_LONG;
                    ret = CO_SDO_RT_endedWithClientAbort;
                }
                /* is end of transfer? */
                else if (!bufferPartial) {
                    /* error if OD variable was not written completely, but SDO
                     * download finished */
                    if (odRet == ODR_PARTIAL) {
                        abortCode = CO_SDO_AB_DATA_SHORT;
                        ret = CO_SDO_RT_endedWithClientAbort;
                    }
                    /* data transfer finished successfully */
                    else {
                        ret = CO_SDO_RT_ok_communicationEnd;
                    }
                }
                else {
                    ret = CO_SDO_RT_waitingLocalTransfer;
                }
            }
        }

        if (ret != CO_SDO_RT_waitingLocalTransfer) {
            SDO_C->state = CO_SDO_ST_IDLE;
        }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_TIMERNEXT
        /* Inform OS to call this function again without delay. */
        else if (timerNext_us != NULL) {
            *timerNext_us = 0;
        }
#endif
    }
#endif /* CO_CONFIG_SDO_CLI_LOCAL */
    /* CAN data received ******************************************************/
    else if (CO_FLAG_READ(SDO_C->CANrxNew)) {
        /* is SDO abort */
        if (SDO_C->CANrxData[0] == 0x80) {
            uint32_t code;
            memcpy(&code, &SDO_C->CANrxData[4], sizeof(code));
            abortCode = (CO_SDO_abortCode_t)CO_SWAP_32(code);
            SDO_C->state = CO_SDO_ST_IDLE;
            ret = CO_SDO_RT_endedWithServerAbort;
        }
        else if (abort) {
            abortCode = (SDOabortCode != NULL)
                      ? *SDOabortCode : CO_SDO_AB_DEVICE_INCOMPAT;
            SDO_C->state = CO_SDO_ST_ABORT;
        }
        else switch (SDO_C->state) {
            case CO_SDO_ST_DOWNLOAD_INITIATE_RSP: {
                if (SDO_C->CANrxData[0] == 0x60) {
                    /* verify index and subindex */
                    uint16_t index;
                    uint8_t subindex;
                    index = ((uint16_t) SDO_C->CANrxData[2]) << 8;
                    index |= SDO_C->CANrxData[1];
                    subindex = SDO_C->CANrxData[3];
                    if (index != SDO_C->index || subindex != SDO_C->subIndex) {
                        abortCode = CO_SDO_AB_PRAM_INCOMPAT;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED
                    if (SDO_C->finished) {
                        /* expedited transfer */
                        SDO_C->state = CO_SDO_ST_IDLE;
                        ret = CO_SDO_RT_ok_communicationEnd;
                    }
                    else {
                        /* segmented transfer - prepare the first segment */
                        SDO_C->toggle = 0x00;
                        SDO_C->state = CO_SDO_ST_DOWNLOAD_SEGMENT_REQ;
                    }
#else
                    /* expedited transfer */
                    SDO_C->state = CO_SDO_ST_IDLE;
                    ret = CO_SDO_RT_ok_communicationEnd;
#endif
                }
                else {
                    abortCode = CO_SDO_AB_CMD;
                    SDO_C->state = CO_SDO_ST_ABORT;
                }
                break;
            }

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED
            case CO_SDO_ST_DOWNLOAD_SEGMENT_RSP: {
                if ((SDO_C->CANrxData[0] & 0xEF) == 0x20) {
                    /* verify and alternate toggle bit */
                    uint8_t toggle = SDO_C->CANrxData[0] & 0x10;
                    if (toggle != SDO_C->toggle) {
                        abortCode = CO_SDO_AB_TOGGLE_BIT;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }
                    SDO_C->toggle = (toggle == 0x00) ? 0x10 : 0x00;

                    /* is end of transfer? */
                    if (SDO_C->finished) {
                        SDO_C->state = CO_SDO_ST_IDLE;
                        ret = CO_SDO_RT_ok_communicationEnd;
                    }
                    else {
                        SDO_C->state = CO_SDO_ST_DOWNLOAD_SEGMENT_REQ;
                    }
                }
                else {
                    abortCode = CO_SDO_AB_CMD;
                    SDO_C->state = CO_SDO_ST_ABORT;
                }
                break;
            }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED */

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
            case CO_SDO_ST_DOWNLOAD_BLK_INITIATE_RSP: {
                if ((SDO_C->CANrxData[0] & 0xFB) == 0xA0) {
                    /* verify index and subindex */
                    uint16_t index;
                    uint8_t subindex;
                    index = ((uint16_t) SDO_C->CANrxData[2]) << 8;
                    index |= SDO_C->CANrxData[1];
                    subindex = SDO_C->CANrxData[3];
                    if (index != SDO_C->index || subindex != SDO_C->subIndex) {
                        abortCode = CO_SDO_AB_PRAM_INCOMPAT;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }

                    SDO_C->block_crc = 0;
                    SDO_C->block_blksize = SDO_C->CANrxData[4];
                    if (SDO_C->block_blksize < 1 || SDO_C->block_blksize > 127)
                        SDO_C->block_blksize = 127;
                    SDO_C->block_seqno = 0;
                    CO_fifo_altBegin(&SDO_C->bufFifo, 0);
                    SDO_C->state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ;
                }
                else {
                    abortCode = CO_SDO_AB_CMD;
                    SDO_C->state = CO_SDO_ST_ABORT;
                }
                break;
            }

            case CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ:
            case CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP: {
                if (SDO_C->CANrxData[0] == 0xA2) {
                    /* check number of segments */
                    if (SDO_C->CANrxData[1] < SDO_C->block_seqno) {
                        /* NOT all segments transferred successfully.
                         * Re-transmit data after erroneous segment. */
                        CO_fifo_altBegin(&SDO_C->bufFifo,
                                         (size_t)SDO_C->CANrxData[1] * 7);
                        SDO_C->finished = false;
                    }
                    else if (SDO_C->CANrxData[1] > SDO_C->block_seqno) {
                        /* something strange from server, break transmission */
                        abortCode = CO_SDO_AB_CMD;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }

                    /* confirm successfully transmitted data */
                    CO_fifo_altFinish(&SDO_C->bufFifo, &SDO_C->block_crc);

                    if (SDO_C->finished) {
                        SDO_C->state = CO_SDO_ST_DOWNLOAD_BLK_END_REQ;
                    } else {
                        SDO_C->block_blksize = SDO_C->CANrxData[2];
                        SDO_C->block_seqno = 0;
                        CO_fifo_altBegin(&SDO_C->bufFifo, 0);
                        SDO_C->state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ;
                    }
                }
                else {
                    abortCode = CO_SDO_AB_CMD;
                    SDO_C->state = CO_SDO_ST_ABORT;
                }
                break;
            }

            case CO_SDO_ST_DOWNLOAD_BLK_END_RSP: {
                if (SDO_C->CANrxData[0] == 0xA1) {
                    /*  SDO block download successfully transferred */
                    SDO_C->state = CO_SDO_ST_IDLE;
                    ret = CO_SDO_RT_ok_communicationEnd;
                }
                else {
                    abortCode = CO_SDO_AB_CMD;
                    SDO_C->state = CO_SDO_ST_ABORT;
                }
                break;
            }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK */

            default: {
                abortCode = CO_SDO_AB_CMD;
                SDO_C->state = CO_SDO_ST_ABORT;
                break;
            }
        }
        SDO_C->timeoutTimer = 0;
        timeDifference_us = 0;
        CO_FLAG_CLEAR(SDO_C->CANrxNew);
    }
    else if (abort) {
        abortCode = (SDOabortCode != NULL)
                  ? *SDOabortCode : CO_SDO_AB_DEVICE_INCOMPAT;
        SDO_C->state = CO_SDO_ST_ABORT;
    }

    /* Timeout timers and transmit bufferFull flag ****************************/
    if (ret == CO_SDO_RT_waitingResponse) {
        if (SDO_C->timeoutTimer < SDO_C->SDOtimeoutTime_us) {
            SDO_C->timeoutTimer += timeDifference_us;
        }
        if (SDO_C->timeoutTimer >= SDO_C->SDOtimeoutTime_us) {
            abortCode = CO_SDO_AB_TIMEOUT;
            SDO_C->state = CO_SDO_ST_ABORT;
        }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_TIMERNEXT
        else if (timerNext_us != NULL) {
            /* check again after timeout time elapsed */
            uint32_t diff = SDO_C->SDOtimeoutTime_us - SDO_C->timeoutTimer;
            if (*timerNext_us > diff) {
                *timerNext_us = diff;
            }
        }
#endif
        if (SDO_C->CANtxBuff->bufferFull) {
            ret = CO_SDO_RT_transmittBufferFull;
        }
    }

    /* Transmit CAN data ******************************************************/
    if (ret == CO_SDO_RT_waitingResponse) {
        size_t count;
        memset((void *)&SDO_C->CANtxBuff->data[0], 0, 8);

        switch (SDO_C->state) {
        case CO_SDO_ST_DOWNLOAD_INITIATE_REQ: {
            SDO_C->CANtxBuff->data[0] = 0x20;
            SDO_C->CANtxBuff->data[1] = (uint8_t)SDO_C->index;
            SDO_C->CANtxBuff->data[2] = (uint8_t)(SDO_C->index >> 8);
            SDO_C->CANtxBuff->data[3] = SDO_C->subIndex;

            /* get count of data bytes to transfer */
            count = CO_fifo_getOccupied(&SDO_C->bufFifo);

            /* is expedited transfer, <= 4bytes of data */
            if ((SDO_C->sizeInd == 0 && count <= 4)
                || (SDO_C->sizeInd > 0 && SDO_C->sizeInd <= 4)
            ) {
                SDO_C->CANtxBuff->data[0] |= 0x02;

                /* verify length, indicate data size */
                if (count == 0 || (SDO_C->sizeInd > 0 &&
                                   SDO_C->sizeInd != count)
                ) {
                    SDO_C->state = CO_SDO_ST_IDLE;
                    abortCode = CO_SDO_AB_TYPE_MISMATCH;
                    ret = CO_SDO_RT_endedWithClientAbort;
                    break;
                }
                if (SDO_C->sizeInd > 0) {
                    SDO_C->CANtxBuff->data[0] |= 0x01 | ((4 - count) << 2);
                }

                /* copy data */
                CO_fifo_read(&SDO_C->bufFifo,
                             &SDO_C->CANtxBuff->data[4], count, NULL);
                SDO_C->sizeTran = count;
                SDO_C->finished = true;
            }
            else {
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED
                /* segmented transfer, indicate data size */
                if (SDO_C->sizeInd > 0) {
                    uint32_t size = CO_SWAP_32((uint32_t)SDO_C->sizeInd);
                    SDO_C->CANtxBuff->data[0] |= 0x01;
                    memcpy(&SDO_C->CANtxBuff->data[4], &size, sizeof(size));
                }
#else
                SDO_C->state = CO_SDO_ST_IDLE;
                abortCode = CO_SDO_AB_UNSUPPORTED_ACCESS;
                ret = CO_SDO_RT_endedWithClientAbort;
                break;
#endif
            }

            /* reset timeout timer and send message */
            SDO_C->timeoutTimer = 0;
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_DOWNLOAD_INITIATE_RSP;
            break;
        }

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED
        case CO_SDO_ST_DOWNLOAD_SEGMENT_REQ: {
            /* fill data bytes */
            count = CO_fifo_read(&SDO_C->bufFifo,
                                 &SDO_C->CANtxBuff->data[1],
                                 7, NULL);

            /* verify if sizeTran is too large */
            SDO_C->sizeTran += count;
            if (SDO_C->sizeInd > 0 && SDO_C->sizeTran > SDO_C->sizeInd) {
                SDO_C->sizeTran -= count;
                abortCode = CO_SDO_AB_DATA_LONG;
                SDO_C->state = CO_SDO_ST_ABORT;
                break;
            }

            /* SDO command specifier */
            SDO_C->CANtxBuff->data[0] = (uint8_t)(SDO_C->toggle | ((7 - count) << 1));

            /* is end of transfer? Verify also sizeTran */
            if (CO_fifo_getOccupied(&SDO_C->bufFifo) == 0 && !bufferPartial) {
                if (SDO_C->sizeInd > 0 && SDO_C->sizeTran < SDO_C->sizeInd) {
                    abortCode = CO_SDO_AB_DATA_SHORT;
                    SDO_C->state = CO_SDO_ST_ABORT;
                    break;
                }
                SDO_C->CANtxBuff->data[0] |= 0x01;
                SDO_C->finished = true;
            }

            /* reset timeout timer and send message */
            SDO_C->timeoutTimer = 0;
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_DOWNLOAD_SEGMENT_RSP;
            break;
        }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED */

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
        case CO_SDO_ST_DOWNLOAD_BLK_INITIATE_REQ: {
            SDO_C->CANtxBuff->data[0] = 0xC4;
            SDO_C->CANtxBuff->data[1] = (uint8_t)SDO_C->index;
            SDO_C->CANtxBuff->data[2] = (uint8_t)(SDO_C->index >> 8);
            SDO_C->CANtxBuff->data[3] = SDO_C->subIndex;

            /* indicate data size */
            if (SDO_C->sizeInd > 0) {
                uint32_t size = CO_SWAP_32((uint32_t)SDO_C->sizeInd);
                SDO_C->CANtxBuff->data[0] |= 0x02;
                memcpy(&SDO_C->CANtxBuff->data[4], &size, sizeof(size));
            }

            /* reset timeout timer and send message */
            SDO_C->timeoutTimer = 0;
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_DOWNLOAD_BLK_INITIATE_RSP;
            break;
        }

        case CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ: {
            if (CO_fifo_altGetOccupied(&SDO_C->bufFifo) < 7 && bufferPartial) {
                /* wait until data are refilled */
                break;
            }
            SDO_C->CANtxBuff->data[0] = ++SDO_C->block_seqno;

            /* get up to 7 data bytes */
            count = CO_fifo_altRead(&SDO_C->bufFifo,
                                    &SDO_C->CANtxBuff->data[1], 7);
            SDO_C->block_noData = (uint8_t)(7 - count);

            /* verify if sizeTran is too large */
            SDO_C->sizeTran += count;
            if (SDO_C->sizeInd > 0 && SDO_C->sizeTran > SDO_C->sizeInd) {
                SDO_C->sizeTran -= count;
                abortCode = CO_SDO_AB_DATA_LONG;
                SDO_C->state = CO_SDO_ST_ABORT;
                break;
            }

            /* is end of transfer? Verify also sizeTran */
            if (CO_fifo_altGetOccupied(&SDO_C->bufFifo) == 0 && !bufferPartial){
                if (SDO_C->sizeInd > 0 && SDO_C->sizeTran < SDO_C->sizeInd) {
                    abortCode = CO_SDO_AB_DATA_SHORT;
                    SDO_C->state = CO_SDO_ST_ABORT;
                    break;
                }
                SDO_C->CANtxBuff->data[0] |= 0x80;
                SDO_C->finished = true;
                SDO_C->state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP;
            }
            /* are all segments in current block transferred? */
            else if (SDO_C->block_seqno >= SDO_C->block_blksize) {
                SDO_C->state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP;
            }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_TIMERNEXT
            else {
                /* Inform OS to call this function again without delay. */
                if (timerNext_us != NULL) {
                    *timerNext_us = 0;
                }
            }
#endif
            /* reset timeout timer and send message */
            SDO_C->timeoutTimer = 0;
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            break;
        }

        case CO_SDO_ST_DOWNLOAD_BLK_END_REQ: {
            SDO_C->CANtxBuff->data[0] = 0xC1 | (SDO_C->block_noData << 2);
            SDO_C->CANtxBuff->data[1] = (uint8_t) SDO_C->block_crc;
            SDO_C->CANtxBuff->data[2] = (uint8_t) (SDO_C->block_crc >> 8);

            /* reset timeout timer and send message */
            SDO_C->timeoutTimer = 0;
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_DOWNLOAD_BLK_END_RSP;
            break;
        }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK */

        default: {
            break;
        }
        }
    }

    if (ret == CO_SDO_RT_waitingResponse) {
        if (SDO_C->state == CO_SDO_ST_ABORT) {
            uint32_t code = CO_SWAP_32((uint32_t)abortCode);
            /* Send SDO abort message */
            SDO_C->CANtxBuff->data[0] = 0x80;
            SDO_C->CANtxBuff->data[1] = (uint8_t)SDO_C->index;
            SDO_C->CANtxBuff->data[2] = (uint8_t)(SDO_C->index >> 8);
            SDO_C->CANtxBuff->data[3] = SDO_C->subIndex;

            memcpy(&SDO_C->CANtxBuff->data[4], &code, sizeof(code));
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_IDLE;
            ret = CO_SDO_RT_endedWithClientAbort;
        }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
        else if (SDO_C->state == CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ) {
            ret = CO_SDO_RT_blockDownldInProgress;
        }
#endif
    }

    if (sizeTransferred != NULL) {
        *sizeTransferred = SDO_C->sizeTran;
    }
    if (SDOabortCode != NULL) {
        *SDOabortCode = abortCode;
    }

    return ret;
}


/******************************************************************************
 * UPLOAD                                                                     *
 ******************************************************************************/
CO_SDO_return_t CO_SDOclientUploadInitiate(CO_SDOclient_t *SDO_C,
                                           uint16_t index,
                                           uint8_t subIndex,
                                           uint16_t SDOtimeoutTime_ms,
                                           bool_t blockEnable)
{
    /* verify parameters */
    if (SDO_C == NULL || !SDO_C->valid) {
        return CO_SDO_RT_wrongArguments;
    }

    /* save parameters */
    SDO_C->index = index;
    SDO_C->subIndex = subIndex;
    SDO_C->sizeInd = 0;
    SDO_C->sizeTran = 0;
    SDO_C->finished = false;
    CO_fifo_reset(&SDO_C->bufFifo);
    SDO_C->SDOtimeoutTime_us = (uint32_t)SDOtimeoutTime_ms * 1000;
    SDO_C->timeoutTimer = 0;
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
    SDO_C->block_SDOtimeoutTime_us = (uint32_t)SDOtimeoutTime_ms * 700;
#endif

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL
    /* if node-ID of the SDO server is the same as node-ID of this node, then
     * transfer data within this node */
    if (SDO_C->OD != NULL && SDO_C->nodeId != 0
        && SDO_C->nodeIDOfTheSDOServer == SDO_C->nodeId
    ) {
        SDO_C->OD_IO.read = NULL;
        SDO_C->state = CO_SDO_ST_UPLOAD_LOCAL_TRANSFER;
    }
    else
#endif
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
    if (blockEnable) {
        SDO_C->state = CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ;
    }
    else
#endif
    {
        SDO_C->state = CO_SDO_ST_UPLOAD_INITIATE_REQ;
    }

    CO_FLAG_CLEAR(SDO_C->CANrxNew);

    return CO_SDO_RT_ok_communicationEnd;
}


/******************************************************************************/
CO_SDO_return_t CO_SDOclientUpload(CO_SDOclient_t *SDO_C,
                                   uint32_t timeDifference_us,
                                   bool_t abort,
                                   CO_SDO_abortCode_t *SDOabortCode,
                                   size_t *sizeIndicated,
                                   size_t *sizeTransferred,
                                   uint32_t *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    CO_SDO_return_t ret = CO_SDO_RT_waitingResponse;
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;

    if (SDO_C == NULL || !SDO_C->valid) {
        abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
        ret = CO_SDO_RT_wrongArguments;
    }
    else if (SDO_C->state == CO_SDO_ST_IDLE) {
        ret = CO_SDO_RT_ok_communicationEnd;
    }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_LOCAL
    /* Transfer data locally **************************************************/
    else if (SDO_C->state == CO_SDO_ST_UPLOAD_LOCAL_TRANSFER && !abort) {
        /* search object dictionary in first pass */
        if (SDO_C->OD_IO.read == NULL) {
            ODR_t odRet;

            odRet = OD_getSub(OD_find(SDO_C->OD, SDO_C->index), SDO_C->subIndex,
                              &SDO_C->OD_IO, false);

            if (odRet != ODR_OK) {
                abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                ret = CO_SDO_RT_endedWithClientAbort;
            }
            else if ((SDO_C->OD_IO.stream.attribute & ODA_SDO_RW) == 0) {
                abortCode = CO_SDO_AB_UNSUPPORTED_ACCESS;
                ret = CO_SDO_RT_endedWithClientAbort;
            }
            else if ((SDO_C->OD_IO.stream.attribute & ODA_SDO_R) == 0) {
                abortCode = CO_SDO_AB_WRITEONLY;
                ret = CO_SDO_RT_endedWithClientAbort;
            }
            else if (SDO_C->OD_IO.read == NULL) {
                abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                ret = CO_SDO_RT_endedWithClientAbort;
            }
        }

        size_t countFifo = CO_fifo_getSpace(&SDO_C->bufFifo);

        /* skip copying if buffer full */
        if (countFifo == 0) {
            ret = CO_SDO_RT_uploadDataBufferFull;
        }
        /* read data, in several passes if necessary */
        else if (SDO_C->OD_IO.read != NULL) {
            /* Get size of data in Object Dictionary. If size is not indicated
             * use maximum SDO client buffer size. Prepare temp buffer. */
            OD_size_t countData = SDO_C->OD_IO.stream.dataLength;
            OD_size_t countBuf = (countData > 0 && countData <= countFifo)
                                 ? countData : (OD_size_t)countFifo;
            OD_size_t countRd = 0;
            uint8_t buf[CO_CONFIG_SDO_CLI_BUFFER_SIZE + 1];
            bool_t lock = OD_mappable(&SDO_C->OD_IO.stream);

            /* load data from OD variable into the buffer */
            if (lock) { CO_LOCK_OD(SDO_C->CANdevTx); }
            ODR_t odRet = SDO_C->OD_IO.read(&SDO_C->OD_IO.stream,
                                            buf, countBuf, &countRd);
            if (lock) { CO_UNLOCK_OD(SDO_C->CANdevTx); }

            if (odRet != ODR_OK && odRet != ODR_PARTIAL) {
                abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                ret = CO_SDO_RT_endedWithServerAbort;
            }
            else {
                /* if data is string, send only data up to null termination */
                if (countRd > 0
                    && (SDO_C->OD_IO.stream.attribute & ODA_STR) != 0
                ) {
                    buf[countRd] = 0; /* (buf is one byte larger) */
                    OD_size_t countStr = (OD_size_t)strlen((char *)buf);
                    if (countStr == 0) countStr = 1; /* no zero length */
                    if (countStr < countRd) {
                        /* string terminator found, finish read, shorten data */
                        countRd = countStr;
                        odRet = ODR_OK;
                        SDO_C->OD_IO.stream.dataLength =
                            (OD_size_t)SDO_C->sizeTran + countRd;
                    }
                }

                CO_fifo_write(&SDO_C->bufFifo, buf, countRd, NULL);
                SDO_C->sizeTran += countRd;

                /* verify if size of data uploaded is too large */
                SDO_C->sizeInd = SDO_C->OD_IO.stream.dataLength;
                if (SDO_C->sizeInd > 0 && SDO_C->sizeTran > SDO_C->sizeInd) {
                    abortCode = CO_SDO_AB_DATA_LONG;
                    ret = CO_SDO_RT_endedWithClientAbort;
                }
                /* If no more segments to be upload, finish */
                else if (odRet == ODR_OK) {
                    /* verify size of data uploaded */
                    if (SDO_C->sizeInd > 0 && SDO_C->sizeTran < SDO_C->sizeInd){
                        abortCode = CO_SDO_AB_DATA_SHORT;
                        ret = CO_SDO_RT_endedWithClientAbort;
                    }
                    else {
                        ret = CO_SDO_RT_ok_communicationEnd;
                    }
                }
                else {
                    ret = CO_SDO_RT_waitingLocalTransfer;
                }
            }
        }

        if (ret != CO_SDO_RT_uploadDataBufferFull
            && ret != CO_SDO_RT_waitingLocalTransfer
        ) {
            SDO_C->state = CO_SDO_ST_IDLE;
        }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_TIMERNEXT
        /* Inform OS to call this function again without delay. */
        else if (timerNext_us != NULL) {
            *timerNext_us = 0;
        }
#endif
    }
#endif /* CO_CONFIG_SDO_CLI_LOCAL */
    /* CAN data received ******************************************************/
    else if (CO_FLAG_READ(SDO_C->CANrxNew)) {
        /* is SDO abort */
        if (SDO_C->CANrxData[0] == 0x80) {
            uint32_t code;
            memcpy(&code, &SDO_C->CANrxData[4], sizeof(code));
            abortCode = (CO_SDO_abortCode_t)CO_SWAP_32(code);
            SDO_C->state = CO_SDO_ST_IDLE;
            ret = CO_SDO_RT_endedWithServerAbort;
        }
        else if (abort) {
            abortCode = (SDOabortCode != NULL)
                      ? *SDOabortCode : CO_SDO_AB_DEVICE_INCOMPAT;
            SDO_C->state = CO_SDO_ST_ABORT;
        }
        else switch (SDO_C->state) {
            case CO_SDO_ST_UPLOAD_INITIATE_RSP: {
                if ((SDO_C->CANrxData[0] & 0xF0) == 0x40) {
                    /* verify index and subindex */
                    uint16_t index;
                    uint8_t subindex;
                    index = ((uint16_t) SDO_C->CANrxData[2]) << 8;
                    index |= SDO_C->CANrxData[1];
                    subindex = SDO_C->CANrxData[3];
                    if (index != SDO_C->index || subindex != SDO_C->subIndex) {
                        abortCode = CO_SDO_AB_PRAM_INCOMPAT;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }

                    if (SDO_C->CANrxData[0] & 0x02) {
                        /* Expedited transfer */
                        size_t count = 4;
                        /* is size indicated? */
                        if (SDO_C->CANrxData[0] & 0x01) {
                            count -= (SDO_C->CANrxData[0] >> 2) & 0x03;
                        }
                        /* copy data, indicate size and finish */
                        CO_fifo_write(&SDO_C->bufFifo,
                                      &SDO_C->CANrxData[4],
                                      count, NULL);
                        SDO_C->sizeTran = count;
                        SDO_C->state = CO_SDO_ST_IDLE;
                        ret = CO_SDO_RT_ok_communicationEnd;
                    }
                    else {
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED
                        /* segmented transfer, is size indicated? */
                        if (SDO_C->CANrxData[0] & 0x01) {
                            uint32_t size;
                            memcpy(&size, &SDO_C->CANrxData[4], sizeof(size));
                            SDO_C->sizeInd = CO_SWAP_32(size);
                        }
                        SDO_C->toggle = 0x00;
                        SDO_C->state = CO_SDO_ST_UPLOAD_SEGMENT_REQ;
#else
                        abortCode = CO_SDO_AB_UNSUPPORTED_ACCESS;
                        SDO_C->state = CO_SDO_ST_ABORT;
#endif
                    }
                }
                else {
                    abortCode = CO_SDO_AB_CMD;
                    SDO_C->state = CO_SDO_ST_ABORT;
                }
                break;
            }

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED
            case CO_SDO_ST_UPLOAD_SEGMENT_RSP: {
                if ((SDO_C->CANrxData[0] & 0xE0) == 0x00) {
                    size_t count, countWr;

                    /* verify and alternate toggle bit */
                    uint8_t toggle = SDO_C->CANrxData[0] & 0x10;
                    if (toggle != SDO_C->toggle) {
                        abortCode = CO_SDO_AB_TOGGLE_BIT;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }
                    SDO_C->toggle = (toggle == 0x00) ? 0x10 : 0x00;

                    /* get data size and write data to the buffer */
                    count = 7 - ((SDO_C->CANrxData[0] >> 1) & 0x07);
                    countWr = CO_fifo_write(&SDO_C->bufFifo,
                                            &SDO_C->CANrxData[1],
                                            count, NULL);
                    SDO_C->sizeTran += countWr;

                    /* verify, if there was not enough space in fifo buffer */
                    if (countWr != count) {
                        abortCode = CO_SDO_AB_OUT_OF_MEM;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }

                    /* verify if size of data uploaded is too large */
                    if (SDO_C->sizeInd > 0
                        && SDO_C->sizeTran > SDO_C->sizeInd
                    ) {
                        abortCode = CO_SDO_AB_DATA_LONG;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }

                    /* If no more segments to be upload, finish */
                    if (SDO_C->CANrxData[0] & 0x01) {
                        /* verify size of data uploaded */
                        if (SDO_C->sizeInd > 0
                            && SDO_C->sizeTran < SDO_C->sizeInd
                        ) {
                            abortCode = CO_SDO_AB_DATA_SHORT;
                            SDO_C->state = CO_SDO_ST_ABORT;
                        } else {
                            SDO_C->state = CO_SDO_ST_IDLE;
                            ret = CO_SDO_RT_ok_communicationEnd;
                        }
                    }
                    else {
                        SDO_C->state = CO_SDO_ST_UPLOAD_SEGMENT_REQ;
                    }
                }
                else {
                    abortCode = CO_SDO_AB_CMD;
                    SDO_C->state = CO_SDO_ST_ABORT;
                }
                break;
            }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED */

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
            case CO_SDO_ST_UPLOAD_BLK_INITIATE_RSP: {
                if ((SDO_C->CANrxData[0] & 0xF9) == 0xC0) {
                    uint16_t index;
                    uint8_t subindex;

                    /* get server CRC support info and data size */
                    if ((SDO_C->CANrxData[0] & 0x04) != 0) {
                        SDO_C->block_crcEnabled = true;
                    } else {
                        SDO_C->block_crcEnabled = false;
                    }
                    if (SDO_C->CANrxData[0] & 0x02) {
                        uint32_t size;
                        memcpy(&size, &SDO_C->CANrxData[4], sizeof(size));
                        SDO_C->sizeInd = CO_SWAP_32(size);
                    }

                    /* verify index and subindex */
                    index = ((uint16_t) SDO_C->CANrxData[2]) << 8;
                    index |= SDO_C->CANrxData[1];
                    subindex = SDO_C->CANrxData[3];
                    if (index != SDO_C->index || subindex != SDO_C->subIndex) {
                        abortCode = CO_SDO_AB_PRAM_INCOMPAT;
                        SDO_C->state = CO_SDO_ST_ABORT;
                    }
                    else {
                        SDO_C->state = CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ2;
                    }
                }
                /* switch to regular transfer, CO_SDO_ST_UPLOAD_INITIATE_RSP */
                else if ((SDO_C->CANrxData[0] & 0xF0) == 0x40) {
                    /* verify index and subindex */
                    uint16_t index;
                    uint8_t subindex;
                    index = ((uint16_t) SDO_C->CANrxData[2]) << 8;
                    index |= SDO_C->CANrxData[1];
                    subindex = SDO_C->CANrxData[3];
                    if (index != SDO_C->index || subindex != SDO_C->subIndex) {
                        abortCode = CO_SDO_AB_PRAM_INCOMPAT;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }

                    if (SDO_C->CANrxData[0] & 0x02) {
                        /* Expedited transfer */
                        size_t count = 4;
                        /* is size indicated? */
                        if (SDO_C->CANrxData[0] & 0x01) {
                            count -= (SDO_C->CANrxData[0] >> 2) & 0x03;
                        }
                        /* copy data, indicate size and finish */
                        CO_fifo_write(&SDO_C->bufFifo,
                                      &SDO_C->CANrxData[4],
                                      count, NULL);
                        SDO_C->sizeTran = count;
                        SDO_C->state = CO_SDO_ST_IDLE;
                        ret = CO_SDO_RT_ok_communicationEnd;
                    }
                    else {
                        /* segmented transfer, is size indicated? */
                        if (SDO_C->CANrxData[0] & 0x01) {
                            uint32_t size;
                            memcpy(&size, &SDO_C->CANrxData[4], sizeof(size));
                            SDO_C->sizeInd = CO_SWAP_32(size);
                        }
                        SDO_C->toggle = 0x00;
                        SDO_C->state = CO_SDO_ST_UPLOAD_SEGMENT_REQ;
                    }
                }
                else {
                    abortCode = CO_SDO_AB_CMD;
                    SDO_C->state = CO_SDO_ST_ABORT;
                }
                break;
            }

            case CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ: {
                /* data are copied directly in the receive function */
                break;
            }

            case CO_SDO_ST_UPLOAD_BLK_END_SREQ: {
                if ((SDO_C->CANrxData[0] & 0xE3) == 0xC1) {
                    /* Get number of data bytes in last segment, that do not
                     * contain data. Then copy remaining data into fifo */
                    uint8_t noData = ((SDO_C->CANrxData[0] >> 2) & 0x07);
                    CO_fifo_write(&SDO_C->bufFifo,
                                  &SDO_C->block_dataUploadLast[0],
                                  7 - noData,
                                  &SDO_C->block_crc);
                    SDO_C->sizeTran += 7 - noData;

                    /* verify length */
                    if (SDO_C->sizeInd > 0
                        && SDO_C->sizeTran != SDO_C->sizeInd
                    ) {
                        abortCode = (SDO_C->sizeTran > SDO_C->sizeInd) ?
                                    CO_SDO_AB_DATA_LONG : CO_SDO_AB_DATA_SHORT;
                        SDO_C->state = CO_SDO_ST_ABORT;
                        break;
                    }

                    /* verify CRC */
                    if (SDO_C->block_crcEnabled) {
                        uint16_t crcServer;
                        crcServer = ((uint16_t) SDO_C->CANrxData[2]) << 8;
                        crcServer |= SDO_C->CANrxData[1];
                        if (crcServer != SDO_C->block_crc) {
                            abortCode = CO_SDO_AB_CRC;
                            SDO_C->state = CO_SDO_ST_ABORT;
                            break;
                        }
                    }
                    SDO_C->state = CO_SDO_ST_UPLOAD_BLK_END_CRSP;
                }
                else {
                    abortCode = CO_SDO_AB_CMD;
                    SDO_C->state = CO_SDO_ST_ABORT;
                }
                break;
            }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK */

            default: {
                abortCode = CO_SDO_AB_CMD;
                SDO_C->state = CO_SDO_ST_ABORT;
                break;
            }
        }
        SDO_C->timeoutTimer = 0;
        timeDifference_us = 0;
        CO_FLAG_CLEAR(SDO_C->CANrxNew);
    }
    else if (abort) {
        abortCode = (SDOabortCode != NULL)
                  ? *SDOabortCode : CO_SDO_AB_DEVICE_INCOMPAT;
        SDO_C->state = CO_SDO_ST_ABORT;
    }

    /* Timeout timers and transmit bufferFull flag ****************************/
    if (ret == CO_SDO_RT_waitingResponse) {
        if (SDO_C->timeoutTimer < SDO_C->SDOtimeoutTime_us) {
            SDO_C->timeoutTimer += timeDifference_us;
        }
        if (SDO_C->timeoutTimer >= SDO_C->SDOtimeoutTime_us) {
            if (SDO_C->state == CO_SDO_ST_UPLOAD_SEGMENT_REQ ||
                SDO_C->state == CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP
            ) {
                /* application didn't empty buffer */
                abortCode = CO_SDO_AB_GENERAL;
            } else {
                abortCode = CO_SDO_AB_TIMEOUT;
            }
            SDO_C->state = CO_SDO_ST_ABORT;
        }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_TIMERNEXT
        else if (timerNext_us != NULL) {
            /* check again after timeout time elapsed */
            uint32_t diff = SDO_C->SDOtimeoutTime_us - SDO_C->timeoutTimer;
            if (*timerNext_us > diff) {
                *timerNext_us = diff;
            }
        }
#endif

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
        /* Timeout for sub-block reception */
        if (SDO_C->state == CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ) {
            if (SDO_C->block_timeoutTimer < SDO_C->block_SDOtimeoutTime_us) {
                SDO_C->block_timeoutTimer += timeDifference_us;
            }
            if (SDO_C->block_timeoutTimer >= SDO_C->block_SDOtimeoutTime_us) {
                /* SDO_C->state will change, processing will continue in this
                 * thread. Make memory barrier here with CO_FLAG_CLEAR() call.*/
                SDO_C->state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP;
                CO_FLAG_CLEAR(SDO_C->CANrxNew);
            }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_TIMERNEXT
            else if (timerNext_us != NULL) {
                /* check again after timeout time elapsed */
                uint32_t diff = SDO_C->block_SDOtimeoutTime_us -
                                SDO_C->block_timeoutTimer;
                if (*timerNext_us > diff) {
                    *timerNext_us = diff;
                }
            }
#endif
        }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK */

        if (SDO_C->CANtxBuff->bufferFull) {
            ret = CO_SDO_RT_transmittBufferFull;
        }
    }

    /* Transmit CAN data ******************************************************/
    if (ret == CO_SDO_RT_waitingResponse) {
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
        size_t count;
#endif
        memset((void *)&SDO_C->CANtxBuff->data[0], 0, 8);

        switch (SDO_C->state) {
        case CO_SDO_ST_UPLOAD_INITIATE_REQ: {
            SDO_C->CANtxBuff->data[0] = 0x40;
            SDO_C->CANtxBuff->data[1] = (uint8_t)SDO_C->index;
            SDO_C->CANtxBuff->data[2] = (uint8_t)(SDO_C->index >> 8);
            SDO_C->CANtxBuff->data[3] = SDO_C->subIndex;

            /* reset timeout timer and send message */
            SDO_C->timeoutTimer = 0;
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_UPLOAD_INITIATE_RSP;
            break;
        }

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED
        case CO_SDO_ST_UPLOAD_SEGMENT_REQ: {
            /* verify, if there is enough space in data buffer */
            if (CO_fifo_getSpace(&SDO_C->bufFifo) < 7) {
                ret = CO_SDO_RT_uploadDataBufferFull;
                break;
            }
            SDO_C->CANtxBuff->data[0] = 0x60 | SDO_C->toggle;

            /* reset timeout timer and send message */
            SDO_C->timeoutTimer = 0;
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_UPLOAD_SEGMENT_RSP;
            break;
        }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_SEGMENTED */

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
        case CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ: {
            SDO_C->CANtxBuff->data[0] = 0xA4;
            SDO_C->CANtxBuff->data[1] = (uint8_t)SDO_C->index;
            SDO_C->CANtxBuff->data[2] = (uint8_t)(SDO_C->index >> 8);
            SDO_C->CANtxBuff->data[3] = SDO_C->subIndex;

            /* calculate number of block segments from free buffer space */
            count = CO_fifo_getSpace(&SDO_C->bufFifo) / 7;
            if (count > 127) {
                count = 127;
            }
            else if (count == 0) {
                abortCode = CO_SDO_AB_OUT_OF_MEM;
                SDO_C->state = CO_SDO_ST_ABORT;
                break;
            }
            SDO_C->block_blksize = (uint8_t)count;
            SDO_C->CANtxBuff->data[4] = SDO_C->block_blksize;
            SDO_C->CANtxBuff->data[5] = CO_CONFIG_SDO_CLI_PST;

            /* reset timeout timer and send message */
            SDO_C->timeoutTimer = 0;
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_UPLOAD_BLK_INITIATE_RSP;
            break;
        }

        case CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ2: {
            SDO_C->CANtxBuff->data[0] = 0xA3;

            /* reset timeout timers, seqno and send message */
            SDO_C->timeoutTimer = 0;
            SDO_C->block_timeoutTimer = 0;
            SDO_C->block_seqno = 0;
            SDO_C->block_crc = 0;
            /* Block segments will be received in different thread. Make memory
             * barrier here with CO_FLAG_CLEAR() call. */
            SDO_C->state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ;
            CO_FLAG_CLEAR(SDO_C->CANrxNew);
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            break;
        }

        case CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP: {
            SDO_C->CANtxBuff->data[0] = 0xA2;
            SDO_C->CANtxBuff->data[1] = SDO_C->block_seqno;
#ifdef CO_DEBUG_SDO_CLIENT
            bool_t transferShort = SDO_C->block_seqno != SDO_C->block_blksize;
            uint8_t seqnoStart = SDO_C->block_seqno;
#endif

            /* Is last segment? */
            if (SDO_C->finished) {
                SDO_C->state = CO_SDO_ST_UPLOAD_BLK_END_SREQ;
            }
            else {
                /* verify if size of data uploaded is too large */
                if (SDO_C->sizeInd > 0 && SDO_C->sizeTran > SDO_C->sizeInd) {
                    abortCode = CO_SDO_AB_DATA_LONG;
                    SDO_C->state = CO_SDO_ST_ABORT;
                    break;
                }

                /* calculate number of block segments from free buffer space */
                count = CO_fifo_getSpace(&SDO_C->bufFifo) / 7;
                if (count >= 127) {
                    count = 127;
                }
                else if (CO_fifo_getOccupied(&SDO_C->bufFifo) > 0) {
                    /* application must empty data buffer first */
                    ret = CO_SDO_RT_uploadDataBufferFull;
#ifdef CO_DEBUG_SDO_CLIENT
                    if (transferShort) {
                        char msg[80];
                        sprintf(msg,
                                "sub-block, uploadDataBufferFull: sequno=%02X",
                                seqnoStart);
                        CO_DEBUG_SDO_CLIENT(msg);
                    }
#endif
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_TIMERNEXT
                    /* Inform OS to call this function again without delay. */
                    if (timerNext_us != NULL) {
                        *timerNext_us = 0;
                    }
#endif
                    break;
                }
                SDO_C->block_blksize = (uint8_t)count;
                SDO_C->block_seqno = 0;
                /* Block segments will be received in different thread. Make
                 * memory barrier here with CO_FLAG_CLEAR() call. */
                SDO_C->state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ;
                CO_FLAG_CLEAR(SDO_C->CANrxNew);
            }

            SDO_C->CANtxBuff->data[2] = SDO_C->block_blksize;

            /* reset block_timeoutTimer, but not SDO_C->timeoutTimer */
            SDO_C->block_timeoutTimer = 0;
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
#ifdef CO_DEBUG_SDO_CLIENT
            if (transferShort && !SDO_C->finished) {
                char msg[80];
                sprintf(msg,
                        "sub-block restarted: sequnoPrev=%02X, blksize=%02X",
                        seqnoStart, SDO_C->block_blksize);
                CO_DEBUG_SDO_CLIENT(msg);
            }
#endif
            break;
        }

        case CO_SDO_ST_UPLOAD_BLK_END_CRSP: {
            SDO_C->CANtxBuff->data[0] = 0xA1;

            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_IDLE;
            ret = CO_SDO_RT_ok_communicationEnd;
            break;
        }
#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK */

        default: {
            break;
        }
        }
    }

    if (ret == CO_SDO_RT_waitingResponse) {
        if (SDO_C->state == CO_SDO_ST_ABORT) {
            uint32_t code = CO_SWAP_32((uint32_t)abortCode);
            /* Send SDO abort message */
            SDO_C->CANtxBuff->data[0] = 0x80;
            SDO_C->CANtxBuff->data[1] = (uint8_t)SDO_C->index;
            SDO_C->CANtxBuff->data[2] = (uint8_t)(SDO_C->index >> 8);
            SDO_C->CANtxBuff->data[3] = SDO_C->subIndex;

            memcpy(&SDO_C->CANtxBuff->data[4], &code, sizeof(code));
            CO_CANsend(SDO_C->CANdevTx, SDO_C->CANtxBuff);
            SDO_C->state = CO_SDO_ST_IDLE;
            ret = CO_SDO_RT_endedWithClientAbort;
        }
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK
        else if (SDO_C->state == CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ) {
            ret = CO_SDO_RT_blockUploadInProgress;
        }
#endif
    }

    if (sizeIndicated != NULL) {
        *sizeIndicated = SDO_C->sizeInd;
    }
    if (sizeTransferred != NULL) {
        *sizeTransferred = SDO_C->sizeTran;
    }
    if (SDOabortCode != NULL) {
        *SDOabortCode = abortCode;
    }

    return ret;
}


/******************************************************************************/
size_t CO_SDOclientUploadBufRead(CO_SDOclient_t *SDO_C,
                                 uint8_t *buf,
                                 size_t count)
{
    size_t ret = 0;
    if (SDO_C != NULL && buf != NULL) {
        ret = CO_fifo_read(&SDO_C->bufFifo, buf, count, NULL);
    }
    return ret;
}


/******************************************************************************/
void CO_SDOclientClose(CO_SDOclient_t *SDO_C) {
    if (SDO_C != NULL) {
        SDO_C->state = CO_SDO_ST_IDLE;
    }
}

#endif /* (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE */
