/*
 * CANopen TIME object.
 *
 * @file        CO_TIME.c
 * @ingroup     CO_TIME
 * @author      Julien PEYREGNE
 * @copyright   2019 - 2020 Janez Paternoster
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

#include "301/CO_TIME.h"

#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_TIME_receive(void *object, void *msg) {
    CO_TIME_t *TIME = object;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    if (DLC == CO_TIME_MSG_LENGTH) {
        memcpy(TIME->timeStamp, data, sizeof(TIME->timeStamp));
        CO_FLAG_SET(TIME->CANrxNew);

#if (CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE
        /* Optional signal to RTOS, which can resume task, which handles TIME.*/
        if (TIME->pFunctSignalPre != NULL) {
            TIME->pFunctSignalPre(TIME->functSignalObjectPre);
        }
#endif
    }
}


#if (CO_CONFIG_TIME) & CO_CONFIG_FLAG_OD_DYNAMIC
/*
 * Custom function for writing OD object "COB-ID time stamp"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_1012(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if (stream == NULL || stream->subIndex != 0 || buf == NULL
        || count != sizeof(uint32_t) || countWritten == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_TIME_t *TIME = stream->object;

    /* verify written value */
    uint32_t cobIdTimeStamp = CO_getUint32(buf);
    uint16_t CAN_ID = cobIdTimeStamp & 0x7FF;
    if ((cobIdTimeStamp & 0x3FFFF800) != 0 || CO_IS_RESTRICTED_CAN_ID(CAN_ID)) {
        return ODR_INVALID_VALUE;
    }

    /* update object */
    TIME->isConsumer = (cobIdTimeStamp & 0x80000000L) != 0;
    TIME->isProducer = (cobIdTimeStamp & 0x40000000L) != 0;

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}
#endif


CO_ReturnError_t CO_TIME_init(CO_TIME_t *TIME,
                              OD_entry_t *OD_1012_cobIdTimeStamp,
                              CO_CANmodule_t *CANdevRx,
                              uint16_t CANdevRxIdx,
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER
                              CO_CANmodule_t *CANdevTx,
                              uint16_t CANdevTxIdx,
#endif
                              uint32_t *errInfo)
{
    /* verify arguments */
    if (TIME == NULL || OD_1012_cobIdTimeStamp == NULL || CANdevRx == NULL
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER
        || CANdevTx == NULL
#endif
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    memset(TIME, 0, sizeof(CO_TIME_t));

    /* get parameters from object dictionary and configure extension */
    uint32_t cobIdTimeStamp;
    ODR_t odRet = OD_get_u32(OD_1012_cobIdTimeStamp, 0, &cobIdTimeStamp, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) *errInfo = OD_getIndex(OD_1012_cobIdTimeStamp);
        return CO_ERROR_OD_PARAMETERS;
    }
#if (CO_CONFIG_TIME) & CO_CONFIG_FLAG_OD_DYNAMIC
    TIME->OD_1012_extension.object = TIME;
    TIME->OD_1012_extension.read = OD_readOriginal;
    TIME->OD_1012_extension.write = OD_write_1012;
    OD_extension_init(OD_1012_cobIdTimeStamp, &TIME->OD_1012_extension);
#endif

    /* Configure object variables */
    uint16_t cobId = cobIdTimeStamp & 0x7FF;
    TIME->isConsumer = (cobIdTimeStamp & 0x80000000L) != 0;
    TIME->isProducer = (cobIdTimeStamp & 0x40000000L) != 0;
    CO_FLAG_CLEAR(TIME->CANrxNew);

    /* configure TIME consumer message reception */
	if (TIME->isConsumer) {
        CO_ReturnError_t ret = CO_CANrxBufferInit(
                CANdevRx,       /* CAN device */
                CANdevRxIdx,    /* rx buffer index */
                cobId,          /* CAN identifier */
                0x7FF,          /* mask */
                0,              /* rtr */
                (void*)TIME,    /* object passed to receive function */
                CO_TIME_receive);/*this function will process received message*/
        if (ret != CO_ERROR_NO)
            return ret;
    }

#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER
    /* configure TIME producer message transmission */
    TIME->CANdevTx = CANdevTx;
    TIME->CANtxBuff = CO_CANtxBufferInit(
            CANdevTx,           /* CAN device */
            CANdevTxIdx,        /* index of specific buffer inside CAN module */
            cobId,              /* CAN identifier */
            0,                  /* rtr */
            CO_TIME_MSG_LENGTH, /* number of data bytes */
            0);                 /* synchronous message flag bit */

    if (TIME->CANtxBuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
#endif

    return CO_ERROR_NO;
}


#if (CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE
void CO_TIME_initCallbackPre(CO_TIME_t *TIME,
                             void *object,
                             void (*pFunctSignalPre)(void *object))
{
    if (TIME != NULL) {
        TIME->functSignalObjectPre = object;
        TIME->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif


bool_t CO_TIME_process(CO_TIME_t *TIME,
                       bool_t NMTisPreOrOperational,
                       uint32_t timeDifference_us)
{
    bool_t timestampReceived = false;

    /* Was TIME stamp message just received */
    if (NMTisPreOrOperational && TIME->isConsumer) {
        if(CO_FLAG_READ(TIME->CANrxNew)) {
            uint32_t ms_swapped = CO_getUint32(&TIME->timeStamp[0]);
            uint16_t days_swapped = CO_getUint16(&TIME->timeStamp[4]);
            TIME->ms = CO_SWAP_32(ms_swapped) & 0x0FFFFFFF;
            TIME->days = CO_SWAP_16(days_swapped);
            TIME->residual_us = 0;
            timestampReceived = true;

            CO_FLAG_CLEAR(TIME->CANrxNew);
        }
    }
    else {
        CO_FLAG_CLEAR(TIME->CANrxNew);
    }

    /* Update time */
    uint32_t ms = 0;
    if (!timestampReceived && timeDifference_us > 0) {
        uint32_t us = timeDifference_us + TIME->residual_us;
        ms = us / 1000;
        TIME->residual_us = us % 1000;
        TIME->ms += ms;
        if (TIME->ms >= ((uint32_t)1000*60*60*24)) {
            TIME->ms -= ((uint32_t)1000*60*60*24);
            TIME->days += 1;
        }
    }

#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER
    if (NMTisPreOrOperational && TIME->isProducer
        && TIME->producerInterval_ms > 0
    ) {
        if (TIME->producerTimer_ms >= TIME->producerInterval_ms) {
            TIME->producerTimer_ms -= TIME->producerInterval_ms;

            uint32_t ms_swapped = CO_SWAP_32(TIME->ms);
            uint16_t days_swapped = CO_SWAP_16(TIME->days);
            CO_setUint32(&TIME->CANtxBuff->data[0], ms_swapped);
            CO_setUint16(&TIME->CANtxBuff->data[4], days_swapped);
            CO_CANsend(TIME->CANdevTx, TIME->CANtxBuff);
        }
        else {
            TIME->producerTimer_ms += ms;
        }
    }
    else {
        TIME->producerTimer_ms = TIME->producerInterval_ms;
    }
#endif

    return timestampReceived;
}

#endif /* (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE */
