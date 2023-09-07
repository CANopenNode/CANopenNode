/*
 * CANopen Node Guarding slave and master objects.
 *
 * @file        CO_Node_Guarding.c
 * @ingroup     CO_Node_Guarding
 * @author      Janez Paternoster
 * @copyright   2023 Janez Paternoster
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
#include "CANopen.h"
#include "CO_Node_Guarding.h"

#if CO_NODE_GUARDING_SLAVE > 0

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_ngs_receive(void *object, const CO_CANrxMsg_t *msg) {
    (void) msg;
    CO_nodeGuardingSlave_t *ngs = (CO_nodeGuardingSlave_t*)object;

    SET_CANrxNew(ngs->CANrxNew);
}


/******************************************************************************/
CO_ReturnError_t CO_nodeGuardingSlave_init(CO_nodeGuardingSlave_t *ngs,
                                           CO_EM_t *em,
                                           uint16_t CANidNodeGuarding,
                                           CO_CANmodule_t *CANdevRx,
                                           uint16_t CANdevRxIdx,
                                           CO_CANmodule_t *CANdevTx,
                                           uint16_t CANdevTxIdx)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if (ngs == NULL || em == NULL || CANdevRx == NULL || CANdevTx == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear the object */
    memset(ngs, 0, sizeof(CO_nodeGuardingSlave_t));

    /* Configure object variables */
    ngs->em = em;

    /* configure CAN reception */
    ret = CO_CANrxBufferInit(
            CANdevRx,           /* CAN device */
            CANdevRxIdx,        /* rx buffer index */
            CANidNodeGuarding,  /* CAN identifier */
            0x7FF,              /* mask */
            1,                  /* rtr */
            (void*)ngs,         /* object passed to receive function */
            CO_ngs_receive);    /* this function will process received message*/
    if (ret != CO_ERROR_NO) {
        return ret;
    }

    /* configure CAN transmission */
    ngs->CANdevTx = CANdevTx;
    ngs->CANtxBuff = CO_CANtxBufferInit(
            CANdevTx,           /* CAN device */
            CANdevTxIdx,        /* index of specific buffer inside CAN module */
            CANidNodeGuarding,  /* CAN identifier */
            0,                  /* rtr */
            1,                  /* number of data bytes */
            0);                 /* synchronous message flag bit */
    if (ngs->CANtxBuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


/******************************************************************************/
void CO_nodeGuardingSlave_process(CO_nodeGuardingSlave_t *ngs,
                                  CO_NMT_internalState_t NMTstate,
                                  uint16_t timeDifference_ms,
                                  uint16_t guardTime,
                                  uint8_t lifeTimeFactor)
{

    /* was RTR just received */
    if (IS_CANrxNew(ngs->CANrxNew)) {
        ngs->lifeTimer = (uint32_t)guardTime * lifeTimeFactor;

        /* send response */
        ngs->CANtxBuff->data[0] = (uint8_t) NMTstate;
        if (ngs->toggle) {
            ngs->CANtxBuff->data[0] |= 0x80;
            ngs->toggle = 1;
        }
        else {
            ngs->toggle = 0;
        }
        CO_CANsend(ngs->CANdevTx, ngs->CANtxBuff);

        if (ngs->lifeTimeTimeout) {
            /* error bit is shared with HB consumer */
            CO_errorReset(ngs->em, CO_EM_HEARTBEAT_CONSUMER, 0);
            ngs->lifeTimeTimeout = 0;
        }

        CLEAR_CANrxNew(ngs->CANrxNew);
    }

    /* verify "Life time" timeout and update the timer */
    else if (ngs->lifeTimer > 0) {
        if (timeDifference_ms < ngs->lifeTimer) {
            ngs->lifeTimer -= timeDifference_ms;
        }
        else {
            ngs->lifeTimer = 0;
            ngs->lifeTimeTimeout = 1;

            /* error bit is shared with HB consumer */
            CO_errorReport(ngs->em, CO_EM_HEARTBEAT_CONSUMER,
                           CO_EMC_HEARTBEAT, 0);
        }
    }

    return;
}

#endif /* CO_NODE_GUARDING_SLAVE > 0 */




#if CO_NODE_GUARDING_MASTER > 0
/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 *
 * Function receives messages from CAN identifier from 0x700 to 0x7FF. It
 * searches matching node->ident from nodes array.
 */
static void CO_ngm_receive(void *object, const CO_CANrxMsg_t *msg) {
    CO_nodeGuardingMaster_t *ngm = (CO_nodeGuardingMaster_t*)object;

    uint16_t ident = CO_CANrxMsg_readIdent(msg);
    CO_nodeGuardingMasterNode_t *node = &ngm->nodes[0];

    if (msg->DLC == 1) {
        for (uint8_t i=0; i<CO_NODE_GUARDING_MASTER; i++) {
            if (ident == node->ident) {
                uint8_t toggle = msg->data[0] & 0x80;
                if (toggle == node->toggle) {
                    node->responseRecived = 1;
                    node->NMTstate = (CO_NMT_internalState_t)(msg->data[0] & 0x7F);
                    node->toggle = (toggle != 0) ? 0x00 : 0x80;
                }
                break;
            }
            node ++;
        }
    }
}


/******************************************************************************/
CO_ReturnError_t CO_nodeGuardingMaster_init(CO_nodeGuardingMaster_t *ngm,
                                            CO_EM_t *em,
                                            CO_CANmodule_t *CANdevRx,
                                            uint16_t CANdevRxIdx,
                                            CO_CANmodule_t *CANdevTx,
                                            uint16_t CANdevTxIdx)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if (ngm == NULL || em == NULL || CANdevRx == NULL || CANdevTx == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear the object */
    memset(ngm, 0, sizeof(CO_nodeGuardingMaster_t));

    /* Configure object variables */
    ngm->em = em;

    /* configure CAN reception. One buffer will receive all messages
     * from CAN-id 0x700 to 0x7FF. */
    ret = CO_CANrxBufferInit(
            CANdevRx,           /* CAN device */
            CANdevRxIdx,        /* rx buffer index */
            CO_CAN_ID_HEARTBEAT,/* CAN identifier = 0x700 */
            0x780,              /* mask - accept any combination of lower 7 bits*/
            0,                  /* rtr */
            (void*)ngm,         /* object passed to receive function */
            CO_ngm_receive);    /* this function will process received message*/
    if (ret != CO_ERROR_NO) {
        return ret;
    }

    /* configure CAN transmission */
    ngm->CANdevTx = CANdevTx;
    ngm->CANdevTxIdx = CANdevTxIdx;
    ngm->CANtxBuff = CO_CANtxBufferInit(
            CANdevTx,           /* CAN device */
            CANdevTxIdx,        /* index of specific buffer inside CAN module */
            CO_CAN_ID_HEARTBEAT,/* CAN identifier - will be changed later.*/
            1,                  /* rtr */
            1,                  /* number of data bytes (rtr indication only) */
            0);                 /* synchronous message flag bit */
    if (ngm->CANtxBuff == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_nodeGuardingMaster_initNode(CO_nodeGuardingMaster_t *ngm,
                                                uint8_t index,
                                                uint8_t nodeId,
                                                uint16_t guardTime_ms)
{
    if (ngm == NULL || index >= CO_NODE_GUARDING_MASTER
        || nodeId < 1 || nodeId > 0x7F
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CO_nodeGuardingMasterNode_t *node = &ngm->nodes[index];

    node->guardTime_ms = guardTime_ms;
    node->guardTimer = 0;
    node->ident = CO_CAN_ID_HEARTBEAT + nodeId;
    node->NMTstate = CO_NMT_UNKNOWN; /* for the first time */
    node->toggle = 0;
    node->responseRecived = 1; /* for the first time */
    node->CANtxWasBusy = 0;
    node->monitoringActive = 0;

#if CO_NODE_GUARDING_MASTER == 1
    ngm->CANtxBuff = CO_CANtxBufferInit(ngm->CANdevTx,
                                        ngm->CANdevTxIdx,
                                        node->ident,
                                        1, 1, 0);
#endif

    return  CO_ERROR_NO;
}


/******************************************************************************/
void CO_nodeGuardingMaster_process(CO_nodeGuardingMaster_t *ngm,
                                   uint16_t timeDifference_ms)
{
    bool_t allMonitoredActiveCurrent = 1;
    bool_t allMonitoredOperationalCurrent = 1;
    CO_nodeGuardingMasterNode_t *node = &ngm->nodes[0];

    for (uint8_t i=0; i<CO_NODE_GUARDING_MASTER; i++) {
        if (node->guardTime_ms > 0 && node->ident > CO_CAN_ID_HEARTBEAT) {
            if (timeDifference_ms < node->guardTimer) {
                node->guardTimer -= timeDifference_ms;
            }
            else {
                /* it is time to send new rtr, but first verify last response */
                if (!node->CANtxWasBusy) {
                    if (!node->responseRecived) {
                        node->monitoringActive = 0;
                        /* error bit is shared with HB consumer */
                        CO_errorReport(ngm->em, CO_EM_HEARTBEAT_CONSUMER,
                                       CO_EMC_HEARTBEAT, node->ident & 0x7F);
                    }
                    else if (node->NMTstate != CO_NMT_UNKNOWN) {
                        node->monitoringActive = 1;
                        CO_errorReset(ngm->em, CO_EM_HEARTBEAT_CONSUMER,
                                      node->ident & 0x7F);
                    }
                }

                if (ngm->CANtxBuff->bufferFull) {
                    node->guardTimer = 0;
                    node->CANtxWasBusy = 1;
                }
                else {
#if CO_NODE_GUARDING_MASTER > 1
                    ngm->CANtxBuff = CO_CANtxBufferInit(ngm->CANdevTx,
                                                        ngm->CANdevTxIdx,
                                                        node->ident,
                                                        1, 1, 0);
#endif
                    CO_CANsend(ngm->CANdevTx, ngm->CANtxBuff);
                    node->CANtxWasBusy = 0;
                    node->responseRecived = 0;
                    node->guardTimer = node->guardTime_ms;
                }
            }

            if (allMonitoredActiveCurrent) {
                if (node->monitoringActive) {
                    if (node->NMTstate != CO_NMT_OPERATIONAL) {
                        allMonitoredOperationalCurrent = 0;
                    }
                }
                else {
                    allMonitoredActiveCurrent = 0;
                    allMonitoredOperationalCurrent = 0;
                }
            }
        } /* if node enabled */

        node ++;
    } /* for */

    ngm->allMonitoredActive = allMonitoredActiveCurrent;
    ngm->allMonitoredOperational = allMonitoredOperationalCurrent;

    return;
}

#endif /* CO_NODE_GUARDING_MASTER > 0 */
