/*
 * CAN module object for Microchip dsPIC30F microcontroller.
 *
 * @file        CO_driver.c
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


#include "CO_driver.h"
#include "CO_Emergency.h"


extern const CO_CANbitRateData_t  CO_CANbitRateData[8];

/**
 * Macro and Constants - CAN module registers - offset.
 */
    #define CAN_REG(base, offset) (*((volatile uint16_t *) (((uintptr_t) base) + offset)))

    #define C_RXF0SID    0x00
    #define C_RXF0EIDH   0x02
    #define C_RXF0EIDL   0x04
    #define C_RXF1SID    0x08
    #define C_RXF1EIDH   0x0A
    #define C_RXF1EIDL   0x0C
    #define C_RXF2SID    0x10
    #define C_RXF2EIDH   0x12
    #define C_RXF2EIDL   0x14
    #define C_RXF3SID    0x18
    #define C_RXF3EIDH   0x1A
    #define C_RXF3EIDL   0x1C
    #define C_RXF4SID    0x20
    #define C_RXF4EIDH   0x22
    #define C_RXF4EIDL   0x24
    #define C_RXF5SID    0x28
    #define C_RXF5EIDH   0x2A
    #define C_RXF5EIDL   0x2C
    #define C_RXM0SID    0x30
    #define C_RXM0EIDH   0x32
    #define C_RXM0EIDL   0x34
    #define C_RXM1SID    0x38
    #define C_RXM1EIDH   0x3A
    #define C_RXM1EIDL   0x3C

    #define C_TXBUF2     0x40
    #define C_TXBUF1     0x50
    #define C_TXBUF0     0x60
        #define C_TXSID   0x0
        #define C_TXEID   0x2
        #define C_TXDLC   0x4
        #define C_TXB     0x6
        #define C_TXCON   0xE

    #define C_RXBUF1     0x70
    #define C_RXBUF0     0x80
        #define C_RXCON   0xE
    /* register alignment as in CO_CANrxMsg_t */

    #define C_CTRL       0x90
    #define C_CFG1       0x92
    #define C_CFG2       0x94
    #define C_INTF       0x96
    #define C_INTE       0x98
    #define C_EC         0x9A


/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANdriverState){
    uint16_t C_CTRLcopy = CAN_REG(CANdriverState, C_CTRL);

    /* set REQOP = 0x4 */
    C_CTRLcopy &= 0xFCFF;
    C_CTRLcopy |= 0x0400;
    CAN_REG(CANdriverState, C_CTRL) = C_CTRLcopy;

    /* while OPMODE != 4 */
    while((CAN_REG(CANdriverState, C_CTRL) & 0x00E0) != 0x0080);
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){
    uint16_t C_CTRLcopy = CAN_REG(CANmodule->CANdriverState, C_CTRL);

    /* set REQOP = 0x0 */
    C_CTRLcopy &= 0xF8FF;
    CAN_REG(CANmodule->CANdriverState, C_CTRL) = C_CTRLcopy;

    /* while OPMODE != 0 */
    while((CAN_REG(CANmodule->CANdriverState, C_CTRL) & 0x00E0) != 0x0000);

    CANmodule->CANnormal = true;
}


/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        void                   *CANdriverState,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate)
{
    uint16_t i;

    /* verify arguments */
    if(CANmodule==NULL || rxArray==NULL || txArray==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->CANdriverState = CANdriverState;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = false;
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


    /* Configure control register */
    CAN_REG(CANdriverState, C_CTRL) = 0x0400;


    /* Configure CAN timing */
    switch(CANbitRate){
        case 10:   i=0; break;
        case 20:   i=1; break;
        case 50:   i=2; break;
        default:
        case 125:  i=3; break;
        case 250:  i=4; break;
        case 500:  i=5; break;
        case 800:  i=6; break;
        case 1000: i=7; break;
    }

    if(CO_CANbitRateData[i].scale == 1)
        CAN_REG(CANdriverState, C_CTRL) |= 0x0800;

    CAN_REG(CANdriverState, C_CFG1) = (CO_CANbitRateData[i].SJW - 1) << 6 |
                                         (CO_CANbitRateData[i].BRP - 1);

    CAN_REG(CANdriverState, C_CFG2) = ((uint16_t)(CO_CANbitRateData[i].phSeg2 - 1)) << 8 |
                                        0x0080 |
                                        (CO_CANbitRateData[i].phSeg1 - 1) << 3 |
                                        (CO_CANbitRateData[i].PROP - 1);


    /* setup RX and TX control registers */
    CAN_REG(CANdriverState, C_RXBUF0 + C_RXCON) = 0x0040;
    CAN_REG(CANdriverState, C_RXBUF1 + C_RXCON) = 0x0000;
    CAN_REG(CANdriverState, C_TXBUF0 + C_TXCON) = 0x0000;
    CAN_REG(CANdriverState, C_TXBUF1 + C_TXCON) = 0x0000;
    CAN_REG(CANdriverState, C_TXBUF2 + C_TXCON) = 0x0000;


    /* CAN module hardware filters */
    CAN_REG(CANdriverState, C_RXF0SID) = 0x0000;
    CAN_REG(CANdriverState, C_RXF1SID) = 0x0000;
    CAN_REG(CANdriverState, C_RXF2SID) = 0x0000;
    CAN_REG(CANdriverState, C_RXF3SID) = 0x0000;
    CAN_REG(CANdriverState, C_RXF4SID) = 0x0000;
    CAN_REG(CANdriverState, C_RXF5SID) = 0x0000;
    /* CAN module filters are not used, all messages with standard 11-bit */
    /* identifier will be received */
    /* Set masks so, that all messages with standard identifier are accepted */
    CAN_REG(CANdriverState, C_RXM0SID) = 0x0001;
    CAN_REG(CANdriverState, C_RXM1SID) = 0x0001;


    /* CAN interrupt registers */
    /* clear interrupt flags */
    CAN_REG(CANdriverState, C_INTF) = 0x0000;
    /* enable both two receive interrupts and one transmit interrupt for TX0 */
    CAN_REG(CANdriverState, C_INTE) = 0x0007;
    /* CAN interrupt (combined) must be configured by application */

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule){
    CO_CANsetConfigurationMode(CANmodule->CANdriverState);
}


/******************************************************************************/
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg){
    return (rxMsg->ident >> 2) & 0x7FF;
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
        uint16_t RXF, RXM;

        /* Configure object variables */
        buffer->object = object;
        buffer->pFunct = pFunct;


        /* CAN identifier and CAN mask, bit aligned with CAN module registers */
        RXF = (ident & 0x07FF) << 2;
        if(rtr){
            RXF |= 0x02;
        }
        RXM = (mask & 0x07FF) << 2;
        RXM |= 0x02;

        /* configure filter and mask */
        if(RXF != buffer->ident || RXM != buffer->mask){
            buffer->ident = RXF;
            buffer->mask = RXM;
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

        /* CAN identifier, bit aligned with CAN module registers */
        uint16_t TXF;
        TXF = ident << 5;
        TXF &= 0xF800;
        TXF |= (ident & 0x003F) << 2;
        if(rtr){
            TXF |= 0x02;
        }

        /* write to buffer */
        buffer->ident = TXF;
        buffer->DLC = noOfBytes;
        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}


/* Copy message to CAN module - internal usage only.
 *
 * @param dest Destination address of CAN module transmit buffer.
 * @param src Pointer to source message <CO_CANtx>.
 */
static void CO_CANsendToModule(uint16_t dest, CO_CANtx_t *src){
    uint8_t DLC;
    volatile uint8_t *CANdataBuffer;
    uint8_t *pData;

    /* CAN-ID + RTR */
    CAN_REG(dest, C_TXSID) = src->ident;

    /* Data lenght */
    DLC = src->DLC;
    if(DLC > 8) DLC = 8;
    CAN_REG(dest, C_TXDLC) = DLC << 3;

    /* copy data */
    CANdataBuffer = (volatile uint8_t*)(dest + C_TXB);
    pData = src->data;
    for(; DLC>0; DLC--) *(CANdataBuffer++) = *(pData++);

    /* control register, transmit request */
    CAN_REG(dest, C_TXCON) |= 0x08;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;
    uint16_t addr = CANmodule->CANdriverState;

    /* Verify overflow */
    if(buffer->bufferFull){
        if(!CANmodule->firstCANtxMessage){
            /* don't set error, if bootup message is still on buffers */
            CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, 0);
        }
        err = CO_ERROR_TX_OVERFLOW;
    }

    CO_LOCK_CAN_SEND();
    /* if CAN TB buffer is free, copy message to it */
    if((CAN_REG(addr, C_TXBUF0 + C_TXCON) & 0x8) == 0 && CANmodule->CANtxCount == 0){
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        CO_CANsendToModule(addr + C_TXBUF0, buffer);
    }
    /* if no buffer is free, message will be sent by interrupt */
    else{
        buffer->bufferFull = true;
        CANmodule->CANtxCount++;
    }
    CO_UNLOCK_CAN_SEND();

    return err;
}


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule){

    /* See generic driver for implemetation. */
}


/******************************************************************************/
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule){
    uint8_t err;
    CO_EM_t* em = (CO_EM_t*)CANmodule->em;

    err = CAN_REG(CANmodule->CANdriverState, C_INTF)>>8;

    if(CANmodule->errOld != err){
        CANmodule->errOld = err;

        /* CAN RX bus overflow */
        if(err & 0xC0){
            CO_errorReport(em, CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, err);
            CAN_REG(CANmodule->CANdriverState, C_INTF) &= 0x3FFF;/* clear bits */
        }

        /* CAN TX bus off */
        if(err & 0x20){
            CO_errorReport(em, CO_EM_CAN_TX_BUS_OFF, CO_EMC_BUS_OFF_RECOVERED, err);
        }
        else{
            CO_errorReset(em, CO_EM_CAN_TX_BUS_OFF, err);
        }

        /* CAN TX bus passive */
        if(err & 0x10){
            if(!CANmodule->firstCANtxMessage) CO_errorReport(em, CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
        }
        else{
            int8_t isError = CO_isError(em, CO_EM_CAN_TX_BUS_PASSIVE);
            if(isError){
                CO_errorReset(em, CO_EM_CAN_TX_BUS_PASSIVE, err);
                CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
            }
        }

        /* CAN RX bus passive */
        if(err & 0x08){
            CO_errorReport(em, CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
        }
        else{
            CO_errorReset(em, CO_EM_CAN_RX_BUS_PASSIVE, err);
        }

        /* CAN TX or RX bus warning */
        if(err & 0x19){
            CO_errorReport(em, CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, err);
        }
        else{
            CO_errorReset(em, CO_EM_CAN_BUS_WARNING, err);
        }
    }
}


/******************************************************************************/
void CO_CANinterrupt(CO_CANmodule_t *CANmodule){
    uint16_t ICODE;
    ICODE = CAN_REG(CANmodule->CANdriverState, C_CTRL) & 0xE;

    /* receive interrupt 0 (New CAN messagge is available in RX buffer 0) */
    if(ICODE == 0xC){
        CO_CANrxMsg_t *rcvMsg;      /* pointer to received message in CAN module */
        uint16_t index;             /* index of received message */
        uint16_t rcvMsgIdent;       /* identifier of the received message */
        CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;

        rcvMsg = (CO_CANrxMsg_t*) (CANmodule->CANdriverState + C_RXBUF0);
        rcvMsgIdent = rcvMsg->ident;
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

        /* Call specific function, which will process the message */
        if(msgMatched && (buffer != NULL) && (buffer->pFunct != NULL)){
            buffer->pFunct(buffer->object, rcvMsg);
        }

        /* Clear RXFUL flag */
        rcvMsg->CON &= 0xFF7F;

        /* Clear interrupt flag */
        CAN_REG(CANmodule->CANdriverState, C_INTF) &= 0xFFFE;
    }


    /* receive interrupt 1 (New CAN messagge is available in RX buffer 1) */
    else if(ICODE == 0xA){
        CO_CANrxMsg_t *rcvMsg;      /* pointer to received message in CAN module */
        uint16_t index;             /* index of received message */
        uint16_t rcvMsgIdent;       /* identifier of the received message */
        CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;

        rcvMsg = (CO_CANrxMsg_t*) (CANmodule->CANdriverState + C_RXBUF1);
        rcvMsgIdent = rcvMsg->ident;
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

        /* Call specific function, which will process the message */
        if(msgMatched && (buffer != NULL) && (buffer->pFunct != NULL)){
            buffer->pFunct(buffer->object, rcvMsg);
        }

        /* Clear RXFUL flag */
        rcvMsg->CON &= 0xFF7F;

        /* Clear interrupt flag */
        CAN_REG(CANmodule->CANdriverState, C_INTF) &= 0xFFFD;
    }


    /* transmit interrupt (TX buffer is free) */
    else if(ICODE == 0x8){
        /* Clear interrupt flag */
        CAN_REG(CANmodule->CANdriverState, C_INTF) &= 0xFFFB;
        /* First CAN message (bootup) was sent successfully */
        CANmodule->firstCANtxMessage = false;
        /* clear flag from previous message */
        CANmodule->bufferInhibitFlag = false;
        /* Are there any new messages waiting to be send and buffer is free */
        if(CANmodule->CANtxCount > 0U && (CAN_REG(CANmodule->CANdriverState, C_TXBUF0 + C_TXCON) & 0x8) == 0){
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
                    CO_CANsendToModule(CANmodule->CANdriverState + C_TXBUF0, buffer);
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
