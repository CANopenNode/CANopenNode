/*
 * CANopen Service Data Object - server.
 *
 * @file        CO_SDOserver.c
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

#include <string.h>

#include "301/CO_SDOserver.h"
#include "301/crc16-ccitt.h"

/* verify configuration */
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
 #if CO_CONFIG_SDO_SRV_BUFFER_SIZE < 8
  #error CO_CONFIG_SDO_SRV_BUFFER_SIZE must be greater or equal than 8.
 #endif
#endif
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
 #if !((CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED)
  #error CO_CONFIG_SDO_SRV_SEGMENTED must be enabled.
 #endif
 #if !((CO_CONFIG_CRC16) & CO_CONFIG_CRC16_ENABLE)
  #error CO_CONFIG_CRC16_ENABLE must be enabled.
 #endif
 #if CO_CONFIG_SDO_SRV_BUFFER_SIZE < (127*7)
  #error CO_CONFIG_SDO_SRV_BUFFER_SIZE must be greater or equal than (127*7).
 #endif
#endif

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_SDO_receive(void *object, void *msg) {
    CO_SDOserver_t *SDO = (CO_SDOserver_t *)object;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);


    /* verify message length and message queue overflow (if previous message
     * was not processed yet) */
    if (DLC == 8 && !CO_FLAG_READ(SDO->CANrxNew)) {
        if (data[0] == 0x80) {
            /* abort from client, just make idle */
            SDO->state = CO_SDO_ST_IDLE;
        }
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
        else if (SDO->state == CO_SDO_ST_UPLOAD_BLK_END_CRSP && data[0]==0xA1) {
            /*  SDO block download successfully transferred, just make idle */
            SDO->state = CO_SDO_ST_IDLE;
        }
        else if (SDO->state == CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ) {
            /* just in case, condition should always pass */
            if (SDO->bufOffsetWr <= (CO_CONFIG_SDO_SRV_BUFFER_SIZE - 7)) {
                /* block download, copy data directly */
                uint8_t seqno;
                CO_SDO_state_t state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ;

                seqno = data[0] & 0x7F;
                SDO->timeoutTimer = 0;
                SDO->block_timeoutTimer = 0;

                /* break sub-block if sequence number is too large */
                if (seqno > SDO->block_blksize) {
                    state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP;
                }
                /* verify if sequence number is correct */
                else if (seqno == (SDO->block_seqno + 1)) {
                    SDO->block_seqno = seqno;

                    /* Copy data. There is always enough space in buffer,
                    * because block_blksize was calculated before */
                    memcpy(SDO->buf + SDO->bufOffsetWr, &data[1], 7);
                    SDO->bufOffsetWr += 7;
                    SDO->sizeTran += 7;

                    /* is this the last segment? */
                    if ((data[0] & 0x80) != 0) {
                        SDO->finished = true;
                        state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP;
                    }
                    else if (seqno == SDO->block_blksize) {
                        /* all segments in sub-block has been transferred */
                        state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP;
                    }
                }
                else if (seqno == SDO->block_seqno || SDO->block_seqno == 0U) {
                    /* Ignore message, if it is duplicate or if sequence didn't
                    * start yet. */
                }
                else {
                    /* seqno is totally wrong, break sub-block. Data after last
                     * good seqno will be re-transmitted. */
                    state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP;
                }

                if (state != CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ) {
                    /* SDO->state has changed, processing will continue in
                     * another thread. Make memory barrier here with
                     * CO_FLAG_CLEAR() call. */
                    CO_FLAG_CLEAR(SDO->CANrxNew);
                    SDO->state = state;
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE
                    /* Optional signal to RTOS, which can resume task, which
                     * handles SDO server processing. */
                    if (SDO->pFunctSignalPre != NULL) {
                        SDO->pFunctSignalPre(SDO->functSignalObjectPre);
                    }
#endif
                }
            }
        }
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK */
        else {
            /* copy data and set 'new message' flag, data will be processed in
             * CO_SDOserver_process() */
            memcpy(SDO->CANrxData, data, DLC);
            CO_FLAG_SET(SDO->CANrxNew);
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE
            /* Optional signal to RTOS, which can resume task, which handles
            * SDO server processing. */
            if (SDO->pFunctSignalPre != NULL) {
                SDO->pFunctSignalPre(SDO->functSignalObjectPre);
            }
#endif
        }
    }
}


/*
 * Custom function for reading OD variable _SDO server parameter_, default
 * channel
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static OD_size_t OD_read_1200_default(OD_stream_t *stream, uint8_t subIndex,
                                      void *buf, OD_size_t count,
                                      ODR_t *returnCode)
{
    if (stream == NULL || buf == NULL || returnCode == NULL
        || count < 1 || (count < 4 && subIndex > 0)
    ) {
        if (returnCode != NULL) *returnCode = ODR_DEV_INCOMPAT;
        return 0;
    }

    CO_SDOserver_t *SDO = (CO_SDOserver_t *)stream->object;
    *returnCode = ODR_OK;

    switch (subIndex) {
        case 0: /* Highest sub-index supported */
            CO_setUint8(buf, 2);
            return stream->dataLength = sizeof(uint8_t);

        case 1: /* COB-ID client -> server */
            CO_setUint32(buf, CO_CAN_ID_SDO_CLI + SDO->nodeId);
            return stream->dataLength = sizeof(uint32_t);

        case 2: /* COB-ID server -> client */
            CO_setUint32(buf, CO_CAN_ID_SDO_SRV + SDO->nodeId);
            return stream->dataLength = sizeof(uint32_t);

        default:
            *returnCode = ODR_SUB_NOT_EXIST;
            return 0;
    }
}


/* helper for configuring CANrx and CANtx *************************************/
static CO_ReturnError_t CO_SDOserver_init_canRxTx(CO_SDOserver_t *SDO,
                                                  CO_CANmodule_t *CANdevRx,
                                                  uint16_t CANdevRxIdx,
                                                  uint16_t CANdevTxIdx,
                                                  uint32_t COB_IDClientToServer,
                                                  uint32_t COB_IDServerToClient)
{
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_OD_DYNAMIC
    /* proceed only, if parameters change */
    if (COB_IDClientToServer == SDO->COB_IDClientToServer
        && COB_IDServerToClient == SDO->COB_IDServerToClient
    ) {
        return CO_ERROR_NO;
    }
    /* store variables */
    SDO->COB_IDClientToServer = COB_IDClientToServer;
    SDO->COB_IDServerToClient = COB_IDServerToClient;
#endif

    /* verify valid bit */
    uint16_t idC2S = ((COB_IDClientToServer & 0x80000000L) == 0) ?
                     (uint16_t)COB_IDClientToServer : 0;
    uint16_t idS2C = ((COB_IDServerToClient & 0x80000000L) == 0) ?
                     (uint16_t)COB_IDServerToClient : 0;
    if (idC2S != 0 && idS2C != 0) {
        SDO->valid = true;
    }
    else {
        idC2S = 0;
        idS2C = 0;
        SDO->valid = false;
    }

    /* configure SDO server CAN reception */
    CO_ReturnError_t ret = CO_CANrxBufferInit(
            CANdevRx,               /* CAN device */
            CANdevRxIdx,            /* rx buffer index */
            idC2S,                  /* CAN identifier */
            0x7FF,                  /* mask */
            0,                      /* rtr */
            (void*)SDO,             /* object passed to receive function */
            CO_SDO_receive);        /* this function will process rx msg */

    /* configure SDO server CAN transmission */
    SDO->CANtxBuff = CO_CANtxBufferInit(
            SDO->CANdevTx,          /* CAN device */
            CANdevTxIdx,            /* index of buffer inside CAN module */
            idS2C,                  /* CAN identifier */
            0,                      /* rtr */
            8,                      /* number of data bytes */
            0);                     /* synchronous message flag bit */

    if (SDO->CANtxBuff == NULL) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
        SDO->valid = false;
    }

    return ret;
}


