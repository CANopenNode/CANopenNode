/*
 * CAN module object for Microchip dsPIC33F or PIC24H microcontroller.
 *
 * @file        CO_driver.c
 * @author      Janez Paternoster
 * @author      Peter Rozsahegyi (EDS)
 * @author      Jens Nielsen (CAN receive)
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


/* Globals */
    extern const CO_CANbitRateData_t  CO_CANbitRateData[8];

#if CO_CAN1msgBuffSize > 0
    __eds__ CO_CANrxMsg_t CO_CAN1msg[CO_CAN1msgBuffSize] __eds __dma __attribute__((aligned(128)));
#endif
#if CO_CAN2msgBuffSize > 0
    __eds__ CO_CANrxMsg_t CO_CAN2msg[CO_CAN1msgBuffSize] __eds __dma __attribute__((aligned(128)));
#endif


/* Macro and Constants - CAN module registers and DMA registers - offset. */
    #define CAN_REG(base, offset) (*((volatile uint16_t *) (((uintptr_t) base) + offset)))

    #define C_CTRL1      0x00
    #define C_CTRL2      0x02
    #define C_VEC        0x04
    #define C_FCTRL      0x06
    #define C_FIFO       0x08
    #define C_INTF       0x0A
    #define C_INTE       0x0C
    #define C_EC         0x0E
    #define C_CFG1       0x10
    #define C_CFG2       0x12
    #define C_FEN1       0x14
    #define C_FMSKSEL1   0x18
    #define C_FMSKSEL2   0x1A

    /* WIN == 0 */
    #define C_RXFUL1     0x20
    #define C_RXFUL2     0x22
    #define C_RXOVF1     0x28
    #define C_RXOVF2     0x2A
    #define C_TR01CON    0x30
    #define C_TR23CON    0x32
    #define C_TR45CON    0x34
    #define C_TR67CON    0x36
    #define C_RXD        0x40
    #define C_TXD        0x42

    /* WIN == 1 */
    #define C_BUFPNT1    0x20
    #define C_BUFPNT2    0x22
    #define C_BUFPNT3    0x24
    #define C_BUFPNT4    0x26
    #define C_RXM0SID    0x30
    #define C_RXM1SID    0x34
    #define C_RXM2SID    0x38
    #define C_RXF0SID    0x40  /* filter1 = +4, ...., filter 15 = +4*15 */


    #define DMA_REG(base, offset) (*((volatile uint16_t *) (base + offset)))

    #define DMA_CON      0x0
    #define DMA_REQ      0x2
#ifndef __HAS_EDS__
    #define DMA_STA      0x4
    #define DMA_STB      0x6
    #define DMA_PAD      0x8
    #define DMA_CNT      0xA
#else
    #define DMA_STAL     0x4
    #define DMA_STAH     0x6
    #define DMA_STBL     0x8
    #define DMA_STBH     0xA
    #define DMA_PAD      0xC
    #define DMA_CNT      0xE
#endif


/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANdriverState){
    uint16_t C_CTRL1copy = CAN_REG(CANdriverState, C_CTRL1);

    /* set REQOP = 0x4 */
    C_CTRL1copy &= 0xFCFF;
    C_CTRL1copy |= 0x0400;
    CAN_REG(CANdriverState, C_CTRL1) = C_CTRL1copy;

    /* while OPMODE != 4 */
    while((CAN_REG(CANdriverState, C_CTRL1) & 0x00E0) != 0x0080);
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){
    uint16_t C_CTRL1copy = CAN_REG(CANmodule->CANdriverState, C_CTRL1);

    /* set REQOP = 0x0 */
    C_CTRL1copy &= 0xF8FF;
    CAN_REG(CANmodule->CANdriverState, C_CTRL1) = C_CTRL1copy;

    /* while OPMODE != 0 */
    while((CAN_REG(CANmodule->CANdriverState, C_CTRL1) & 0x00E0) != 0x0000);

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
    volatile uint16_t *pRXF;

    uint16_t DMArxBaseAddress;
    uint16_t DMAtxBaseAddress;
    __eds__ CO_CANrxMsg_t *CANmsgBuff;
    uint8_t CANmsgBuffSize;
    uint16_t CANmsgBuffDMAoffset;
#if defined(__HAS_EDS__)
    uint16_t CANmsgBuffDMApage;
#endif

    /* verify arguments */
    if(CANmodule==NULL || rxArray==NULL || txArray==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Get global addresses for CAN module 1 or 2. */
    if(CANdriverState == ADDR_CAN1) {
        DMArxBaseAddress = CO_CAN1_DMA0;
        DMAtxBaseAddress = CO_CAN1_DMA1;
        CANmsgBuff = &CO_CAN1msg[0];
        CANmsgBuffSize = CO_CAN1msgBuffSize;
        CANmsgBuffDMAoffset = __builtin_dmaoffset(&CO_CAN1msg[0]);
    #if defined(__HAS_EDS__)
        CANmsgBuffDMApage = __builtin_dmapage(&CO_CAN1msg[0]);
    #endif
    }
#if CO_CAN2msgBuffSize > 0
    else if(((uintptr_t) CANdriverState) == ADDR_CAN2) {
        DMArxBaseAddress = CO_CAN2_DMA0;
        DMAtxBaseAddress = CO_CAN2_DMA1;
        CANmsgBuff = &CO_CAN2msg[0];
        CANmsgBuffSize = CO_CAN2msgBuffSize;
        CANmsgBuffDMAoffset = __builtin_dmaoffset(&CO_CAN2msg[0]);
    #if defined(__HAS_EDS__)
        CANmsgBuffDMApage = __builtin_dmapage(&CO_CAN2msg[0]);
    #endif
    }
#endif
    else {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->CANdriverState = CANdriverState;
    CANmodule->CANmsgBuff = CANmsgBuff;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = (rxSize <= 16U) ? true : false;
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


    /* Configure control registers */
    CAN_REG(CANdriverState, C_CTRL1) = 0x0400;
    CAN_REG(CANdriverState, C_CTRL2) = 0x0000;


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

    if(CO_CANbitRateData[i].scale == 2)
        CAN_REG(CANdriverState, C_CTRL1) |= 0x0800;

    CAN_REG(CANdriverState, C_CFG1) = (CO_CANbitRateData[i].SJW - 1) << 6 |
                                        (CO_CANbitRateData[i].BRP - 1);

    CAN_REG(CANdriverState, C_CFG2) = ((uint16_t)(CO_CANbitRateData[i].phSeg2 - 1)) << 8 |
                                        0x0080 |
                                        (CO_CANbitRateData[i].phSeg1 - 1) << 3 |
                                        (CO_CANbitRateData[i].PROP - 1);


    /* setup RX and TX control registers */
    CAN_REG(CANdriverState, C_CTRL1) &= 0xFFFE;     /* WIN = 0 - use buffer registers */
    CAN_REG(CANdriverState, C_RXFUL1) = 0x0000;
    CAN_REG(CANdriverState, C_RXFUL2) = 0x0000;
    CAN_REG(CANdriverState, C_RXOVF1) = 0x0000;
    CAN_REG(CANdriverState, C_RXOVF2) = 0x0000;
    CAN_REG(CANdriverState, C_TR01CON) = 0x0080;    /* use one buffer for transmission */
    CAN_REG(CANdriverState, C_TR23CON) = 0x0000;
    CAN_REG(CANdriverState, C_TR45CON) = 0x0000;
    CAN_REG(CANdriverState, C_TR67CON) = 0x0000;


    /* CAN module hardware filters */
    CAN_REG(CANdriverState, C_CTRL1) |= 0x0001;     /* WIN = 1 - use filter registers */
    CAN_REG(CANdriverState, C_FEN1) = 0xFFFF;       /* enable all 16 filters */
    CAN_REG(CANdriverState, C_FMSKSEL1) = 0x0000;   /* all filters are using mask 0 */
    CAN_REG(CANdriverState, C_FMSKSEL2) = 0x0000;
    CAN_REG(CANdriverState, C_BUFPNT1) = 0xFFFF;    /* use FIFO for all filters */
    CAN_REG(CANdriverState, C_BUFPNT2) = 0xFFFF;
    CAN_REG(CANdriverState, C_BUFPNT3) = 0xFFFF;
    CAN_REG(CANdriverState, C_BUFPNT4) = 0xFFFF;
    /* set all filters to 0 */
    pRXF = &CAN_REG(CANdriverState, C_RXF0SID);
    for(i=0; i<16; i++){
        *pRXF = 0x0000;
        pRXF += 2;
    }
    if(CANmodule->useCANrxFilters){
        /* CAN module filters are used, they will be configured with */
        /* CO_CANrxBufferInit() functions, called by separate CANopen */
        /* init functions. */
        /* All mask bits are 1, so received message must match filter */
        CAN_REG(CANdriverState, C_RXM0SID) = 0xFFE8;
        CAN_REG(CANdriverState, C_RXM1SID) = 0xFFE8;
        CAN_REG(CANdriverState, C_RXM2SID) = 0xFFE8;
    }
    else{
        /* CAN module filters are not used, all messages with standard 11-bit */
        /* identifier will be received */
        /* Set masks so, that all messages with standard identifier are accepted */
        CAN_REG(CANdriverState, C_RXM0SID) = 0x0008;
        CAN_REG(CANdriverState, C_RXM1SID) = 0x0008;
        CAN_REG(CANdriverState, C_RXM2SID) = 0x0008;
    }

    /* WIN = 0 - use buffer registers for default */
    CAN_REG(CANdriverState, C_CTRL1) &= 0xFFFE;


    /* Configure DMA controller */
    /* Set size of buffer in DMA RAM (FIFO Area Starts with Tx/Rx buffer TRB1 (FSA = 1)) */
    /* Use maximum 16 buffers, because we have 16-bit system. */
    if (CANmsgBuffSize >= 16) {
        CAN_REG(CANdriverState, C_FCTRL) = 0x8001;
        CANmodule->CANmsgBuffSize = 16;
    }
    else if(CANmsgBuffSize >= 12) {
        CAN_REG(CANdriverState, C_FCTRL) = 0x6001;
        CANmodule->CANmsgBuffSize = 12;
    }
    else if(CANmsgBuffSize >=  8) {
        CAN_REG(CANdriverState, C_FCTRL) = 0x4001;
        CANmodule->CANmsgBuffSize = 8;
    }
    else if(CANmsgBuffSize >=  6) {
        CAN_REG(CANdriverState, C_FCTRL) = 0x2001;
        CANmodule->CANmsgBuffSize = 6;
    }
    else if(CANmsgBuffSize >=  4) {
        CAN_REG(CANdriverState, C_FCTRL) = 0x0001;
        CANmodule->CANmsgBuffSize = 4;
    }
    else {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* DMA chanel initialization for ECAN reception */
    DMA_REG(DMArxBaseAddress, DMA_CON) = 0x0020;
    DMA_REG(DMArxBaseAddress, DMA_PAD) = (volatile uint16_t) &CAN_REG(CANdriverState, C_RXD);
    DMA_REG(DMArxBaseAddress, DMA_CNT) = 7;
    DMA_REG(DMArxBaseAddress, DMA_REQ) = (CANdriverState==ADDR_CAN1) ? 34 : 55;

#ifndef __HAS_EDS__
    DMA_REG(DMArxBaseAddress, DMA_STA) = CANmsgBuffDMAoffset;
#else
    DMA_REG(DMArxBaseAddress, DMA_STAL) = CANmsgBuffDMAoffset;
    DMA_REG(DMArxBaseAddress, DMA_STAH) = CANmsgBuffDMApage;
#endif

    DMA_REG(DMArxBaseAddress, DMA_CON) = 0x8020;

    /* DMA chanel initialization for ECAN transmission */
    DMA_REG(DMAtxBaseAddress, DMA_CON) = 0x2020;
    DMA_REG(DMAtxBaseAddress, DMA_PAD) = (volatile uint16_t) &CAN_REG(CANdriverState, C_TXD);
    DMA_REG(DMAtxBaseAddress, DMA_CNT) = 7;
    DMA_REG(DMAtxBaseAddress, DMA_REQ) = (CANdriverState==ADDR_CAN1) ? 70 : 71;

#ifndef __HAS_EDS__
    DMA_REG(DMAtxBaseAddress, DMA_STA) = CANmsgBuffDMAoffset;
#else
    DMA_REG(DMAtxBaseAddress, DMA_STAL) = CANmsgBuffDMAoffset;
    DMA_REG(DMAtxBaseAddress, DMA_STAH) = CANmsgBuffDMApage;
#endif

    DMA_REG(DMAtxBaseAddress, DMA_CON) = 0xA020;


    /* CAN interrupt registers */
    /* clear interrupt flags */
    CAN_REG(CANdriverState, C_INTF) = 0x0000;
    /* enable receive and transmit interrupt */
    CAN_REG(CANdriverState, C_INTE) = 0x0003;
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
        uint16_t addr = CANmodule->CANdriverState;

        /* Configure object variables */
        buffer->object = object;
        buffer->pFunct = pFunct;


        /* CAN identifier and CAN mask, bit aligned with CAN module registers (in DMA RAM) */
        RXF = (ident & 0x07FF) << 2;
        if(rtr){
            RXF |= 0x02;
        }
        RXM = (mask & 0x07FF) << 2;
        RXM |= 0x02;

        /* configure filter and mask */
        if(RXF != buffer->ident || RXM != buffer->mask){
            volatile uint16_t C_CTRL1old = CAN_REG(addr, C_CTRL1);
            CAN_REG(addr, C_CTRL1) = C_CTRL1old | 0x0001;     /* WIN = 1 - use filter registers */
            buffer->ident = RXF;
            buffer->mask = RXM;

            /* Set CAN hardware module filter and mask. */
            if(CANmodule->useCANrxFilters){
                volatile uint16_t *pRXF;
                volatile uint16_t *pRXM0, *pRXM1, *pRXM2;
                uint16_t selectMask;

                /* align RXF and RXM with C_RXF_SID and C_RXM_SID registers */
                RXF &= 0xFFFD; RXF <<= 3;
                RXM &= 0xFFFD; RXM <<= 3; RXM |= 0x0008;

                /* write to filter */
                pRXF = &CAN_REG(addr, C_RXF0SID); /* pointer to first filter register */
                pRXF += index * 2;   /* now points to C_RXFiSID (i == index) */
                *pRXF = RXF;         /* write value to filter */

                /* configure mask (There are three masks, each of them can be asigned to any filter. */
                /*                 First mask has always the value 0xFFE8 - all 11 bits must match). */
                pRXM0 = &CAN_REG(addr, C_RXM0SID);
                pRXM1 = &CAN_REG(addr, C_RXM1SID);
                pRXM2 = &CAN_REG(addr, C_RXM2SID);
                if(RXM == 0xFFE8){
                    selectMask = 0;
                }
                else if(RXM == *pRXM1 || *pRXM1 == 0xFFE8){
                    /* RXM is equal to mask 1 or mask 1 was not yet configured. */
                    *pRXM1 = RXM;
                    selectMask = 1;
                }
                else if(RXM == *pRXM2 || *pRXM2 == 0xFFE8){
                    /* RXM is equal to mask 2 or mask 2 was not yet configured. */
                    *pRXM2 = RXM;
                    selectMask = 2;
                }
                else{
                    /* not enough masks */
                    ret = CO_ERROR_OUT_OF_MEMORY;
                    selectMask = 0;
                }
                if(ret == CO_ERROR_NO){
                    /* now write to appropriate mask select register */
                    if(index<8){
                        uint16_t clearMask = ~(0x0003 << (index << 1));
                        selectMask = selectMask << (index << 1);
                        CAN_REG(addr, C_FMSKSEL1) =
                            (CAN_REG(addr, C_FMSKSEL1) & clearMask) | selectMask;
                    }
                    else{
                        uint16_t clearMask = ~(0x0003 << ((index-8) << 1));
                        selectMask = selectMask << ((index-8) << 1);
                        CAN_REG(addr, C_FMSKSEL2) =
                            (CAN_REG(addr, C_FMSKSEL2) & clearMask) | selectMask;
                    }
                }
            }
            CAN_REG(addr, C_CTRL1) = C_CTRL1old;
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
        TXF = (ident & 0x07FF) << 2;
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
 * @param CANdriverState CAN module base address
 * @param dest Pointer to CAN module transmit buffer
 * @param src Pointer to source message
 */
static void CO_CANsendToModule(void *CANdriverState, __eds__ CO_CANrxMsg_t *dest, CO_CANtx_t *src){
    uint8_t DLC;
    __eds__ uint8_t *CANdataBuffer;
    uint8_t *pData;
    volatile uint16_t C_CTRL1old;

    /* CAN-ID + RTR */
    dest->ident = src->ident;

    /* Data lenght */
    DLC = src->DLC;
    if(DLC > 8) DLC = 8;
    dest->DLC = DLC;

    /* copy data */
    CANdataBuffer = &(dest->data[0]);
    pData = src->data;
    for(; DLC>0; DLC--) *(CANdataBuffer++) = *(pData++);

    /* control register, transmit request */
    C_CTRL1old = CAN_REG(CANdriverState, C_CTRL1);
    CAN_REG(CANdriverState, C_CTRL1) = C_CTRL1old & 0xFFFE;     /* WIN = 0 - use buffer registers */
    CAN_REG(CANdriverState, C_TR01CON) |= 0x08;
    CAN_REG(CANdriverState, C_CTRL1) = C_CTRL1old;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;
    uint16_t addr = CANmodule->CANdriverState;
    volatile uint16_t C_CTRL1old, C_TR01CONcopy;

    /* Verify overflow */
    if(buffer->bufferFull){
        if(!CANmodule->firstCANtxMessage){
            /* don't set error, if bootup message is still on buffers */
            CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, (buffer->ident >> 2) & 0x7FF);
        }
        err = CO_ERROR_TX_OVERFLOW;
    }

    CO_LOCK_CAN_SEND();
    /* read C_TR01CON */
    C_CTRL1old = CAN_REG(addr, C_CTRL1);
    CAN_REG(addr, C_CTRL1) = C_CTRL1old & 0xFFFE;     /* WIN = 0 - use buffer registers */
    C_TR01CONcopy = CAN_REG(addr, C_TR01CON);
    CAN_REG(addr, C_CTRL1) = C_CTRL1old;

    /* if CAN TX buffer is free, copy message to it */
    if((C_TR01CONcopy & 0x8) == 0 && CANmodule->CANtxCount == 0){
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        CO_CANsendToModule(addr, CANmodule->CANmsgBuff, buffer);
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
    uint32_t tpdoDeleted = 0U;

    CO_LOCK_CAN_SEND();
    /* Abort message from CAN module, if there is synchronous TPDO.
     * Take special care with this functionality. */
    if(CANmodule->bufferInhibitFlag){
        volatile uint16_t C_CTRL1old = CAN_REG(CANmodule->CANdriverState, C_CTRL1);
        CAN_REG(CANmodule->CANdriverState, C_CTRL1) = C_CTRL1old & 0xFFFE;     /* WIN = 0 - use buffer registers */
        CAN_REG(CANmodule->CANdriverState, C_TR01CON) &= 0xFFF7; /* clear TXREQ */
        CAN_REG(CANmodule->CANdriverState, C_CTRL1) = C_CTRL1old;
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
    uint16_t err;
    CO_EM_t* em = (CO_EM_t*)CANmodule->em;

    err = CAN_REG(CANmodule->CANdriverState, C_INTF) >> 8;
    if(CAN_REG(CANmodule->CANdriverState, C_INTF) & 4){
        err |= 0x80;
    }

    if(CANmodule->errOld != err){
        CANmodule->errOld = err;

        /* CAN RX bus overflow */
        if(err & 0xC0){
            CO_errorReport(em, CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, err);
            CAN_REG(CANmodule->CANdriverState, C_INTF) &= 0xFFFB;/* clear bits */
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
void CO_CANinterrupt(CO_CANmodule_t *CANmodule) {

    /* receive interrupt (New CAN message is available in RX FIFO buffer) */
    if(CAN_REG(CANmodule->CANdriverState, C_INTF) & 0x02) {
        uint16_t C_CTRL1old;
        uint16_t C_RXFUL1copy;
        uint16_t C_FIFOcopy;
        uint8_t FNRB, FBP;

        CO_DISABLE_INTERRUPTS();
        C_CTRL1old = CAN_REG(CANmodule->CANdriverState, C_CTRL1);
        CAN_REG(CANmodule->CANdriverState, C_CTRL1) = C_CTRL1old & 0xFFFE;     /* WIN = 0 - use buffer registers */
        C_RXFUL1copy = CAN_REG(CANmodule->CANdriverState, C_RXFUL1);
        CAN_REG(CANmodule->CANdriverState, C_CTRL1) = C_CTRL1old;

        /* We will service the buffers indicated by RXFUL copy, clear interrupt
         * flag now and let interrupt hit again if more messages are received */
        CAN_REG(CANmodule->CANdriverState, C_INTF) &= 0xFFFD;
        C_FIFOcopy = CAN_REG(CANmodule->CANdriverState, C_FIFO);
        CO_ENABLE_INTERRUPTS();

        /* FNRB tells us which buffer to read in FIFO */
        FNRB = C_FIFOcopy & 0x3F;
        /* FBP tells us the next FIFO entry that will be written */
        FBP = C_FIFOcopy >> 8;

        while(C_RXFUL1copy != 0) {
            __eds__ CO_CANrxMsg_t *rcvMsg;/* pointer to received message in CAN module */
            uint16_t index;             /* index of received message */
            uint16_t rcvMsgIdent;       /* identifier of the received message */
            CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */
            bool_t msgMatched = false;
            uint16_t mask;

            mask = 1 << FNRB;

            if((C_RXFUL1copy & mask) == 0) {
                /* This should not happen. However, if it does happen
                 * (in case of debugging), get FNRB from loop. */
                for(FNRB=1; FNRB<CANmodule->CANmsgBuffSize; FNRB++) {
                    mask = 1 << FNRB;
                    if((C_RXFUL1copy & mask)) {
                        break;
                    }
                }
            }

            /* RXFUL is set for this buffer, service it */
            rcvMsg = &CANmodule->CANmsgBuff[FNRB];
            rcvMsgIdent = rcvMsg->ident;
            if(CANmodule->useCANrxFilters) {
                /* CAN module filters are used. Message with known 11-bit identifier has */
                /* been received */
                index = rcvMsg->FILHIT;
                if(index < CANmodule->rxSize) {
                    buffer = &CANmodule->rxArray[index];
                    /* verify also RTR */
                    if(((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U) {
                        msgMatched = true;
                    }
                }
            }
            else {
                /* CAN module filters are not used, message with any standard 11-bit identifier */
                /* has been received. Search rxArray form CANmodule for the same CAN-ID. */
                buffer = &CANmodule->rxArray[0];
                for(index = CANmodule->rxSize; index > 0U; index--) {
                    if(((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U) {
                        msgMatched = true;
                        break;
                    }
                    buffer++;
                }
            }

            /* Call specific function, which will process the message */
            if(msgMatched && (buffer != NULL) && (buffer->pFunct != NULL)) {
        #ifdef __HAS_EDS__
                CO_CANrxMsg_t _rcvMsg = *rcvMsg;
                buffer->pFunct(buffer->object, &_rcvMsg);
        #else
                buffer->pFunct(buffer->object, rcvMsg);
        #endif
            }

            /* Clear RXFUL flag */
            CO_DISABLE_INTERRUPTS();
            C_CTRL1old = CAN_REG(CANmodule->CANdriverState, C_CTRL1);
            CAN_REG(CANmodule->CANdriverState, C_CTRL1) = C_CTRL1old & 0xFFFE;     /* WIN = 0 - use buffer registers */
            CAN_REG(CANmodule->CANdriverState, C_RXFUL1) &= ~(mask);
            CAN_REG(CANmodule->CANdriverState, C_CTRL1) = C_CTRL1old;
            CO_ENABLE_INTERRUPTS();
            C_RXFUL1copy &= ~(mask);

            /* Now update FNRB, it will point to a new buffer after RXFUL was cleared */
            FNRB = (CAN_REG(CANmodule->CANdriverState, C_FIFO) & 0x3F);
        }
    }

    /* transmit interrupt (TX buffer is free) */
    if(CAN_REG(CANmodule->CANdriverState, C_INTF) & 0x01) {

        /* Clear interrupt flag */
        CAN_REG(CANmodule->CANdriverState, C_INTF) &= 0xFFFE;
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
                    CO_CANsendToModule(CANmodule->CANdriverState, CANmodule->CANmsgBuff, buffer);
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
