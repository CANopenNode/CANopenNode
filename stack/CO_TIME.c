/*
 * CANopen TIME object.
 *
 * @file        CO_TIME.c
 * @ingroup     CO_TIME
 * @author      Julien PEYREGNE
 * @copyright   
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

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received.
 */
static void CO_TIME_receive(void *object, const CO_CANrxMsg_t *msg){
    CO_TIME_t *TIME;
    uint8_t operState;

    TIME = (CO_TIME_t*)object;   /* this is the correct pointer type of the first argument */
    operState = *TIME->operatingState;
	
    if((operState == CO_NMT_OPERATIONAL) || (operState == CO_NMT_PRE_OPERATIONAL)){
        SET_CANrxNew(TIME->CANrxNew);
        // Process Time from msg buffer
        CO_memcpy((uint8_t*)&TIME->Time.ullValue, msg->data, msg->DLC);
    }
    else{
        TIME->receiveError = (uint16_t)msg->DLC;
    }
}
	
/******************************************************************************/
CO_ReturnError_t CO_TIME_init(
    CO_TIME_t         *TIME, 
    CO_EM_t         *em,
    CO_SDO_t        *SDO,
    uint8_t         *operatingState,
    uint32_t        COB_ID_TIMEMessage,
    uint32_t        TIMECyclePeriod,
    CO_CANmodule_t  *CANdevRx,
    uint16_t        CANdevRxIdx,
    CO_CANmodule_t *CANdevTx,
    uint16_t        CANdevTxIdx)
{
    /* verify arguments */
    if(TIME==NULL || em==NULL || SDO==NULL || operatingState==NULL ||
    CANdevRx==NULL || CANdevTx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
	
    /* Configure object variables */
    TIME->isConsumer = (COB_ID_TIMEMessage&0x80000000L) ? true : false;
    TIME->isProducer = (COB_ID_TIMEMessage&0x40000000L) ? true : false;
    TIME->COB_ID = COB_ID_TIMEMessage&0x7FF; // 11 bit ID

    TIME->periodTime = TIMECyclePeriod;
    TIME->periodTimeoutTime = TIMECyclePeriod / 2 * 3;
    /* overflow? */
    if(TIME->periodTimeoutTime < TIMECyclePeriod) 
        TIME->periodTimeoutTime = 0xFFFFFFFFL;

    CLEAR_CANrxNew(TIME->CANrxNew);
    TIME->timer = 0;
    TIME->receiveError = 0U;

    TIME->em = em;
    TIME->operatingState = operatingState;

    
    /* configure TIME consumer message reception */
    TIME->CANdevRx = CANdevRx;
    TIME->CANdevRxIdx = CANdevRxIdx;
	if(TIME->isConsumer)
            CO_CANrxBufferInit(
                CANdevRx,               /* CAN device */
                CANdevRxIdx,            /* rx buffer index */
                TIME->COB_ID,           /* CAN identifier */
                0x7FF,                  /* mask */
                0,                      /* rtr */
                (void*)TIME,            /* object passed to receive function */
                CO_TIME_receive);       /* this function will process received message */


    /* configure TIME producer message transmission */
    TIME->CANdevTx = CANdevTx;
    TIME->CANdevTxIdx = CANdevTxIdx;
    if(TIME->isProducer)
        TIME->TXbuff = CO_CANtxBufferInit(
            CANdevTx,               /* CAN device */
            CANdevTxIdx,            /* index of specific buffer inside CAN module */
            TIME->COB_ID,           /* CAN identifier */
            0,                      /* rtr */
            TIME_MSG_LENGTH,        /* number of data bytes */
            0);                     /* synchronous message flag bit */

    return CO_ERROR_NO;
}
	
/******************************************************************************/
uint8_t CO_TIME_process(
        CO_TIME_t  *TIME,
        uint32_t timeDifference_ms)
{
    uint8_t ret = 0;
    uint32_t timerNew;

    if(*TIME->operatingState == CO_NMT_OPERATIONAL || *TIME->operatingState == CO_NMT_PRE_OPERATIONAL){
        /* update TIME timer, no overflow */
        timerNew = TIME->timer + timeDifference_ms;
        if(timerNew > TIME->timer) 
            TIME->timer = timerNew;

        /* was TIME just received */
        if(TIME->CANrxNew){
            TIME->timer = 0;
            ret = 1;
            CLEAR_CANrxNew(TIME->CANrxNew);
        }
		
        /* TIME producer */
        if(TIME->isProducer && TIME->periodTime){
            if(TIME->timer >= TIME->periodTime){
                TIME->timer = 0;
                ret = 1;
                CO_memcpy(TIME->TXbuff->data, (const uint8_t*)&TIME->Time.ullValue, TIME_MSG_LENGTH);
                CO_CANsend(TIME->CANdevTx, TIME->TXbuff);
            }
        }

        /* Verify TIME timeout if node is consumer */
        if(TIME->isConsumer && TIME->periodTime && TIME->timer > TIME->periodTimeoutTime 
        && *TIME->operatingState == CO_NMT_OPERATIONAL)
            CO_errorReport(TIME->em, CO_EM_TIME_TIMEOUT, CO_EMC_COMMUNICATION, TIME->timer);
    }
    else {
        CLEAR_CANrxNew(TIME->CANrxNew);
    }

    /* verify error from receive function */
    if(TIME->receiveError != 0U){
        CO_errorReport(TIME->em, CO_EM_TIME_LENGTH, CO_EMC_TIME_DATA_LENGTH, (uint32_t)TIME->receiveError);
        TIME->receiveError = 0U;
    }

    return ret;
}
