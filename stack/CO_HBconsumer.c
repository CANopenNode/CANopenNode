/*
 * CANopen Heartbeat consumer object.
 *
 * @file        CO_HBconsumer.c
 * @ingroup     CO_HBconsumer
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
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
void CO_HBconsumer_initCallbackTimeout(
    CO_HBconsumer_t        *HBcons,
    uint8_t                 idx,
    void                   *object,
    void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx, void *object))
{
    CO_HBconsNode_t *monitoredNode;

    if (HBcons==NULL || idx>HBcons->numberOfMonitoredNodes) {
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

    if (HBcons==NULL || idx>HBcons->numberOfMonitoredNodes) {
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
                        monitoredNode->HBstate = CO_HBconsumer_ACTIVE;
                        monitoredNode->timeoutTimer = 0;  /* reset timer */
                        timeDifference_ms = 0;
                    }
                    CLEAR_CANrxNew(monitoredNode->CANrxNew);
                }

                /* Verify timeout */
                if(monitoredNode->timeoutTimer < monitoredNode->time) {
                    monitoredNode->timeoutTimer += timeDifference_ms;
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

    if (HBcons==NULL || idx>HBcons->numberOfMonitoredNodes) {
        return CO_HBconsumer_UNCONFIGURED;
    }

    monitoredNode = &HBcons->monitoredNodes[idx];
    return monitoredNode->HBstate;
}
