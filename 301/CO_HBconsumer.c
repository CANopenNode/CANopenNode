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

#include "301/CO_driver.h"
#include "301/CO_SDOserver.h"
#include "301/CO_Emergency.h"
#include "301/CO_NMT_Heartbeat.h"
#include "301/CO_HBconsumer.h"

#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE

/* Verify HB consumer configuration *******************************************/
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE \
    && (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI
#error CO_CONFIG_HB_CONS_CALLBACK_CHANGE and CO_CONFIG_HB_CONS_CALLBACK_MULTI cannot be set simultaneously!
#endif

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_HBcons_receive(void *object, void *msg){
    CO_HBconsNode_t *HBconsNode;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    HBconsNode = (CO_HBconsNode_t*) object; /* this is the correct pointer type of the first argument */

    /* verify message length */
    if(DLC == 1){
        /* copy data and set 'new message' flag. */
        HBconsNode->NMTstate = (CO_NMT_internalState_t)data[0];
        CO_FLAG_SET(HBconsNode->CANrxNew);
    }

#if (CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_CALLBACK_PRE
    /* Optional signal to RTOS, which can resume task, which handles HBcons. */
    if(HBconsNode->pFunctSignalPre != NULL) {
        HBconsNode->pFunctSignalPre(HBconsNode->functSignalObjectPre);
    }
#endif
}


/*
 * OD function for accessing _Consumer Heartbeat Time_ (index 0x1016) from SDO server.
 *
 * For more information see file CO_SDOserver.h.
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
    CO_ReturnError_t ret = CO_ERROR_NO;

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
    HBcons->allMonitoredActive = false;
    HBcons->allMonitoredOperational = CO_NMT_UNKNOWN;
    HBcons->NMTisPreOrOperationalPrev = false;
    HBcons->CANdevRx = CANdevRx;
    HBcons->CANdevRxIdxStart = CANdevRxIdxStart;
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE
    HBcons->pFunctSignalNmtChanged = NULL;
#endif

    for(i=0; i<HBcons->numberOfMonitoredNodes; i++) {
        uint8_t nodeId = (HBcons->HBconsTime[i] >> 16U) & 0xFFU;
        uint16_t time = HBcons->HBconsTime[i] & 0xFFFFU;
        ret = CO_HBconsumer_initEntry(HBcons, i, nodeId, time);
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_CALLBACK_PRE
            HBcons->monitoredNodes[i].pFunctSignalPre = NULL;
#endif
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI
            HBcons->monitoredNodes[i].pFunctSignalNmtChanged = NULL;
            HBcons->monitoredNodes[i].pFunctSignalHbStarted = NULL;
            HBcons->monitoredNodes[i].pFunctSignalTimeout = NULL;
            HBcons->monitoredNodes[i].pFunctSignalRemoteReset = NULL;
#endif
    }

    /* Configure Object dictionary entry at index 0x1016 */
    CO_OD_configure(SDO, OD_H1016_CONSUMER_HB_TIME, CO_ODF_1016, (void*)HBcons, 0, 0);

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_HBconsumer_initEntry(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        uint8_t                 nodeId,
        uint16_t                consumerTime_ms)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if (HBcons == NULL || idx >= HBcons->numberOfMonitoredNodes) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    if((consumerTime_ms != 0) && (nodeId != 0)){
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

    /* Configure one monitored node */
    if (ret == CO_ERROR_NO) {
        uint16_t COB_ID;

        CO_HBconsNode_t * monitoredNode = &HBcons->monitoredNodes[idx];
        monitoredNode->nodeId = nodeId;
        monitoredNode->time_us = (int32_t)consumerTime_ms * 1000;
        monitoredNode->NMTstate = CO_NMT_UNKNOWN;
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE \
    || (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI
        monitoredNode->NMTstatePrev = CO_NMT_UNKNOWN;
#endif
        CO_FLAG_CLEAR(monitoredNode->CANrxNew);

        /* is channel used */
        if (monitoredNode->nodeId && monitoredNode->time_us) {
            COB_ID = monitoredNode->nodeId + CO_CAN_ID_HEARTBEAT;
            monitoredNode->HBstate = CO_HBconsumer_UNKNOWN;
        }
        else {
            COB_ID = 0;
            monitoredNode->time_us = 0;
            monitoredNode->HBstate = CO_HBconsumer_UNCONFIGURED;
        }

        /* configure Heartbeat consumer CAN reception */
        if (monitoredNode->HBstate != CO_HBconsumer_UNCONFIGURED) {
            ret = CO_CANrxBufferInit(HBcons->CANdevRx,
                                     HBcons->CANdevRxIdxStart + idx,
                                     COB_ID,
                                     0x7FF,
                                     0,
                                     (void*)&HBcons->monitoredNodes[idx],
                                     CO_HBcons_receive);
        }
    }
    return ret;
}


#if (CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_HBconsumer_initCallbackPre(
        CO_HBconsumer_t        *HBcons,
        void                   *object,
        void                  (*pFunctSignal)(void *object))
{
    if (HBcons != NULL) {
        uint8_t i;
        for(i=0; i<HBcons->numberOfMonitoredNodes; i++) {
            HBcons->monitoredNodes[i].pFunctSignalPre = pFunctSignal;
            HBcons->monitoredNodes[i].functSignalObjectPre = object;
        }
    }
}
#endif


#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE
/******************************************************************************/
void CO_HBconsumer_initCallbackNmtChanged(
        CO_HBconsumer_t        *HBcons,
        void                   *object,
        void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx,
                                              CO_NMT_internalState_t NMTstate,
                                              void *object))
{
    if (HBcons==NULL) {
        return;
    }

    HBcons->pFunctSignalNmtChanged = pFunctSignal;
    HBcons->pFunctSignalObjectNmtChanged = object;
}
#endif


#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI
/******************************************************************************/
void CO_HBconsumer_initCallbackNmtChanged(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        void                   *object,
        void                  (*pFunctSignal)(uint8_t nodeId, uint8_t idx,
                                              CO_NMT_internalState_t NMTstate,
                                              void *object))
{
    if (HBcons==NULL || idx>=HBcons->numberOfMonitoredNodes) {
        return;
    }

    CO_HBconsNode_t * const monitoredNode = &HBcons->monitoredNodes[idx];
    monitoredNode->pFunctSignalNmtChanged = pFunctSignal;
    monitoredNode->pFunctSignalObjectNmtChanged = object;
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
#endif /* (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI */


/******************************************************************************/
void CO_HBconsumer_process(
        CO_HBconsumer_t        *HBcons,
        bool_t                  NMTisPreOrOperational,
        uint32_t                timeDifference_us,
        uint32_t               *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    bool_t allMonitoredActiveCurrent = true;
    uint8_t allMonitoredOperationalCurrent = CO_NMT_OPERATIONAL;

    if (NMTisPreOrOperational && HBcons->NMTisPreOrOperationalPrev) {
        for (uint8_t i=0; i<HBcons->numberOfMonitoredNodes; i++) {
            uint32_t timeDifference_us_copy = timeDifference_us;
            CO_HBconsNode_t * const monitoredNode = &HBcons->monitoredNodes[i];

            if (monitoredNode->HBstate == CO_HBconsumer_UNCONFIGURED) {
                /* continue, if node is not monitored */
                continue;
            }
            /* Verify if received message is heartbeat or bootup */
            if (CO_FLAG_READ(monitoredNode->CANrxNew)) {
                if (monitoredNode->NMTstate == CO_NMT_INITIALIZING) {
                    /* bootup message*/
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI
                    if (monitoredNode->pFunctSignalRemoteReset != NULL) {
                        monitoredNode->pFunctSignalRemoteReset(
                            monitoredNode->nodeId, i,
                            monitoredNode->functSignalObjectRemoteReset);
                    }
#endif
                    if (monitoredNode->HBstate == CO_HBconsumer_ACTIVE) {
                        CO_errorReport(HBcons->em,
                                       CO_EM_HB_CONSUMER_REMOTE_RESET,
                                       CO_EMC_HEARTBEAT, i);
                    }
                    monitoredNode->HBstate = CO_HBconsumer_UNKNOWN;

                }
                else {
                    /* heartbeat message */
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI
                    if (monitoredNode->HBstate != CO_HBconsumer_ACTIVE &&
                        monitoredNode->pFunctSignalHbStarted != NULL) {
                        monitoredNode->pFunctSignalHbStarted(
                            monitoredNode->nodeId, i,
                            monitoredNode->functSignalObjectHbStarted);
                    }
#endif
                    monitoredNode->HBstate = CO_HBconsumer_ACTIVE;
                    /* reset timer */
                    monitoredNode->timeoutTimer = 0;
                    timeDifference_us_copy = 0;
                }
                CO_FLAG_CLEAR(monitoredNode->CANrxNew);
            }

            /* Verify timeout */
            if (monitoredNode->HBstate == CO_HBconsumer_ACTIVE) {
                monitoredNode->timeoutTimer += timeDifference_us_copy;

                if (monitoredNode->timeoutTimer >= monitoredNode->time_us) {
                    /* timeout expired */
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI
                    if (monitoredNode->pFunctSignalTimeout!=NULL) {
                        monitoredNode->pFunctSignalTimeout(
                            monitoredNode->nodeId, i,
                            monitoredNode->functSignalObjectTimeout);
                    }
#endif
                    CO_errorReport(HBcons->em, CO_EM_HEARTBEAT_CONSUMER,
                                   CO_EMC_HEARTBEAT, i);
                    monitoredNode->NMTstate = CO_NMT_UNKNOWN;
                    monitoredNode->HBstate = CO_HBconsumer_TIMEOUT;
                }

#if (CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_TIMERNEXT
                else if (timerNext_us != NULL) {
                    /* Calculate timerNext_us for next timeout checking. */
                    uint32_t diff = monitoredNode->time_us
                                  - monitoredNode->timeoutTimer;
                    if (*timerNext_us > diff) {
                        *timerNext_us = diff;
                    }
                }
#endif
            }

            if(monitoredNode->HBstate != CO_HBconsumer_ACTIVE) {
                allMonitoredActiveCurrent = false;
            }
            if (monitoredNode->NMTstate != CO_NMT_OPERATIONAL) {
                allMonitoredOperationalCurrent = CO_NMT_UNKNOWN;
            }
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE \
    || (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI
            /* Verify, if NMT state of monitored node changed */
            if(monitoredNode->NMTstate != monitoredNode->NMTstatePrev) {
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE
                if (HBcons->pFunctSignalNmtChanged != NULL) {
                    HBcons->pFunctSignalNmtChanged(
                        monitoredNode->nodeId, i, monitoredNode->NMTstate,
                        HBcons->pFunctSignalObjectNmtChanged);
#else
                if (monitoredNode->pFunctSignalNmtChanged != NULL) {
                    monitoredNode->pFunctSignalNmtChanged(
                        monitoredNode->nodeId, i, monitoredNode->NMTstate,
                        monitoredNode->pFunctSignalObjectNmtChanged);
#endif
                }
                monitoredNode->NMTstatePrev = monitoredNode->NMTstate;
            }
#endif
        }
    }
    else if (NMTisPreOrOperational || HBcons->NMTisPreOrOperationalPrev) {
        /* (pre)operational state changed, clear variables */
        for(uint8_t i=0; i<HBcons->numberOfMonitoredNodes; i++) {
            CO_HBconsNode_t * const monitoredNode = &HBcons->monitoredNodes[i];
            monitoredNode->NMTstate = CO_NMT_UNKNOWN;
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE \
    || (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_MULTI
            monitoredNode->NMTstatePrev = CO_NMT_UNKNOWN;
#endif
            CO_FLAG_CLEAR(monitoredNode->CANrxNew);
            if (monitoredNode->HBstate != CO_HBconsumer_UNCONFIGURED) {
                monitoredNode->HBstate = CO_HBconsumer_UNKNOWN;
            }
        }
        allMonitoredActiveCurrent = false;
        allMonitoredOperationalCurrent = CO_NMT_UNKNOWN;
    }

    /* Clear emergencies when all monitored nodes becomes active.
     * We only have one emergency index for all monitored nodes! */
    if (!HBcons->allMonitoredActive && allMonitoredActiveCurrent) {
        CO_errorReset(HBcons->em, CO_EM_HEARTBEAT_CONSUMER, 0);
        CO_errorReset(HBcons->em, CO_EM_HB_CONSUMER_REMOTE_RESET, 0);
    }

    HBcons->allMonitoredActive = allMonitoredActiveCurrent;
    HBcons->allMonitoredOperational = allMonitoredOperationalCurrent;
    HBcons->NMTisPreOrOperationalPrev = NMTisPreOrOperational;
}


#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_QUERY_FUNCT
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
#endif /* (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_QUERY_FUNCT */

#endif /* (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE */
