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
 #if CO_CONFIG_SDO_SRV_BUFFER_SIZE < 20
  #error CO_CONFIG_SDO_SRV_BUFFER_SIZE must be greater or equal than 20.
 #endif
#endif
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
 #if !((CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED)
  #error CO_CONFIG_SDO_SRV_SEGMENTED must be enabled.
 #endif
 #if !((CO_CONFIG_CRC16) & CO_CONFIG_CRC16_ENABLE)
  #error CO_CONFIG_CRC16_ENABLE must be enabled.
 #endif
 #if CO_CONFIG_SDO_SRV_BUFFER_SIZE < 900
  #error CO_CONFIG_SDO_SRV_BUFFER_SIZE must be greater or equal than 900.
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

    /* ignore messages with wrong length */
    if (DLC == 8) {
        if (data[0] == 0x80) {
            /* abort from client, just make idle */
            SDO->state = CO_SDO_ST_IDLE;
        }
        else if (CO_FLAG_READ(SDO->CANrxNew)) {
            /* ignore message if previous message was not processed yet */
        }
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
        else if (SDO->state == CO_SDO_ST_UPLOAD_BLK_END_CRSP && data[0]==0xA1) {
            /*  SDO block download successfully transferred, just make idle */
            SDO->state = CO_SDO_ST_IDLE;
        }
        else if (SDO->state == CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ) {
            /* just in case, condition should always pass */
            if (SDO->bufOffsetWr <= (CO_CONFIG_SDO_SRV_BUFFER_SIZE - (7+2))) {
                /* block download, copy data directly */
                CO_SDO_state_t state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ;
                uint8_t seqno = data[0] & 0x7F;
                SDO->timeoutTimer = 0;
                SDO->block_timeoutTimer = 0;

                /* verify if sequence number is correct */
                if (seqno <= SDO->block_blksize
                    && seqno == (SDO->block_seqno + 1)
                ) {
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
                /* If message is duplicate or sequence didn't start yet, ignore
                 * it. Otherwise seqno is wrong, so break sub-block. Data after
                 * last good seqno will be re-transmitted. */
                else if (seqno != SDO->block_seqno && SDO->block_seqno != 0U) {
                    state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP;
#ifdef CO_DEBUG_SDO_SERVER
                    char msg[80];
                    sprintf(msg,
                            "sub-block, rx WRONG: sequno=%02X, previous=%02X",
                            seqno, SDO->block_seqno);
                    CO_DEBUG_SDO_SERVER(msg);
#endif
                }
#ifdef CO_DEBUG_SDO_SERVER
                else {
                    char msg[80];
                    sprintf(msg,
                            "sub-block, rx ignored: sequno=%02X, expected=%02X",
                            seqno, SDO->block_seqno + 1);
                    CO_DEBUG_SDO_SERVER(msg);
                }
#endif

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
        else if (SDO->state == CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_RSP) {
            /* ignore subsequent server messages, if response was requested */
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
static ODR_t OD_write_1201_additional(OD_stream_t *stream, const void *buf,
                                      OD_size_t count, OD_size_t *countWritten)
{
    /* "count" is already verified in *_init() function */
    if (stream == NULL || buf == NULL || countWritten == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    CO_SDOserver_t *SDO = (CO_SDOserver_t *)stream->object;

    switch (stream->subIndex) {
        case 0: /* Highest sub-index supported */
            return ODR_READONLY;

        case 1: { /* COB-ID client -> server */
            uint32_t COB_ID = CO_getUint32(buf);
            uint16_t CAN_ID = (uint16_t)(COB_ID & 0x7FF);
            uint16_t CAN_ID_cur = (uint16_t)(SDO->COB_IDClientToServer & 0x7FF);
            bool_t valid = (COB_ID & 0x80000000) == 0;

            /* SDO client must not be valid when changing COB_ID */
            if ((COB_ID & 0x3FFFF800) != 0
                || (valid && SDO->valid && CAN_ID != CAN_ID_cur)
                || (valid && CO_IS_RESTRICTED_CAN_ID(CAN_ID))
            ) {
                return ODR_INVALID_VALUE;
            }
            CO_SDOserver_init_canRxTx(SDO,
                                      SDO->CANdevRx,
                                      SDO->CANdevRxIdx,
                                      SDO->CANdevTxIdx,
                                      COB_ID,
                                      SDO->COB_IDServerToClient);
            break;
        }

        case 2: { /* COB-ID server -> client */
            uint32_t COB_ID = CO_getUint32(buf);
            uint16_t CAN_ID = (uint16_t)(COB_ID & 0x7FF);
            uint16_t CAN_ID_cur = (uint16_t)(SDO->COB_IDServerToClient & 0x7FF);
            bool_t valid = (COB_ID & 0x80000000) == 0;

            /* SDO client must not be valid when changing COB_ID */
            if ((COB_ID & 0x3FFFF800) != 0
                || (valid && SDO->valid && CAN_ID != CAN_ID_cur)
                || (valid && CO_IS_RESTRICTED_CAN_ID(CAN_ID))
            ) {
                return ODR_INVALID_VALUE;
            }
            CO_SDOserver_init_canRxTx(SDO,
                                      SDO->CANdevRx,
                                      SDO->CANdevRxIdx,
                                      SDO->CANdevTxIdx,
                                      SDO->COB_IDClientToServer,
                                      COB_ID);
            break;
        }

        case 3: { /* Node-ID of the SDO server */
            if (count != 1) {
                return ODR_TYPE_MISMATCH;
            }
            uint8_t nodeId = CO_getUint8(buf);
            if (nodeId < 1 || nodeId > 127) {
                return ODR_INVALID_VALUE;
            }
            break;
        }

        default:
            return ODR_SUB_NOT_EXIST;
    }

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}
#endif /* (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_OD_DYNAMIC */


/******************************************************************************/
CO_ReturnError_t CO_SDOserver_init(CO_SDOserver_t *SDO,
                                   OD_t *OD,
                                   OD_entry_t *OD_1200_SDOsrvPar,
                                   uint8_t nodeId,
                                   uint16_t SDOtimeoutTime_ms,
                                   CO_CANmodule_t *CANdevRx,
                                   uint16_t CANdevRxIdx,
                                   CO_CANmodule_t *CANdevTx,
                                   uint16_t CANdevTxIdx,
                                   uint32_t *errInfo)
{
    /* verify arguments */
    if (SDO == NULL || OD == NULL || CANdevRx == NULL || CANdevTx == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    SDO->OD = OD;
    SDO->nodeId = nodeId;
#if ((CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED)
    SDO->SDOtimeoutTime_us = (uint32_t)SDOtimeoutTime_ms * 1000;
#endif
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
    SDO->block_SDOtimeoutTime_us = (uint32_t)SDOtimeoutTime_ms * 700;
#endif
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
    else {
        uint16_t OD_SDOsrvParIdx = OD_getIndex(OD_1200_SDOsrvPar);

        if (OD_SDOsrvParIdx == OD_H1200_SDO_SERVER_1_PARAM) {
            /* configure default SDO channel and SDO server parameters for it */
            if (nodeId < 1 || nodeId > 127) return CO_ERROR_ILLEGAL_ARGUMENT;

            CanId_ClientToServer = CO_CAN_ID_SDO_CLI + nodeId;
            CanId_ServerToClient = CO_CAN_ID_SDO_SRV + nodeId;
            SDO->valid = true;

            OD_set_u32(OD_1200_SDOsrvPar, 1, CanId_ClientToServer, true);
            OD_set_u32(OD_1200_SDOsrvPar, 2, CanId_ServerToClient, true);
        }
        else if (OD_SDOsrvParIdx > OD_H1200_SDO_SERVER_1_PARAM
                && OD_SDOsrvParIdx <= (OD_H1200_SDO_SERVER_1_PARAM + 0x7F)
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
                if (errInfo != NULL) *errInfo = OD_SDOsrvParIdx;
                return CO_ERROR_OD_PARAMETERS;
            }


            CanId_ClientToServer = ((COB_IDClientToServer32 & 0x80000000) == 0)
                                ? (uint16_t)(COB_IDClientToServer32 & 0x7FF) : 0;
            CanId_ServerToClient = ((COB_IDServerToClient32 & 0x80000000) == 0)
                                ? (uint16_t)(COB_IDServerToClient32 & 0x7FF) : 0;

    #if (CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_OD_DYNAMIC
            SDO->OD_1200_extension.object = SDO;
            SDO->OD_1200_extension.read = OD_readOriginal;
            SDO->OD_1200_extension.write = OD_write_1201_additional;
            ODR_t odRetE = OD_extension_init(OD_1200_SDOsrvPar,
                                            &SDO->OD_1200_extension);
            if (odRetE != ODR_OK) {
                if (errInfo != NULL) *errInfo = OD_SDOsrvParIdx;
                return CO_ERROR_OD_PARAMETERS;
            }
    #endif
        }
        else {
            return CO_ERROR_ILLEGAL_ARGUMENT;
        }
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
static inline void reverseBytes(void *start, OD_size_t size) {
    uint8_t *lo = (uint8_t *)start;
    uint8_t *hi = (uint8_t *)start + size - 1;
    while (lo < hi) {
        uint8_t swap = *lo;
        *lo++ = *hi;
        *hi-- = swap;
    }
}
#endif


#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
/** Helper function for writing data to Object dictionary. Function swaps data
 * if necessary, calcualtes (and verifies CRC) writes data to OD and verifies
 * data lengths.
 *
 * @param SDO SDO server
 * @param [out] abortCode SDO abort code in case of error
 * @param crcOperation 0=none, 1=calculate, 2=calculate and compare
 * @parma crcClient crc checksum to campare with
 *
 * Returns true on success, otherwise write also abortCode and sets state to
 * CO_SDO_ST_ABORT */
static bool_t validateAndWriteToOD(CO_SDOserver_t *SDO,
                                   CO_SDO_abortCode_t *abortCode,
                                   uint8_t crcOperation,
                                   uint16_t crcClient)
{
    OD_size_t bufOffsetWrOrig = SDO->bufOffsetWr;

    if (SDO->finished) {
        /* Verify if size of data downloaded matches size indicated. */
        if (SDO->sizeInd > 0 && SDO->sizeTran != SDO->sizeInd) {
            *abortCode = (SDO->sizeTran > SDO->sizeInd) ?
                         CO_SDO_AB_DATA_LONG : CO_SDO_AB_DATA_SHORT;
            SDO->state = CO_SDO_ST_ABORT;
            return false;
        }

#ifdef CO_BIG_ENDIAN
        /* swap int16_t .. uint64_t data if necessary */
        if ((SDO->OD_IO.stream.attribute & ODA_MB) != 0) {
            reverseBytes(SDO->buf, SDO->bufOffsetWr);
        }
#endif

        OD_size_t sizeInOd = SDO->OD_IO.stream.dataLength;

        /* If dataType is string, then size of data downloaded may be
         * shorter than size of OD data buffer. If so, add two zero bytes
         * to terminate (unicode) string. Shorten also OD data size,
         * (temporary, send information about EOF into OD_IO.write) */
        if ((SDO->OD_IO.stream.attribute & ODA_STR) != 0
            && (sizeInOd == 0 || SDO->sizeTran < sizeInOd)
            && (SDO->bufOffsetWr + 2) <= CO_CONFIG_SDO_SRV_BUFFER_SIZE
        ) {
            SDO->buf[SDO->bufOffsetWr++] = 0;
            SDO->sizeTran++;
            if (sizeInOd == 0 || SDO->sizeTran < sizeInOd) {
                SDO->buf[SDO->bufOffsetWr++] = 0;
                SDO->sizeTran++;
            }
            SDO->OD_IO.stream.dataLength = SDO->sizeTran;
        }
        /* Indicate OD data size, if not indicated. Can be used for EOF check.*/
        else if (sizeInOd == 0) {
            SDO->OD_IO.stream.dataLength = SDO->sizeTran;
        }
        /* Verify if size of data downloaded matches data size in OD. */
        else if (SDO->sizeTran != sizeInOd) {
            *abortCode = (SDO->sizeTran > sizeInOd) ?
                         CO_SDO_AB_DATA_LONG : CO_SDO_AB_DATA_SHORT;
            SDO->state = CO_SDO_ST_ABORT;
            return false;
        }
    }
    else {
        /* Verify if size of data downloaded is not too large. */
        if (SDO->sizeInd > 0 && SDO->sizeTran > SDO->sizeInd) {
            *abortCode = CO_SDO_AB_DATA_LONG;
            SDO->state = CO_SDO_ST_ABORT;
            return false;
        }
    }

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
    /* calculate crc on current data */
    if (SDO->block_crcEnabled && crcOperation > 0) {
        SDO->block_crc = crc16_ccitt(SDO->buf, bufOffsetWrOrig, SDO->block_crc);
        if (crcOperation == 2 && crcClient != SDO->block_crc) {
            *abortCode = CO_SDO_AB_CRC;
            SDO->state = CO_SDO_ST_ABORT;
            return false;
        }
    }
#endif
    /* may be unused */
    (void) crcOperation; (void) crcClient; (void) bufOffsetWrOrig;

    /* write data */
    OD_size_t countWritten = 0;
    bool_t lock = OD_mappable(&SDO->OD_IO.stream);

    if (lock) { CO_LOCK_OD(SDO->CANdevTx); }
    ODR_t odRet = SDO->OD_IO.write(&SDO->OD_IO.stream, SDO->buf,
                                   SDO->bufOffsetWr, &countWritten);
    if (lock) { CO_UNLOCK_OD(SDO->CANdevTx); }

    SDO->bufOffsetWr = 0;

    /* verify write error value */
    if (odRet != ODR_OK && odRet != ODR_PARTIAL) {
        *abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
        SDO->state = CO_SDO_ST_ABORT;
        return false;
    }
    else if (SDO->finished && odRet == ODR_PARTIAL) {
        /* OD variable was not written completely, but SDO download finished */
        *abortCode = CO_SDO_AB_DATA_SHORT;
        SDO->state = CO_SDO_ST_ABORT;
        return false;
    }
    else if (!SDO->finished && odRet == ODR_OK) {
        /* OD variable was written completely, but SDO download still has data*/
        *abortCode = CO_SDO_AB_DATA_LONG;
        SDO->state = CO_SDO_ST_ABORT;
        return false;
    }

    return true;
}


/** Helper function for reading data from Object dictionary. Function also swaps
 * data if necessary and calcualtes CRC.
 *
 * @param SDO SDO server
 * @param [out] abortCode SDO abort code in case of error
 * @parma countMinimum if data size in buffer is less than countMinimum, then
 * buffer is refilled from OD variable
 * @param calculateCrc if true, crc is calculated
 *
 * Returns true on success, otherwise write also abortCode and sets state to
 * CO_SDO_ST_ABORT */
static bool_t readFromOd(CO_SDOserver_t *SDO,
                         CO_SDO_abortCode_t *abortCode,
                         OD_size_t countMinimum,
                         bool_t calculateCrc)
{
    OD_size_t countRemain = SDO->bufOffsetWr - SDO->bufOffsetRd;

    if (!SDO->finished && countRemain < countMinimum) {
        /* first move remaining data to the start of the buffer */
        memmove(SDO->buf, SDO->buf + SDO->bufOffsetRd, countRemain);
        SDO->bufOffsetRd = 0;
        SDO->bufOffsetWr = countRemain;

        /* Get size of free data buffer */
        OD_size_t countRdRequest = CO_CONFIG_SDO_SRV_BUFFER_SIZE - countRemain;

        /* load data from OD variable into the buffer */
        OD_size_t countRd = 0;
        uint8_t *bufShifted = SDO->buf + countRemain;
        bool_t lock = OD_mappable(&SDO->OD_IO.stream);

        if (lock) { CO_LOCK_OD(SDO->CANdevTx); }
        ODR_t odRet = SDO->OD_IO.read(&SDO->OD_IO.stream, bufShifted,
                                      countRdRequest, &countRd);
        if (lock) { CO_UNLOCK_OD(SDO->CANdevTx); }

        if (odRet != ODR_OK && odRet != ODR_PARTIAL) {
            *abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
            SDO->state = CO_SDO_ST_ABORT;
            return false;
        }

        /* if data is string, send only data up to null termination */
        if (countRd > 0 && (SDO->OD_IO.stream.attribute & ODA_STR) != 0) {
            bufShifted[countRd] = 0; /* (SDO->buf is one byte larger) */
            OD_size_t countStr = (OD_size_t)strlen((char *)bufShifted);
            if (countStr == 0) countStr = 1; /* zero length is not allowed */
            if (countStr < countRd) {
                /* string terminator found, read is finished, shorten data */
                countRd = countStr;
                odRet = ODR_OK;
                SDO->OD_IO.stream.dataLength = SDO->sizeTran + countRd;
            }
        }

        /* partial or finished read */
        SDO->bufOffsetWr = countRemain + countRd;
        if (SDO->bufOffsetWr == 0 || odRet == ODR_PARTIAL) {
            SDO->finished = false;
            if (SDO->bufOffsetWr < countMinimum) {
                *abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                SDO->state = CO_SDO_ST_ABORT;
                return false;
            }
        }
        else {
            SDO->finished = true;
        }

#ifdef CO_BIG_ENDIAN
        /* swap data if necessary */
        if ((SDO->OD_IO.stream.attribute & ODA_MB) != 0) {
            if (SDO->finished) {
                /* int16_t .. uint64_t */
                reverseBytes(bufShifted, countRd);
            }
            else {
                abortCode = CO_SDO_AB_PRAM_INCOMPAT;
                SDO->state = CO_SDO_ST_ABORT;
                return false;
            }
        }
#endif

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK
        /* update the crc */
        if (calculateCrc && SDO->block_crcEnabled) {
            SDO->block_crc = crc16_ccitt(bufShifted, countRd, SDO->block_crc);
        }
#endif

    }
    return true;
}
#endif


/******************************************************************************/
CO_SDO_return_t CO_SDOserver_process(CO_SDOserver_t *SDO,
                                     bool_t NMTisPreOrOperational,
                                     uint32_t timeDifference_us,
                                     uint32_t *timerNext_us)
{
    if (SDO == NULL) {
        return CO_SDO_RT_wrongArguments;
    }

    (void)timerNext_us; /* may be unused */

    CO_SDO_return_t ret = CO_SDO_RT_waitingResponse;
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
    bool_t isNew = CO_FLAG_READ(SDO->CANrxNew);


    if (SDO->valid && SDO->state == CO_SDO_ST_IDLE && !isNew) {
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
                SDO->index = ((uint16_t)SDO->CANrxData[2]) << 8
                             | SDO->CANrxData[1];
                SDO->subIndex = SDO->CANrxData[3];
                odRet = OD_getSub(OD_find(SDO->OD, SDO->index), SDO->subIndex,
                                  &SDO->OD_IO, false);
                if (odRet != ODR_OK) {
                    abortCode = (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                    SDO->state = CO_SDO_ST_ABORT;
                }
                else {
                    /* verify read/write attributes */
                    if ((SDO->OD_IO.stream.attribute & ODA_SDO_RW) == 0) {
                        abortCode = CO_SDO_AB_UNSUPPORTED_ACCESS;
                        SDO->state = CO_SDO_ST_ABORT;
                    }
                    else if (upload
                             && (SDO->OD_IO.stream.attribute & ODA_SDO_R) == 0
                    ) {
                        abortCode = CO_SDO_AB_WRITEONLY;
                        SDO->state = CO_SDO_ST_ABORT;
                    }
                    else if (!upload
                             && (SDO->OD_IO.stream.attribute & ODA_SDO_W) == 0
                    ) {
                        abortCode = CO_SDO_AB_READONLY;
                        SDO->state = CO_SDO_ST_ABORT;
                    }
                }
            }

#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
            /* load data from object dictionary, if upload and no error */
            if (upload && abortCode == CO_SDO_AB_NONE) {
                SDO->bufOffsetRd = SDO->bufOffsetWr = 0;
                SDO->sizeTran = 0;
                SDO->finished = false;

                if (readFromOd(SDO, &abortCode, 7, false)) {
                    /* Size of variable in OD (may not be known yet) */
                    if (SDO->finished) {
                        /* OD variable was completely read, its size is known */

                        SDO->sizeInd = SDO->OD_IO.stream.dataLength;

                        if (SDO->sizeInd == 0) {
                            SDO->sizeInd = SDO->bufOffsetWr;
                        }
                        else if (SDO->sizeInd != SDO->bufOffsetWr) {
                            abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                            SDO->state = CO_SDO_ST_ABORT;
                        }
                    }
                    else {
                        /* If data type is string, size is not known */
                        SDO->sizeInd = (SDO->OD_IO.stream.attribute&ODA_STR)==0
                                     ? SDO->OD_IO.stream.dataLength
                                     : 0;
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

                /* Size of OD variable (>0 if indicated) */
                OD_size_t sizeInOd = SDO->OD_IO.stream.dataLength;

                /* Get SDO data size (indicated by SDO client or get from OD) */
                OD_size_t dataSizeToWrite = 4;
                if (SDO->CANrxData[0] & 0x01)
                    dataSizeToWrite -= (SDO->CANrxData[0] >> 2) & 0x03;
                else if (sizeInOd > 0 && sizeInOd < 4)
                    dataSizeToWrite = sizeInOd;

                /* copy data to the temp buffer, swap data if necessary */
                uint8_t buf[6] = {0};
                memcpy(buf, &SDO->CANrxData[4], dataSizeToWrite);
#ifdef CO_BIG_ENDIAN
                if ((SDO->OD_IO.stream.attribute & ODA_MB) != 0) {
                    reverseBytes(buf, dataSizeToWrite);
                }
#endif

                /* If dataType is string, then size of data downloaded may be
                 * shorter as size of OD data buffer. If so, add two zero bytes
                 * to terminate (unicode) string. Shorten also OD data size,
                 * (temporary, send information about EOF into OD_IO.write) */
                if ((SDO->OD_IO.stream.attribute & ODA_STR) != 0
                    && (sizeInOd == 0 || dataSizeToWrite < sizeInOd)
                ) {
                    OD_size_t delta = sizeInOd - dataSizeToWrite;
                    dataSizeToWrite += delta == 1 ? 1 : 2;
                    SDO->OD_IO.stream.dataLength = dataSizeToWrite;
                }
                else if (sizeInOd == 0) {
                    SDO->OD_IO.stream.dataLength = dataSizeToWrite;
                }
                /* Verify if size of data downloaded matches data size in OD. */
                else if (dataSizeToWrite != sizeInOd) {
                    abortCode = (dataSizeToWrite > sizeInOd) ?
                                CO_SDO_AB_DATA_LONG : CO_SDO_AB_DATA_SHORT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }

                /* Copy data */
                OD_size_t countWritten = 0;
                bool_t lock = OD_mappable(&SDO->OD_IO.stream);

                if (lock) { CO_LOCK_OD(SDO->CANdevTx); }
                ODR_t odRet = SDO->OD_IO.write(&SDO->OD_IO.stream, buf,
                                               dataSizeToWrite, &countWritten);
                if (lock) { CO_UNLOCK_OD(SDO->CANdevTx); }

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
                    if (sizeInOd > 0) {
                        if (SDO->sizeInd > sizeInOd) {
                            abortCode = CO_SDO_AB_DATA_LONG;
                            SDO->state = CO_SDO_ST_ABORT;
                            break;
                        }
                        /* strings are allowed to be shorter */
                        else if (SDO->sizeInd < sizeInOd
                                 && (SDO->OD_IO.stream.attribute & ODA_STR) == 0
                        ) {
                            abortCode = CO_SDO_AB_DATA_SHORT;
                            SDO->state = CO_SDO_ST_ABORT;
                            break;
                        }
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
                SDO->finished = (SDO->CANrxData[0] & 0x01) != 0;

                /* verify and alternate toggle bit */
                uint8_t toggle = SDO->CANrxData[0] & 0x10;
                if (toggle != SDO->toggle) {
                    abortCode = CO_SDO_AB_TOGGLE_BIT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }

                /* get data size and write data to the buffer */
                OD_size_t count = 7 - ((SDO->CANrxData[0] >> 1) & 0x07);
                memcpy(SDO->buf + SDO->bufOffsetWr, &SDO->CANrxData[1], count);
                SDO->bufOffsetWr += count;
                SDO->sizeTran += count;

                /* if data size exceeds variable size, abort */
                if (SDO->OD_IO.stream.dataLength > 0
                    && SDO->sizeTran > SDO->OD_IO.stream.dataLength
                ) {
                    abortCode = CO_SDO_AB_DATA_LONG;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }

                /* if necessary, empty the buffer */
                if (SDO->finished
                    || (CO_CONFIG_SDO_SRV_BUFFER_SIZE - SDO->bufOffsetWr)<(7+2)
                ) {
                    if (!validateAndWriteToOD(SDO, &abortCode, 0, 0))
                        break;
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
                if (sizeInOd > 0) {
                    if (SDO->sizeInd > sizeInOd) {
                        abortCode = CO_SDO_AB_DATA_LONG;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
                    /* strings are allowed to be shorter */
                    else if (SDO->sizeInd < sizeInOd
                             && (SDO->OD_IO.stream.attribute & ODA_STR) == 0
                    ) {
                        abortCode = CO_SDO_AB_DATA_SHORT;
                        SDO->state = CO_SDO_ST_ABORT;
                        break;
                    }
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

                uint16_t crcClient = 0;
                if (SDO->block_crcEnabled) {
                    crcClient = ((uint16_t) SDO->CANrxData[2]) << 8;
                    crcClient |= SDO->CANrxData[1];
                }

                if (!validateAndWriteToOD(SDO, &abortCode, 2, crcClient))
                    break;

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
                    SDO->block_crc = crc16_ccitt(SDO->buf, SDO->bufOffsetWr, 0);
                }
                else {
                    SDO->block_crcEnabled = false;
                }

                /* get blksize and verify it */
                SDO->block_blksize = SDO->CANrxData[4];
                if (SDO->block_blksize < 1 || SDO->block_blksize > 127) {
                    abortCode = CO_SDO_AB_BLOCK_SIZE;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
                }

                /* verify, if there is enough data */
                if (!SDO->finished && SDO->bufOffsetWr < SDO->block_blksize*7U){
                    abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
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
                SDO->block_blksize = SDO->CANrxData[2];
                if (SDO->block_blksize < 1 || SDO->block_blksize > 127) {
                    abortCode = CO_SDO_AB_BLOCK_SIZE;
                    SDO->state = CO_SDO_ST_ABORT;
                    break;
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
                if (!readFromOd(SDO, &abortCode, SDO->block_blksize * 7, true))
                    break;


                if (SDO->bufOffsetWr == SDO->bufOffsetRd) {
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
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
        SDO->timeoutTimer = 0;
#endif
        timeDifference_us = 0;
        CO_FLAG_CLEAR(SDO->CANrxNew);
    } /* if (isNew) */

    /* Timeout timers and transmit bufferFull flag ****************************/
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
#if (CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_SEGMENTED
            SDO->timeoutTimer = 0;
#endif
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
            SDO->toggle = (SDO->toggle == 0x00) ? 0x10 : 0x00;

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
                SDO->CANtxBuff->data[0] = (uint8_t)(0x43|((4-SDO->sizeInd)<<2));
                memcpy(&SDO->CANtxBuff->data[4], &SDO->buf, SDO->sizeInd);
                SDO->state = CO_SDO_ST_IDLE;
                ret = CO_SDO_RT_ok_communicationEnd;
            }
            else {
                /* data will be transferred with segmented transfer */
                if (SDO->sizeInd > 0) {
                    /* indicate data size, if known */
                    uint32_t sizeInd = SDO->sizeInd;
                    uint32_t sizeIndSw = CO_SWAP_32(sizeInd);
                    SDO->CANtxBuff->data[0] = 0x41;
                    memcpy(&SDO->CANtxBuff->data[4],
                           &sizeIndSw, sizeof(sizeIndSw));
                }
                else {
                    SDO->CANtxBuff->data[0] = 0x40;
                }
                SDO->toggle = 0x00;
                SDO->timeoutTimer = 0;
                SDO->state = CO_SDO_ST_UPLOAD_SEGMENT_REQ;
            }
#else /* Expedited transfer only */
            /* load data from OD variable */
            OD_size_t count = 0;
            bool_t lock = OD_mappable(&SDO->OD_IO.stream);

            if (lock) { CO_LOCK_OD(SDO->CANdevTx); }
            ODR_t odRet = SDO->OD_IO.read(&SDO->OD_IO.stream,
                                          &SDO->CANtxBuff->data[4], 4, &count);
            if (lock) { CO_UNLOCK_OD(SDO->CANdevTx); }

            /* strings are allowed to be shorter */
            if (odRet == ODR_PARTIAL
                && (SDO->OD_IO.stream.attribute & ODA_STR) != 0
            ) {
                odRet = ODR_OK;
            }

            if (odRet != ODR_OK || count == 0) {
                abortCode = (odRet == ODR_OK) ? CO_SDO_AB_DEVICE_INCOMPAT :
                            (CO_SDO_abortCode_t)OD_getSDOabCode(odRet);
                SDO->state = CO_SDO_ST_ABORT;
                break;
            }

#ifdef CO_BIG_ENDIAN
            /* swap data if necessary */
            if ((SDO->OD_IO.stream.attribute & ODA_MB) != 0) {
                reverseBytes(buf, dataSizeToWrite);
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
            if (!readFromOd(SDO, &abortCode, 7, false))
                break;

            /* SDO command specifier with toggle bit */
            SDO->CANtxBuff->data[0] = SDO->toggle;
            SDO->toggle = (SDO->toggle == 0x00) ? 0x10 : 0x00;

            OD_size_t count = SDO->bufOffsetWr - SDO->bufOffsetRd;
            /* verify, if this is the last segment */
            if (count < 7 || (SDO->finished && count == 7)) {
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
                    ret = CO_SDO_RT_waitingResponse;
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
            OD_size_t count = (CO_CONFIG_SDO_SRV_BUFFER_SIZE-2) / 7;
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
#ifdef CO_DEBUG_SDO_SERVER
            bool_t transferShort = SDO->block_seqno != SDO->block_blksize;
            uint8_t seqnoStart = SDO->block_seqno;
#endif

            /* Is last segment? */
            if (SDO->finished) {
                SDO->state = CO_SDO_ST_DOWNLOAD_BLK_END_REQ;
            }
            else {
                /* calculate number of block segments from free buffer space */
                OD_size_t count;
                count = (CO_CONFIG_SDO_SRV_BUFFER_SIZE-2-SDO->bufOffsetWr)/7;
                if (count >= 127) {
                    count = 127;
                }
                else if (SDO->bufOffsetWr > 0) {
                    /* it is necessary to empty the buffer */
                    if (!validateAndWriteToOD(SDO, &abortCode, 1, 0))
                        break;

                    count =(CO_CONFIG_SDO_SRV_BUFFER_SIZE-2-SDO->bufOffsetWr)/7;
                    if (count >= 127) {
                        count = 127;
                    }
                }

                SDO->block_blksize = (uint8_t)count;
                SDO->block_seqno = 0;
                /* Block segments will be received in different thread. Make
                 * memory barrier here with CO_FLAG_CLEAR() call. */
                SDO->state = CO_SDO_ST_DOWNLOAD_BLK_SUBBLOCK_REQ;
                CO_FLAG_CLEAR(SDO->CANrxNew);
            }

            SDO->CANtxBuff->data[2] = SDO->block_blksize;

            /* reset block_timeoutTimer, but not SDO->timeoutTimer */
            SDO->block_timeoutTimer = 0;
            CO_CANsend(SDO->CANdevTx, SDO->CANtxBuff);
#ifdef CO_DEBUG_SDO_SERVER
            if (transferShort && !SDO->finished) {
                char msg[80];
                sprintf(msg,
                        "sub-block restarted: sequnoPrev=%02X, blksize=%02X",
                        seqnoStart, SDO->block_blksize);
                CO_DEBUG_SDO_SERVER(msg);
            }
#endif
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
            if (count < 7 || (SDO->finished && count == 7)) {
                SDO->CANtxBuff->data[0] |= 0x80;
            }
            else {
                count = 7;
            }

            /* copy data segment to CAN message */
            memcpy(&SDO->CANtxBuff->data[1], SDO->buf + SDO->bufOffsetRd,
                   count);
            SDO->bufOffsetRd += count;
            SDO->block_noData = (uint8_t)(7 - count);
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