#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_OD_DYNAMIC
/*
 * Custom function for writing OD object _SDO server parameter_, additional
 * channels
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static OD_size_t OD_write_1201_additional(OD_stream_t *stream, uint8_t subIndex,
                                          const void *buf, OD_size_t count,
                                          ODR_t *returnCode)
{
    /* "count" is already verified in *_init() function */
    if (stream == NULL || buf == NULL || returnCode == NULL) {
        if (returnCode != NULL) *returnCode = ODR_DEV_INCOMPAT;
        return 0;
    }

    CO_SDOserver_t *SDO = (CO_SDOserver_t *)stream->object;
    *returnCode = ODR_OK;
    uint32_t COB_ID;
    uint8_t nodeId;

    switch (subIndex) {
        case 0: /* Highest sub-index supported */
            *returnCode = ODR_READONLY;
            return 0;

        case 1: /* COB-ID client -> server */
            COB_ID = CO_getUint32(buf);

            /* SDO client must not be valid when changing COB_ID */
            if ((COB_ID & 0x3FFFF800) != 0
                || ((uint16_t)COB_ID != (uint16_t)SDO->COB_IDClientToServer
                    && SDO->valid && (COB_ID & 0x80000000))
            ) {
                *returnCode = ODR_INVALID_VALUE;
                return 0;
            }
            CO_SDOserver_init_canRxTx(SDO,
                                      SDO->CANdevRx,
                                      SDO->CANdevRxIdx,
                                      SDO->CANdevTxIdx,
                                      COB_ID,
                                      SDO->COB_IDServerToClient);
            break;

        case 2: /* COB-ID server -> client */
            COB_ID = CO_getUint32(buf);

            /* SDO client must not be valid when changing COB_ID */
            if ((COB_ID & 0x3FFFF800) != 0
                || ((uint16_t)COB_ID != (uint16_t)SDO->COB_IDServerToClient
                    && SDO->valid && (COB_ID & 0x80000000))
            ) {
                *returnCode = ODR_INVALID_VALUE;
                return 0;
            }
            CO_SDOserver_init_canRxTx(SDO,
                                      SDO->CANdevRx,
                                      SDO->CANdevRxIdx,
                                      SDO->CANdevTxIdx,
                                      SDO->COB_IDClientToServer,
                                      COB_ID);
            break;

        case 3: /* Node-ID of the SDO server */
            if (count != 1) {
                *returnCode = ODR_TYPE_MISMATCH;
                return 0;
            }
            nodeId = CO_getUint8(buf);
            if (nodeId < 1 || nodeId > 127) {
                *returnCode = ODR_INVALID_VALUE;
                return 0;
            }
            break;

        default:
            *returnCode = ODR_SUB_NOT_EXIST;
            return 0;
    }

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, subIndex, buf, count, returnCode);
}
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_OD_DYNAMIC */


/******************************************************************************/
CO_ReturnError_t CO_SDOserver_init(CO_SDOserver_t *SDO,
                                   const OD_t *OD,
                                   const OD_entry_t *OD_1200_SDOsrvPar,
                                   uint8_t nodeId,
                                   uint16_t SDOtimeoutTime_ms,
                                   CO_CANmodule_t *CANdevRx,
                                   uint16_t CANdevRxIdx,
                                   CO_CANmodule_t *CANdevTx,
                                   uint16_t CANdevTxIdx)
{
    /* verify arguments */
    if (SDO == NULL || OD == NULL || CANdevRx == NULL || CANdevTx == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    SDO->OD = OD;
    SDO->nodeId = nodeId;
    SDO->SDOtimeoutTime_us = (uint32_t)SDOtimeoutTime_ms * 1000;
    SDO->state = CO_SDO_ST_IDLE;

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE
    SDO->pFunctSignalPre = NULL;
    SDO->functSignalObjectPre = NULL;
#endif

    /* configure CAN identifiers and SDO server parameters if available */
    uint16_t CanId_ClientToServer, CanId_ServerToClient;

    if (OD_1200_SDOsrvPar == NULL) {
        /* configure default SDO channel */
        if (nodeId < 1 || nodeId > 127) return CO_ERROR_ILLEGAL_ARGUMENT;

        CanId_ClientToServer = CO_CAN_ID_SDO_CLI + nodeId;
        CanId_ServerToClient = CO_CAN_ID_SDO_SRV + nodeId;
        SDO->valid = true;
    }
    else if (OD_getIndex(OD_1200_SDOsrvPar) == OD_H1200_SDO_SERVER_1_PARAM) {
        /* configure default SDO channel and SDO server parameters for it */
        if (nodeId < 1 || nodeId > 127) return CO_ERROR_ILLEGAL_ARGUMENT;

        CanId_ClientToServer = CO_CAN_ID_SDO_CLI + nodeId;
        CanId_ServerToClient = CO_CAN_ID_SDO_SRV + nodeId;
        SDO->valid = true;
        ODR_t odRetE = OD_extensionIO_init(OD_1200_SDOsrvPar,
                                           (void *) SDO,
                                           OD_read_1200_default,
                                           NULL);
        if (odRetE != ODR_OK) {
            CO_errinfo(CANdevTx, OD_getIndex(OD_1200_SDOsrvPar));
            return CO_ERROR_OD_PARAMETERS;
        }
    }
    else if (OD_getIndex(OD_1200_SDOsrvPar) > OD_H1200_SDO_SERVER_1_PARAM
         && OD_getIndex(OD_1200_SDOsrvPar) <= (OD_H1200_SDO_SERVER_1_PARAM+0x7F)
    ) {
        /* configure additional SDO channel and SDO server parameters for it */
        uint8_t maxSubIndex;
        uint32_t COB_IDClientToServer32, COB_IDServerToClient32;

        /* get and verify parameters from Object Dictionary (initial values) */
        ODR_t odRet0 = OD_get_u8(OD_1200_SDOsrvPar, 0, &maxSubIndex, true);
        ODR_t odRet1 = OD_get_u32(OD_1200_SDOsrvPar, 1,
                                  &COB_IDClientToServer32, true);
        ODR_t odRet2 = OD_get_u32(OD_1200_SDOsrvPar, 2,
                                  &COB_IDServerToClient32, true);

        if (odRet0 != ODR_OK || (maxSubIndex != 2 && maxSubIndex != 3)
            || odRet1 != ODR_OK || odRet2 != ODR_OK
        ) {
            CO_errinfo(CANdevTx, OD_getIndex(OD_1200_SDOsrvPar));
            return CO_ERROR_OD_PARAMETERS;
        }


        CanId_ClientToServer = ((COB_IDClientToServer32 & 0x80000000) == 0)
                               ? (uint16_t)(COB_IDClientToServer32 & 0x7FF) : 0;
        CanId_ServerToClient = ((COB_IDServerToClient32 & 0x80000000) == 0)
                               ? (uint16_t)(COB_IDServerToClient32 & 0x7FF) : 0;

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_OD_DYNAMIC
        ODR_t odRetE = OD_extensionIO_init(OD_1200_SDOsrvPar,
                                           (void *) SDO,
                                           OD_readOriginal,
                                           OD_write_1201_additional);
        if (odRetE != ODR_OK) {
            CO_errinfo(CANdevTx, OD_getIndex(OD_1200_SDOsrvPar));
            return CO_ERROR_OD_PARAMETERS;
        }
#endif
    }
    else {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CO_FLAG_CLEAR(SDO->CANrxNew);

    /* store the parameters and configure CANrx and CANtx */
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_OD_DYNAMIC
    SDO->CANdevRx = CANdevRx;
    SDO->CANdevRxIdx = CANdevRxIdx;
    SDO->CANdevTxIdx = CANdevTxIdx;
    /* set to zero to make sure CO_SDOserver_init_canRxTx() will reconfig CAN */
    SDO->COB_IDClientToServer = 0;
    SDO->COB_IDServerToClient = 0;
#endif
    SDO->CANdevTx = CANdevTx;

    return CO_SDOserver_init_canRxTx(SDO,
                                     CANdevRx,
                                     CANdevRxIdx,
                                     CANdevTxIdx,
                                     CanId_ClientToServer,
                                     CanId_ServerToClient);
}


#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_SDOserver_initCallbackPre(CO_SDOserver_t *SDO,
                                  void *object,
                                  void (*pFunctSignalPre)(void *object))
{
    if (SDO != NULL) {
        SDO->functSignalObjectPre = object;
        SDO->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif


#ifdef CO_BIG_ENDIAN
static inline void reverseBytes(void *start, int size) {
    unsigned char *lo = start;
    unsigned char *hi = start + size - 1;
    unsigned char swap;
    while (lo < hi) {
        swap = *lo;
        *lo++ = *hi;
        *hi-- = swap;
    }
}
#endif


/******************************************************************************/
CO_SDO_return_t CO_SDOserver_process(CO_SDOserver_t *SDO,
                                     bool_t NMTisPreOrOperational,
                                     uint32_t timeDifference_us,
                                     uint32_t *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    CO_SDO_return_t ret = CO_SDO_RT_waitingResponse;
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
    bool_t isNew = CO_FLAG_READ(SDO->CANrxNew);


    if (SDO == NULL) {
        ret = CO_SDO_RT_wrongArguments;
    }
    else if (SDO->valid && SDO->state == CO_SDO_ST_IDLE && !isNew) {
        /* Idle and nothing new */
        ret = CO_SDO_RT_ok_communicationEnd;
    }
    else if (!NMTisPreOrOperational || !SDO->valid) {
        /* SDO is allowed only in operational or pre-operational NMT state
         * and must be valid */
        SDO->state = CO_SDO_ST_IDLE;
        CO_FLAG_CLEAR(SDO->CANrxNew);
        ret = CO_SDO_RT_ok_communicationEnd;
    }
    /* CAN data received ******************************************************/
    else if (isNew) {
        if (SDO->state == CO_SDO_ST_IDLE) { /* new SDO communication? */
            bool_t upload = false;

            if ((SDO->CANrxData[0] & 0xF0) == 0x20) {
                SDO->state = CO_SDO_ST_DOWNLOAD_INITIATE_REQ;
            }
            else if (SDO->CANrxData[0] == 0x40) {
                upload = true;
                SDO->state = CO_SDO_ST_UPLOAD_INITIATE_REQ;
            }
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
            else if ((SDO->CANrxData[0] & 0xF9) == 0xC0) {
                SDO->state = CO_SDO_ST_DOWNLOAD_BLK_INITIATE_REQ;
            }
            else if ((SDO->CANrxData[0] & 0xFB) == 0xA0) {
                upload = true;
                SDO->state = CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ;
            }
#endif
            else {
                abortCode = CO_SDO_AB_CMD;
                SDO->state = CO_SDO_ST_ABORT;
            }

            /* if no error search object dictionary for new SDO request */
            if (abortCode == CO_SDO_AB_NONE) {
                ODR_t odRet;
                OD_subEntry_t subEntry;

                SDO->index = ((uint16_t)SDO->CANrxData[2]) << 8
                             | SDO->CANrxData[1];
                SDO->subIndex = SDO->CANrxData[3];
                odRet = OD_getSub(OD_find(SDO->OD, SDO->index), SDO->subIndex,
                                  &subEntry, &SDO->OD_IO, false);
                if (odRet != ODR_OK) {
                    abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                    SDO->state = CO_SDO_ST_ABORT;
                }
                else {
                    SDO->attribute = subEntry.attribute;

                    /* verify read/write attributes */
                    if (upload && (SDO->attribute & ODA_SDO_R) == 0) {
                        abortCode = CO_SDO_AB_WRITEONLY;
                        SDO->state = CO_SDO_ST_ABORT;
                    }
                    else if (!upload && (SDO->attribute & ODA_SDO_W) == 0) {
                        abortCode = CO_SDO_AB_READONLY;
                        SDO->state = CO_SDO_ST_ABORT;
                    }
                }
            }

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
            /* load data from object dictionary, if upload and no error */
            if (upload && abortCode == CO_SDO_AB_NONE) {
                ODR_t odRet;

                /* load data from OD variable into the buffer */
                SDO->bufOffsetWr = SDO->OD_IO.read(&SDO->OD_IO.stream,
                                                   SDO->subIndex,
                                                   SDO->buf,
                                                   CO_CONFIG_SDO_SRV_BUFFER_SIZE,
                                                   &odRet);
                SDO->bufOffsetRd = 0;

                if (odRet != ODR_OK && odRet != ODR_PARTIAL) {
                    abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                    SDO->state = CO_SDO_ST_ABORT;
                }
                else if (odRet == ODR_PARTIAL && SDO->bufOffsetWr < 7) {
                    abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                    SDO->state = CO_SDO_ST_ABORT;
                }
                else {
                    SDO->finished = (odRet != ODR_PARTIAL);
#ifdef CO_BIG_ENDIAN
                    /* swap data if necessary */
                    if ((SDO->attribute & ODA_MB) != 0) {
                        if (!SDO->finished) {
                            abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                            SDO->state = CO_SDO_ST_ABORT;
                        }
                        else {
                            reverseBytes(SDO->buf, SDO->bufOffsetWr);
                        }
                    }
#endif
                }
            }
            /* get and verify OD data size, if upload and still no error */
            if (upload && abortCode == CO_SDO_AB_NONE) {
                /* Size of variable in OD (may not be known yet) */
                SDO->sizeInd = SDO->OD_IO.stream.dataLength;
                SDO->sizeTran = 0;
                if (SDO->finished) {
                    /* OD variable was completely read, so its size is known */
                    if (SDO->sizeInd == 0) {
                        SDO->sizeInd = SDO->bufOffsetWr;
                    }
                    else if (SDO->sizeInd != SDO->bufOffsetWr) {
                        abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                        SDO->state = CO_SDO_ST_ABORT;
                    }
                }
            }
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED */
        } /* (SDO->state == CO_SDO_ST_IDLE) */

        if (SDO->state != CO_SDO_ST_IDLE && SDO->state != CO_SDO_ST_ABORT)
        switch (SDO->state) {
        case CO_SDO_ST_DOWNLOAD_INITIATE_REQ: {
            if (SDO->CANrxData[0] & 0x02) {
                /* Expedited transfer, max 4 bytes of data */
                OD_size_t sizeIndicated = 4;
                OD_size_t sizeInOd = SDO->OD_IO.stream.dataLength;
                ODR_t odRet;

                /* is size indicated? */
                if (SDO->CANrxData[0] & 0x01) {
                    sizeIndicated -= (SDO->CANrxData[0] >> 2) & 0x03;
                    if (sizeInOd > 0 && sizeIndicated != sizeInOd) {
                        abortCode = (sizeIndicated < sizeInOd) ?
                                    CO_SDO_AB_DATA_SHORT :
                                    CO_SDO_AB_DATA_LONG;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                }
                else {
                    if (sizeInOd > 4) {
                        abortCode = CO_SDO_AB_DATA_SHORT;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                    else {
                        sizeIndicated = sizeInOd;
                    }
                }

                /* swap data if necessary, then copy data */
#ifdef CO_BIG_ENDIAN
                if ((SDO->attribute & ODA_MB) != 0) {
                    reverseBytes(&SDO->CANrxData[4], sizeIndicated);
                }
#endif
                SDO->OD_IO.write(&SDO->OD_IO.stream, SDO->subIndex,
                                 &SDO->CANrxData[4], sizeIndicated, &odRet);
                if (odRet != ODR_OK) {
                    abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
                else {
                    SDO->state = CO_SDO_ST_DOWNLOAD_INITIATE_RSP;
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
                    SDO->finished = true;
#endif
                }
            }
            else {
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
                /* segmented transfer, is size indicated? */
                if (SDO->CANrxData[0] & 0x01) {
                    uint32_t size;
                    OD_size_t sizeInOd = SDO->OD_IO.stream.dataLength;

                    memcpy(&size, &SDO->CANrxData[4], sizeof(size));
                    SDO->sizeInd = CO_SWAP_32(size);

                    /* Indicated size of SDO matches sizeof OD variable? */
                    if (sizeInOd > 0 && SDO->sizeInd != sizeInOd) {
                        abortCode = (SDO->sizeInd < sizeInOd) ?
                                    CO_SDO_AB_DATA_SHORT :
                                    CO_SDO_AB_DATA_LONG;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                }
                else {
                    SDO->sizeInd = 0;
                }
                SDO->state = CO_SDO_ST_DOWNLOAD_INITIATE_RSP;
                SDO->finished = false;
#else
                abortCode = CO_SDO_AB_UNSUPPORTED_ACCESS;
                SDO->state = CO_SDO_ST_ABORT;
#endif
            }
            break;
        }

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
        case CO_SDO_ST_DOWNLOAD_SEGMENT_REQ: {
            if ((SDO->CANrxData[0] & 0xE0) == 0x00) {
                OD_size_t count;
                OD_size_t sizeInOd = SDO->OD_IO.stream.dataLength;
                bool_t lastSegment = (SDO->CANrxData[0] & 0x01) != 0;

                /* verify and alternate toggle bit */
                uint8_t toggle = SDO->CANrxData[0] & 0x10;
                if (toggle != SDO->toggle) {
                    abortCode = CO_SDO_AB_TOGGLE_BIT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
                SDO->toggle = (toggle == 0x00) ? 0x10 : 0x00;

                /* get data size and write data to the buffer */
                count = 7 - ((SDO->CANrxData[0] >> 1) & 0x07);
                memcpy(SDO->buf + SDO->bufOffsetWr, &SDO->CANrxData[1], count);
                SDO->bufOffsetWr += count;
                SDO->sizeTran += count;

                /* verify if size of data downloaded is too large */
                if ((SDO->sizeInd > 0 && SDO->sizeTran > SDO->sizeInd)
                    || (sizeInOd > 0 && SDO->sizeTran > sizeInOd)
                ) {
                    abortCode = CO_SDO_AB_DATA_LONG;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }

                /* if necessary, empty the buffer */
                if (lastSegment
                    || (CO_CONFIG_SDO_SRV_BUFFER_SIZE - SDO->bufOffsetWr) < 7
                ) {
                    ODR_t odRet;
#ifdef CO_BIG_ENDIAN
                    /* swap data if necessary */
                    if ((SDO->attribute & ODA_MB) != 0) {
                        reverseBytes(SDO->buf, SDO->bufOffsetWr);
                    }
#endif
                    /* write data */
                    SDO->OD_IO.write(&SDO->OD_IO.stream, SDO->subIndex,
                                     SDO->buf, SDO->bufOffsetWr, &odRet);
                    SDO->bufOffsetWr = 0;

                    if (odRet != ODR_OK && odRet != ODR_PARTIAL) {
                        abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                    else if (lastSegment && odRet == ODR_PARTIAL) {
                        /* OD variable was not written completelly, but SDO
                         * download finished */
                        abortCode = CO_SDO_AB_DATA_SHORT;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                    else if (!lastSegment && odRet == ODR_OK) {
                        /* OD variable was written completelly, but SDO download
                         * still has data */
                        abortCode = CO_SDO_AB_DATA_LONG;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                }

                /* If no more segments to be downloaded, finish */
                if (lastSegment) {
                    /* verify size of data */
                    if (SDO->sizeInd > 0 && SDO->sizeTran < SDO->sizeInd) {
                        abortCode = CO_SDO_AB_DATA_SHORT;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                    else {
                        SDO->finished = true;
                    }
                }
                SDO->state = CO_SDO_ST_DOWNLOAD_SEGMENT_RSP;
            }
            else {
                abortCode = CO_SDO_AB_CMD;
                SDO->state = CO_SDO_ST_ABORT;
            }
            break;
        }
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED */

        case CO_SDO_ST_UPLOAD_INITIATE_REQ: {
            SDO->state = CO_SDO_ST_UPLOAD_INITIATE_RSP;
            break;
        }

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
        case CO_SDO_ST_UPLOAD_SEGMENT_REQ: {
            if ((SDO->CANrxData[0] & 0xEF) == 0x60) {
                /* verify and alternate toggle bit */
                uint8_t toggle = SDO->CANrxData[0] & 0x10;
                if (toggle != SDO->toggle) {
                    abortCode = CO_SDO_AB_TOGGLE_BIT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
                SDO->state = CO_SDO_ST_UPLOAD_SEGMENT_RSP;
            }
            else {
                abortCode = CO_SDO_AB_CMD;
                SDO->state = CO_SDO_ST_ABORT;
            }
            break;
        }
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED */

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
        case CO_SDO_ST_DOWNLOAD_BLK_INITIATE_REQ: {
            SDO->block_crcEnabled = (SDO->CANrxData[0] & 0x04) != 0;

            /* is size indicated? */
            if ((SDO->CANrxData[0] & 0x02) != 0) {
                uint32_t size;
                OD_size_t sizeInOd = SDO->OD_IO.stream.dataLength;

                memcpy(&size, &SDO->CANrxData[4], sizeof(size));
                SDO->sizeInd = CO_SWAP_32(size);

                /* Indicated size of SDO matches sizeof OD variable? */
                if (sizeInOd > 0 && SDO->sizeInd != sizeInOd) {
                    abortCode = (SDO->sizeInd < sizeInOd) ?
                                CO_SDO_AB_DATA_SHORT :
                                CO_SDO_AB_DATA_LONG;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
            }
            else {
                SDO->sizeInd = 0;
            }
            SDO->state = CO_SDO_ST_DOWNLOAD_BLK_INITIATE_RSP;
            SDO->finished = false;
            break;
        }

        case CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ: {
            /* data are copied directly in the receive function */
            break;
        }

        case CO_SDO_ST_DOWNLOAD_BLK_END_REQ: {
            if ((SDO->CANrxData[0] & 0xE3) == 0xC1) {
                ODR_t odRet;

                /* Get number of data bytes in last segment, that do not
                    * contain data. Then reduce buffer. */
                uint8_t noData = ((SDO->CANrxData[0] >> 2) & 0x07);
                if (SDO->bufOffsetWr <= noData) {
                    /* just in case, should never happen */
                    abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
                SDO->sizeTran -= noData;
                SDO->bufOffsetWr -= noData;

                /* verify length */
                if (SDO->sizeInd > 0 && SDO->sizeTran != SDO->sizeInd) {
                    abortCode = (SDO->sizeTran > SDO->sizeInd) ?
                                CO_SDO_AB_DATA_LONG : CO_SDO_AB_DATA_SHORT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }

#ifdef CO_BIG_ENDIAN
                /* swap data if necessary */
                if ((SDO->attribute & ODA_MB) != 0) {
                    reverseBytes(SDO->buf, SDO->bufOffsetWr);
                }
#endif

                /* calculeate and verify CRC */
                if (SDO->block_crcEnabled) {
                    uint16_t crcClient;
                    SDO->block_crc = crc16_ccitt((unsigned char *)SDO->buf,
                                                 SDO->bufOffsetWr,
                                                 SDO->block_crc);

                    crcClient = ((uint16_t) SDO->CANrxData[2]) << 8;
                    crcClient |= SDO->CANrxData[1];
                    if (crcClient != SDO->block_crc) {
                        abortCode = CO_SDO_AB_CRC;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                }

                /* write the data */
                SDO->OD_IO.write(&SDO->OD_IO.stream, SDO->subIndex,
                                 SDO->buf, SDO->bufOffsetWr, &odRet);
                SDO->bufOffsetWr = 0;
                if (odRet == ODR_PARTIAL) {
                    /* OD variable was not written completelly, but SDO
                     * download finished */
                    abortCode = CO_SDO_AB_DATA_SHORT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
                else if (odRet != ODR_OK) {
                    abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }

                SDO->state = CO_SDO_ST_DOWNLOAD_BLK_END_RSP;
            }
            else {
                abortCode = CO_SDO_AB_CMD;
                SDO->state = CO_SDO_ST_ABORT;
            }
            break;
        }

        case CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ: {
            /* if pst (protocol switch threshold, byte5) is larger than data
             * size of OD variable, then switch to segmented transfer */
            if (SDO->sizeInd > 0 && SDO->CANrxData[5] > 0
                && SDO->CANrxData[5] >= SDO->sizeInd)
            {
                SDO->state = CO_SDO_ST_UPLOAD_INITIATE_RSP;
            }
            else {
                /* data were already loaded from OD variable, verify crc */
                if ((SDO->CANrxData[0] & 0x04) != 0) {
                    SDO->block_crcEnabled = true;
                    SDO->block_crc = crc16_ccitt((unsigned char *)SDO->buf,
                                                 SDO->bufOffsetWr,
                                                 0);
                }
                else {
                    SDO->block_crcEnabled = false;
                }

                /* get blksize and verify it */
                SDO->block_blksize = SDO->CANrxData[4];
                if (SDO->block_blksize < 1 || SDO->block_blksize > 127) {
                    SDO->block_blksize = 127;
                }

                /* verify, if there is enough data */
                if (!SDO->finished && SDO->bufOffsetWr < SDO->block_blksize*7) {
                    abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                    SDO->state = CO_SDO_ST_ABORT;
                }
                SDO->state = CO_SDO_ST_UPLOAD_BLK_INITIATE_RSP;
            }
            break;
        }

        case CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ2: {
            if (SDO->CANrxData[0] == 0xA3) {
                SDO->block_seqno = 0;
                SDO->state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ;
            }
            else {
                abortCode = CO_SDO_AB_CMD;
                SDO->state = CO_SDO_ST_ABORT;
            }
            break;
        }

        case CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ:
        case CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP: {
            if (SDO->CANrxData[0] == 0xA2) {
                OD_size_t countRemain, countMinimum;

                SDO->block_blksize = SDO->CANrxData[2];
                if (SDO->block_blksize < 1 || SDO->block_blksize > 127) {
                    SDO->block_blksize = 127;
                }

                /* check number of segments */
                if (SDO->CANrxData[1] < SDO->block_seqno) {
                    /* NOT all segments transferred successfully.
                     * Re-transmit data after erroneous segment. */
                    OD_size_t cntFailed = SDO->block_seqno - SDO->CANrxData[1];
                    cntFailed = cntFailed * 7 - SDO->block_noData;
                    SDO->bufOffsetRd -= cntFailed;
                    SDO->sizeTran -= cntFailed;
                }
                else if (SDO->CANrxData[1] > SDO->block_seqno) {
                    /* something strange from server, break transmission */
                    abortCode = CO_SDO_AB_CMD;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }

                /* refill data buffer if necessary */
                countRemain = SDO->bufOffsetWr - SDO->bufOffsetRd;
                countMinimum = SDO->block_blksize * 7;
                if (!SDO->finished && countRemain < countMinimum) {
                    ODR_t odRet;
                    OD_size_t countRd;

                    /* first move remaining data to the start of the buffer */
                    memmove(SDO->buf, SDO->buf + SDO->bufOffsetRd, countRemain);
                    SDO->bufOffsetRd = 0;
                    SDO->bufOffsetWr = countRemain;

                    /* load data from OD variable into the buffer */
                    countRd = SDO->OD_IO.read(&SDO->OD_IO.stream, SDO->subIndex,
                                SDO->buf + SDO->bufOffsetWr,
                                CO_CONFIG_SDO_SRV_BUFFER_SIZE - SDO->bufOffsetWr,
                                &odRet);

                    countRemain += countRd;
                    if (odRet != ODR_OK && odRet != ODR_PARTIAL) {
                        abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                    else if (odRet == ODR_PARTIAL) {
                        SDO->finished = false;
                        if (countRemain < countMinimum) {
                            abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                            SDO->state = CO_SDO_ST_ABORT;
                            break;
                        }
                    }
                    else {
                        SDO->finished = true;
                    }

                    /* update the crc */
                    if (SDO->block_crcEnabled) {
                        SDO->block_crc = crc16_ccitt(
                                (unsigned char *)SDO->buf + SDO->bufOffsetWr,
                                countRd,
                                SDO->block_crc);
                    }
                    SDO->bufOffsetWr = countRemain;
                }

                if (countRemain == 0) {
                    SDO->state = CO_SDO_ST_UPLOAD_BLK_END_SREQ;
                }
                else {
                    SDO->block_seqno = 0;
                    SDO->state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ;
                }
            }
            else {
                abortCode = CO_SDO_AB_CMD;
                SDO->state = CO_SDO_ST_ABORT;
            }
            break;
        }
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK */

        default: {
            /* unknown message received */
            abortCode = CO_SDO_AB_CMD;
            SDO->state = CO_SDO_ST_ABORT;
        }
        } /* switch (SDO->state) */
        SDO->timeoutTimer = 0;
        timeDifference_us = 0;
        CO_FLAG_CLEAR(SDO->CANrxNew);
    } /* if (isNew) */

    /* Timeout timers *********************************************************/
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
    if (ret == CO_SDO_RT_waitingResponse) {
        if (SDO->timeoutTimer < SDO->SDOtimeoutTime_us) {
            SDO->timeoutTimer += timeDifference_us;
        }
        if (SDO->timeoutTimer >= SDO->SDOtimeoutTime_us) {
            abortCode = CO_SDO_AB_TIMEOUT;
            SDO->state = CO_SDO_ST_ABORT;
        }
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_TIMERNEXT
        else if (timerNext_us != NULL) {
            /* check again after timeout time elapsed */
            uint32_t diff = SDO->SDOtimeoutTime_us - SDO->timeoutTimer;
            if (*timerNext_us > diff) {
                *timerNext_us = diff;
            }
        }
#endif

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
        /* Timeout for sub-block transmission */
        if (SDO->state == CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ) {
            if (SDO->block_timeoutTimer < SDO->block_SDOtimeoutTime_us) {
                SDO->block_timeoutTimer += timeDifference_us;
            }
            if (SDO->block_timeoutTimer >= SDO->block_SDOtimeoutTime_us) {
                /* SDO->state will change, processing will continue in this
                 * thread. Make memory barrier here with CO_FLAG_CLEAR() call.*/
                SDO->state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP;
                CO_FLAG_CLEAR(SDO->CANrxNew);
            }
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_TIMERNEXT
            else if (timerNext_us != NULL) {
                /* check again after timeout time elapsed */
                uint32_t diff = SDO->block_SDOtimeoutTime_us -
                                SDO->block_timeoutTimer;
                if (*timerNext_us > diff) {
                    *timerNext_us = diff;
                }
            }
#endif
        }
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK */

        if (SDO->CANtxBuff->bufferFull) {
            ret = CO_SDO_RT_transmittBufferFull;
        }
    }
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED */

    /* Transmit CAN data ******************************************************/
    if (ret == CO_SDO_RT_waitingResponse) {
        /* clear response buffer */
        memset(SDO->CANtxBuff->data, 0, sizeof(SDO->CANtxBuff->data));

        switch (SDO->state) {
        case CO_SDO_ST_DOWNLOAD_INITIATE_RSP: {
            SDO->CANtxBuff->data[0] = 0x60;
            SDO->CANtxBuff->data[1] = (uint8_t)SDO->index;
            SDO->CANtxBuff->data[2] = (uint8_t)(SDO->index >> 8);
            SDO->CANtxBuff->data[3] = SDO->subIndex;

            /* reset timeout timer and send message */
            SDO->timeoutTimer = 0;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
            if (SDO->finished) {
                SDO->state = CO_SDO_ST_IDLE;
                ret = CO_SDO_RT_ok_communicationEnd;
            }
            else {
                SDO->toggle = 0x00;
                SDO->sizeTran = 0;
                SDO->bufOffsetWr = 0;
                SDO->bufOffsetRd = 0;
                SDO->state = CO_SDO_ST_DOWNLOAD_SEGMENT_REQ;
            }
#else
            SDO->state = CO_SDO_ST_IDLE;
            ret = CO_SDO_RT_ok_communicationEnd;
#endif
            break;
        }

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
        case CO_SDO_ST_DOWNLOAD_SEGMENT_RSP: {
            SDO->CANtxBuff->data[0] = 0x20 | SDO->toggle;

            /* reset timeout timer and send message */
            SDO->timeoutTimer = 0;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            if (SDO->finished) {
                SDO->state = CO_SDO_ST_IDLE;
                ret = CO_SDO_RT_ok_communicationEnd;
            }
            else {
                SDO->state = CO_SDO_ST_DOWNLOAD_SEGMENT_REQ;
            }
            break;
        }
#endif

        case CO_SDO_ST_UPLOAD_INITIATE_RSP: {
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
            /* data were already loaded from OD variable */
            if (SDO->sizeInd > 0 && SDO->sizeInd <= 4) {
                /* expedited transfer */
                SDO->CANtxBuff->data[0] = 0x43 | ((4 - SDO->sizeInd) << 2);
                memcpy(&SDO->CANtxBuff->data[4], &SDO->buf,
                       sizeof(SDO->sizeInd));
                SDO->state = CO_SDO_ST_IDLE;
                ret = CO_SDO_RT_ok_communicationEnd;
            }
            else {
                /* data will be transfered with segmented transfer */
                if (SDO->sizeInd > 0) {
                    /* indicate data size, if known */
                    uint32_t sizeInd = SDO->sizeInd;
                    uint32_t sizeIndSw = CO_SWAP_32(sizeInd);
                    SDO->CANtxBuff->data[0] = 0x41;
                    memcpy(&SDO->CANtxBuff->data[4],
                           &sizeIndSw, sizeof(sizeIndSw));
                }
                SDO->toggle = 0x00;
                SDO->timeoutTimer = 0;
                SDO->state = CO_SDO_ST_UPLOAD_SEGMENT_REQ;
            }
#else /* Expedited transfer only */
            /* load data from OD variable */
            ODR_t odRet;
            OD_size_t count = SDO->OD_IO.read(&SDO->OD_IO.stream, SDO->subIndex,
                                              &SDO->CANtxBuff->data[4], 4,
                                              &odRet);
            if (odRet != ODR_OK || count == 0) {
                abortCode = (odRet == ODR_OK) ? CO_SDO_AB_DEVICE_INCOMPAT :
                            (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                SDO->state = CO_SDO_ST_ABORT;
                break;
            }

#ifdef CO_BIG_ENDIAN
            /* swap data if necessary */
            if ((SDO->attribute & ODA_MB) != 0) {
                reverseBytes(&SDO->CANtxBuff->data[4], count);
            }
#endif
            SDO->CANtxBuff->data[0] = 0x43 | ((4 - count) << 2);
            SDO->state = CO_SDO_ST_IDLE;
            ret = CO_SDO_RT_ok_communicationEnd;
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED */

            /* send message */
            SDO->CANtxBuff->data[1] = (uint8_t)SDO->index;
            SDO->CANtxBuff->data[2] = (uint8_t)(SDO->index >> 8);
            SDO->CANtxBuff->data[3] = SDO->subIndex;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            break;
        }

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
        case CO_SDO_ST_UPLOAD_SEGMENT_RSP: {
            /* refill the data buffer if necessary */
            if (!SDO->finished && (SDO->bufOffsetWr - SDO->bufOffsetRd) < 7) {
                /* first move remaining data to the start of the buffer */
                SDO->bufOffsetWr -= SDO->bufOffsetRd;
                memmove(SDO->buf, SDO->buf + SDO->bufOffsetRd,
                        SDO->bufOffsetWr);
                SDO->bufOffsetRd = 0;

                /* load data from OD variable into the buffer */
                ODR_t odRet;
                OD_size_t countRd = SDO->OD_IO.read(&SDO->OD_IO.stream,
                                SDO->subIndex,
                                SDO->buf + SDO->bufOffsetWr,
                                CO_CONFIG_SDO_SRV_BUFFER_SIZE - SDO->bufOffsetWr,
                                &odRet);
                SDO->bufOffsetWr += countRd;

                if (odRet != ODR_OK && odRet != ODR_PARTIAL) {
                    abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
                else if (odRet == ODR_PARTIAL) {
                    SDO->finished = false;
                    if (SDO->bufOffsetWr < 7) {
                        abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                }
                else {
                    SDO->finished = true;
                }
            }

            /* SDO command specifier with toggle bit */
            SDO->CANtxBuff->data[0] = SDO->toggle;
            SDO->toggle = (SDO->toggle == 0x00) ? 0x10 : 0x00;

            OD_size_t count = SDO->bufOffsetWr - SDO->bufOffsetRd;
            /* verify, if this is the last segment */
            if (count <= 7) {
                /* indicate last segment and nnn */
                SDO->CANtxBuff->data[0] |= ((7 - count) << 1) | 0x01;
                SDO->state = CO_SDO_ST_IDLE;
                ret = CO_SDO_RT_ok_communicationEnd;
            }
            else {
                SDO->timeoutTimer = 0;
                SDO->state = CO_SDO_ST_UPLOAD_SEGMENT_REQ;
                count = 7;
            }

            /* copy data segment to CAN message */
            memcpy(&SDO->CANtxBuff->data[1], SDO->buf + SDO->bufOffsetRd,
                   count);
            SDO->bufOffsetRd += count;
            SDO->sizeTran += count;

            /* verify if sizeTran is too large or too short if last segment */
            if (SDO->sizeInd > 0) {
                if (SDO->sizeTran > SDO->sizeInd) {
                    abortCode = CO_SDO_AB_DATA_LONG;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
                else if (ret == CO_SDO_RT_ok_communicationEnd
                         && SDO->sizeTran < SDO->sizeInd
                ) {
                    abortCode = CO_SDO_AB_DATA_SHORT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
            }

            /* send message */
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            break;
        }
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED */

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
        case CO_SDO_ST_DOWNLOAD_BLK_INITIATE_RSP: {
            SDO->CANtxBuff->data[0] = 0xA4;
            SDO->CANtxBuff->data[1] = (uint8_t)SDO->index;
            SDO->CANtxBuff->data[2] = (uint8_t)(SDO->index >> 8);
            SDO->CANtxBuff->data[3] = SDO->subIndex;

            /* calculate number of block segments from free buffer space */
            OD_size_t count = CO_CONFIG_SDO_SRV_BUFFER_SIZE / 7;
            if (count > 127) {
                count = 127;
            }
            SDO->block_blksize = (uint8_t)count;
            SDO->CANtxBuff->data[4] = SDO->block_blksize;

            /* reset variables */
            SDO->sizeTran = 0;
            SDO->finished = false;
            SDO->bufOffsetWr = 0;
            SDO->bufOffsetRd = 0;
            SDO->block_seqno = 0;
            SDO->block_crc = 0;
            SDO->timeoutTimer = 0;
            SDO->block_timeoutTimer = 0;

            /* Block segments will be received in different thread. Make memory
             * barrier here with CO_FLAG_CLEAR() call. */
            SDO->state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ;
            CO_FLAG_CLEAR(SDO->CANrxNew);
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            break;
        }

        case CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP: {
            SDO->CANtxBuff->data[0] = 0xA2;
            SDO->CANtxBuff->data[1] = SDO->block_seqno;

            /* Is last segment? */
            if (SDO->finished) {
                SDO->state = CO_SDO_ST_DOWNLOAD_BLK_END_REQ;
            }
            else {
                /* verify if size of data downloaded is too large */
                if (SDO->sizeInd > 0 && SDO->sizeTran > SDO->sizeInd) {
                    abortCode = CO_SDO_AB_DATA_LONG;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }

                /* calculate number of block segments from free buffer space */
                OD_size_t count;
                count = (CO_CONFIG_SDO_SRV_BUFFER_SIZE - SDO->bufOffsetWr) / 7;
                if (count >= 127) {
                    count = 127;
                }
                else if (SDO->bufOffsetWr > 0) {
                    /* it is necessary to empty the buffer */
#ifdef CO_BIG_ENDIAN
                    /* swap data if necessary */
                    if ((SDO->attribute & ODA_MB) != 0) {
                        reverseBytes(SDO->buf, SDO->bufOffsetWr);
                    }
#endif
                    /* calculate crc on current data */
                    SDO->block_crc = crc16_ccitt((unsigned char *)SDO->buf,
                                                 SDO->bufOffsetWr,
                                                 SDO->block_crc);

                    /* write data */
                    ODR_t odRet;
                    SDO->OD_IO.write(&SDO->OD_IO.stream, SDO->subIndex,
                                     SDO->buf, SDO->bufOffsetWr, &odRet);
                    SDO->bufOffsetWr = 0;

                    if (odRet == ODR_OK) {
                        /* OD variable was written completelly, but SDO download
                         * still has data */
                        abortCode = CO_SDO_AB_DATA_LONG;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                    else if (odRet != ODR_PARTIAL) {
                        abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }

                    count = CO_CONFIG_SDO_SRV_BUFFER_SIZE / 7;
                    if (count >= 127) count = 127;
                }

                SDO->block_blksize = (uint8_t)count;
                SDO->block_seqno = 0;
                /* Block segments will be received in different thread. Make
                 * memory barrier here with CO_FLAG_CLEAR() call. */
                SDO->state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ;
                CO_FLAG_CLEAR(SDO->CANrxNew);
            }

            SDO->CANtxBuff->data[2] = SDO->block_blksize;

            /* reset timeout timer and send message */
            SDO->timeoutTimer = 0;
            SDO->block_timeoutTimer = 0;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            break;
        }

        case CO_SDO_ST_DOWNLOAD_BLK_END_RSP: {
            SDO->CANtxBuff->data[0] = 0xA1;

            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            SDO->state = CO_SDO_ST_IDLE;
            ret = CO_SDO_RT_ok_communicationEnd;
            break;
        }

        case CO_SDO_ST_UPLOAD_BLK_INITIATE_RSP: {
            SDO->CANtxBuff->data[0] = 0xC4;
            SDO->CANtxBuff->data[1] = (uint8_t)SDO->index;
            SDO->CANtxBuff->data[2] = (uint8_t)(SDO->index >> 8);
            SDO->CANtxBuff->data[3] = SDO->subIndex;

            /* indicate data size */
            if (SDO->sizeInd > 0) {
                uint32_t size = CO_SWAP_32(SDO->sizeInd);
                SDO->CANtxBuff->data[0] |= 0x02;
                memcpy(&SDO->CANtxBuff->data[4], &size, sizeof(size));
            }

            /* reset timeout timer and send message */
            SDO->timeoutTimer = 0;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            SDO->state = CO_SDO_ST_UPLOAD_BLK_INITIATE_REQ2;
            break;
        }

        case CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ: {
            /* write header and get current count */
            SDO->CANtxBuff->data[0] = ++SDO->block_seqno;
            OD_size_t count = SDO->bufOffsetWr - SDO->bufOffsetRd;
            /* verify, if this is the last segment */
            if (count <= 7) {
                SDO->CANtxBuff->data[0] |= 0x80;
            }
            else {
                count = 7;
            }

            /* copy data segment to CAN message */
            memcpy(&SDO->CANtxBuff->data[1], SDO->buf + SDO->bufOffsetRd,
                   count);
            SDO->bufOffsetRd += count;
            SDO->block_noData = 7 - count;
            SDO->sizeTran += count;

            /* verify if sizeTran is too large or too short if last segment */
            if (SDO->sizeInd > 0) {
                if (SDO->sizeTran > SDO->sizeInd) {
                    abortCode = CO_SDO_AB_DATA_LONG;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
                else if (SDO->bufOffsetWr == SDO->bufOffsetRd
                         && SDO->sizeTran < SDO->sizeInd
                ) {
                    abortCode = CO_SDO_AB_DATA_SHORT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }
            }

            /* is last segment or all segments in current block transferred? */
            if (SDO->bufOffsetWr == SDO->bufOffsetRd
                || SDO->block_seqno >= SDO->block_blksize
            ) {
                SDO->state = CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_CRSP;
            }
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_TIMERNEXT
            else {
                /* Inform OS to call this function again without delay. */
                if (timerNext_us != NULL) {
                    *timerNext_us = 0;
                }
            }
#endif
            /* reset timeout timer and send message */
            SDO->timeoutTimer = 0;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            break;
        }

        case CO_SDO_ST_UPLOAD_BLK_END_SREQ: {
            SDO->CANtxBuff->data[0] = 0xC1 | (SDO->block_noData << 2);
            SDO->CANtxBuff->data[1] = (uint8_t) SDO->block_crc;
            SDO->CANtxBuff->data[2] = (uint8_t) (SDO->block_crc >> 8);

            /* reset timeout timer and send message */
            SDO->timeoutTimer = 0;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            SDO->state = CO_SDO_ST_UPLOAD_BLK_END_CRSP;
            break;
        }
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK */

        default: {
            break;
        }
        } /* switch (SDO->state) */
    }

    if (ret == CO_SDO_RT_waitingResponse) {
        if (SDO->state == CO_SDO_ST_ABORT) {
            uint32_t code = CO_SWAP_32((uint32_t)abortCode);
            /* Send SDO abort message */
            SDO->CANtxBuff->data[0] = 0x80;
            SDO->CANtxBuff->data[1] = (uint8_t)SDO->index;
            SDO->CANtxBuff->data[2] = (uint8_t)(SDO->index >> 8);
            SDO->CANtxBuff->data[3] = SDO->subIndex;

            memcpy(&SDO->CANtxBuff->data[4], &code, sizeof(code));
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
            SDO->state = CO_SDO_ST_IDLE;
            ret = CO_SDO_RT_endedWithServerAbort;
        }
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
        else if (SDO->state == CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ) {
            ret = CO_SDO_RT_blockDownldInProgress;
        }
        else if (SDO->state == CO_SDO_ST_UPLOAD_BLK_SUBBLOCK_SREQ) {
            ret = CO_SDO_RT_blockUploadInProgress;
        }
#endif
    }

    return ret;
}
