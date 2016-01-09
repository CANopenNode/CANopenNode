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


#include "CO_driver.h"
#include "CO_SDO.h"
#include "CO_Emergency.h"
#include "CO_NMT_Heartbeat.h"
#include "CO_HBconsumer.h"

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_HBcons_receive(void *object, const CO_CANrxMsg_t *msg);
static void CO_HBcons_receive(void *object, const CO_CANrxMsg_t *msg){
    CO_HBconsNode_t *HBconsNode;

    HBconsNode = (CO_HBconsNode_t*) object; /* this is the correct pointer type of the first argument */

    /* verify message length */
    if(msg->DLC == 1){
        /* copy data and set 'new message' flag. */
        HBconsNode->NMTstate = msg->data[0];
        HBconsNode->CANrxNew = true;
    }
}


/*
 * Configure one monitored node.
 */
static void CO_HBcons_monitoredNodeConfig(
        CO_HBconsumer_t        *HBcons,
        uint8_t                 idx,
        uint32_t                HBconsTime)
{
    uint16_t COB_ID;
    uint16_t NodeID;
    CO_HBconsNode_t *monitoredNode;

    if(idx >= HBcons->numberOfMonitoredNodes) return;

    NodeID = (uint16_t)((HBconsTime>>16)&0xFF);
    monitoredNode = &HBcons->monitoredNodes[idx];
    monitoredNode->time = (uint16_t)HBconsTime;
    monitoredNode->NMTstate = 0;
    monitoredNode->monStarted = false;

    /* is channel used */
    if(NodeID && monitoredNode->time){
        COB_ID = NodeID + 0x700;
    }
    else{
        COB_ID = 0;
        monitoredNode->time = 0;
    }

    /* configure Heartbeat consumer CAN reception */
    CO_CANrxBufferInit(
            HBcons->CANdevRx,
            HBcons->CANdevRxIdxStart + idx,
            COB_ID,
            0x7FF,
            0,
            (void*)&HBcons->monitoredNodes[idx],
            CO_HBcons_receive);
}


/*
 * OD function for accessing _Consumer Heartbeat Time_ (index 0x1016) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static CO_SDO_abortCode_t CO_ODF_1016(CO_ODF_arg_t *ODF_arg);
static CO_SDO_abortCode_t CO_ODF_1016(CO_ODF_arg_t *ODF_arg){
    CO_HBconsumer_t *HBcons;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    HBcons = (CO_HBconsumer_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading){
        uint8_t NodeID;
        uint16_t HBconsTime;

        NodeID = (value >> 16U) & 0xFFU;
        HBconsTime = value & 0xFFFFU;

        if((value & 0xFF800000U) != 0){
            ret = CO_SDO_AB_PRAM_INCOMPAT;
        }
        else if((HBconsTime != 0) && (NodeID != 0)){
            uint8_t i;
            /* there must not be more entries with same index and time different than zero */
            for(i = 0U; i<HBcons->numberOfMonitoredNodes; i++){
                uint32_t objectCopy = HBcons->HBconsTime[i];
                uint8_t NodeIDObj = (objectCopy >> 16U) & 0xFFU;
                uint16_t HBconsTimeObj = objectCopy & 0xFFFFU;
                if(((ODF_arg->subIndex-1U) != i) && (HBconsTimeObj != 0) && (NodeID == NodeIDObj)){
                    ret = CO_SDO_AB_PRAM_INCOMPAT;
                }
            }
        }
        else{
            ret = CO_SDO_AB_NONE;
        }

        /* Configure */
        if(ret == CO_SDO_AB_NONE){
            CO_HBcons_monitoredNodeConfig(HBcons, ODF_arg->subIndex-1U, value);
        }
    }

    return ret;
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

    for(i=0; i<HBcons->numberOfMonitoredNodes; i++)
        CO_HBcons_monitoredNodeConfig(HBcons, i, HBcons->HBconsTime[i]);

    /* Configure Object dictionary entry at index 0x1016 */
    CO_OD_configure(SDO, OD_H1016_CONSUMER_HB_TIME, CO_ODF_1016, (void*)HBcons, 0, 0);

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_HBconsumer_process(
        CO_HBconsumer_t        *HBcons,
        bool_t                  NMTisPreOrOperational,
        uint16_t                timeDifference_ms)
{
    uint8_t i;
    uint8_t AllMonitoredOperationalCopy;
    CO_HBconsNode_t *monitoredNode;

    AllMonitoredOperationalCopy = 5;
    monitoredNode = &HBcons->monitoredNodes[0];

    if(NMTisPreOrOperational){
        for(i=0; i<HBcons->numberOfMonitoredNodes; i++){
            if(monitoredNode->time){/* is node monitored */
                /* Verify if new Consumer Heartbeat message received */
                if(monitoredNode->CANrxNew){
                    if(monitoredNode->NMTstate){
                        /* not a bootup message */
                        monitoredNode->monStarted = true;
                        monitoredNode->timeoutTimer = 0;  /* reset timer */
                        timeDifference_ms = 0;
                    }
                    monitoredNode->CANrxNew = false;
                }
                /* Verify timeout */
                if(monitoredNode->timeoutTimer < monitoredNode->time) monitoredNode->timeoutTimer += timeDifference_ms;

                if(monitoredNode->monStarted){
                    if(monitoredNode->timeoutTimer >= monitoredNode->time){
                        CO_errorReport(HBcons->em, CO_EM_HEARTBEAT_CONSUMER, CO_EMC_HEARTBEAT, i);
                        monitoredNode->NMTstate = 0;
                    }
                    else if(monitoredNode->NMTstate == 0){
                        /* there was a bootup message */
                        CO_errorReport(HBcons->em, CO_EM_HB_CONSUMER_REMOTE_RESET, CO_EMC_HEARTBEAT, i);
                    }
                }
                if(monitoredNode->NMTstate != CO_NMT_OPERATIONAL)
                    AllMonitoredOperationalCopy = 0;
            }
            monitoredNode++;
        }
    }
    else{ /* not in (pre)operational state */
        for(i=0; i<HBcons->numberOfMonitoredNodes; i++){
            monitoredNode->NMTstate = 0;
            monitoredNode->CANrxNew = false;
            monitoredNode->monStarted = false;
            monitoredNode++;
        }
        AllMonitoredOperationalCopy = 0;
    }
    HBcons->allMonitoredOperational = AllMonitoredOperationalCopy;
}
