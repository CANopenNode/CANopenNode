/*
 * CANopen SYNC object.
 *
 * @file        CO_SYNC.c
 * @ingroup     CO_SYNC
 * @author      Janez Paternoster
 * @copyright   2021 Janez Paternoster
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

#include "301/CO_SYNC.h"

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_SYNC_receive(void *object, void *msg) {
    CO_SYNC_t *SYNC = object;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    bool_t syncReceived = false;

    if (SYNC->counterOverflowValue == 0) {
        if (DLC == 0) {
            syncReceived = true;
        }
        else {
            SYNC->receiveError = DLC | 0x40;
        }
    }
    else {
        if (DLC == 1) {
            uint8_t *data = CO_CANrxMsg_readData(msg);
            SYNC->counter = data[0];
            syncReceived = true;
        }
        else {
            SYNC->receiveError = DLC | 0x80;
        }
    }

    if (syncReceived) {
        /* toggle PDO receive buffer */
        SYNC->CANrxToggle = SYNC->CANrxToggle ? false : true;

        CO_FLAG_SET(SYNC->CANrxNew);

#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_CALLBACK_PRE
        /* Optional signal to RTOS, which can resume task, which handles SYNC.*/
        if (SYNC->pFunctSignalPre != NULL) {
            SYNC->pFunctSignalPre(SYNC->functSignalObjectPre);
        }
#endif
    }
}


#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_OD_DYNAMIC
/*
 * Custom function for writing OD object "COB-ID sync message"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_1005(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if (stream == NULL || stream->subIndex != 0 || buf == NULL
        || count != sizeof(uint32_t) || countWritten == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_SYNC_t *SYNC = stream->object;
    uint32_t cobIdSync = CO_getUint32(buf);
    uint16_t CAN_ID = (uint16_t)(cobIdSync & 0x7FF);

    /* verify written value */
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
    bool_t isProducer = (cobIdSync & 0x40000000) != 0;
    if ((cobIdSync & 0xBFFFF800) != 0 || CO_IS_RESTRICTED_CAN_ID(CAN_ID)
        || (SYNC->isProducer && isProducer && CAN_ID != SYNC->CAN_ID)
    ) {
        return ODR_INVALID_VALUE;
    }
#else
    if ((cobIdSync & 0xFFFFF800) != 0 || CO_IS_RESTRICTED_CAN_ID(CAN_ID)) {
        return ODR_INVALID_VALUE;
    }
#endif

    /* Configure CAN receive and transmit buffers */
    if (CAN_ID != SYNC->CAN_ID) {
        CO_ReturnError_t CANret = CO_CANrxBufferInit(
            SYNC->CANdevRx,     /* CAN device */
            SYNC->CANdevRxIdx,  /* rx buffer index */
            CAN_ID,             /* CAN identifier */
            0x7FF,              /* mask */
            0,                  /* rtr */
            (void*)SYNC,        /* object passed to receive function */
            CO_SYNC_receive);   /* this function will process received message*/

        if (CANret != CO_ERROR_NO) {
            return ODR_DEV_INCOMPAT;
        }

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
        SYNC->CANtxBuff = CO_CANtxBufferInit(
            SYNC->CANdevTx,     /* CAN device */
            SYNC->CANdevTxIdx,  /* index of specific buffer inside CAN module */
            CAN_ID,             /* CAN identifier */
            0,                  /* rtr */
            SYNC->counterOverflowValue != 0 ? 1 : 0, /* number of data bytes */
            0);                 /* synchronous message flag bit */

        if (SYNC->CANtxBuff == NULL) {
            SYNC->isProducer = false;
            return ODR_DEV_INCOMPAT;
        }
#endif

        SYNC->CAN_ID = CAN_ID;
    }

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
    SYNC->isProducer = isProducer;
    if (isProducer) {
        SYNC->counter = 0;
        SYNC->timer = 0;
    }
#endif /* CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER */

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
/*
 * Custom function for writing OD object "Synchronous counter overflow value"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_1019(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if (stream == NULL || stream->subIndex != 0 || buf == NULL
        || count != sizeof(uint8_t) || countWritten == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_SYNC_t *SYNC = stream->object;
    uint8_t syncCounterOvf = CO_getUint8(buf);

    /* verify written value */
    if (syncCounterOvf == 1 || syncCounterOvf > 240) {
        return ODR_INVALID_VALUE;
    }
    if (*SYNC->OD_1006_period != 0) {
        return ODR_DATA_DEV_STATE;
    }

    /* Configure CAN transmit buffer */
    SYNC->CANtxBuff = CO_CANtxBufferInit(
        SYNC->CANdevTx,     /* CAN device */
        SYNC->CANdevTxIdx,  /* index of specific buffer inside CAN module */
        SYNC->CAN_ID,       /* CAN identifier */
        0,                  /* rtr */
        syncCounterOvf != 0 ? 1 : 0, /* number of data bytes */
        0);                 /* synchronous message flag bit */

    if (SYNC->CANtxBuff == NULL) {
        SYNC->isProducer = false;
        return ODR_DEV_INCOMPAT;
    }

    SYNC->counterOverflowValue = syncCounterOvf;

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}
#endif /* (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER */
#endif /* (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_OD_DYNAMIC */


/******************************************************************************/
CO_ReturnError_t CO_SYNC_init(CO_SYNC_t *SYNC,
                              CO_EM_t *em,
                              OD_entry_t *OD_1005_cobIdSync,
                              OD_entry_t *OD_1006_commCyclePeriod,
                              OD_entry_t *OD_1007_syncWindowLen,
                              OD_entry_t *OD_1019_syncCounterOvf,
                              CO_CANmodule_t *CANdevRx,
                              uint16_t CANdevRxIdx,
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
                              CO_CANmodule_t *CANdevTx,
                              uint16_t CANdevTxIdx,
#endif
                              uint32_t *errInfo)
{
    ODR_t odRet;

    /* verify arguments */
    if (SYNC == NULL || em == NULL || OD_1005_cobIdSync == NULL
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
        || OD_1006_commCyclePeriod == NULL || CANdevTx == NULL
#endif
        || CANdevRx == NULL
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear object */
    memset(SYNC, 0, sizeof(CO_SYNC_t));

    /* get and verify "COB-ID SYNC message" from OD and configure extension */
    uint32_t cobIdSync = 0x00000080;

    odRet = OD_get_u32(OD_1005_cobIdSync, 0, &cobIdSync, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1005_cobIdSync);
        return CO_ERROR_OD_PARAMETERS;
    }
#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_OD_DYNAMIC
    SYNC->OD_1005_extension.object = SYNC;
    SYNC->OD_1005_extension.read = OD_readOriginal;
    SYNC->OD_1005_extension.write = OD_write_1005;
    OD_extension_init(OD_1005_cobIdSync, &SYNC->OD_1005_extension);
#endif

    /* get and verify "Communication cycle period" from OD */
    SYNC->OD_1006_period = OD_getPtr(OD_1006_commCyclePeriod, 0,
                                     sizeof(uint32_t), NULL);
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
    if (SYNC->OD_1006_period == NULL) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1006_commCyclePeriod);
        return CO_ERROR_OD_PARAMETERS;
    }
#else
    if (OD_1006_commCyclePeriod != NULL && SYNC->OD_1006_period == NULL) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1006_commCyclePeriod);
        return CO_ERROR_OD_PARAMETERS;
    }
#endif

    /* get "Synchronous window length" from OD (optional parameter) */
    SYNC->OD_1007_window = OD_getPtr(OD_1007_syncWindowLen, 0,
                                     sizeof(uint32_t), NULL);
    if (OD_1007_syncWindowLen != NULL && SYNC->OD_1007_window == NULL) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1007_syncWindowLen);
        return CO_ERROR_OD_PARAMETERS;
    }

    /* get and verify optional "Synchronous counter overflow value" from OD and
     * configure extension */
    uint8_t syncCounterOvf = 0;

    if (OD_1019_syncCounterOvf != NULL) {
        odRet = OD_get_u8(OD_1019_syncCounterOvf, 0, &syncCounterOvf, true);
        if (odRet != ODR_OK) {
            if (errInfo != NULL) *errInfo = OD_getIndex(OD_1019_syncCounterOvf);
            return CO_ERROR_OD_PARAMETERS;
        }
        if (syncCounterOvf == 1) syncCounterOvf = 2;
        else if (syncCounterOvf > 240) syncCounterOvf = 240;

#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_OD_DYNAMIC
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
        SYNC->OD_1019_extension.object = SYNC;
        SYNC->OD_1019_extension.read = OD_readOriginal;
        SYNC->OD_1019_extension.write = OD_write_1019;
        OD_extension_init(OD_1019_syncCounterOvf, &SYNC->OD_1019_extension);
#endif
#endif
    }
    SYNC->counterOverflowValue = syncCounterOvf;

    /* Configure object variables */
    SYNC->em = em;
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
    SYNC->isProducer = (cobIdSync & 0x40000000) != 0;
#endif
#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_OD_DYNAMIC
    SYNC->CAN_ID = cobIdSync & 0x7FF;
    SYNC->CANdevRx = CANdevRx;
    SYNC->CANdevRxIdx = CANdevRxIdx;
 #if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
    SYNC->CANdevTx = CANdevTx;
    SYNC->CANdevTxIdx = CANdevTxIdx;
 #endif
