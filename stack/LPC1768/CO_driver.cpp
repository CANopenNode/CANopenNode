/*
 * CAN module object for LPC1768 microcontroller using Mbed SDK.
 *
 * @file        CO_driver.cpp
 * @author      Benoit Rapidel
 * @copyright   2016 Benoit Rapidel
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


#include "mbed.h"
#include "LPC17xx.h"
#include "CAN.h"

extern "C" {
#include "CO_driver.h"
#include "CO_Emergency.h"

#include "CO_CAN.h"
}

CAN CANport0(MBED_CAN_RX, MBED_CAN_TX);
CAN *CANport = NULL;


/* helper functions */

CANMessage toCANMessage(CO_CANtx_t *CO_msg) {
    CANMessage msg;
    msg.id = (uint32_t) CO_msg->ident & 0x07FFU;
    msg.len = (uint32_t) CO_msg->DLC;
    msg.type =
        ((uint32_t) CO_msg->ident & 0x8000U) == 0x8000 ?
        CANRemote : CANData;
    memcpy(msg.data, CO_msg->data, CO_msg->DLC);

    return msg;
}

void fromCANMessage(CANMessage *msg, CO_CANrxMsg_t *CO_msg) {
    CO_msg->ident = (uint32_t) msg->id & 0x07FFU;
    CO_msg->DLC = (uint32_t) msg->len;
    CO_msg->ident =
        (msg->type == CANRemote) ? CO_msg->ident | 0x8000 : CO_msg->ident;
    memcpy(CO_msg->data, msg->data, CO_msg->DLC);
}

/******************************************************************************/
void CO_CANsetConfigurationMode(int32_t CANbaseAddress){
    /* Put CAN module in configuration mode */
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){
    /* Put CAN module in normal mode */
    // CANport->mode(CAN::Reset);

    // can_mode(LPC_CAN2->, CAN::Normal);
    // LPC_IOCON->PIO0_4 = PIN_INPUT P
    CANmodule->CANnormal = true;
}


/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        int32_t                 CANbaseAddress,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate)
{
    uint16_t i;
    uint16_t freq_err = 0;

    /* verify arguments */
    if(CANmodule==NULL || rxArray==NULL || txArray==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->CANbaseAddress = CANbaseAddress;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = false; //(rxSize <= 32U) ? true : false;/* microcontroller dependent */
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
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


    /* Configure CAN module registers */
    CANport = &CANport0;
    // CANport->reset();


    /* Configure CAN timing */
    int CANbaudRate = CANbitRate * 1000;
    freq_err = CANport->frequency(CANbaudRate);
    if(freq_err != 1) {
        return CO_ERROR_PARAMETERS;
    }


    /* Configure CAN module hardware filters */
    if(CANmodule->useCANrxFilters){
        /* CAN module filters are used, they will be configured with */
        /* CO_CANrxBufferInit() functions, called by separate CANopen */
        /* init functions. */
        /* Configure all masks so, that received message must match filter */
        // TODO
    }
    else{
        /* CAN module filters are not used, all messages with standard 11-bit */
        /* identifier will be received */
        /* Configure mask 0 so, that all messages with standard identifier are accepted */
        // CANport->filter(0, 0, CANAny);
    }


    /* configure CAN interrupt registers */


    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule){
    /* turn off the module */
}


/******************************************************************************/
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg){
    return (uint16_t) rxMsg->ident;
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

        /* CAN identifier and CAN mask, bit aligned with CAN module. Different on different microcontrollers. */
        buffer->ident = ident & 0x07FFU;
        if(rtr){
            buffer->ident |= 0x0800U;
        }
        buffer->mask = (mask & 0x07FFU) | 0x0800U;

        /* Set CAN hardware module filter and mask. */
        if(CANmodule->useCANrxFilters){
            // TODO
        }
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

        /* CAN identifier, DLC and rtr, bit aligned with CAN module transmit buffer.
         * Microcontroller specific. */
        buffer->ident = ((uint32_t) ident & 0x07FFU);
        buffer->DLC = ((uint32_t) noOfBytes & 0xFU);
        /* toggle RTR bit if CAN message is remote type */
        if (rtr) buffer->ident |= 0x8000U;

        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;

    CANMessage msg;

    /* Verify overflow */
    if(buffer->bufferFull){
        if(!CANmodule->firstCANtxMessage){
            /* don't set error, if bootup message is still on buffers */
            CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, buffer->ident);
        }
        err = CO_ERROR_TX_OVERFLOW;
        // USBport.printf("BUFF_FULL OVERFLOW\r\n");
    }

    CO_LOCK_CAN_SEND();
    /* if CAN TX buffer is free, copy message to it */
    if ((MBED_CHECK_TX_BUFFERS) && CANmodule->CANtxCount == 0){
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        /* copy message and txRequest */
        // USBport.printf("SEND\r\n");
        CANport->write(toCANMessage(buffer));
    }
    /* if no buffer is free, message will be sent by interrupt */
    else{
        // USBport.printf("BUFF_FULL\r\n");
        buffer->bufferFull = true;
        CANmodule->CANtxCount++;
    }
    CO_UNLOCK_CAN_SEND();

    return err;
}


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule){
    uint32_t tpdoDeleted = 0U;

    CO_LOCK_CAN_SEND();
    /* Abort message from CAN module, if there is synchronous TPDO.
     * Take special care with this functionality. */
    if (((MBED_CAN_REG->GSR & (1 << 2)) == 0) && CANmodule->bufferInhibitFlag){
        /* clear TXREQ */
        MBED_CAN_REG->CMR = (1 << 5) | (1 << 1); // Select TX buffer 1 and run AT command (abort)
        MBED_CAN_REG->CMR = (1 << 6) | (1 << 1); // Select TX buffer 2 and run AT command (abort)
        MBED_CAN_REG->CMR = (1 << 7) | (1 << 1); // Select TX buffer 3 and run AT command (abort)

        CANmodule->bufferInhibitFlag = false;
        tpdoDeleted = 1U;
    }
    /* delete also pending synchronous TPDOs in TX buffers */
    if(CANmodule->CANtxCount != 0U){
        uint16_t i;
        CO_CANtx_t *buffer = &CANmodule->txArray[0];
        for(i = CANmodule->txSize; i > 0U; i--){
            if(buffer->bufferFull){
                if(buffer->syncFlag){
                    buffer->bufferFull = false;
                    CANmodule->CANtxCount--;
                    tpdoDeleted = 2U;
                }
            }
            buffer++;
        }
    }
    CO_UNLOCK_CAN_SEND();


    if(tpdoDeleted != 0U){
        CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_TPDO_OUTSIDE_WINDOW, CO_EMC_COMMUNICATION, tpdoDeleted);
    }
}


/******************************************************************************/
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule){
    uint16_t rxErrors, txErrors, overflow;
    CO_EM_t* em = (CO_EM_t*)CANmodule->em;
    uint32_t err;

    /* get error counters from module. Id possible, function may use different way to
     * determine errors. */
    rxErrors = CANport->rderror();
    txErrors = CANport->tderror();
    overflow = (MBED_CAN_REG->GSR & 0x2);

    err = ((uint32_t)txErrors << 16) | ((uint32_t)rxErrors << 8) | overflow;

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
                CO_errorReset(em, CO_EM_CAN_BUS_WARNING, err);
            }
        }

        if(overflow != 0U){                                 /* CAN RX bus overflow */
            CO_errorReport(em, CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, err);
        }
    }
}


/******************************************************************************/
void CO_CANinterrupt(CO_CANmodule_t *CANmodule){
    uint16_t intStatus;

    intStatus = MBED_CAN_REG->ICR;

    /* receive interrupt */
    if (intStatus & 0x1) {
        CO_CANrxMsg_t rcvMsgBuf;    /* buffer for the received message in CAN module */
        CO_CANrxMsg_t *rcvMsg;      /* pointer to received message in CAN module */
        uint16_t index;             /* index of received message */
        uint32_t rcvMsgIdent;       /* identifier of the received message */
        CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;
        CANMessage msg;

        CANport->read(msg);
        fromCANMessage(&msg, &rcvMsgBuf); /* get message from module here */
        rcvMsg = &rcvMsgBuf;
        rcvMsgIdent = rcvMsg->ident;
        if(CANmodule->useCANrxFilters){
            /* CAN module filters are used. Message with known 11-bit identifier has */
            /* been received */
            index = 0;  /* get index of the received message here. Or something similar */
            if(index < CANmodule->rxSize){
                buffer = &CANmodule->rxArray[index];
                /* verify also RTR */
                if(((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U){
                    msgMatched = true;
                }
            }
        }
        else{
            /* CAN module filters are not used, message with any standard 11-bit identifier */
            /* has been received. Search rxArray form CANmodule for the same CAN-ID. */
            buffer = &CANmodule->rxArray[0];
            for(index = CANmodule->rxSize; index > 0U; index--){
                if(((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U){
                    msgMatched = true;
                    break;
                }
                buffer++;
            }
        }

        /* Call specific function, which will process the message */
        if(msgMatched && (buffer != NULL) && (buffer->pFunct != NULL)){
            buffer->pFunct(buffer->object, rcvMsg);
        }

        /* Clear interrupt flag */
        /* The interrupt flag is cleaned by CANport.read() function call */
    }


    /* transmit interrupt */
    else if ((intStatus & 0x2) || (intStatus & 0x200) || (intStatus & 0x400)) {
        /* Clear interrupt flag */

        /* First CAN message (bootup) was sent successfully */
        CANmodule->firstCANtxMessage = false;
        /* clear flag from previous message */
        CANmodule->bufferInhibitFlag = false;
        /* Are there any new messages waiting to be send */
        if(CANmodule->CANtxCount > 0U){
            uint16_t i;             /* index of transmitting message */

            /* first buffer */
            CO_CANtx_t *buffer = &CANmodule->txArray[0];
            /* search through whole array of pointers to transmit message buffers. */
            for(i = CANmodule->txSize; i > 0U; i--){
                /* if message buffer is full, send it. */
                if(buffer->bufferFull){
                    buffer->bufferFull = false;
                    CANmodule->CANtxCount--;

                    /* Copy message to CAN buffer */
                    CANmodule->bufferInhibitFlag = buffer->syncFlag;
                    /* canSend... */
                    CANport->write(toCANMessage(buffer));
                    break;                      /* exit for loop */
                }
                buffer++;
            }/* end of for loop */

            /* Clear counter if no more messages */
            if(i == 0U){
                CANmodule->CANtxCount = 0U;
            }
        }
    }
}
