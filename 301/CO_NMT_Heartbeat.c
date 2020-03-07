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


#include "301/CO_driver.h"
#include "301/CO_SDOserver.h"
#include "301/CO_Emergency.h"
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
        uint8_t currentOperatingState = NMT->operatingState;

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

        if(NMT->pFunctNMT!=NULL && currentOperatingState!=NMT->operatingState){
            NMT->pFunctNMT((CO_NMT_internalState_t) NMT->operatingState);
        }
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
    /* verify arguments */
    if (NMT == NULL || emPr == NULL || NMT_CANdevRx == NULL ||
        NMT_CANdevTx == NULL || HB_CANdevTx == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* blinking bytes and LEDS */
#if CO_CONFIG_NMT_LEDS > 0
    NMT->LEDtimer               = 0;
    NMT->LEDflickering          = 0;
    NMT->LEDblinking            = 0;
    NMT->LEDsingleFlash         = 0;
    NMT->LEDdoubleFlash         = 0;
    NMT->LEDtripleFlash         = 0;
    NMT->LEDquadrupleFlash      = 0;
    NMT->LEDgreenRun            = -1;
    NMT->LEDredError            = 1;
#endif /* CO_CONFIG_NMT_LEDS */

    /* Configure object variables */
    NMT->operatingState         = CO_NMT_INITIALIZING;
    NMT->nodeId                 = nodeId;
    NMT->firstHBTime            = (int32_t)firstHBTime_ms * 1000;
    NMT->resetCommand           = 0;
    NMT->HBproducerTimer        = 0;
    NMT->emPr                   = emPr;
    NMT->pFunctNMT              = NULL;

    /* configure NMT CAN reception */
    CO_CANrxBufferInit(
            NMT_CANdevRx,       /* CAN device */
            NMT_rxIdx,          /* rx buffer index */
            CANidRxNMT,         /* CAN identifier */
            0x7FF,              /* mask */
            0,                  /* rtr */
            (void*)NMT,         /* object passed to receive function */
            CO_NMT_receive);    /* this function will process received message */

    /* configure NMT CAN transmission */
    NMT->NMT_CANdevTx = NMT_CANdevTx;
    NMT->NMT_TXbuff = CO_CANtxBufferInit(
            NMT_CANdevTx,       /* CAN device */
            NMT_txIdx,          /* index of specific buffer inside CAN module */
            CANidTxNMT,         /* CAN identifier */
            0,                  /* rtr */
            2,                  /* number of data bytes */
            0);                 /* synchronous message flag bit */

    /* configure HB CAN transmission */
    NMT->HB_CANdevTx = HB_CANdevTx;
    NMT->HB_TXbuff = CO_CANtxBufferInit(
            HB_CANdevTx,        /* CAN device */
            HB_txIdx,           /* index of specific buffer inside CAN module */
            CANidTxHB,          /* CAN identifier */
            0,                  /* rtr */
            1,                  /* number of data bytes */
            0);                 /* synchronous message flag bit */

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_NMT_initCallback(
        CO_NMT_t               *NMT,
        void                  (*pFunctNMT)(CO_NMT_internalState_t state))
{
    if(NMT != NULL){
        NMT->pFunctNMT = pFunctNMT;
        if(NMT->pFunctNMT != NULL){
            NMT->pFunctNMT((CO_NMT_internalState_t) NMT->operatingState);
        }
    }
}


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
    uint8_t CANpassive;

    uint8_t currentOperatingState = NMT->operatingState;
    uint32_t HBtime = (uint32_t)HBtime_ms * 1000;

    NMT->HBproducerTimer += timeDifference_us;

    /* Heartbeat producer message & Bootup message */
    if ((HBtime != 0 && NMT->HBproducerTimer >= HBtime) || NMT->operatingState == CO_NMT_INITIALIZING) {

        /* Start from the beginning. If OS is slow, time sliding may occur. However, heartbeat is
         * not for synchronization, it is for health report. */
        NMT->HBproducerTimer = 0;

        NMT->HB_TXbuff->data[0] = NMT->operatingState;
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


    /* CAN passive flag */
    CANpassive = 0;
    if(CO_isError(NMT->emPr->em, CO_EM_CAN_TX_BUS_PASSIVE) || CO_isError(NMT->emPr->em, CO_EM_CAN_RX_BUS_PASSIVE))
        CANpassive = 1;


#if CO_CONFIG_NMT_LEDS > 0
    NMT->LEDtimer += timeDifference_us;
    if (NMT->LEDtimer >= 50000) {
        NMT->LEDtimer -= 50000;

        if (timerNext_us != NULL) {
            uint32_t diff = 50000 - NMT->LEDtimer;
            if (*timerNext_us > diff) {
                *timerNext_us = diff;
            }
        }

        if (++NMT->LEDflickering >= 1) NMT->LEDflickering = -1;

        if (++NMT->LEDblinking >= 4) NMT->LEDblinking = -4;

        if (++NMT->LEDsingleFlash >= 4) NMT->LEDsingleFlash = -20;

        switch (++NMT->LEDdoubleFlash) {
            case    4: NMT->LEDdoubleFlash = -104; break;
            case -100: NMT->LEDdoubleFlash =  100; break;
            case  104: NMT->LEDdoubleFlash =  -20; break;
            default: break;
        }

        switch (++NMT->LEDtripleFlash) {
            case    4: NMT->LEDtripleFlash = -104; break;
            case -100: NMT->LEDtripleFlash =  100; break;
            case  104: NMT->LEDtripleFlash = -114; break;
            case -110: NMT->LEDtripleFlash =  110; break;
            case  114: NMT->LEDtripleFlash =  -20; break;
            default: break;
        }

        switch (++NMT->LEDquadrupleFlash) {
            case    4: NMT->LEDquadrupleFlash = -104; break;
            case -100: NMT->LEDquadrupleFlash =  100; break;
            case  104: NMT->LEDquadrupleFlash = -114; break;
            case -110: NMT->LEDquadrupleFlash =  110; break;
            case  114: NMT->LEDquadrupleFlash = -124; break;
            case -120: NMT->LEDquadrupleFlash =  120; break;
            case  124: NMT->LEDquadrupleFlash =  -20; break;
            default: break;
        }
    }

    /* CANopen green RUN LED (DR 303-3) */
    switch(NMT->operatingState){
        case CO_NMT_STOPPED:          NMT->LEDgreenRun = NMT->LEDsingleFlash;   break;
        case CO_NMT_PRE_OPERATIONAL:  NMT->LEDgreenRun = NMT->LEDblinking;      break;
        case CO_NMT_OPERATIONAL:      NMT->LEDgreenRun = 1;                     break;
        default: break;
    }


    /* CANopen red ERROR LED (DR 303-3) */
    if(CO_isError(NMT->emPr->em, CO_EM_CAN_TX_BUS_OFF))
        NMT->LEDredError = 1;

    else if(CO_isError(NMT->emPr->em, CO_EM_SYNC_TIME_OUT))
        NMT->LEDredError = NMT->LEDtripleFlash;

    else if(CO_isError(NMT->emPr->em, CO_EM_HEARTBEAT_CONSUMER) || CO_isError(NMT->emPr->em, CO_EM_HB_CONSUMER_REMOTE_RESET))
        NMT->LEDredError = NMT->LEDdoubleFlash;

    else if(CANpassive || CO_isError(NMT->emPr->em, CO_EM_CAN_BUS_WARNING))
        NMT->LEDredError = NMT->LEDsingleFlash;

    else if(errorRegister)
        NMT->LEDredError = (NMT->LEDblinking>=0)?-1:1;

    else
        NMT->LEDredError = -1;
#endif /* CO_CONFIG_NMT_LEDS */


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
        if (NMT->pFunctNMT != NULL) {
            NMT->pFunctNMT((CO_NMT_internalState_t) NMT->operatingState);
        }
        /* execute next CANopen processing immediately */
        if (timerNext_us != NULL) {
            *timerNext_us = 0;
        }
    }

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

    return (CO_NMT_reset_cmd_t) NMT->resetCommand;
}


/******************************************************************************/
CO_NMT_internalState_t CO_NMT_getInternalState(
        CO_NMT_t               *NMT)
{
    if(NMT != NULL){
        return (CO_NMT_internalState_t) NMT->operatingState;
    }
    return CO_NMT_INITIALIZING;
}


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
