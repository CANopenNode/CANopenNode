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

#include "301/CO_SDOserver.h"
#include "301/CO_Emergency.h"
#include "301/CO_NMT_Heartbeat.h"
#include "301/CO_TIME.h"

#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE

#define DIV_ROUND_UP(_n, _d) (((_n) + (_d) - 1) / (_d))

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received.
 */
static void CO_TIME_receive(void *object, void *msg){
    CO_TIME_t *TIME;
    CO_NMT_internalState_t operState;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    TIME = (CO_TIME_t*)object;   /* this is the correct pointer type of the first argument */
    operState = *TIME->operatingState;

    if((operState == CO_NMT_OPERATIONAL) || (operState == CO_NMT_PRE_OPERATIONAL)){
        // Process Time from msg buffer
        memcpy(&TIME->Time.ullValue, data, DLC);
        CO_FLAG_SET(TIME->CANrxNew);

#if (CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE
        /* Optional signal to RTOS, which can resume task, which handles TIME. */
        if(TIME->pFunctSignalPre != NULL) {
            TIME->pFunctSignalPre(TIME->functSignalObjectPre);
        }
#endif
    }
    else{
        TIME->receiveError = (uint16_t)DLC;
    }
}

/******************************************************************************/
CO_ReturnError_t CO_TIME_init(
        CO_TIME_t              *TIME,
        CO_EM_t                *em,
        CO_SDO_t               *SDO,
        CO_NMT_internalState_t *operatingState,
        uint32_t                COB_ID_TIMEMessage,
        uint32_t                TIMECyclePeriod,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if(TIME==NULL || em==NULL || SDO==NULL || operatingState==NULL ||
    CANdevRx==NULL || CANdevTx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    TIME->isConsumer = (COB_ID_TIMEMessage&0x80000000L) ? true : false;
    TIME->isProducer = (COB_ID_TIMEMessage&0x40000000L) ? true : false;
    TIME->COB_ID = COB_ID_TIMEMessage&0x7FF; // 11 bit ID

    TIME->periodTime = TIMECyclePeriod;
    TIME->periodTimeoutTime = TIMECyclePeriod / 2 * 3;
    /* overflow? */
    if(TIME->periodTimeoutTime < TIMECyclePeriod)
        TIME->periodTimeoutTime = 0xFFFFFFFFL;

    CO_FLAG_CLEAR(TIME->CANrxNew);
    TIME->timer = 0;
    TIME->receiveError = 0U;

    TIME->em = em;
    TIME->operatingState = operatingState;

#if (CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE
    TIME->pFunctSignalPre = NULL;
    TIME->functSignalObjectPre = NULL;
#endif


    /* configure TIME consumer message reception */
    TIME->CANdevRx = CANdevRx;
    TIME->CANdevRxIdx = CANdevRxIdx;
	if (TIME->isConsumer) {
        ret = CO_CANrxBufferInit(
                CANdevRx,               /* CAN device */
                CANdevRxIdx,            /* rx buffer index */
                TIME->COB_ID,           /* CAN identifier */
                0x7FF,                  /* mask */
                0,                      /* rtr */
                (void*)TIME,            /* object passed to receive function */
                CO_TIME_receive);       /* this function will process received message */
    }

    /* configure TIME producer message transmission */
    TIME->CANdevTx = CANdevTx;
    TIME->CANdevTxIdx = CANdevTxIdx;
    if (TIME->isProducer) {
        TIME->TXbuff = CO_CANtxBufferInit(
            CANdevTx,               /* CAN device */
            CANdevTxIdx,            /* index of specific buffer inside CAN module */
            TIME->COB_ID,           /* CAN identifier */
            0,                      /* rtr */
            TIME_MSG_LENGTH,        /* number of data bytes */
            0);                     /* synchronous message flag bit */

        if (TIME->TXbuff == NULL) {
            ret = CO_ERROR_ILLEGAL_ARGUMENT;
        }
    }

    return ret;
}

#if (CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_TIME_initCallbackPre(
        CO_TIME_t              *TIME,
        void                   *object,
        void                  (*pFunctSignalPre)(void *object))
{
    if(TIME != NULL){
        TIME->functSignalObjectPre = object;
        TIME->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif

/******************************************************************************/
uint8_t CO_TIME_process(
        CO_TIME_t              *TIME,
        uint32_t                timeDifference_us)
{
    uint8_t ret = 0;
    uint32_t timerNew;

    if(*TIME->operatingState == CO_NMT_OPERATIONAL || *TIME->operatingState == CO_NMT_PRE_OPERATIONAL){
        /* update TIME timer, no overflow */
        uint32_t timeDifference_ms = DIV_ROUND_UP(timeDifference_us, 1000);
        timerNew = TIME->timer + timeDifference_ms;
        if(timerNew > TIME->timer)
            TIME->timer = timerNew;

        /* was TIME just received */
        if(CO_FLAG_READ(TIME->CANrxNew)){
            TIME->timer = 0;
            ret = 1;
            CO_FLAG_CLEAR(TIME->CANrxNew);
        }

        /* TIME producer */
        if(TIME->isProducer && TIME->periodTime){
            if(TIME->timer >= TIME->periodTime){
                TIME->timer = 0;
                ret = 1;
                memcpy(TIME->TXbuff->data, &TIME->Time.ullValue, TIME_MSG_LENGTH);
                CO_CANsend(TIME->CANdevTx, TIME->TXbuff);
            }
        }

        /* Verify TIME timeout if node is consumer */
        if(TIME->isConsumer && TIME->periodTime && TIME->timer > TIME->periodTimeoutTime
        && *TIME->operatingState == CO_NMT_OPERATIONAL)
            CO_errorReport(TIME->em, CO_EM_TIME_TIMEOUT, CO_EMC_COMMUNICATION, TIME->timer);
    }
    else {
        CO_FLAG_CLEAR(TIME->CANrxNew);
    }

    /* verify error from receive function */
    if(TIME->receiveError != 0U){
        CO_errorReport(TIME->em, CO_EM_TIME_LENGTH, CO_EMC_TIME_DATA_LENGTH, (uint32_t)TIME->receiveError);
        TIME->receiveError = 0U;
    }

    return ret;
}

#endif /* (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE */
