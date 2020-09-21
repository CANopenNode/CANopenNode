/*
 * CANopen Heartbeat consumer object.
 *
 * @file        CO_HBconsumer.c
 * @ingroup     CO_HBconsumer
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

#include "CANopen.h"
#include "CO_HBconsumer.h"

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_HBcons_receive(void *object, const CO_CANrxMsg_t *msg){
    CO_HBconsNode_t *HBconsNode;

    HBconsNode = (CO_HBconsNode_t*) object; /* this is the correct pointer type of the first argument */

    /* verify message length */
    if(msg->DLC == 1){
        /* copy data and set 'new message' flag. */
        HBconsNode->NMTstate = (CO_NMT_internalState_t)msg->data[0];
        SET_CANrxNew(HBconsNode->CANrxNew);
    }
}


/*
 * Configure one monitored node.
 */
static void CO_HBcons_monitoredNodeConfig(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        uint8_t                 nodeId,
        uint16_t                time)
{
    uint16_t COB_ID;
    CO_HBconsNode_t *monitoredNode;

    if(idx >= HBcons->numberOfMonitoredNodes) return;

    monitoredNode = &HBcons->monitoredNodes[idx];
    monitoredNode->nodeId = nodeId;
    monitoredNode->time = time;
    monitoredNode->NMTstate = CO_NMT_INITIALIZING;
    monitoredNode->HBstate = CO_HBconsumer_UNCONFIGURED;

    /* is channel used */
    if(monitoredNode->nodeId && monitoredNode->time){
        COB_ID = monitoredNode->nodeId + CO_CAN_ID_HEARTBEAT;
        monitoredNode->HBstate = CO_HBconsumer_UNKNOWN;

    }
    else{
        COB_ID = 0;
        monitoredNode->time = 0;
    }

    /* configure Heartbeat consumer CAN reception */
    if (monitoredNode->HBstate != CO_HBconsumer_UNCONFIGURED) {
        CO_CANrxBufferInit(
                HBcons->CANdevRx,
                HBcons->CANdevRxIdxStart + idx,
                COB_ID,
                0x7FF,
                0,
                (void*)&HBcons->monitoredNodes[idx],
                CO_HBcons_receive);
    }
}


/*
 * OD function for accessing _Consumer Heartbeat Time_ (index 0x1016) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static CO_SDO_abortCode_t CO_ODF_1016(CO_ODF_arg_t *ODF_arg)
{
    CO_HBconsumer_t *HBcons;
    uint8_t NodeID;
    uint16_t HBconsTime;
    uint32_t value;
    CO_ReturnError_t ret;

    if(ODF_arg->reading){
        return CO_SDO_AB_NONE;
    }

    HBcons = (CO_HBconsumer_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);
    NodeID = (value >> 16U) & 0xFFU;
    HBconsTime = value & 0xFFFFU;

    if((value & 0xFF800000U) != 0){
        return CO_SDO_AB_PRAM_INCOMPAT;
    }

    ret = CO_HBconsumer_initEntry(HBcons, ODF_arg->subIndex-1U, NodeID, HBconsTime);
    if (ret != CO_ERROR_NO) {
        return CO_SDO_AB_PRAM_INCOMPAT;
    }
    return CO_SDO_AB_NONE;
}


/******************************************************************************/
CO_ReturnError_t CO_HBconsumer_init(
        CO_HBconsumer_t        *HBcons,
        CO_EM_t                *em,
        CO_SDO_t               *SDO,
        const uint32_t          HBconsTime[],
        CO_HBconsNode_t         monitoredNodes[],
        uint8_t                 numberOfMonitoredNodes,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdxStart)
{
    uint8_t i;

    /* verify arguments */
    if(HBcons==NULL || em==NULL || SDO==NULL || HBconsTime==NULL ||
        monitoredNodes==NULL || CANdevRx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    HBcons->em = em;
    HBcons->HBconsTime = HBconsTime;
    HBcons->monitoredNodes = monitoredNodes;
    HBcons->numberOfMonitoredNodes = numberOfMonitoredNodes;
    HBcons->allMonitoredOperational = 0;
    HBcons->CANdevRx = CANdevRx;
    HBcons->CANdevRxIdxStart = CANdevRxIdxStart;

    for(i=0; i<HBcons->numberOfMonitoredNodes; i++) {
        uint8_t nodeId = (HBcons->HBconsTime[i] >> 16U) & 0xFFU;
        uint16_t time = HBcons->HBconsTime[i] & 0xFFFFU;
        CO_HBconsumer_initEntry(HBcons, i, nodeId, time);
    }

    /* Configure Object dictionary entry at index 0x1016 */
    CO_OD_configure(SDO, OD_H1016_CONSUMER_HB_TIME, CO_ODF_1016, (void*)HBcons, 0, 0);

    return CO_ERROR_NO;
}


/******************************************************************************/
CO_ReturnError_t CO_HBconsumer_initEntry(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        uint8_t                 nodeId,
        uint16_t                consumerTime)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if(HBcons==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    if((consumerTime != 0) && (nodeId != 0)){
        uint8_t i;
        /* there must not be more entries with same index and time different than zero */
        for(i = 0U; i<HBcons->numberOfMonitoredNodes; i++){
            uint32_t objectCopy = HBcons->HBconsTime[i];
            uint8_t NodeIDObj = (objectCopy >> 16U) & 0xFFU;
            uint16_t HBconsTimeObj = objectCopy & 0xFFFFU;
            if((idx != i) && (HBconsTimeObj != 0) && (nodeId == NodeIDObj)){
                ret = CO_ERROR_ILLEGAL_ARGUMENT;
            }
        }
    }

    /* Configure */
    if(ret == CO_ERROR_NO){
        CO_HBcons_monitoredNodeConfig(HBcons, idx, nodeId, consumerTime);
    }
    return ret;
}


/******************************************************************************/
void CO_HBconsumer_initCallbackHeartbeatStarted(
    CO_HBconsumer_t        *HBcons,
    uint8_t                 idx,
    void                   *object,
    void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx, void *object))
{
    CO_HBconsNode_t *monitoredNode;

    if (HBcons==NULL || idx>=HBcons->numberOfMonitoredNodes) {
        return;
    }

    monitoredNode = &HBcons->monitoredNodes[idx];
    monitoredNode->pFunctSignalHbStarted = pFunctSignal;
    monitoredNode->functSignalObjectHbStarted = object;
}


/******************************************************************************/
void CO_HBconsumer_initCallbackTimeout(
    CO_HBconsumer_t        *HBcons,
    uint8_t                 idx,
    void                   *object,
    void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx, void *object))
{
    CO_HBconsNode_t *monitoredNode;

    if (HBcons==NULL || idx>=HBcons->numberOfMonitoredNodes) {
        return;
    }

    monitoredNode = &HBcons->monitoredNodes[idx];
    monitoredNode->pFunctSignalTimeout = pFunctSignal;
    monitoredNode->functSignalObjectTimeout = object;
}


/******************************************************************************/
void CO_HBconsumer_initCallbackRemoteReset(
    CO_HBconsumer_t        *HBcons,
    uint8_t                 idx,
    void                   *object,
    void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx, void *object))
{
    CO_HBconsNode_t *monitoredNode;

    if (HBcons==NULL || idx>=HBcons->numberOfMonitoredNodes) {
        return;
    }

    monitoredNode = &HBcons->monitoredNodes[idx];
    monitoredNode->pFunctSignalRemoteReset = pFunctSignal;
    monitoredNode->functSignalObjectRemoteReset = object;
}

/******************************************************************************/
void CO_HBconsumer_process(
        CO_HBconsumer_t        *HBcons,
        bool_t                  NMTisPreOrOperational,
        uint16_t                timeDifference_ms)
{
    uint8_t i;
    uint8_t emcyHeartbeatTimeoutActive = 0;
    uint8_t emcyRemoteResetActive = 0;
    uint8_t AllMonitoredOperationalCopy;
    CO_HBconsNode_t *monitoredNode;

    AllMonitoredOperationalCopy = 5;
    monitoredNode = &HBcons->monitoredNodes[0];

    if(NMTisPreOrOperational){
        for(i=0; i<HBcons->numberOfMonitoredNodes; i++){
            uint16_t timeDifference_ms_copy = timeDifference_ms;
            if(monitoredNode->time > 0){/* is node monitored */
                /* Verify if received message is heartbeat or bootup */
                if(IS_CANrxNew(monitoredNode->CANrxNew)){
                    if(monitoredNode->NMTstate == CO_NMT_INITIALIZING){
                        /* bootup message, call callback */
                        if (monitoredNode->pFunctSignalRemoteReset != NULL) {
                            monitoredNode->pFunctSignalRemoteReset(monitoredNode->nodeId, i,
                                monitoredNode->functSignalObjectRemoteReset);
                        }
                    }
                    else {
                        /* heartbeat message */
                        if (monitoredNode->HBstate!=CO_HBconsumer_ACTIVE &&
                            monitoredNode->pFunctSignalHbStarted!=NULL) {
                            monitoredNode->pFunctSignalHbStarted(monitoredNode->nodeId, i,
                                monitoredNode->functSignalObjectHbStarted);
                        }
                        monitoredNode->HBstate = CO_HBconsumer_ACTIVE;
                        monitoredNode->timeoutTimer = 0;  /* reset timer */
                        timeDifference_ms_copy = 0;
                    }
                    CLEAR_CANrxNew(monitoredNode->CANrxNew);
                }

                /* Verify timeout */
                if(monitoredNode->timeoutTimer < monitoredNode->time) {
                    monitoredNode->timeoutTimer += timeDifference_ms_copy;
                }
                if(monitoredNode->HBstate!=CO_HBconsumer_UNCONFIGURED &&
                   monitoredNode->HBstate!=CO_HBconsumer_UNKNOWN) {
                    if(monitoredNode->timeoutTimer >= monitoredNode->time){
                        /* timeout expired */
                        CO_errorReport(HBcons->em, CO_EM_HEARTBEAT_CONSUMER, CO_EMC_HEARTBEAT, i);
                        emcyHeartbeatTimeoutActive = 1;

                        monitoredNode->NMTstate = CO_NMT_INITIALIZING;
                        if (monitoredNode->HBstate!=CO_HBconsumer_TIMEOUT &&
                            monitoredNode->pFunctSignalTimeout!=NULL) {
                            monitoredNode->pFunctSignalTimeout(monitoredNode->nodeId, i,
                                monitoredNode->functSignalObjectTimeout);
                        }
                        monitoredNode->HBstate = CO_HBconsumer_TIMEOUT;
                    }
                    else if(monitoredNode->NMTstate == CO_NMT_INITIALIZING){
                        /* there was a bootup message */
                        CO_errorReport(HBcons->em, CO_EM_HB_CONSUMER_REMOTE_RESET, CO_EMC_HEARTBEAT, i);
                        emcyRemoteResetActive = 1;

                        monitoredNode->HBstate = CO_HBconsumer_UNKNOWN;
                    }
                }
                if(monitoredNode->NMTstate != CO_NMT_OPERATIONAL) {
                    AllMonitoredOperationalCopy = 0;
                }
            }
            monitoredNode++;
        }
    }
    else{ /* not in (pre)operational state */
        for(i=0; i<HBcons->numberOfMonitoredNodes; i++){
            monitoredNode->NMTstate = CO_NMT_INITIALIZING;
            CLEAR_CANrxNew(monitoredNode->CANrxNew);
            if(monitoredNode->HBstate != CO_HBconsumer_UNCONFIGURED){
                monitoredNode->HBstate = CO_HBconsumer_UNKNOWN;
            }
            monitoredNode++;
        }
        AllMonitoredOperationalCopy = 0;
    }
    /* clear emergencies. We only have one emergency index for all
     * monitored nodes! */
    if ( ! emcyHeartbeatTimeoutActive) {
        CO_errorReset(HBcons->em, CO_EM_HEARTBEAT_CONSUMER, 0);
    }
    if ( ! emcyRemoteResetActive) {
        CO_errorReset(HBcons->em, CO_EM_HB_CONSUMER_REMOTE_RESET, 0);
    }

    HBcons->allMonitoredOperational = AllMonitoredOperationalCopy;
}


/******************************************************************************/
int8_t CO_HBconsumer_getIdxByNodeId(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 nodeId)
{
    uint8_t i;
    CO_HBconsNode_t *monitoredNode;

    if (HBcons == NULL) {
        return -1;
    }

    /* linear search for the node */
    monitoredNode = &HBcons->monitoredNodes[0];
    for(i=0; i<HBcons->numberOfMonitoredNodes; i++){
        if (monitoredNode->nodeId == nodeId) {
            return i;
        }
        monitoredNode ++;
    }
    /* not found */
    return -1;
}


/******************************************************************************/
CO_HBconsumer_state_t CO_HBconsumer_getState(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx)
{
    CO_HBconsNode_t *monitoredNode;

    if (HBcons==NULL || idx>=HBcons->numberOfMonitoredNodes) {
        return CO_HBconsumer_UNCONFIGURED;
    }

    monitoredNode = &HBcons->monitoredNodes[idx];
    return monitoredNode->HBstate;
}

/******************************************************************************/
int8_t CO_HBconsumer_getNmtState(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        CO_NMT_internalState_t *nmtState)
{
    CO_HBconsNode_t *monitoredNode;

    if (HBcons==NULL || nmtState==NULL || idx>=HBcons->numberOfMonitoredNodes) {
        return -1;
    }
    *nmtState = CO_NMT_INITIALIZING;

    monitoredNode = &HBcons->monitoredNodes[idx];

    if (monitoredNode->HBstate == CO_HBconsumer_ACTIVE) {
      *nmtState = monitoredNode->NMTstate;
      return 0;
    }
    return -1;
}
