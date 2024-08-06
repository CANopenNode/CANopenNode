/*
 * CANopen NMT and Heartbeat producer object.
 *
 * @file        CO_NMT_Heartbeat.c
 * @ingroup     CO_NMT_Heartbeat
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
 *
 * This file is part of <https://github.com/CANopenNode/CANopenNode>, a CANopen Stack.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */

#include "301/CO_NMT_Heartbeat.h"

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN message with correct identifier
 * will be received. For more information and description of parameters see file CO_driver.h.
 */
static void
CO_NMT_receive(void* object, void* msg) {
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    const uint8_t* data = CO_CANrxMsg_readData(msg);
    CO_NMT_command_t command = (CO_NMT_command_t)data[0];
    uint8_t nodeId = data[1];

    CO_NMT_t* NMT = (CO_NMT_t*)object;

    if ((DLC == 2U) && ((nodeId == 0U) || (nodeId == NMT->nodeId))) {
        NMT->internalCommand = command;

#if ((CO_CONFIG_NMT)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0
        /* Optional signal to RTOS, which can resume task, which handles NMT. */
        if (NMT->pFunctSignalPre != NULL) {
            NMT->pFunctSignalPre(NMT->functSignalObjectPre);
        }
#endif
    }
}

/*
 * Custom function for writing OD object "Producer heartbeat time"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t
OD_write_1017(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if ((stream == NULL) || (stream->subIndex != 0U) || (buf == NULL) || (count != sizeof(uint16_t))
        || (countWritten == NULL)) {
        return ODR_DEV_INCOMPAT;
    }

    CO_NMT_t* NMT = (CO_NMT_t*)stream->object;

    /* update object, send Heartbeat immediately */
    NMT->HBproducerTime_us = (uint32_t)CO_getUint16(buf) * 1000U;
    NMT->HBproducerTimer = 0;

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

CO_ReturnError_t
CO_NMT_init(CO_NMT_t* NMT, OD_entry_t* OD_1017_ProducerHbTime, CO_EM_t* em, uint8_t nodeId, uint16_t NMTcontrol,
            uint16_t firstHBTime_ms, CO_CANmodule_t* NMT_CANdevRx, uint16_t NMT_rxIdx, uint16_t CANidRxNMT,
#if (((CO_CONFIG_NMT)&CO_CONFIG_NMT_MASTER) != 0) || defined CO_DOXYGEN
            CO_CANmodule_t* NMT_CANdevTx, uint16_t NMT_txIdx, uint16_t CANidTxNMT,
#endif
            CO_CANmodule_t* HB_CANdevTx, uint16_t HB_txIdx, uint16_t CANidTxHB, uint32_t* errInfo) {
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if ((NMT == NULL) || (OD_1017_ProducerHbTime == NULL) || (em == NULL) || (NMT_CANdevRx == NULL)
        || (HB_CANdevTx == NULL)
#if ((CO_CONFIG_NMT)&CO_CONFIG_NMT_MASTER) != 0
        || (NMT_CANdevTx == NULL)
#endif
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear the object */
    (void)memset(NMT, 0, sizeof(CO_NMT_t));

    /* Configure object variables */
    NMT->operatingState = CO_NMT_INITIALIZING;
    NMT->operatingStatePrev = CO_NMT_INITIALIZING;
    NMT->nodeId = nodeId;
    NMT->NMTcontrol = NMTcontrol;
    NMT->em = em;
    NMT->HBproducerTimer = (uint32_t)firstHBTime_ms * 1000U;

    /* get and verify required "Producer heartbeat time" from Object Dict. */
    uint16_t HBprodTime_ms;
    ODR_t odRet = OD_get_u16(OD_1017_ProducerHbTime, 0, &HBprodTime_ms, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = OD_getIndex(OD_1017_ProducerHbTime);
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    NMT->HBproducerTime_us = (uint32_t)HBprodTime_ms * 1000U;

    NMT->OD_1017_extension.object = NMT;
    NMT->OD_1017_extension.read = OD_readOriginal;
    NMT->OD_1017_extension.write = OD_write_1017;
    odRet = OD_extension_init(OD_1017_ProducerHbTime, &NMT->OD_1017_extension);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = OD_getIndex(OD_1017_ProducerHbTime);
        }
        return CO_ERROR_OD_PARAMETERS;
    }

    if (NMT->HBproducerTimer > NMT->HBproducerTime_us) {
        NMT->HBproducerTimer = NMT->HBproducerTime_us;
    }

    /* configure NMT CAN reception */
    ret = CO_CANrxBufferInit(NMT_CANdevRx, NMT_rxIdx, CANidRxNMT, 0x7FF, false, (void*)NMT, CO_NMT_receive);
    if (ret != CO_ERROR_NO) {
        return ret;
    }

#if ((CO_CONFIG_NMT)&CO_CONFIG_NMT_MASTER) != 0
    /* configure NMT CAN transmission */
    NMT->NMT_CANdevTx = NMT_CANdevTx;
    NMT->NMT_TXbuff = CO_CANtxBufferInit(NMT_CANdevTx, NMT_txIdx, CANidTxNMT, false, 2, false);
    if (NMT->NMT_TXbuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
#endif

    /* configure HB CAN transmission */
    NMT->HB_CANdevTx = HB_CANdevTx;
    NMT->HB_TXbuff = CO_CANtxBufferInit(HB_CANdevTx, HB_txIdx, CANidTxHB, false, 1, false);
    if (NMT->HB_TXbuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}

#if ((CO_CONFIG_NMT)&CO_CONFIG_FLAG_CALLBACK_PRE) != 0
void
CO_NMT_initCallbackPre(CO_NMT_t* NMT, void* object, void (*pFunctSignal)(void* object)) {
    if (NMT != NULL) {
        NMT->pFunctSignalPre = pFunctSignal;
        NMT->functSignalObjectPre = object;
    }
}
#endif

#if ((CO_CONFIG_NMT)&CO_CONFIG_NMT_CALLBACK_CHANGE) != 0
void
CO_NMT_initCallbackChanged(CO_NMT_t* NMT, void (*pFunctNMT)(CO_NMT_internalState_t state)) {
    if (NMT != NULL) {
        NMT->pFunctNMT = pFunctNMT;
        if (NMT->pFunctNMT != NULL) {
            NMT->pFunctNMT(NMT->operatingState);
        }
    }
}
#endif

CO_NMT_reset_cmd_t
CO_NMT_process(CO_NMT_t* NMT, CO_NMT_internalState_t* NMTstate, uint32_t timeDifference_us, uint32_t* timerNext_us) {
    (void)timerNext_us; /* may be unused */
    CO_NMT_internalState_t NMTstateCpy = NMT->operatingState;
    CO_NMT_reset_cmd_t resetCommand = CO_RESET_NOT;
    bool_t NNTinit = NMTstateCpy == CO_NMT_INITIALIZING;

    NMT->HBproducerTimer = (NMT->HBproducerTimer > timeDifference_us) ? (NMT->HBproducerTimer - timeDifference_us) : 0U;

    /* Send heartbeat producer message if:
     * - First start, send bootup message or
     * - HB producer enabled and: Timer expired or NMT->operatingState changed */
    if (NNTinit
        || ((NMT->HBproducerTime_us != 0U)
            && ((NMT->HBproducerTimer == 0U) || (NMTstateCpy != NMT->operatingStatePrev)))) {
        NMT->HB_TXbuff->data[0] = (uint8_t)NMTstateCpy;
        (void)CO_CANsend(NMT->HB_CANdevTx, NMT->HB_TXbuff);

        if (NMTstateCpy == CO_NMT_INITIALIZING) {
            /* NMT slave self starting */
            NMTstateCpy = (((uint16_t)NMT->NMTcontrol & (uint16_t)CO_NMT_STARTUP_TO_OPERATIONAL) != 0U)
                              ? CO_NMT_OPERATIONAL
                              : CO_NMT_PRE_OPERATIONAL;
        } else {
            /* Start timer from the beginning. If OS is slow, time sliding may occur. However,
             * heartbeat is not for synchronization, it is for health report. In case of
             * initializing, timer is set in the CO_NMT_init() function with pre-defined value. */
            NMT->HBproducerTimer = NMT->HBproducerTime_us;
        }
    }
    NMT->operatingStatePrev = NMTstateCpy;

    /* process internal NMT commands, received from CO_NMT_receive() or CO_NMT_sendCommand() */
    if (NMT->internalCommand != CO_NMT_NO_COMMAND) {
        switch (NMT->internalCommand) {
            case CO_NMT_ENTER_OPERATIONAL: NMTstateCpy = CO_NMT_OPERATIONAL; break;
            case CO_NMT_ENTER_STOPPED: NMTstateCpy = CO_NMT_STOPPED; break;
            case CO_NMT_ENTER_PRE_OPERATIONAL: NMTstateCpy = CO_NMT_PRE_OPERATIONAL; break;
            case CO_NMT_RESET_NODE: resetCommand = CO_RESET_APP; break;
            case CO_NMT_RESET_COMMUNICATION: resetCommand = CO_RESET_COMM; break;
            case CO_NMT_NO_COMMAND:
            default:
                /* done */
                break;
        }
        NMT->internalCommand = CO_NMT_NO_COMMAND;
    }

    /* verify NMT transitions based on error register */
    bool_t ErrOnBusOffHB = (((uint16_t)NMT->NMTcontrol & (uint16_t)CO_NMT_ERR_ON_BUSOFF_HB) != 0U);
    bool_t ErrBusOff = CO_isError(NMT->em, CO_EM_CAN_TX_BUS_OFF);
    bool_t ErrHbCons = CO_isError(NMT->em, CO_EM_HEARTBEAT_CONSUMER);
    bool_t ErrHbConsRemote = CO_isError(NMT->em, CO_EM_HB_CONSUMER_REMOTE_RESET);
    bool_t busOff_HB = ErrOnBusOffHB && (ErrBusOff || ErrHbCons || ErrHbConsRemote);

    bool_t ErrNMTErrReg = (((uint16_t)NMT->NMTcontrol & (uint16_t)CO_NMT_ERR_ON_ERR_REG) != 0U);
    bool_t ErrNMTcontrol = ((CO_getErrorRegister(NMT->em) & (uint8_t)NMT->NMTcontrol) != 0U);
    bool_t errRegMasked = ErrNMTErrReg && ErrNMTcontrol;

    if ((NMTstateCpy == CO_NMT_OPERATIONAL) && (busOff_HB || errRegMasked)) {
        NMTstateCpy = (((uint16_t)NMT->NMTcontrol & (uint16_t)CO_NMT_ERR_TO_STOPPED) != 0U) ? CO_NMT_STOPPED
                                                                                            : CO_NMT_PRE_OPERATIONAL;
    } else if ((((uint16_t)NMT->NMTcontrol & (uint16_t)CO_NMT_ERR_FREE_TO_OPERATIONAL) != 0U)
               && (NMTstateCpy == CO_NMT_PRE_OPERATIONAL) && (!busOff_HB && !errRegMasked)) {
        NMTstateCpy = CO_NMT_OPERATIONAL;
    } else { /* MISRA C 2004 14.10 */
    }

#if ((CO_CONFIG_NMT)&CO_CONFIG_NMT_CALLBACK_CHANGE) != 0
    /* Notify operating state change */
    if ((NMT->operatingStatePrev != NMTstateCpy) || NNTinit) {
        if (NMT->pFunctNMT != NULL) {
            NMT->pFunctNMT(NMTstateCpy);
        }
    }
#endif

#if ((CO_CONFIG_NMT)&CO_CONFIG_FLAG_TIMERNEXT) != 0
    /* Calculate, when next Heartbeat needs to be send */
    if ((NMT->HBproducerTime_us != 0U) && (timerNext_us != NULL)) {
        if (NMT->operatingStatePrev != NMTstateCpy) {
            *timerNext_us = 0;
        } else if (*timerNext_us > NMT->HBproducerTimer) {
            *timerNext_us = NMT->HBproducerTimer;
        } else { /* MISRA C 2004 14.10 */
        }
    }
#endif

    NMT->operatingState = NMTstateCpy;
    if (NMTstate != NULL) {
        *NMTstate = NMTstateCpy;
    }

    return resetCommand;
}

#if ((CO_CONFIG_NMT)&CO_CONFIG_NMT_MASTER) != 0
CO_ReturnError_t
CO_NMT_sendCommand(CO_NMT_t* NMT, CO_NMT_command_t command, uint8_t nodeID) {
    /* verify arguments */
    if (NMT == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Apply NMT command also to this node, if set so. */
    if ((nodeID == 0U) || (nodeID == NMT->nodeId)) {
        NMT->internalCommand = command;
    }

    /* Send NMT master message. */
    NMT->NMT_TXbuff->data[0] = (uint8_t)command;
    NMT->NMT_TXbuff->data[1] = nodeID;
    return CO_CANsend(NMT->NMT_CANdevTx, NMT->NMT_TXbuff);
}
#endif