#endif

    /* configure SYNC CAN reception and transmission */
    CO_ReturnError_t ret = CO_CANrxBufferInit(
            CANdevRx,           /* CAN device */
            CANdevRxIdx,        /* rx buffer index */
            cobIdSync & 0x7FF,  /* CAN identifier */
            0x7FF,              /* mask */
            0,                  /* rtr */
            (void*)SYNC,        /* object passed to receive function */
            CO_SYNC_receive);   /* this function will process received message*/
    if (ret != CO_ERROR_NO)
        return ret;

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
    SYNC->CANtxBuff = CO_CANtxBufferInit(
            CANdevTx,           /* CAN device */
            CANdevTxIdx,        /* index of specific buffer inside CAN module */
            cobIdSync & 0x7FF,  /* CAN identifier */
            0,                  /* rtr */
            syncCounterOvf != 0 ? 1 : 0, /* number of data bytes */
            0);                 /* synchronous message flag bit */

    if (SYNC->CANtxBuff == NULL)
        return CO_ERROR_ILLEGAL_ARGUMENT;
#endif

    return CO_ERROR_NO;
}


#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_SYNC_initCallbackPre(
        CO_SYNC_t              *SYNC,
        void                   *object,
        void                  (*pFunctSignalPre)(void *object))
{
    if (SYNC != NULL) {
        SYNC->functSignalObjectPre = object;
        SYNC->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif


/******************************************************************************/
CO_SYNC_status_t CO_SYNC_process(CO_SYNC_t *SYNC,
                                 bool_t NMTisPreOrOperational,
                                 uint32_t timeDifference_us,
                                 uint32_t *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    CO_SYNC_status_t syncStatus = CO_SYNC_NONE;

    if (NMTisPreOrOperational) {
        /* update sync timer, no overflow */
        uint32_t timerNew = SYNC->timer + timeDifference_us;
        if (timerNew > SYNC->timer) SYNC->timer = timerNew;

        /* was SYNC just received */
        if (CO_FLAG_READ(SYNC->CANrxNew)) {
            SYNC->timer = 0;
            syncStatus = CO_SYNC_RX_TX;
            CO_FLAG_CLEAR(SYNC->CANrxNew);
        }

        uint32_t OD_1006_period = SYNC->OD_1006_period != NULL
                                ? *SYNC->OD_1006_period : 0;

        if (OD_1006_period > 0) {
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
            if (SYNC->isProducer) {
                if (SYNC->timer >= OD_1006_period) {
                    syncStatus = CO_SYNC_RX_TX;
                    CO_SYNCsend(SYNC);
                }
 #if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_TIMERNEXT
                /* Calculate when next SYNC needs to be sent */
                if (timerNext_us != NULL) {
                    uint32_t diff = OD_1006_period - SYNC->timer;
                    if (*timerNext_us > diff) {
                        *timerNext_us = diff;
                    }
                }
 #endif
            }
            else
#endif /* (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER */

            /* Verify timeout of SYNC */
            if (SYNC->timeoutError == 1) {
                /* periodTimeout is 1,5 * OD_1006_period, no overflow */
                uint32_t periodTimeout = OD_1006_period + (OD_1006_period >> 1);
                if (periodTimeout < OD_1006_period) periodTimeout = 0xFFFFFFFF;

                if (SYNC->timer > periodTimeout) {
                    CO_errorReport(SYNC->em, CO_EM_SYNC_TIME_OUT,
                                   CO_EMC_COMMUNICATION, SYNC->timer);
                    SYNC->timeoutError = 2;
                }
#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_TIMERNEXT
                else if (timerNext_us != NULL) {
                    uint32_t diff = periodTimeout - SYNC->timer;
                    if (*timerNext_us > diff) {
                        *timerNext_us = diff;
                    }
                }
#endif
            }
        } /* if (OD_1006_period > 0) */

        /* Synchronous PDOs are allowed only inside time window */
        if (SYNC->OD_1007_window != NULL && *SYNC->OD_1007_window > 0
            && SYNC->timer > *SYNC->OD_1007_window
        ) {
            if (!SYNC->syncIsOutsideWindow) {
                syncStatus = CO_SYNC_PASSED_WINDOW;
            }
            SYNC->syncIsOutsideWindow = true;
        }
        else {
            SYNC->syncIsOutsideWindow = false;
        }

        /* verify error from receive function */
        if (SYNC->receiveError != 0) {
            CO_errorReport(SYNC->em, CO_EM_SYNC_LENGTH,
                           CO_EMC_SYNC_DATA_LENGTH, SYNC->receiveError);
            SYNC->receiveError = 0;
        }
    } /* if (NMTisPreOrOperational) */
    else {
        CO_FLAG_CLEAR(SYNC->CANrxNew);
        SYNC->receiveError = 0;
        SYNC->counter = 0;
        SYNC->timer = 0;
    }

    if (syncStatus == CO_SYNC_RX_TX) {
        if (SYNC->timeoutError == 2) {
            CO_errorReset(SYNC->em, CO_EM_SYNC_TIME_OUT, 0);
        }
        SYNC->timeoutError = 1;
    }

    return syncStatus;
}

#endif /* (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE */
