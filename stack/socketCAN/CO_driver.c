/*
 * CAN module object for BECK SC243 computer.
 *
 * @file        CO_driver.c
 * @version     SVN: \$Id$
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <http://canopennode.sourceforge.net>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "CO_driver.h"
#include "CO_Emergency.h"
#include <string.h> /* for memcpy */

#ifdef USE_CAN_CALLBACKS
int CAN1callback(CanEvent event, const CanMsg *msg);
int CAN2callback(CanEvent event, const CanMsg *msg);
#endif


/******************************************************************************/
void CO_CANsetConfigurationMode(uint16_t CANbaseAddress){
    canEnableRx(CANbaseAddress, FALSE);
}


/******************************************************************************/
void CO_CANsetNormalMode(uint16_t CANbaseAddress){
    canEnableRx(CANbaseAddress, TRUE);
}


/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        uint16_t                CANbaseAddress,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate)
{
    uint16_t i;

    /* Configure object variables */
    CANmodule->CANbaseAddress = CANbaseAddress;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->bufferInhibitFlag = CO_false; /* True, if CAN message was sent, reset by interrupt. */
    CANmodule->firstCANtxMessage = CO_true;
    CANmodule->error = 0;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;
    CANmodule->em = NULL;

    for(i=0U; i<rxSize; i++){
        rxArray[i].ident = 0U;
        rxArray[i].pFunct = NULL;
    }
    for(i=0U; i<txSize; i++){
        txArray[i].bufferFull = CO_false;
    }

    /* initialize port */
    CanError e = canInit(CANbaseAddress, CANbitRate, 0);
    if(e == CAN_ERROR_ILLEGAL_BAUDRATE)
        e = canInit(CANbaseAddress, 125, 0);
    switch(e){
        case CO_ERROR_NO: break;
        case CAN_ERROR_OUT_OF_MEMORY: return CO_ERROR_OUT_OF_MEMORY;
        default: return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Set acceptance filters to accept all messages with standard identifier, also accept rtr */
    CanFilterSc2x3 filter;
    filter.controllerType         = CAN_FILTER_CONTROLLER_TYPE_SC2X3;
    filter.structVer              = 1;
    filter.mode                   = CAN_FILTER_SC2X3_MODE_32_BIT;
    filter.filters.f32[0].idMask  = canEncodeId(0x7FF, FALSE, TRUE);
    filter.filters.f32[0].idValue = canEncodeId(0x7FF, FALSE, FALSE);
    switch(canSetFilter(CANbaseAddress,  (CanFilter *)&filter)){
        case CO_ERROR_NO: break;
        default: return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* purge buffers */
    canEnableRx(CANbaseAddress, FALSE);
    canPurgeRx(CANbaseAddress);
    canPurgeTx(CANbaseAddress, FALSE);

    /* register callback function */
#ifdef USE_CAN_CALLBACKS
    if(CANbaseAddress == CAN_PORT_CAN1)
        canRegisterCallback(CANbaseAddress, CAN1callback, (1 << CAN_EVENT_RX) | (1 << CAN_EVENT_TX) | (1 << CAN_EVENT_BUS_OFF) | (1 << CAN_EVENT_OVERRUN));
    else if(CANbaseAddress == CAN_PORT_CAN2)
        canRegisterCallback(CANbaseAddress, CAN2callback, (1 << CAN_EVENT_RX) | (1 << CAN_EVENT_TX) | (1 << CAN_EVENT_BUS_OFF) | (1 << CAN_EVENT_OVERRUN));
#endif

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule){
    canDeinit(CANmodule->CANbaseAddress);
}


/******************************************************************************/
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg){
    return (uint16_t) canDecodeId(rxMsg->ident);
}


/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        CO_bool_t               rtr,
        void                   *object,
        void                  (*pFunct)(void *object, const CO_CANrxMsg_t *message))
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    if((CANmodule!=NULL) && (object!=NULL) && (pFunct!=NULL) && (index < CANmodule->rxSize)){
        /* buffer, which will be configured */
        CO_CANrx_t *buffer = &CANmodule->rxArray[index];

        /* Configure object variables */
        buffer->object = object;
        buffer->pFunct = pFunct;

        /* Configure CAN identifier and CAN mask, bit aligned with CAN module. */
        /* No hardware filtering is used. */
        buffer->ident = canEncodeId(ident, FALSE, rtr);
        buffer->mask = canEncodeId(mask, FALSE, FALSE);
    }
    else{
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        CO_bool_t               rtr,
        uint8_t                 noOfBytes,
        CO_bool_t               syncFlag)
{
    CO_CANtx_t *buffer = NULL;

    if((CANmodule != NULL) && (index < CANmodule->txSize)){
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

        /* CAN identifier, bit aligned with CAN module registers */
        buffer->ident = canEncodeId(ident, FALSE, rtr);

        buffer->DLC = noOfBytes;
        buffer->bufferFull = CO_false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;
    CanError canErr = CAN_ERROR_NO;

#ifdef USE_CAN_CALLBACKS
    /* Verify overflow */
    if(buffer->bufferFull){
        if(!CANmodule->firstCANtxMessage){
            /* don't set error, if bootup message is still on buffers */
            CO_errorReport((CO_EM_t*)CANmodule->em, ERROR_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, canDecodeId(buffer->ident));
        }
        err = CO_ERROR_TX_OVERFLOW;
    }

    CO_DISABLE_INTERRUPTS();
    /* if CAN TX buffer is free, copy message to it */
    if(CANmodule->bufferInhibitFlag == CO_false && CANmodule->CANtxCount == 0){
        canErr = canSend(CANmodule->CANbaseAddress, (const CanMsg*) buffer, FALSE);
        CANmodule->bufferInhibitFlag = CO_true;   /* indicate, that message is on CAN module */
#ifdef CO_LOG_CAN_MESSAGES
        memcpy((void*)&CANmodule->txRecord, (void*)buffer, sizeof(CO_CANtx_t));
#endif
    }
    /* if no buffer is free, message will be sent by interrupt */
    else{
        buffer->bufferFull = CO_true;
        CANmodule->CANtxCount++;
    }
    CO_ENABLE_INTERRUPTS();

#else
    CO_DISABLE_INTERRUPTS();
    canErr = canSend(CANmodule->CANbaseAddress, (const CanMsg*) buffer, FALSE);
#ifdef CO_LOG_CAN_MESSAGES
    void CO_logMessage(const CanMsg *msg);
    CO_logMessage((const CanMsg*) buffer);
#endif
    CO_ENABLE_INTERRUPTS();
#endif

    if(canErr != CAN_ERROR_NO){
        CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_GENERIC_ERROR, CO_EMC_GENERIC, canErr);
        err = CO_ERROR_TX_OVERFLOW;
    }

    return err;
}


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule){
#ifdef USE_CAN_CALLBACKS
    uint32_t tpdoDeleted = 0U;

    CO_DISABLE_INTERRUPTS();
    /* Abort message from CAN module, if there is synchronous TPDO.
     * Functionality is not used. */

    /* delete also pending synchronous TPDOs in TX buffers */
    if(CANmodule->CANtxCount != 0U){
        uint16_t i;
        CO_CANtx_t *buffer = &CANmodule->txArray[0];
        for(i = CANmodule->txSize; i > 0U; i--){
            if(buffer->bufferFull){
                if(buffer->syncFlag){
                    buffer->bufferFull = CO_false;
                    CANmodule->CANtxCount--;
                    tpdoDeleted = 2U;
                }
            }
            buffer++;
        }
    }
    CO_ENABLE_INTERRUPTS();


    if(tpdoDeleted != 0U){
        CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_TPDO_OUTSIDE_WINDOW, CO_EMC_COMMUNICATION, tpdoDeleted);
    }
#endif
}


/******************************************************************************/
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule){
#if function_canGetErrorCounters_works_without_loosing_messages
    unsigned rxErrors, txErrors;
    CO_EM_t* em = (CO_EM_t*)CANmodule->em;
    uint32_t err;

    canGetErrorCounters(CANmodule->CANbaseAddress, &rxErrors, &txErrors);
    if(txErrors > 0xFFFF) txErrors = 0xFFFF;
    if(rxErrors > 0xFF) rxErrors = 0xFF;

    err = ((uint32_t)txErrors << 16) | ((uint32_t)rxErrors << 8) | CANmodule->error;

    if(CANmodule->errOld != err){
        CANmodule->errOld = err;

        if(txErrors >= 256U){                               /* bus off */
            CO_errorReport(em, CO_EM_CAN_TX_BUS_OFF, CO_EMC_BUS_OFF_RECOVERED, err);
        }
        else{                                               /* not bus off */
            CO_errorReset(em, CO_EM_CAN_TX_BUS_OFF, err);

            if((rxErrors >= 96U) || (txErrors >= 96U)){     /* bus warning */
                CO_errorReport(em, CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, err);
            }

            if(rxErrors >= 128U){                           /* RX bus passive */
                CO_errorReport(em, CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
            }
            else{
                CO_errorReset(em, CO_EM_CAN_RX_BUS_PASSIVE, err);
            }

            if(txErrors >= 128U){                           /* TX bus passive */
                if(!CANmodule->firstCANtxMessage){
                    CO_errorReport(em, CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
                }
            }
            else{
                CO_bool_t isError = CO_isError(em, CO_EM_CAN_TX_BUS_PASSIVE);
                if(isError){
                    CO_errorReset(em, CO_EM_CAN_TX_BUS_PASSIVE, err);
                    CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
                }
            }

            if((rxErrors < 96U) && (txErrors < 96U)){       /* no error */
                CO_bool_t isError = CO_isError(em, CO_EM_CAN_BUS_WARNING);
                if(isError){
                    CO_errorReset(em, CO_EM_CAN_BUS_WARNING, err);
                    CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
                }
            }
        }

        if(CANmodule->error & 0x02){                       /* CAN RX bus overflow */
            CO_errorReport(em, CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, err);
        }
    }
#endif
}


#ifdef USE_CAN_CALLBACKS
/******************************************************************************/
int CO_CANinterrupt(CO_CANmodule_t *CANmodule, CanEvent event, const CanMsg *msg){

    /* receive interrupt (New CAN messagge is available in RX FIFO buffer) */
    if(event == CAN_EVENT_RX){
        CO_CANrxMsg_t *rcvMsg;      /* pointer to received message in CAN module */
        uint16_t i;                 /* index of received message */
        CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */
        CO_bool_t msgMatched = CO_false;

        rcvMsg = (CO_CANrxMsg_t *) msg;  /* structures are aligned */
        /* CAN module filters are not used, message with any standard 11-bit identifier */
        /* has been received. Search rxArray form CANmodule for the same CAN-ID. */
        buffer = &CANmodule->rxArray[0];
        for(i = CANmodule->rxSize; i > 0; i--){
            if(((rcvMsg->ident ^ buffer->ident) & buffer->mask) == 0){
                msgMatched = CO_true;
                break;
            }
            buffer++;
        }

        /* Call specific function, which will process the message */
        if(msgMatched && (buffer != NULL) && (buffer->pFunct != NULL)){
            buffer->pFunct(buffer->object, rcvMsg);
        }

#ifdef CO_LOG_CAN_MESSAGES
        void CO_logMessage(const CanMsg *msg);
        CO_logMessage(msg);
#endif

        return 0;
    }

    /* transmit interrupt (TX buffer is free) */
    if(event == CAN_EVENT_TX){
        /* First CAN message (bootup) was sent successfully */
        CANmodule->firstCANtxMessage = CO_false;
        /* clear flag from previous message */
        CANmodule->bufferInhibitFlag = CO_false;

#ifdef CO_LOG_CAN_MESSAGES
        void CO_logMessage(const CanMsg *msg);
        CO_logMessage((const CanMsg*) &CANmodule->txRecord);
#endif
        /* Are there any new messages waiting to be send */
        if(CANmodule->CANtxCount > 0U){
            uint16_t i;             /* index of transmitting message */

            /* first buffer */
            CO_CANtx_t *buffer = &CANmodule->txArray[0];
            /* search through whole array of pointers to transmit message buffers. */
            for(i = CANmodule->txSize; i > 0U; i--){
                /* if message buffer is full, send it. */
                if(buffer->bufferFull){
                    buffer->bufferFull = CO_false;
                    CANmodule->CANtxCount--;
                    CANmodule->bufferInhibitFlag = CO_true;   /* indicate, that message is on CAN module */
                    /* Copy message to CAN buffer and exit for loop. */
                    canSend(CANmodule->CANbaseAddress, (const CanMsg*) buffer, FALSE);
#ifdef CO_LOG_CAN_MESSAGES
                    memcpy((void*)&CANmodule->txRecord, (void*)buffer, sizeof(CO_CANtx_t));
#endif
                    break;                      /* exit for loop */
                }
                buffer++;
            }/* end of for loop */

            /* Clear counter if no more messages */
            if(i == 0U){
                CANmodule->CANtxCount = 0U;
            }
        }
        return 0;
    }

    if(event == CAN_EVENT_BUS_OFF){
        CANmodule->error |= 0x01;
        return 0;
    }

    if(event == CAN_EVENT_OVERRUN){
        CANmodule->error |= 0x02;
        return 0;
    }
    return 0;
}
#else
void CO_CANreceive(CO_CANmodule_t *CANmodule){
    /* pool the messages from receive buffer */
    while(canPeek(CANmodule->CANbaseAddress, 0) == CAN_ERROR_NO){
        CO_CANrxMsg_t rcvMsg;
        uint16_t i;         /* index of received message */
        CO_CANrx_t *buffer;/* receive message buffer from CO_CANmodule_t object. */
        CO_bool_t msgMatched = CO_false;
        canRecv(CANmodule->CANbaseAddress, (CanMsg*)&rcvMsg, 0);

        buffer = &CANmodule->rxArray[0];
        for(i = CANmodule->rxSize; i > 0; i--){
            if(((rcvMsg.ident ^ buffer->ident) & buffer->mask) == 0){
                msgMatched = CO_true;
                break;
            }
            buffer++;
        }

        /* Call specific function, which will process the message */
        if(msgMatched && (buffer->pFunct != 0)){
            buffer->pFunct(buffer->object, &rcvMsg);
        }

#ifdef CO_LOG_CAN_MESSAGES
        void CO_logMessage(const CanMsg *msg);
        CO_logMessage((CanMsg*)&rcvMsg);
#endif
    }
}
#endif
