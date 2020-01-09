/*
 * CAN module object for Microchip PIC32MX microcontroller.
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
unsigned int CO_interruptStatus = 0;


/**
 * Macro and Constants - CAN module registers
 */
    #define CAN_REG(base, offset) (*((volatile uint32_t *) (((uintptr_t) base) + _CAN1_BASE_ADDRESS + (offset))))

    #define CLR          0x04
    #define SET          0x08
    #define INV          0x0C

    #define C_CON        0x000                         /* Control Register */
    #define C_CFG        0x010                         /* Baud Rate Configuration Register */
    #define C_INT        0x020                         /* Interrupt Register */
    #define C_VEC        0x030                         /* Interrupt Code Register */
    #define C_TREC       0x040                         /* Transmit/Receive Error Counter Register */
    #define C_FSTAT      0x050                         /* FIFO Status  Register */
    #define C_RXOVF      0x060                         /* Receive FIFO Overflow Status Register */
    #define C_TMR        0x070                         /* CAN Timer Register */
    #define C_RXM        0x080 /*  + (0..3 x 0x10)      //Acceptance Filter Mask Register */
    #define C_FLTCON     0x0C0 /*  + (0..7(3) x 0x10)   //Filter Control Register */
    #define C_RXF        0x140 /*  + (0..31(15) x 0x10) //Acceptance Filter Register */
    #define C_FIFOBA     0x340                         /* Message Buffer Base Address Register */
    #define C_FIFOCON    0x350 /*  + (0..31(15) x 0x40) //FIFO Control Register */
    #define C_FIFOINT    0x360 /*  + (0..31(15) x 0x40) //FIFO Interrupt Register */
    #define C_FIFOUA     0x370 /*  + (0..31(15) x 0x40) //FIFO User Address Register */
    #define C_FIFOCI     0x380 /*  + (0..31(15) x 0x40) //Module Message Index Register */


/* Number of hardware filters */
/* device PIC32MX530, 550 and 570 has only 16 registers for CAN reception (not 32). */
#ifdef __PIC32MX
#if (__PIC32_FEATURE_SET__ == 530) || (__PIC32_FEATURE_SET__ == 550) || (__PIC32_FEATURE_SET__ == 570)
    #define NO_CAN_RXF  16
#endif
#endif
#ifndef NO_CAN_RXF
    #define NO_CAN_RXF  32
#endif


/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANdriverState){
    uint32_t C_CONcopy = CAN_REG(CANdriverState, C_CON);

    /* switch ON can module */
    C_CONcopy |= 0x00008000;
    CAN_REG(CANdriverState, C_CON) = C_CONcopy;

    /* request configuration mode */
    C_CONcopy &= 0xF8FFFFFF;
    C_CONcopy |= 0x04000000;
    CAN_REG(CANdriverState, C_CON) = C_CONcopy;

    /* wait for configuration mode */
    while((CAN_REG(CANdriverState, C_CON) & 0x00E00000) != 0x00800000);
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){

    /* request normal mode */
    CAN_REG(CANmodule->CANdriverState, C_CON+CLR) = 0x07000000;

    /* wait for normal mode */
    while((CAN_REG(CANmodule->CANdriverState, C_CON) & 0x00E00000) != 0x00000000);

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
    CANmodule->CANmsgBuffSize = 33; /* Must be the same as size of CANmodule->CANmsgBuff array. */
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = (rxSize <= NO_CAN_RXF) ? true : false;
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

    /* clear FIFO */
    if(sizeof(CO_CANrxMsg_t) != 16) while(1);/* some safety */
    uint32_t* f = (uint32_t*) CANmodule->CANmsgBuff;
    for(i=0; i<(CANmodule->CANmsgBuffSize*4); i++){
        *(f++) = 0;
    }


    /* Configure control register (configuration mode, receive timer stamp is enabled, module is on) */
    CAN_REG(CANdriverState, C_CON) = 0x04108000;


    /* Configure FIFO */
    CAN_REG(CANdriverState, C_FIFOBA) = CO_KVA_TO_PA(CANmodule->CANmsgBuff);/* FIFO base address */
    CAN_REG(CANdriverState, C_FIFOCON) = (NO_CAN_RXF==32) ? 0x001F0000 : 0x000F0000;     /* FIFO0: receive FIFO, 32(16) buffers */
    CAN_REG(CANdriverState, C_FIFOCON+0x40) = 0x00000080;/* FIFO1: transmit FIFO, 1 buffer */


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
    CAN_REG(CANdriverState, C_CFG) =
        ((uint32_t)(CO_CANbitRateData[i].phSeg2 - 1)) << 16 |  /* SEG2PH */
        0x00008000                                            |  /* SEG2PHTS = 1, SAM = 0 */
        ((uint32_t)(CO_CANbitRateData[i].phSeg1 - 1)) << 11 |  /* SEG1PH */
        ((uint32_t)(CO_CANbitRateData[i].PROP - 1))   << 8  |  /* PRSEG */
        ((uint32_t)(CO_CANbitRateData[i].SJW - 1))    << 6  |  /* SJW */
        ((uint32_t)(CO_CANbitRateData[i].BRP - 1));            /* BRP */


    /* CAN module hardware filters */
    /* clear all filter control registers (disable filters, mask 0 and FIFO 0 selected for all filters) */
    for(i=0; i<(NO_CAN_RXF/4); i++)
        CAN_REG(CANdriverState, C_FLTCON+i*0x10) = 0x00000000;
    if(CANmodule->useCANrxFilters){
        /* CAN module filters are used, they will be configured with */
        /* CO_CANrxBufferInit() functions, called by separate CANopen */
        /* init functions. */
        /* Configure all masks so, that received message must match filter */
        CAN_REG(CANdriverState, C_RXM) = 0xFFE80000;
        CAN_REG(CANdriverState, C_RXM+0x10) = 0xFFE80000;
        CAN_REG(CANdriverState, C_RXM+0x20) = 0xFFE80000;
        CAN_REG(CANdriverState, C_RXM+0x30) = 0xFFE80000;
    }
    else{
        /* CAN module filters are not used, all messages with standard 11-bit */
        /* identifier will be received */
        /* Configure mask 0 so, that all messages with standard identifier are accepted */
        CAN_REG(CANdriverState, C_RXM) = 0x00080000;
        /* configure one filter for FIFO 0 and enable it */
        CAN_REG(CANdriverState, C_RXF) = 0x00000000;
        CAN_REG(CANdriverState, C_FLTCON) = 0x00000080;
    }


    /* CAN interrupt registers */
    /* Enable 'RX buffer not empty' (RXNEMPTYIE) interrupt in FIFO 0 (third layer interrupt) */
    CAN_REG(CANdriverState, C_FIFOINT) = 0x00010000;
    /* Enable 'Tx buffer empty' (TXEMPTYIE) interrupt in FIFO 1 (third layer interrupt) */
    CAN_REG(CANdriverState, C_FIFOINT+0x40) = 0x00000000; /* will be enabled in CO_CANsend */
    /* Enable receive (RBIE) and transmit (TBIE) buffer interrupt (secont layer interrupt) */
    CAN_REG(CANdriverState, C_INT) = 0x00030000;
    /* CAN interrupt (first layer) must be configured by application */

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule){
    CO_CANsetConfigurationMode(CANmodule->CANdriverState);
}


/******************************************************************************/
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg){
    return rxMsg->ident;
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

        /* CAN identifier and CAN mask, bit aligned with CAN module FIFO buffers (RTR is extra) */
        buffer->ident = ident & 0x07FFU;
        if(rtr){
            buffer->ident |= 0x0800U;
        }
        buffer->mask = (mask & 0x07FFU) | 0x0800U;

        /* Set CAN hardware module filter and mask. */
        if(CANmodule->useCANrxFilters){
            uint32_t RXF, RXM;
            volatile uint32_t *pRXF;
            volatile uint32_t *pRXM0, *pRXM1, *pRXM2, *pRXM3;
            volatile uint8_t *pFLTCON;
            uint8_t selectMask;
            uint16_t addr = CANmodule->CANdriverState;

            /* get correct part of the filter control register */
            pFLTCON = (volatile uint8_t*)(&CAN_REG(addr, C_FLTCON)); /* pointer to first filter control register */
            pFLTCON += (index/4) * 0x10;   /* now points to the correct C_FLTCONi */
            pFLTCON += index%4;   /* now points to correct part of the correct C_FLTCONi */

            /* disable filter and wait if necessary */
            while(*pFLTCON & 0x80) *pFLTCON &= 0x7F;

            /* align RXF and RXM with C_RXF and C_RXM registers */
            RXF = (uint32_t)ident << 21;
            RXM = (uint32_t)mask << 21 | 0x00080000;

            /* write to filter */
            pRXF = &CAN_REG(addr, C_RXF); /* pointer to first filter register */
            pRXF += index * (0x10/4);   /* now points to C_RXFi (i == index) */
            *pRXF = RXF;         /* write value to filter */

            /* configure mask (There are four masks, each of them can be asigned to any filter. */
            /*                 First mask has always the value 0xFFE80000 - all 11 bits must match). */
            pRXM0 = &CAN_REG(addr, C_RXM);
            pRXM1 = &CAN_REG(addr, C_RXM+0x10);
            pRXM2 = &CAN_REG(addr, C_RXM+0x20);
            pRXM3 = &CAN_REG(addr, C_RXM+0x30);
            if(RXM == *pRXM0){
                selectMask = 0;
            }
            else if(RXM == *pRXM1 || *pRXM1 == 0xFFE80000){
                /* RXM is equal to mask 1 or mask 1 was not yet configured. */
                *pRXM1 = RXM;
                selectMask = 1;
            }
            else if(RXM == *pRXM2 || *pRXM2 == 0xFFE80000){
                /* RXM is equal to mask 2 or mask 2 was not yet configured. */
                *pRXM2 = RXM;
                selectMask = 2;
            }
            else if(RXM == *pRXM3 || *pRXM3 == 0xFFE80000){
                /* RXM is equal to mask 3 or mask 3 was not yet configured. */
                *pRXM3 = RXM;
                selectMask = 3;
            }
            else{
                /* not enough masks */
                selectMask = 0;
                ret = CO_ERROR_OUT_OF_MEMORY;
            }
            /* write to appropriate filter control register */
            *pFLTCON = 0x80 | (selectMask << 5); /* enable filter and write filter mask select bit */
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

        /* CAN identifier, DLC and rtr, bit aligned with CAN module transmit buffer */
        buffer->CMSGSID = ident & 0x07FF;
        buffer->CMSGEID = (noOfBytes & 0xF) | (rtr?0x0200:0);

        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;
    uint16_t addr = CANmodule->CANdriverState;
    volatile uint32_t* TX_FIFOcon = &CAN_REG(addr, C_FIFOCON+0x40);
    volatile uint32_t* TX_FIFOconSet = &CAN_REG(addr, C_FIFOCON+0x48);
    uint32_t* TXmsgBuffer = CO_PA_TO_KVA1(CAN_REG(addr, C_FIFOUA+0x40));
    uint32_t* message = (uint32_t*) buffer;
    uint32_t TX_FIFOconCopy;

    /* Verify overflow */
    if(buffer->bufferFull){
        if(!CANmodule->firstCANtxMessage){
            /* don't set error, if bootup message is still on buffers */
            CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, buffer->CMSGSID);
        }
        err = CO_ERROR_TX_OVERFLOW;
    }

    CO_LOCK_CAN_SEND();
    TX_FIFOconCopy = *TX_FIFOcon;
    /* if CAN TX buffer is free, copy message to it */
    if((TX_FIFOconCopy & 0x8) == 0 && CANmodule->CANtxCount == 0){
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        *(TXmsgBuffer++) = *(message++);
        *(TXmsgBuffer++) = *(message++);
        *(TXmsgBuffer++) = *(message++);
        *(TXmsgBuffer++) = *(message++);
        /* if message was aborted, don't set UINC */
        if((TX_FIFOconCopy & 0x40) == 0)
            *TX_FIFOconSet = 0x2000;   /* set UINC */
        *TX_FIFOconSet = 0x0008;   /* set TXREQ */
    }
    /* if no buffer is free, message will be sent by interrupt */
    else{
        buffer->bufferFull = true;
        CANmodule->CANtxCount++;
    }
    /* Enable 'Tx buffer empty' (TXEMPTYIE) interrupt in FIFO 1 (third layer interrupt) */
    CAN_REG(addr, C_FIFOINT+0x48) = 0x01000000;
    CO_UNLOCK_CAN_SEND();

    return err;
}


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule){
    uint32_t tpdoDeleted = 0U;
    volatile uint32_t* TX_FIFOcon = &CAN_REG(CANmodule->CANdriverState, C_FIFOCON+0x40);
    volatile uint32_t* TX_FIFOconClr = &CAN_REG(CANmodule->CANdriverState, C_FIFOCON+0x44);

    CO_LOCK_CAN_SEND();
    /* Abort message from CAN module, if there is synchronous TPDO.
     * Take special care with this functionality. */
    if((*TX_FIFOcon & 0x8) && CANmodule->bufferInhibitFlag){
        *TX_FIFOconClr = 0x0008;   /* clear TXREQ */
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
    uint32_t TREC;
    CO_EM_t* em = (CO_EM_t*)CANmodule->em;
    uint32_t err;

    TREC = CAN_REG(CANmodule->CANdriverState, C_TREC);
    rxErrors = (uint8_t) TREC;
    txErrors = (uint8_t) (TREC>>8);
    if(TREC&0x00200000) txErrors = 256; /* bus off */
    overflow = (CAN_REG(CANmodule->CANdriverState, C_FIFOINT)&0x8) ? 1 : 0;

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
    uint8_t ICODE;
    ICODE = (uint8_t) CAN_REG(CANmodule->CANdriverState, C_VEC) & 0x7F;

    /* receive interrupt (New CAN message is available in RX FIFO 0 buffer) */
    if(ICODE == 0){
        CO_CANrxMsg_t *rcvMsg;      /* pointer to received message in CAN module */
        uint16_t index;             /* index of received message */
        uint16_t rcvMsgIdent;       /* identifier of the received message */
        CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;

        rcvMsg = (CO_CANrxMsg_t*) CO_PA_TO_KVA1(CAN_REG(CANmodule->CANdriverState, C_FIFOUA));
        rcvMsgIdent = rcvMsg->ident;
        if(rcvMsg->RTR) rcvMsgIdent |= 0x0800;
        if(CANmodule->useCANrxFilters){
            /* CAN module filters are used. Message with known 11-bit identifier has */
            /* been received */
            index = rcvMsg->FILHIT;
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

        /* Update the message buffer pointer */
        CAN_REG(CANmodule->CANdriverState, C_FIFOCON+0x08) = 0x2000;   /* set UINC */
    }


    /* transmit interrupt (TX buffer FIFO 1 is free) */
    else if(ICODE == 1){
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
                    uint32_t* TXmsgBuffer = CO_PA_TO_KVA1(CAN_REG(CANmodule->CANdriverState, C_FIFOUA+0x40));
                    uint32_t* message = (uint32_t*) buffer;
                    volatile uint32_t* TX_FIFOconSet = &CAN_REG(CANmodule->CANdriverState, C_FIFOCON+0x48);
                    *(TXmsgBuffer++) = *(message++);
                    *(TXmsgBuffer++) = *(message++);
                    *(TXmsgBuffer++) = *(message++);
                    *(TXmsgBuffer++) = *(message++);
                    *TX_FIFOconSet = 0x2000;    /* set UINC */
                    *TX_FIFOconSet = 0x0008;    /* set TXREQ */
                    break;                      /* exit for loop */
                }
                buffer++;
            }/* end of for loop */

            /* Clear counter if no more messages */
            if(i == 0U){
                CANmodule->CANtxCount = 0U;
            }
        }

        /* if no more messages, disable 'Tx buffer empty' (TXEMPTYIE) interrupt */
        if(CANmodule->CANtxCount == 0U){
            CAN_REG(CANmodule->CANdriverState, C_FIFOINT+0x44) = 0x01000000;
        }
    }
}
