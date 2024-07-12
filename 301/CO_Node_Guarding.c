/*
 * CANopen Node Guarding slave and master objects.
 *
 * @file        CO_Node_Guarding.c
 * @ingroup     CO_Node_Guarding
 * @author      Janez Paternoster
 * @copyright   2023 Janez Paternoster
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

#include "301/CO_Node_Guarding.h"

#if ((CO_CONFIG_NODE_GUARDING)&CO_CONFIG_NODE_GUARDING_SLAVE_ENABLE) != 0

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN message with correct identifier
 * will be received. For more information and description of parameters see file CO_driver.h.
 */
static void
CO_ngs_receive(void* object, void* msg) {
    (void)msg;
    CO_nodeGuardingSlave_t* ngs = (CO_nodeGuardingSlave_t*)object;

    CO_FLAG_SET(ngs->CANrxNew);
}

/*
 * Custom function for writing OD object "Guard time"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t
OD_write_100C(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if ((stream == NULL) || (stream->subIndex != 0U) || (buf == NULL) || (count != sizeof(uint16_t))
        || (countWritten == NULL)) {
        return ODR_DEV_INCOMPAT;
    }

    CO_nodeGuardingSlave_t* ngs = (CO_nodeGuardingSlave_t*)stream->object;

    /* update objects */
    ngs->guardTime_us = (uint32_t)CO_getUint16(buf) * 1000U;
    ngs->lifeTime_us = ngs->guardTime_us * ngs->lifeTimeFactor;

    /* reset running timer */
    if (ngs->lifeTimer > 0U) {
        ngs->lifeTimer = ngs->lifeTime_us;
    }

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

/*
 * Custom function for writing OD object "Life time factor"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t
OD_write_100D(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if ((stream == NULL) || (stream->subIndex != 0U) || (buf == NULL) || (count != sizeof(uint8_t))
        || (countWritten == NULL)) {
        return ODR_DEV_INCOMPAT;
    }

    CO_nodeGuardingSlave_t* ngs = (CO_nodeGuardingSlave_t*)stream->object;

    /* update objects */
    ngs->lifeTimeFactor = (uint8_t)CO_getUint8(buf);
    ngs->lifeTime_us = ngs->guardTime_us * ngs->lifeTimeFactor;

    /* reset running timer */
    if (ngs->lifeTimer > 0U) {
        ngs->lifeTimer = ngs->lifeTime_us;
    }

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

CO_ReturnError_t
CO_nodeGuardingSlave_init(CO_nodeGuardingSlave_t* ngs, OD_entry_t* OD_100C_GuardTime,
                          OD_entry_t* OD_100D_LifeTimeFactor, CO_EM_t* em, uint16_t CANidNodeGuarding,
                          CO_CANmodule_t* CANdevRx, uint16_t CANdevRxIdx, CO_CANmodule_t* CANdevTx,
                          uint16_t CANdevTxIdx, uint32_t* errInfo) {
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if ((ngs == NULL) || (em == NULL) || (CANdevRx == NULL) || (CANdevTx == NULL) || (OD_100C_GuardTime == NULL)
        || (OD_100D_LifeTimeFactor == NULL)) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear the object */
    (void)memset(ngs, 0, sizeof(CO_nodeGuardingSlave_t));

    /* Configure object variables */
    ngs->em = em;

    /* get and verify required "Guard time" from the Object Dictionary */
    uint16_t guardTime_ms;
    ODR_t odRet = OD_get_u16(OD_100C_GuardTime, 0, &guardTime_ms, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = OD_getIndex(OD_100C_GuardTime);
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    ngs->guardTime_us = (uint32_t)guardTime_ms * 1000U;

    ngs->OD_100C_extension.object = ngs;
    ngs->OD_100C_extension.read = OD_readOriginal;
    ngs->OD_100C_extension.write = OD_write_100C;
    odRet = OD_extension_init(OD_100C_GuardTime, &ngs->OD_100C_extension);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = OD_getIndex(OD_100C_GuardTime);
        }
        return CO_ERROR_OD_PARAMETERS;
    }

    /* get and verify required "Life time factor" from the Object Dictionary */
    uint8_t lifeTimeFactor;
    odRet = OD_get_u8(OD_100D_LifeTimeFactor, 0, &lifeTimeFactor, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = OD_getIndex(OD_100D_LifeTimeFactor);
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    ngs->lifeTimeFactor = lifeTimeFactor;
    ngs->lifeTime_us = ngs->guardTime_us * ngs->lifeTimeFactor;

    ngs->OD_100D_extension.object = ngs;
    ngs->OD_100D_extension.read = OD_readOriginal;
    ngs->OD_100D_extension.write = OD_write_100D;
    odRet = OD_extension_init(OD_100D_LifeTimeFactor, &ngs->OD_100D_extension);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = OD_getIndex(OD_100D_LifeTimeFactor);
        }
        return CO_ERROR_OD_PARAMETERS;
    }

    /* configure CAN reception */
    ret = CO_CANrxBufferInit(CANdevRx, CANdevRxIdx, CANidNodeGuarding, 0x7FF, true, (void*)ngs, CO_ngs_receive);
    if (ret != CO_ERROR_NO) {
        return ret;
    }

    /* configure CAN transmission */
    ngs->CANdevTx = CANdevTx;
    ngs->CANtxBuff = CO_CANtxBufferInit(CANdevTx, CANdevTxIdx, CANidNodeGuarding, false, 1, false);
    if (ngs->CANtxBuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}

void
CO_nodeGuardingSlave_process(CO_nodeGuardingSlave_t* ngs, CO_NMT_internalState_t NMTstate, bool_t slaveDisable,
                             uint32_t timeDifference_us, uint32_t* timerNext_us) {
    (void)timerNext_us; /* may be unused */

    if (slaveDisable) {
        ngs->toggle = false;
        ngs->lifeTimer = 0;
        CO_FLAG_CLEAR(ngs->CANrxNew);
        return;
    }

    /* was RTR just received */
    if (CO_FLAG_READ(ngs->CANrxNew)) {
        ngs->lifeTimer = ngs->lifeTime_us;

        /* send response */
        ngs->CANtxBuff->data[0] = (uint8_t)NMTstate;
        if (ngs->toggle) {
            ngs->CANtxBuff->data[0] |= 0x80U;
            ngs->toggle = false;
        } else {
            ngs->toggle = true;
        }
        (void)CO_CANsend(ngs->CANdevTx, ngs->CANtxBuff);

        if (ngs->lifeTimeTimeout) {
            /* error bit is shared with HB consumer */
            CO_errorReset(ngs->em, CO_EM_HEARTBEAT_CONSUMER, 0);
            ngs->lifeTimeTimeout = false;
        }

        CO_FLAG_CLEAR(ngs->CANrxNew);
    }

    /* verify "Life time" timeout and update the timer */
    else if (ngs->lifeTimer > 0U) {
        if (timeDifference_us < ngs->lifeTimer) {
            ngs->lifeTimer -= timeDifference_us;
#if ((CO_CONFIG_NMT)&CO_CONFIG_FLAG_TIMERNEXT) != 0
            /* Calculate, when timeout expires */
            if (timerNext_us != NULL && *timerNext_us > ngs->lifeTimer) {
                *timerNext_us = ngs->lifeTimer;
            }
#endif
        } else {
            ngs->lifeTimer = 0;
            ngs->lifeTimeTimeout = true;

            /* error bit is shared with HB consumer */
            CO_errorReport(ngs->em, CO_EM_HEARTBEAT_CONSUMER, CO_EMC_HEARTBEAT, 0);
        }
    } else { /* MISRA C 2004 14.10 */
    }

    return;
}

#endif /* (CO_CONFIG_NODE_GUARDING) & CO_CONFIG_NODE_GUARDING_SLAVE_ENABLE */

#if ((CO_CONFIG_NODE_GUARDING)&CO_CONFIG_NODE_GUARDING_MASTER_ENABLE) != 0
/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN message with correct identifier
 * will be received. For more information and description of parameters see file CO_driver.h.
 *
 * Function receives messages from CAN identifier from 0x700 to 0x7FF. It
 * searches matching node->ident from nodes array.
 */
static void
CO_ngm_receive(void* object, void* msg) {
    CO_nodeGuardingMaster_t* ngm = (CO_nodeGuardingMaster_t*)object;

    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    const uint8_t* data = CO_CANrxMsg_readData(msg);
    uint16_t ident = CO_CANrxMsg_readIdent(msg);
    CO_nodeGuardingMasterNode_t* node = &ngm->nodes[0];

    if (DLC == 1) {
        for (uint8_t i = 0; i < CO_CONFIG_NODE_GUARDING_MASTER_COUNT; i++) {
            if (ident == node->ident) {
                uint8_t toggle = data[0] & 0x80;
                if (toggle == node->toggle) {
                    node->responseRecived = true;
                    node->NMTstate = (CO_NMT_internalState_t)(data[0] & 0x7F);
                    node->toggle = (toggle != 0) ? 0x00 : 0x80;
                }
                break;
            }
            node++;
        }
    }
}

CO_ReturnError_t
CO_nodeGuardingMaster_init(CO_nodeGuardingMaster_t* ngm, CO_EM_t* em, CO_CANmodule_t* CANdevRx, uint16_t CANdevRxIdx,
                           CO_CANmodule_t* CANdevTx, uint16_t CANdevTxIdx) {
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if (ngm == NULL || em == NULL || CANdevRx == NULL || CANdevTx == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear the object */
    (void)memset(ngm, 0, sizeof(CO_nodeGuardingMaster_t));

    /* Configure object variables */
    ngm->em = em;

    /* configure CAN reception. One buffer will receive all messages from CAN-id 0x700 to 0x7FF. */
    ret = CO_CANrxBufferInit(CANdevRx, CANdevRxIdx, CO_CAN_ID_HEARTBEAT, 0x780, false, (void*)ngm, CO_ngm_receive);
    if (ret != CO_ERROR_NO) {
        return ret;
    }

    /* configure CAN transmission */
    ngm->CANdevTx = CANdevTx;
    ngm->CANdevTxIdx = CANdevTxIdx;
    ngm->CANtxBuff = CO_CANtxBufferInit(CANdevTx, CANdevTxIdx, CO_CAN_ID_HEARTBEAT, true, 1, 0);
    if (ngm->CANtxBuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}

CO_ReturnError_t
CO_nodeGuardingMaster_initNode(CO_nodeGuardingMaster_t* ngm, uint8_t index, uint8_t nodeId, uint16_t guardTime_ms) {
    if (ngm == NULL || index >= CO_CONFIG_NODE_GUARDING_MASTER_COUNT || nodeId < 1 || nodeId > 0x7F) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CO_nodeGuardingMasterNode_t* node = &ngm->nodes[index];

    node->guardTime_us = (uint32_t)guardTime_ms * 1000;
    node->guardTimer = 0;
    node->ident = CO_CAN_ID_HEARTBEAT + nodeId;
    node->NMTstate = CO_NMT_UNKNOWN; /* for the first time */
    node->toggle = false;
    node->responseRecived = true; /* for the first time */
    node->CANtxWasBusy = false;
    node->monitoringActive = false;

#if CO_CONFIG_NODE_GUARDING_MASTER_COUNT == 1
    ngm->CANtxBuff = CO_CANtxBufferInit(ngm->CANdevTx, ngm->CANdevTxIdx, node->ident, true, 1, 0);
#endif

    return CO_ERROR_NO;
}

void
CO_nodeGuardingMaster_process(CO_nodeGuardingMaster_t* ngm, uint32_t timeDifference_us, uint32_t* timerNext_us) {
    (void)timerNext_us; /* may be unused */
    bool_t allMonitoredActiveCurrent = true;
    bool_t allMonitoredOperationalCurrent = true;
    CO_nodeGuardingMasterNode_t* node = &ngm->nodes[0];

    for (uint8_t i = 0; i < CO_CONFIG_NODE_GUARDING_MASTER_COUNT; i++) {
        if (node->guardTime_us > 0 && node->ident > CO_CAN_ID_HEARTBEAT) {
            if (timeDifference_us < node->guardTimer) {
                node->guardTimer -= timeDifference_us;
#if ((CO_CONFIG_NMT)&CO_CONFIG_FLAG_TIMERNEXT) != 0
                /* Calculate, when timeout expires */
                if (timerNext_us != NULL && *timerNext_us > node->guardTimer) {
                    *timerNext_us = node->guardTimer;
                }
#endif
            } else {
                /* it is time to send new rtr, but first verify last response */
                if (!node->CANtxWasBusy) {
                    if (!node->responseRecived) {
                        node->monitoringActive = false;
                        /* error bit is shared with HB consumer */
                        CO_errorReport(ngm->em, CO_EM_HEARTBEAT_CONSUMER, CO_EMC_HEARTBEAT, node->ident & 0x7F);
                    } else if (node->NMTstate != CO_NMT_UNKNOWN) {
                        node->monitoringActive = true;
                        CO_errorReset(ngm->em, CO_EM_HEARTBEAT_CONSUMER, node->ident & 0x7F);
                    }
                }

                if (ngm->CANtxBuff->bufferFull) {
                    node->guardTimer = 0;
                    node->CANtxWasBusy = true;
                } else {
#if CO_CONFIG_NODE_GUARDING_MASTER_COUNT > 1
                    ngm->CANtxBuff = CO_CANtxBufferInit(ngm->CANdevTx, ngm->CANdevTxIdx, node->ident, true, 1, 0);
#endif
                    (void)CO_CANsend(ngm->CANdevTx, ngm->CANtxBuff);
                    node->CANtxWasBusy = false;
                    node->responseRecived = false;
                    node->guardTimer = node->guardTime_us;
                }
            }

            if (allMonitoredActiveCurrent) {
                if (node->monitoringActive) {
                    if (node->NMTstate != CO_NMT_OPERATIONAL) {
                        allMonitoredOperationalCurrent = false;
                    }
                } else {
                    allMonitoredActiveCurrent = false;
                    allMonitoredOperationalCurrent = false;
                }
            }
        } /* if node enabled */

        node++;
    } /* for */

    ngm->allMonitoredActive = allMonitoredActiveCurrent;
    ngm->allMonitoredOperational = allMonitoredOperationalCurrent;

    return;
}

#endif /* (CO_CONFIG_NODE_GUARDING) & CO_CONFIG_NODE_GUARDING_MASTER_ENABLE */
