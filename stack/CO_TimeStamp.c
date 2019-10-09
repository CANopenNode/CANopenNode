/*
 * CANopen TimeStamp object.
 *
 * @file        CO_TimeStamp.c
 * @ingroup     CO_TimeStamp
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
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 */
static void CO_TS_receive(void *object, const CO_CANrxMsg_t *msg){
    CO_TS_t *TS;
    uint8_t operState;
    int i = 0;
    TIME_OF_DAY TempTime;

    TS = (CO_TS_t*)object;   /* this is the correct pointer type of the first argument */
    operState = *TS->operatingState;

    if((operState == CO_NMT_OPERATIONAL) || (operState == CO_NMT_PRE_OPERATIONAL)){
        if(msg->DLC == EMCY_MSG_LENGTH){
            TS->CANrxNew = true;
            // Process Time from msg buffer
            TempTime.ullValue = 0;
            for(i=0; i<msg->DLC; i++)
                TempTime.ullValue += (unsigned long long)msg->data[i] << 8*i;
            TS->Time.ullValue = TempTime.ullValue;
        }
        else{
            TS->receiveError = (uint16_t)msg->DLC;
        }
    }
}
	
/******************************************************************************/
CO_ReturnError_t CO_TS_init(
    CO_TS_t         *TS, 
    CO_EM_t         *em,
    CO_SDO_t        *SDO,
    uint8_t         *operatingState,
    uint32_t        COB_ID_TSMessage,
    uint32_t        TSCyclePeriod,
    CO_CANmodule_t  *CANdevRx,
    uint16_t        CANdevRxIdx)
{
    /* verify arguments */
    if(TS==NULL || em==NULL || SDO==NULL || operatingState==NULL ||
        CANdevRx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
	
    /* Configure object variables */
    TS->COB_ID = COB_ID_TSMessage&0x7FF;

    TS->periodTime = TSCyclePeriod;
    TS->periodTimeoutTime = TSCyclePeriod / 2 * 3;
    /* overflow? */
    if(TS->periodTimeoutTime < TSCyclePeriod) 
		TS->periodTimeoutTime = 0xFFFFFFFFL;

    TS->CANrxNew = false;
    TS->timer = 0;
    TS->receiveError = 0U;

    TS->em = em;
    TS->operatingState = operatingState;

    TS->CANdevRx = CANdevRx;
    TS->CANdevRxIdx = CANdevRxIdx;

    /* configure TS CAN reception */
    CO_CANrxBufferInit(
        CANdevRx,               /* CAN device */
        CANdevRxIdx,            /* rx buffer index */
        TS->COB_ID,           	/* CAN identifier */
        0x7FF,                  /* mask */
        0,                      /* rtr */
        (void*)TS,            	/* object passed to receive function */
        CO_TS_receive);       	/* this function will process received message */

    return CO_ERROR_NO;
}
	
/******************************************************************************/
uint8_t CO_TS_process(
        CO_TS_t  *TS,
        uint32_t timeDifference_ms)
{
    uint8_t ret = 0;
    uint32_t timerNew;

    if(*TS->operatingState == CO_NMT_OPERATIONAL || *TS->operatingState == CO_NMT_PRE_OPERATIONAL){
        /* update timestamp timer, no overflow */
        timerNew = TS->timer + timeDifference_ms;
        if(timerNew > TS->timer) 
			TS->timer = timerNew;

        /* was TS just received */
        if(TS->CANrxNew){
            TS->timer = 0;
            ret = 1;
            TS->CANrxNew = false;
        }

        /* Verify timeout of TS */
        if(TS->periodTime && TS->timer > TS->periodTimeoutTime && *TS->operatingState == CO_NMT_OPERATIONAL)
            CO_errorReport(TS->em, CO_EM_TS_TIME_OUT, CO_EMC_COMMUNICATION, TS->timer);
    }
    else {
        TS->CANrxNew = false;
    }

    /* verify error from receive function */
    if(TS->receiveError != 0U){
        CO_errorReport(TS->em, CO_EM_TS_LENGTH, CO_EMC_TS_DATA_LENGTH, (uint32_t)TS->receiveError);
        TS->receiveError = 0U;
    }

    return ret;
}
