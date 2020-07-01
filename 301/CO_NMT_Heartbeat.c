/*
 * CANopen NMT and Heartbeat producer object.
 *
 * @file        CO_NMT_Heartbeat.c
 * @ingroup     CO_NMT_Heartbeat
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

#include "301/CO_NMT_Heartbeat.h"


/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_NMT_receive(void *object, void *msg){
    CO_NMT_t *NMT;
    uint8_t nodeId;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    NMT = (CO_NMT_t*)object;   /* this is the correct pointer type of the first argument */

    nodeId = data[1];

    if((DLC == 2) && ((nodeId == 0) || (nodeId == NMT->nodeId))){
        uint8_t command = data[0];
#if (CO_CONFIG_NMT) & (CO_CONFIG_NMT_CALLBACK_CHANGE | CO_CONFIG_FLAG_CALLBACK_PRE)
        CO_NMT_internalState_t currentOperatingState = NMT->operatingState;
#endif

        switch(command){
            case CO_NMT_ENTER_OPERATIONAL:
                if((*NMT->emPr->errorRegister) == 0U){
                    NMT->operatingState = CO_NMT_OPERATIONAL;
                }
                break;
            case CO_NMT_ENTER_STOPPED:
                NMT->operatingState = CO_NMT_STOPPED;
                break;
            case CO_NMT_ENTER_PRE_OPERATIONAL:
                NMT->operatingState = CO_NMT_PRE_OPERATIONAL;
                break;
            case CO_NMT_RESET_NODE:
                NMT->resetCommand = CO_RESET_APP;
                break;
            case CO_NMT_RESET_COMMUNICATION:
                NMT->resetCommand = CO_RESET_COMM;
                break;
            default:
                break;
        }

#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE
        if(NMT->pFunctNMT!=NULL && currentOperatingState!=NMT->operatingState){
            NMT->pFunctNMT(NMT->operatingState);
        }
#endif
#if (CO_CONFIG_NMT) & CO_CONFIG_FLAG_CALLBACK_PRE
    /* Optional signal to RTOS, which can resume task, which handles NMT. */
    if(NMT->pFunctSignalPre != NULL && currentOperatingState!=NMT->operatingState) {
        NMT->pFunctSignalPre(NMT->functSignalObjectPre);
    }
#endif
    }
}


/******************************************************************************/
CO_ReturnError_t CO_NMT_init(
        CO_NMT_t               *NMT,
        CO_EMpr_t              *emPr,
        uint8_t                 nodeId,
        uint16_t                firstHBTime_ms,
        CO_CANmodule_t         *NMT_CANdevRx,
        uint16_t                NMT_rxIdx,
        uint16_t                CANidRxNMT,
        CO_CANmodule_t         *NMT_CANdevTx,
        uint16_t                NMT_txIdx,
        uint16_t                CANidTxNMT,
        CO_CANmodule_t         *HB_CANdevTx,
        uint16_t                HB_txIdx,
        uint16_t                CANidTxHB)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if (NMT == NULL || emPr == NULL || NMT_CANdevRx == NULL || HB_CANdevTx == NULL
#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER
        || NMT_CANdevTx == NULL
#endif
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear the object */
    memset(NMT, 0, sizeof(CO_NMT_t));

    /* Configure object variables */
    NMT->operatingState         = CO_NMT_INITIALIZING;
    NMT->operatingStatePrev     = CO_NMT_INITIALIZING;
    NMT->nodeId                 = nodeId;
    NMT->firstHBTime            = (int32_t)firstHBTime_ms * 1000;
    NMT->emPr                   = emPr;

    /* configure NMT CAN reception */
    ret = CO_CANrxBufferInit(
            NMT_CANdevRx,       /* CAN device */
            NMT_rxIdx,          /* rx buffer index */
            CANidRxNMT,         /* CAN identifier */
            0x7FF,              /* mask */
            0,                  /* rtr */
            (void*)NMT,         /* object passed to receive function */
            CO_NMT_receive);    /* this function will process received message */

#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER
    /* configure NMT CAN transmission */
    NMT->NMT_CANdevTx = NMT_CANdevTx;
    NMT->NMT_TXbuff = CO_CANtxBufferInit(
            NMT_CANdevTx,       /* CAN device */
            NMT_txIdx,          /* index of specific buffer inside CAN module */
            CANidTxNMT,         /* CAN identifier */
            0,                  /* rtr */
            2,                  /* number of data bytes */
            0);                 /* synchronous message flag bit */
    if (NMT->NMT_TXbuff == NULL) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }
#endif

    /* configure HB CAN transmission */
    NMT->HB_CANdevTx = HB_CANdevTx;
    NMT->HB_TXbuff = CO_CANtxBufferInit(
            HB_CANdevTx,        /* CAN device */
            HB_txIdx,           /* index of specific buffer inside CAN module */
            CANidTxHB,          /* CAN identifier */
            0,                  /* rtr */
            1,                  /* number of data bytes */
            0);                 /* synchronous message flag bit */

    if (NMT->HB_TXbuff == NULL) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


#if (CO_CONFIG_NMT) & CO_CONFIG_FLAG_CALLBACK_PRE
void CO_NMT_initCallbackPre(
        CO_NMT_t               *NMT,
        void                   *object,
        void                  (*pFunctSignal)(void *object))
{
    if (NMT != NULL) {
        NMT->pFunctSignalPre = pFunctSignal;
        NMT->functSignalObjectPre = object;
    }
}
#endif


#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE
/******************************************************************************/
void CO_NMT_initCallbackChanged(
        CO_NMT_t               *NMT,
        void                  (*pFunctNMT)(CO_NMT_internalState_t state))
{
    if(NMT != NULL){
        NMT->pFunctNMT = pFunctNMT;
        if(NMT->pFunctNMT != NULL){
            NMT->pFunctNMT(NMT->operatingState);
        }
    }
}
#endif


/******************************************************************************/
CO_NMT_reset_cmd_t CO_NMT_process(
        CO_NMT_t               *NMT,
        uint32_t                timeDifference_us,
        uint16_t                HBtime_ms,
        uint32_t                NMTstartup,
        uint8_t                 errorRegister,
        const uint8_t           errorBehavior[],
        uint32_t               *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    uint8_t CANpassive;
    CO_NMT_internalState_t currentOperatingState = NMT->operatingState;
    uint32_t HBtime = (uint32_t)HBtime_ms * 1000;

    NMT->HBproducerTimer += timeDifference_us;

    /* Send heartbeat producer message if:
     * - First start, send bootup message or
     * - HB producer and Timer expired or
     * - HB producer and NMT->operatingState changed, but not from initialised */
    if ((NMT->operatingState == CO_NMT_INITIALIZING) ||
        (HBtime != 0 && (NMT->HBproducerTimer >= HBtime ||
                         NMT->operatingState != NMT->operatingStatePrev)
    )) {
        /* Start from the beginning. If OS is slow, time sliding may occur. However,
         * heartbeat is not for synchronization, it is for health report. */
        NMT->HBproducerTimer = 0;

        NMT->HB_TXbuff->data[0] = (uint8_t) NMT->operatingState;
        CO_CANsend(NMT->HB_CANdevTx, NMT->HB_TXbuff);

        if (NMT->operatingState == CO_NMT_INITIALIZING) {
            /* After bootup messages send first heartbeat earlier */
            if (HBtime > NMT->firstHBTime) {
                NMT->HBproducerTimer = HBtime - NMT->firstHBTime;
            }

            /* NMT slave self starting */
            if (NMTstartup == 0x00000008U) NMT->operatingState = CO_NMT_OPERATIONAL;
            else                           NMT->operatingState = CO_NMT_PRE_OPERATIONAL;
        }
    }
    NMT->operatingStatePrev = NMT->operatingState;

    /* CAN passive flag */
    CANpassive = 0;
    if(CO_isError(NMT->emPr->em, CO_EM_CAN_TX_BUS_PASSIVE) || CO_isError(NMT->emPr->em, CO_EM_CAN_RX_BUS_PASSIVE))
        CANpassive = 1;

    /* in case of error enter pre-operational state */
    if(errorBehavior && (NMT->operatingState == CO_NMT_OPERATIONAL)){
        if(CANpassive && (errorBehavior[2] == 0 || errorBehavior[2] == 2)) errorRegister |= 0x10;

        if(errorRegister){
            /* Communication error */
            if(errorRegister & CO_ERR_REG_COMM_ERR){
                if(errorBehavior[1] == 0){
                    NMT->operatingState = CO_NMT_PRE_OPERATIONAL;
                }
                else if(errorBehavior[1] == 2){
                    NMT->operatingState = CO_NMT_STOPPED;
                }
                else if(CO_isError(NMT->emPr->em, CO_EM_CAN_TX_BUS_OFF)
                     || CO_isError(NMT->emPr->em, CO_EM_HEARTBEAT_CONSUMER)
                     || CO_isError(NMT->emPr->em, CO_EM_HB_CONSUMER_REMOTE_RESET))
                {
                    if(errorBehavior[0] == 0){
                        NMT->operatingState = CO_NMT_PRE_OPERATIONAL;
                    }
                    else if(errorBehavior[0] == 2){
                        NMT->operatingState = CO_NMT_STOPPED;
                    }
                }
            }

            /* Generic error */
            if(errorRegister & CO_ERR_REG_GENERIC_ERR){
                if      (errorBehavior[3] == 0) NMT->operatingState = CO_NMT_PRE_OPERATIONAL;
                else if (errorBehavior[3] == 2) NMT->operatingState = CO_NMT_STOPPED;
            }

            /* Device profile error */
            if(errorRegister & CO_ERR_REG_DEV_PROFILE){
                if      (errorBehavior[4] == 0) NMT->operatingState = CO_NMT_PRE_OPERATIONAL;
                else if (errorBehavior[4] == 2) NMT->operatingState = CO_NMT_STOPPED;
            }

            /* Manufacturer specific error */
            if(errorRegister & CO_ERR_REG_MANUFACTURER){
                if      (errorBehavior[5] == 0) NMT->operatingState = CO_NMT_PRE_OPERATIONAL;
                else if (errorBehavior[5] == 2) NMT->operatingState = CO_NMT_STOPPED;
            }

            /* if operational state is lost, send HB immediately. */
            if(NMT->operatingState != CO_NMT_OPERATIONAL)
                NMT->HBproducerTimer = HBtime;
        }
    }

    if (currentOperatingState != NMT->operatingState) {
#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE
        if (NMT->pFunctNMT != NULL) {
            NMT->pFunctNMT(NMT->operatingState);
        }
#endif
#if (CO_CONFIG_NMT) & CO_CONFIG_FLAG_TIMERNEXT
        /* execute next CANopen processing immediately */
        if (timerNext_us != NULL) {
            *timerNext_us = 0;
        }
#endif
    }

#if (CO_CONFIG_NMT) & CO_CONFIG_FLAG_TIMERNEXT
    /* Calculate, when next Heartbeat needs to be send and lower timerNext_us if necessary. */
    if (HBtime != 0 && timerNext_us != NULL) {
        if (NMT->HBproducerTimer < HBtime) {
            uint32_t diff = HBtime - NMT->HBproducerTimer;
            if (*timerNext_us > diff) {
                *timerNext_us = diff;
            }
        } else {
            *timerNext_us = 0;
        }
    }
#endif

    return (CO_NMT_reset_cmd_t) NMT->resetCommand;
}


#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER
/******************************************************************************/
CO_ReturnError_t CO_NMT_sendCommand(CO_NMT_t *NMT,
                                    CO_NMT_command_t command,
                                    uint8_t nodeID)
{
    CO_ReturnError_t error = CO_ERROR_NO;

    /* verify arguments */
    if (NMT == NULL) {
        error = CO_ERROR_TX_UNCONFIGURED;
    }

    /* Apply NMT command also to this node, if set so. */
    if (error == CO_ERROR_NO && (nodeID == 0 || nodeID == NMT->nodeId)) {
        switch (command) {
        case CO_NMT_ENTER_OPERATIONAL:
            if ((*NMT->emPr->errorRegister) == 0) {
                NMT->operatingState = CO_NMT_OPERATIONAL;
            }
            break;
        case CO_NMT_ENTER_STOPPED:
            NMT->operatingState = CO_NMT_STOPPED;
            break;
        case CO_NMT_ENTER_PRE_OPERATIONAL:
            NMT->operatingState = CO_NMT_PRE_OPERATIONAL;
            break;
        case CO_NMT_RESET_NODE:
            NMT->resetCommand = CO_RESET_APP;
            break;
        case CO_NMT_RESET_COMMUNICATION:
            NMT->resetCommand = CO_RESET_COMM;
            break;
        default:
            error = CO_ERROR_ILLEGAL_ARGUMENT;
            break;
        }
    }

    /* Send NMT master message. */
    if (error == CO_ERROR_NO) {
        NMT->NMT_TXbuff->data[0] = command;
        NMT->NMT_TXbuff->data[1] = nodeID;
        error = CO_CANsend(NMT->NMT_CANdevTx, NMT->NMT_TXbuff);
    }

    return error;
}
#endif
