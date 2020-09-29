/*
 * CANopen SYNC object.
 *
 * @file        CO_SYNC.c
 * @ingroup     CO_SYNC
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
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


#include "301/CO_SDOserver.h"
#include "301/CO_Emergency.h"
#include "301/CO_NMT_Heartbeat.h"
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
    CO_SYNC_t *SYNC;
    CO_NMT_internalState_t operState;

    SYNC = (CO_SYNC_t*)object;   /* this is the correct pointer type of the first argument */
    operState = *SYNC->operatingState;

    if((operState == CO_NMT_OPERATIONAL) || (operState == CO_NMT_PRE_OPERATIONAL)){
        uint8_t DLC = CO_CANrxMsg_readDLC(msg);

        if(SYNC->counterOverflowValue == 0){
            if(DLC == 0U){
                CO_FLAG_SET(SYNC->CANrxNew);
            }
            else{
                SYNC->receiveError = (uint16_t)DLC | 0x0100U;
            }
        }
        else{
            if(DLC == 1U){
                uint8_t *data = CO_CANrxMsg_readData(msg);
                SYNC->counter = data[0];
                CO_FLAG_SET(SYNC->CANrxNew);
            }
            else{
                SYNC->receiveError = (uint16_t)DLC | 0x0200U;
            }
        }
        if(CO_FLAG_READ(SYNC->CANrxNew)) {
            SYNC->CANrxToggle = SYNC->CANrxToggle ? false : true;

#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_CALLBACK_PRE
            /* Optional signal to RTOS, which can resume task, which handles SYNC. */
            if(SYNC->pFunctSignalPre != NULL) {
                SYNC->pFunctSignalPre(SYNC->functSignalObjectPre);
            }
#endif
        }
    }
}


/*
 * Function for accessing _COB ID SYNC Message_ (index 0x1005) from SDO server.
 *
 * For more information see file CO_SDOserver.h.
 */
static CO_SDO_abortCode_t CO_ODF_1005(CO_ODF_arg_t *ODF_arg){
    CO_SYNC_t *SYNC;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    SYNC = (CO_SYNC_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading){
        uint8_t configureSyncProducer = 0;

        /* only 11-bit CAN identifier is supported */
        if(value & 0x20000000UL){
            ret = CO_SDO_AB_INVALID_VALUE;
        }
        else{
            /* is 'generate Sync messge' bit set? */
            if(value & 0x40000000UL){
                /* if bit was set before, value can not be changed */
                if(SYNC->isProducer){
                    ret = CO_SDO_AB_DATA_DEV_STATE;
                }
                else{
                    configureSyncProducer = 1;
                }
            }
        }

        /* configure sync producer */
        if(ret == CO_SDO_AB_NONE){
            SYNC->COB_ID = (uint16_t)(value & 0x7FFU);

            if(configureSyncProducer){
                uint8_t len = 0U;
                if(SYNC->counterOverflowValue != 0U){
                    len = 1U;
                    SYNC->counter = 0U;
                    SYNC->timer = 0U;
                }
                SYNC->CANtxBuff = CO_CANtxBufferInit(
                        SYNC->CANdevTx,         /* CAN device */
                        SYNC->CANdevTxIdx,      /* index of specific buffer inside CAN module */
                        SYNC->COB_ID,           /* CAN identifier */
                        0,                      /* rtr */
                        len,                    /* number of data bytes */
                        0);                     /* synchronous message flag bit */

                if (SYNC->CANtxBuff == NULL) {
                    ret = CO_SDO_AB_DATA_DEV_STATE;
                    SYNC->isProducer = false;
                } else {
                    SYNC->isProducer = true;
                }
            }
            else{
                SYNC->isProducer = false;
            }
        }

        /* configure sync consumer */
        if (ret == CO_SDO_AB_NONE) {
            CO_ReturnError_t CANret = CO_CANrxBufferInit(
                SYNC->CANdevRx,         /* CAN device */
                SYNC->CANdevRxIdx,      /* rx buffer index */
                SYNC->COB_ID,           /* CAN identifier */
                0x7FF,                  /* mask */
                0,                      /* rtr */
                (void*)SYNC,            /* object passed to receive function */
                CO_SYNC_receive);       /* this function will process received message */

            if (CANret != CO_ERROR_NO) {
                ret = CO_SDO_AB_DATA_DEV_STATE;
            }
        }
    }

    return ret;
}


/*
 * Function for accessing _Communication cycle period_ (index 0x1006) from SDO server.
 *
 * For more information see file CO_SDOserver.h.
 */
static CO_SDO_abortCode_t CO_ODF_1006(CO_ODF_arg_t *ODF_arg){
    CO_SYNC_t *SYNC;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    SYNC = (CO_SYNC_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading){
        /* period transition from 0 to something */
        if((SYNC->periodTime == 0) && (value != 0)){
            SYNC->counter = 0;
        }

        SYNC->periodTime = value;
        SYNC->periodTimeoutTime = (value / 2U) * 3U;
        /* overflow? */
        if(SYNC->periodTimeoutTime < value){
            SYNC->periodTimeoutTime = 0xFFFFFFFFUL;
        }

        SYNC->timer = 0;
    }

    return ret;
}


/**
 * Function for accessing _Synchronous counter overflow value_ (index 0x1019) from SDO server.
 *
 * For more information see file CO_SDOserver.h.
 */
static CO_SDO_abortCode_t CO_ODF_1019(CO_ODF_arg_t *ODF_arg){
    CO_SYNC_t *SYNC;
    uint8_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    SYNC = (CO_SYNC_t*) ODF_arg->object;
    value = ODF_arg->data[0];

    if(!ODF_arg->reading){
        uint8_t len = 0U;

        if(SYNC->periodTime){
            ret = CO_SDO_AB_DATA_DEV_STATE;
        }
        else if((value == 1) || (value > 240)){
            ret = CO_SDO_AB_INVALID_VALUE;
        }
        else{
            SYNC->counterOverflowValue = value;
            if(value != 0){
                len = 1U;
            }

            SYNC->CANtxBuff = CO_CANtxBufferInit(
                    SYNC->CANdevTx,         /* CAN device */
                    SYNC->CANdevTxIdx,      /* index of specific buffer inside CAN module */
                    SYNC->COB_ID,           /* CAN identifier */
                    0,                      /* rtr */
                    len,                    /* number of data bytes */
                    0);                     /* synchronous message flag bit */

            if (SYNC->CANtxBuff == NULL) {
                ret = CO_SDO_AB_DATA_DEV_STATE;
            }
        }
    }

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_SYNC_init(
        CO_SYNC_t              *SYNC,
        CO_EM_t                *em,
        CO_SDO_t               *SDO,
        CO_NMT_internalState_t *operatingState,
        uint32_t                COB_ID_SYNCMessage,
        uint32_t                communicationCyclePeriod,
        uint8_t                 synchronousCounterOverflowValue,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx)
{
    uint8_t len = 0;
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if(SYNC==NULL || em==NULL || SDO==NULL || operatingState==NULL ||
        CANdevRx==NULL || CANdevTx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    SYNC->isProducer = (COB_ID_SYNCMessage&0x40000000L) ? true : false;
    SYNC->COB_ID = COB_ID_SYNCMessage&0x7FF;

    SYNC->periodTime = communicationCyclePeriod;
    SYNC->periodTimeoutTime = communicationCyclePeriod / 2 * 3;
    /* overflow? */
    if(SYNC->periodTimeoutTime < communicationCyclePeriod) SYNC->periodTimeoutTime = 0xFFFFFFFFL;

    SYNC->counterOverflowValue = synchronousCounterOverflowValue;
    if(synchronousCounterOverflowValue) len = 1;

    SYNC->curentSyncTimeIsInsideWindow = true;

    CO_FLAG_CLEAR(SYNC->CANrxNew);
    SYNC->CANrxToggle = false;
    SYNC->timer = 0;
    SYNC->counter = 0;
    SYNC->receiveError = 0U;

    SYNC->em = em;
    SYNC->operatingState = operatingState;

    SYNC->CANdevRx = CANdevRx;
    SYNC->CANdevRxIdx = CANdevRxIdx;

#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_CALLBACK_PRE
    SYNC->pFunctSignalPre = NULL;
    SYNC->functSignalObjectPre = NULL;
#endif

    /* Configure Object dictionary entry at index 0x1005, 0x1006 and 0x1019 */
    CO_OD_configure(SDO, OD_H1005_COBID_SYNC,        CO_ODF_1005, (void*)SYNC, 0, 0);
    CO_OD_configure(SDO, OD_H1006_COMM_CYCL_PERIOD,  CO_ODF_1006, (void*)SYNC, 0, 0);
    CO_OD_configure(SDO, OD_H1019_SYNC_CNT_OVERFLOW, CO_ODF_1019, (void*)SYNC, 0, 0);

    /* configure SYNC CAN reception */
    ret = CO_CANrxBufferInit(
            CANdevRx,               /* CAN device */
            CANdevRxIdx,            /* rx buffer index */
            SYNC->COB_ID,           /* CAN identifier */
            0x7FF,                  /* mask */
            0,                      /* rtr */
            (void*)SYNC,            /* object passed to receive function */
            CO_SYNC_receive);       /* this function will process received message */

    /* configure SYNC CAN transmission */
    SYNC->CANdevTx = CANdevTx;
    SYNC->CANdevTxIdx = CANdevTxIdx;
    SYNC->CANtxBuff = CO_CANtxBufferInit(
            CANdevTx,               /* CAN device */
            CANdevTxIdx,            /* index of specific buffer inside CAN module */
            SYNC->COB_ID,           /* CAN identifier */
            0,                      /* rtr */
            len,                    /* number of data bytes */
            0);                     /* synchronous message flag bit */

    if (SYNC->CANtxBuff == NULL) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_SYNC_initCallbackPre(
        CO_SYNC_t              *SYNC,
        void                   *object,
        void                  (*pFunctSignalPre)(void *object))
{
    if(SYNC != NULL){
        SYNC->functSignalObjectPre = object;
        SYNC->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif

/******************************************************************************/
CO_ReturnError_t CO_SYNCsend(CO_SYNC_t *SYNC){
    if(++SYNC->counter > SYNC->counterOverflowValue) SYNC->counter = 1;
    SYNC->timer = 0;
    SYNC->CANrxToggle = SYNC->CANrxToggle ? false : true;
    SYNC->CANtxBuff->data[0] = SYNC->counter;
    return CO_CANsend(SYNC->CANdevTx, SYNC->CANtxBuff);
}

/******************************************************************************/
CO_SYNC_status_t CO_SYNC_process(
        CO_SYNC_t              *SYNC,
        uint32_t                timeDifference_us,
        uint32_t                ObjDict_synchronousWindowLength,
        uint32_t               *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    CO_SYNC_status_t ret = CO_SYNC_NONE;
    uint32_t timerNew;

    if(*SYNC->operatingState == CO_NMT_OPERATIONAL || *SYNC->operatingState == CO_NMT_PRE_OPERATIONAL){
        /* update sync timer, no overflow */
        timerNew = SYNC->timer + timeDifference_us;
        if(timerNew > SYNC->timer) SYNC->timer = timerNew;

        /* was SYNC just received */
        if(CO_FLAG_READ(SYNC->CANrxNew)){
            SYNC->timer = 0;
            ret = CO_SYNC_RECEIVED;
            CO_FLAG_CLEAR(SYNC->CANrxNew);
        }

        /* SYNC producer */
        if(SYNC->isProducer && SYNC->periodTime){
            if(SYNC->timer >= SYNC->periodTime){
                ret = CO_SYNC_RECEIVED;
                CO_SYNCsend(SYNC);
            }
#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_TIMERNEXT
            /* Calculate when next SYNC needs to be sent */
            if(timerNext_us != NULL){
                uint32_t diff = SYNC->periodTime - SYNC->timer;
                if(*timerNext_us > diff){
                    *timerNext_us = diff;
                }
            }
#endif
        }

        /* Synchronous PDOs are allowed only inside time window */
        if(ObjDict_synchronousWindowLength){
            if(SYNC->timer > ObjDict_synchronousWindowLength){
                if(SYNC->curentSyncTimeIsInsideWindow){
                    ret = CO_SYNC_OUTSIDE_WINDOW;
                }
                SYNC->curentSyncTimeIsInsideWindow = false;
            }
            else{
                SYNC->curentSyncTimeIsInsideWindow = true;
            }
        }
        else{
            SYNC->curentSyncTimeIsInsideWindow = true;
        }

        /* Verify timeout of SYNC */
        if(SYNC->periodTime && (*SYNC->operatingState == CO_NMT_OPERATIONAL || *SYNC->operatingState == CO_NMT_PRE_OPERATIONAL)){
            if(SYNC->timer > SYNC->periodTimeoutTime) {
                CO_errorReport(SYNC->em, CO_EM_SYNC_TIME_OUT, CO_EMC_COMMUNICATION, SYNC->timer);
            }
            else {
                CO_errorReset(SYNC->em, CO_EM_SYNC_TIME_OUT, CO_EMC_COMMUNICATION);
#if (CO_CONFIG_SYNC) & CO_CONFIG_FLAG_TIMERNEXT
                if(timerNext_us != NULL) {
                    uint32_t diff = SYNC->periodTimeoutTime - SYNC->timer;
                    if(*timerNext_us > diff){
                        *timerNext_us = diff;
                    }
                }
#endif
            }
        }
    }
    else {
        CO_FLAG_CLEAR(SYNC->CANrxNew);
    }

    /* verify error from receive function */
    if(SYNC->receiveError != 0U){
        CO_errorReport(SYNC->em, CO_EM_SYNC_LENGTH, CO_EMC_SYNC_DATA_LENGTH, (uint32_t)SYNC->receiveError);
        SYNC->receiveError = 0U;
    }

    return ret;
}

#endif /* (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE */
