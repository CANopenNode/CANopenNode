/*
 * CAN module object for Linux SocketCAN.
 *
 * @file        CO_driver.c
 * @author      Janez Paternoster
 * @copyright   2015 Janez Paternoster
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
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->error = 0;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;
    CANmodule->em = NULL;

    for(i=0U; i<rxSize; i++){
        rxArray[i].ident = 0U;
        rxArray[i].pFunct = NULL;
    }
    for(i=0U; i<txSize; i++){
        txArray[i].bufferFull = false;
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
        bool_t                  rtr,
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
        bool_t                  rtr,
        uint8_t                 noOfBytes,
        bool_t                  syncFlag)
{
    CO_CANtx_t *buffer = NULL;

    if((CANmodule != NULL) && (index < CANmodule->txSize)){
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

        /* CAN identifier, bit aligned with CAN module registers */
        buffer->ident = canEncodeId(ident, FALSE, rtr);

        buffer->DLC = noOfBytes;
        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;
    CanError canErr = CAN_ERROR_NO;

    CO_DISABLE_INTERRUPTS();
    canErr = canSend(CANmodule->CANbaseAddress, (const CanMsg*) buffer, FALSE);
#ifdef CO_LOG_CAN_MESSAGES
    void CO_logMessage(const CanMsg *msg);
    CO_logMessage((const CanMsg*) buffer);
#endif
    CO_ENABLE_INTERRUPTS();

    if(canErr != CAN_ERROR_NO){
        CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_GENERIC_ERROR, CO_EMC_GENERIC, canErr);
        err = CO_ERROR_TX_OVERFLOW;
    }

    return err;
}


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule){
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
                bool_t isError = CO_isError(em, CO_EM_CAN_TX_BUS_PASSIVE);
                if(isError){
                    CO_errorReset(em, CO_EM_CAN_TX_BUS_PASSIVE, err);
                    CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
                }
            }

            if((rxErrors < 96U) && (txErrors < 96U)){       /* no error */
                bool_t isError = CO_isError(em, CO_EM_CAN_BUS_WARNING);
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


/******************************************************************************/
void CO_CANreceive(CO_CANmodule_t *CANmodule){
    /* pool the messages from receive buffer */
    while(canPeek(CANmodule->CANbaseAddress, 0) == CAN_ERROR_NO){
        CO_CANrxMsg_t rcvMsg;
        uint16_t i;         /* index of received message */
        CO_CANrx_t *buffer;/* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;
        canRecv(CANmodule->CANbaseAddress, (CanMsg*)&rcvMsg, 0);

        buffer = &CANmodule->rxArray[0];
        for(i = CANmodule->rxSize; i > 0; i--){
            if(((rcvMsg.ident ^ buffer->ident) & buffer->mask) == 0){
                msgMatched = true;
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
