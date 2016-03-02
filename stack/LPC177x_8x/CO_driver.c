/*
 * CAN module object for NXP LPC177x (Cortex M3) and FreeRTOS.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @author      Amit H
 * @copyright   2004 - 2014 Janez Paternoster
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
/*Kernel Includes. ----------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"

#include "board.h"

#include "CO_driver.h"
#include "CO_Emergency.h"
#include "IMAXEON_Config.h"

#include "CO_OD.h"
/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

#define CAN_CTRL_NO         1

#if (CAN_CTRL_NO == 0)
#define LPC_CAN             (LPC_CAN1)
#else
#define LPC_CAN             (LPC_CAN2)
#endif

#define AF_LUT_USED         1
#if AF_LUT_USED
#define FULL_CAN_AF_USED        0
#define REMOVE_CAN_AF_ENTRIES    0
#endif

#define CAN_TX_MSG_STD_ID (0x200)
#define CAN_TX_MSG_REMOTE_STD_ID (0x300)
#define CAN_TX_MSG_EXT_ID (0x10000200)
#define CAN_RX_MSG_ID (0x100)

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

#if AF_LUT_USED
#if FULL_CAN_AF_USED
CAN_STD_ID_ENTRY_T FullCANSection[] = {
	{CAN_CTRL_NO, 0, 0x03},
	{CAN_CTRL_NO, 0, 0x05},
	{CAN_CTRL_NO, 0, 0x07},
	{CAN_CTRL_NO, 0, 0x09},
};
#endif
CAN_STD_ID_ENTRY_T SFFSection[] = {
    {CAN_CTRL_NO, 0, 0x00},
    {CAN_CTRL_NO, 0, 0x01},
    {CAN_CTRL_NO, 0, 0x02},
    {CAN_CTRL_NO, 0, 0x08},
    {CAN_CTRL_NO, 0, 0x10},
    {CAN_CTRL_NO, 0, 0x30},
	{CAN_CTRL_NO, 0, 0x50},
	{CAN_CTRL_NO, 0, 0x70},
	{CAN_CTRL_NO, 0, 0x90},
	{CAN_CTRL_NO, 0, 0xB0},
};
CAN_STD_ID_RANGE_ENTRY_T SffGrpSection[] = {
	{{CAN_CTRL_NO, 0, 0x100}, {CAN_CTRL_NO, 0, 0x400}},
	{{CAN_CTRL_NO, 0, 0x500}, {CAN_CTRL_NO, 0, 0x6FF}},
	{{CAN_CTRL_NO, 0, 0x700}, {CAN_CTRL_NO, 0, 0x780}},
};
CAN_EXT_ID_ENTRY_T EFFSection[] = {
	{CAN_CTRL_NO, ((1 << 11) | 0x03)},
	{CAN_CTRL_NO, ((1 << 11) | 0x05)},
	{CAN_CTRL_NO, ((1 << 11) | 0x07)},
	{CAN_CTRL_NO, ((1 << 11) | 0x09)},
};
CAN_EXT_ID_RANGE_ENTRY_T EffGrpSection[] = {
	{{CAN_CTRL_NO, ((1 << 11) | 0x300)}, {CAN_CTRL_NO, ((1 << 11) | 0x400)}},
	{{CAN_CTRL_NO, ((1 << 11) | 0x500)}, {CAN_CTRL_NO, ((1 << 11) | 0x6FF)}},
	{{CAN_CTRL_NO, ((1 << 11) | 0x700)}, {CAN_CTRL_NO, ((1 << 11) | 0x780)}},
};
CANAF_LUT_T AFSections = {
#if FULL_CAN_AF_USED
	FullCANSection, sizeof(FullCANSection) / sizeof(CAN_STD_ID_ENTRY_T),
#else
	NULL, 0,
#endif
	SFFSection, sizeof(SFFSection) / sizeof(CAN_STD_ID_ENTRY_T),
	SffGrpSection, sizeof(SffGrpSection) / sizeof(CAN_STD_ID_RANGE_ENTRY_T),
	EFFSection, sizeof(EFFSection) / sizeof(CAN_EXT_ID_ENTRY_T),
	EffGrpSection, sizeof(EffGrpSection) / sizeof(CAN_EXT_ID_RANGE_ENTRY_T),
};
#endif /*AF_LUT_USED*/

/*****************************************************************************
 * Private functions
 ****************************************************************************/
static void PrintCANErrorInfo(uint32_t Status);
static void PrintCANMsg(CAN_MSG_T *pMsg);
static void PrintAFLUT(void);
static void SetupAFLUT(void);
/* Insert/Remove entries to/from AF LUT */
static void ChangeAFLUT(void);

/******************************************************************************/
void CO_CANrxMsgHandler(void *object, const CO_CANrxMsg_t *message){

  DEBUGOUT("CO_CANrxMsgHandler Default Rx handler called \r\n");
/*  PrintCANMsg((CAN_MSG_T*)message); */
}

/******************************************************************************/
void CO_CANsetConfigurationMode(uint16_t CANbaseAddress){
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){
            
    Chip_CAN_SetAFMode(LPC_CANAF, CAN_AF_NORMAL_MODE);

    CANmodule->CANnormal = true;
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
    uint8_t nodeId=0;
    
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
#if AF_LUT_USED    
    CANmodule->useCANrxFilters = (rxSize <= 32U) ? true : false;/* microcontroller dependent */
#else
    CANmodule->useCANrxFilters = false;
#endif    
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;
    CANmodule->em = NULL;

     
    DEBUGOUT("CO_CANmodule_init Baud: %d\r\n",(CANbitRate * 1000));
    
    for(i=0U; i<rxSize; i++){
        rxArray[i].ident = 0U;
        rxArray[i].pFunct = CO_CANrxMsgHandler;
    }
    for(i=0U; i<txSize; i++){
        txArray[i].bufferFull = false;
    }
    /* CAN_RD2*/
    Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 4, (IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGMODE_EN));
	/* CAN_TD2*/
    Chip_IOCON_PinMuxSet(LPC_IOCON, 0, 5, (IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGMODE_EN ));
   
    /* CAN_TERMINATION swapped over with nCAN_HEARTBEAT, so this pin is actually CAN_TERMINATION*/
    Chip_IOCON_PinMuxSet(LPC_IOCON, 1, 21, (IOCON_FUNC0 | IOCON_MODE_INACT ));
    Chip_GPIO_WriteDirBit(LPC_GPIO, 1, 21, true);
    Chip_GPIO_WritePortBit(LPC_GPIO, 1,21, true);
            /** CAN nCAN_HEARTBEAT LED*/
    Chip_IOCON_PinMuxSet(LPC_IOCON, CAN_RUN_LED_PORT, CAN_RUN_LED_PIN, IOCON_FUNC0);
    Chip_GPIO_WriteDirBit(LPC_GPIO, CAN_RUN_LED_PORT, CAN_RUN_LED_PIN, true);
    Chip_GPIO_WritePortBit(LPC_GPIO, CAN_RUN_LED_PORT,CAN_RUN_LED_PIN, true);
    
    Chip_IOCON_PinMuxSet(LPC_IOCON, CAN_NODE_ID_0_PORT, CAN_NODE_ID_0_PIN, (IOCON_FUNC0 | IOCON_MODE_INACT));
    Chip_GPIO_WriteDirBit(LPC_GPIO, CAN_NODE_ID_0_PORT, CAN_NODE_ID_0_PIN, false);
    Chip_IOCON_PinMuxSet(LPC_IOCON, CAN_NODE_ID_1_PORT, CAN_NODE_ID_1_PIN, (IOCON_FUNC0 | IOCON_MODE_INACT));
    Chip_GPIO_WriteDirBit(LPC_GPIO, CAN_NODE_ID_1_PORT, CAN_NODE_ID_1_PIN, false);
    Chip_IOCON_PinMuxSet(LPC_IOCON, CAN_NODE_ID_2_PORT, CAN_NODE_ID_2_PIN, (IOCON_FUNC0 | IOCON_MODE_INACT));
    Chip_GPIO_WriteDirBit(LPC_GPIO, CAN_NODE_ID_2_PORT, CAN_NODE_ID_2_PIN, false);
    Chip_IOCON_PinMuxSet(LPC_IOCON, CAN_NODE_ID_3_PORT, CAN_NODE_ID_3_PIN, (IOCON_FUNC0 | IOCON_MODE_INACT));
    Chip_GPIO_WriteDirBit(LPC_GPIO, CAN_NODE_ID_3_PORT, CAN_NODE_ID_3_PIN, false);
    Chip_IOCON_PinMuxSet(LPC_IOCON, CAN_NODE_ID_4_PORT, CAN_NODE_ID_4_PIN, (IOCON_FUNC0 | IOCON_MODE_INACT));
    Chip_GPIO_WriteDirBit(LPC_GPIO, CAN_NODE_ID_4_PORT, CAN_NODE_ID_4_PIN, false);
    
    nodeId = 0;
    nodeId |= (uint8_t)(Chip_GPIO_GetPinState(LPC_GPIO, CAN_NODE_ID_0_PORT, CAN_NODE_ID_0_PIN) ? 1: 0);
    nodeId |= (uint8_t)(Chip_GPIO_GetPinState(LPC_GPIO, CAN_NODE_ID_1_PORT, CAN_NODE_ID_1_PIN) ? (1<<1): 0);
    nodeId |= (uint8_t)(Chip_GPIO_GetPinState(LPC_GPIO, CAN_NODE_ID_2_PORT, CAN_NODE_ID_2_PIN) ? (1<<2): 0);
    nodeId |= (uint8_t)(Chip_GPIO_GetPinState(LPC_GPIO, CAN_NODE_ID_3_PORT, CAN_NODE_ID_3_PIN) ? (1<<3): 0);
    nodeId |= (uint8_t)(Chip_GPIO_GetPinState(LPC_GPIO, CAN_NODE_ID_4_PORT, CAN_NODE_ID_4_PIN) ? (1<<4): 0);

    DEBUGOUT("CO_CANmodule_init nodeId: 0x%x\r\n",nodeId);
    OD_CANNodeID = nodeId;
    
    /* Configure CAN module registers */
	Chip_CAN_Init(LPC_CAN, LPC_CANAF, LPC_CANAF_RAM);
    /* Configure CAN timing */
    /*CANbitRate Valid values are (in kbps): 10, 20, 50, 125, 250, 500, 800, 1000.*/
	Chip_CAN_SetBitRate(LPC_CAN, (CANbitRate * 1000));
    /*Local interrup enable*/
	Chip_CAN_EnableInt(LPC_CAN, CAN_IER_BITMASK);
    /* Configure CAN module hardware filters */
    if(CANmodule->useCANrxFilters){
        DEBUGOUT("\tCAN Rx Acceptance Filters ON \r\n");
#if AF_LUT_USED        
        /* CAN module filters are used, they will be configured with */
        /* CO_CANrxBufferInit() functions, called by separate CANopen */
        /* init functions. */
        /* Configure all masks so, that received message must match filter */
        SetupAFLUT();
		/*ChangeAFLUT();*/
        /* print the configured LUT*/
        PrintAFLUT();
#if FULL_CAN_AF_USED
        Chip_CAN_ConfigFullCANInt(LPC_CANAF, ENABLE);
        Chip_CAN_SetAFMode(LPC_CANAF, CAN_AF_FULL_MODE);
#else
        Chip_CAN_SetAFMode(LPC_CANAF, CAN_AF_NORMAL_MODE);
#endif /*FULL_CAN_AF_USED*/
        
#else                
        DEBUGOUT("\tCAN Rx Acceptance Filters NOT OPERATIONAL for the debug stages\r\n");
        Chip_CAN_SetAFMode(LPC_CANAF, CAN_AF_BYBASS_MODE);
#endif        
    }
    else
	{
        /* CAN module filters are not used, all messages with standard 11-bit */
        /* identifier will be received */
        /* Configure mask 0 so, that all messages with standard identifier are accepted */
        Chip_CAN_SetAFMode(LPC_CANAF, CAN_AF_BYBASS_MODE);
                
        DEBUGOUT("\tCAN Rx Acceptance Filters Bypass \r\n");
    }
    /* configure CAN interrupt registers */
    NVIC_EnableIRQ(CAN_IRQn);
    
    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule){
    /* turn off the module */
    NVIC_DisableIRQ(CAN_IRQn);
}


/******************************************************************************/
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg){
    return (uint16_t) rxMsg->ident;
}

/******************************************************************************/
uint16_t CO_CAN_Get_My_NodeID(void){

    uint16_t     myNodeId=0;
    
    myNodeId = Chip_GPIO_ReadPortBit(LPC_GPIO, CAN_NODE_ID_0_PORT, CAN_NODE_ID_0_PIN);
    
    return myNodeId;
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
        buffer->ident =  ((uint32_t)ident & 0x07FFU);
        buffer->DLC = ((uint32_t)noOfBytes);
        buffer->Type = ((uint32_t)(rtr ? (CAN_REMOTE_MSG) : 0U));

        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;
	CAN_BUFFER_ID_T   TxBuf;
    
    /* Verify overflow */
    if(buffer->bufferFull){
        if(!CANmodule->firstCANtxMessage){
            /* don't set error, if bootup message is still on buffers */
            CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, buffer->ident);
        }
        err = CO_ERROR_TX_OVERFLOW;
    }
  
    CO_LOCK_CAN_SEND();
    
    /* if CAN TX buffer is free, copy message to it */
    TxBuf = Chip_CAN_GetFreeTxBuf(LPC_CAN);
    if( TxBuf < CAN_BUFFER_LAST && CANmodule->CANtxCount == 0){
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        /* copy message and txRequest */
        Chip_CAN_Send(LPC_CAN, TxBuf,(CAN_MSG_T*)buffer);
       
        /*DEBUGOUT("CO_CANsend!!!\r\n");*/
        /*PrintCANMsg((CAN_MSG_T*)buffer);*/
    }
    /* if no buffer is free, message will be sent by interrupt */
    else
    {
        DEBUGOUT("CO_CANsend buffer Full!!!\r\n");
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
    /*Check if any of the buffer is pending transmition and bufferInhibitFlag is true*/
    if(( 0 == (Chip_CAN_GetGlobalStatus(LPC_CAN) & CAN_GSR_TBS)) && CANmodule->bufferInhibitFlag){
       
        /* if not already in progress, a pending Transmission Request for
            the selected Transmit Buffer is cancelled. */
        Chip_CAN_SetCmd(LPC_CAN, CAN_CMR_STB(CAN_BUFFER_1) | CAN_CMR_AT);
        Chip_CAN_SetCmd(LPC_CAN, CAN_CMR_STB(CAN_BUFFER_2) | CAN_CMR_AT);
        Chip_CAN_SetCmd(LPC_CAN, CAN_CMR_STB(CAN_BUFFER_3) | CAN_CMR_AT);

        /* clear TXREQ */
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
    rxErrors = CAN_GSR_RXERR(Chip_CAN_GetGlobalStatus(LPC_CAN)); 
    txErrors = CAN_GSR_TXERR(Chip_CAN_GetGlobalStatus(LPC_CAN)); 
    overflow = (Chip_CAN_GetGlobalStatus(LPC_CAN) & CAN_GSR_DOS) ; /*CAN Data Overrun Status */  CANmodule->txSize;

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

    uint32_t IntStatus;
	CO_CANrxMsg_t RcvMsgBuf;
    CO_CANrxMsg_t *rcvMsg;      /* pointer to received message in CAN module */
    uint16_t index;             /* index of received message */
    uint32_t rcvMsgIdent;       /* identifier of the received message */
    CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */
    bool_t msgMatched = false;
    CAN_BUFFER_ID_T   TxBuf;
                
    /*Read the int status register */
    IntStatus = Chip_CAN_GetIntStatus(LPC_CAN);

    
    /* receive interrupt */
    if(IntStatus & CAN_ICR_RI){
 
        /* get message from module */
        Chip_CAN_Receive(LPC_CAN,(CAN_MSG_T*)&RcvMsgBuf);
        rcvMsg = &RcvMsgBuf;
        rcvMsgIdent = rcvMsg->ident;
         
#if AF_LUT_USED   /*Implement once AF will be used*/
        if(CANmodule->useCANrxFilters){

			/* Since the HW not providing the filter index, ignore this code and run through the rxArray to find a match.*/
            /* Search rxArray form CANmodule for the same CAN-ID. */
            buffer = &CANmodule->rxArray[0];
             for(index = CANmodule->rxSize; index > 0U; index--){
                if(((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U){
                    msgMatched = true;
                    break;
                }
                buffer++;
            }
        }
        else
#endif            
        {
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
		else
		{
			DEBUGOUT("Unsupported Message Received!!!\r\n");  
            PrintCANMsg((CAN_MSG_T*)rcvMsg); 
		}

        /* Clear interrupt flag */
        /*Giving the Command “Release Receive Buffer” will clear RI. done in Chip_CAN_Receive()*/
    }
    /* transmit interrupt */
    else if((IntStatus & CAN_ICR_TI1)||(IntStatus & CAN_ICR_TI2)||(IntStatus & CAN_ICR_TI3)){
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
                     /* if CAN TX buffer is free, copy message to it */
                    TxBuf = Chip_CAN_GetFreeTxBuf(LPC_CAN);
                    if( TxBuf < CAN_BUFFER_LAST){
                        CANmodule->bufferInhibitFlag = buffer->syncFlag;
                        /* copy message and txRequest */
                        Chip_CAN_Send(LPC_CAN, TxBuf,(CAN_MSG_T*)buffer);
                    }
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
    else{
        /* some other interrupt reason */
    }
}

/*****************************************************************************
 * Private functions
 ****************************************************************************/
/* Print error */
static void PrintCANErrorInfo(uint32_t Status)
{
	if (Status & CAN_ICR_EI) {
		DEBUGOUT("Error Warning!\r\n");
	}
	if (Status & CAN_ICR_DOI) {
		DEBUGOUT("Data Overrun!\r\n");
	}
	if (Status & CAN_ICR_EPI) {
		DEBUGOUT("Error Passive!\r\n");
	}
	if (Status & CAN_ICR_ALI) {
		DEBUGOUT("Arbitration lost in the bit: %d(th)\r\n", CAN_ICR_ALCBIT_VAL(Status));
	}
	if (Status & CAN_ICR_BEI) {

		DEBUGOUT("CAN Bus error !!!\r\n");

		if (Status & CAN_ICR_ERRDIR_RECEIVE) {
			DEBUGOUT("\t Error Direction: Transmiting\r\n");
		}
		else {
			DEBUGOUT("\t Error Direction: Receiving\r\n");
		}

		DEBUGOUT("\t Error Location: 0x%2x\r\n", CAN_ICR_ERRBIT_VAL(Status));
		DEBUGOUT("\t Error Type: 0x%1x\r\n", CAN_ICR_ERRC_VAL(Status));
	}
}

/* Print CAN Message */
static void PrintCANMsg(CAN_MSG_T *pMsg)
{
	uint8_t i;
	DEBUGOUT("\t**************************\r\n");
	DEBUGOUT("\tMessage Information: \r\n");
	DEBUGOUT("\tMessage Type: ");
	if (pMsg->ID & CAN_EXTEND_ID_USAGE) {
		DEBUGOUT(" Extend ID Message");
	}
	else {
		DEBUGOUT(" Standard ID Message");
	}
	if (pMsg->Type & CAN_REMOTE_MSG) {
		DEBUGOUT(", Remote Message");
	}
	DEBUGOUT("\r\n");
	DEBUGOUT("\tMessage ID :0x%x\r\n", (pMsg->ID & (~CAN_EXTEND_ID_USAGE)));
	DEBUGOUT("\tMessage Data :");
	for (i = 0; i < pMsg->DLC; i++)
		DEBUGOUT("%x ", pMsg->Data[i]);
	DEBUGOUT("\r\n\t**************************\r\n");
}

#if AF_LUT_USED
/* Print entries in AF LUT */
static void PrintAFLUT(void)
{
	uint16_t i, num;
	CAN_STD_ID_ENTRY_T StdEntry;
	CAN_EXT_ID_ENTRY_T ExtEntry;
	CAN_STD_ID_RANGE_ENTRY_T StdGrpEntry;
	CAN_EXT_ID_RANGE_ENTRY_T ExtGrpEntry;
   
    DEBUGOUT("Print AF LUT... \r\n");
#if FULL_CAN_AF_USED
	/* Full CAN Table */
	DEBUGOUT("\tFULL CAN Table: \r\n");
	num = Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_FULLCAN_SEC);
	for (i = 0; i < num; i++) {
		Chip_CAN_ReadFullCANEntry(LPC_CANAF, LPC_CANAF_RAM, i, &StdEntry);
		DEBUGOUT("\t\t%d: Controller ID: %d, ID: 0x%x, Dis: %1d\r\n",
				 i, StdEntry.CtrlNo, StdEntry.ID_11, StdEntry.Disable);
	}
#endif
	/* Standard ID Table */
	DEBUGOUT("\tIndividual Standard ID Table: \r\n");
	num = Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_SFF_SEC);
	for (i = 0; i < num; i++) {
		Chip_CAN_ReadSTDEntry(LPC_CANAF, LPC_CANAF_RAM, i, &StdEntry);
		DEBUGOUT("\t\t%d: Controller ID: %d, ID: 0x%x, Dis: %1d\r\n",
				 i, StdEntry.CtrlNo, StdEntry.ID_11, StdEntry.Disable);
	}
	/* Group Standard ID Table */
	DEBUGOUT("\tGroup Standard ID Table: \r\n");
	num = Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_SFF_GRP_SEC);
	for (i = 0; i < num; i++) {
		Chip_CAN_ReadGroupSTDEntry(LPC_CANAF, LPC_CANAF_RAM, i, &StdGrpEntry);
		DEBUGOUT("\t\t%d: Controller ID: %d, ID: 0x%x-0x%x, Dis: %1d\r\n",
				 i, StdGrpEntry.LowerID.CtrlNo, StdGrpEntry.LowerID.ID_11,
				 StdGrpEntry.UpperID.ID_11, StdGrpEntry.LowerID.Disable);
	}
	/* Extended ID Table */
	DEBUGOUT("\tExtended ID Table: \r\n");
	num = Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_EFF_SEC);
	for (i = 0; i < num; i++) {
		Chip_CAN_ReadEXTEntry(LPC_CANAF, LPC_CANAF_RAM, i, &ExtEntry);
		DEBUGOUT("\t\t%d: Controller ID: %d, ID: 0x%x,\r\n",
				 i, ExtEntry.CtrlNo, ExtEntry.ID_29);
	}
	/* Group Extended ID Table */
	DEBUGOUT("\tGroup Extended ID Table: \r\n");
	num = Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_EFF_GRP_SEC);
	for (i = 0; i < num; i++) {
		Chip_CAN_ReadGroupEXTEntry(LPC_CANAF, LPC_CANAF_RAM, i, &ExtGrpEntry);
		DEBUGOUT("\t\t%d: Controller ID: %d, ID: 0x%x-0x%x\r\n",
				 i, ExtGrpEntry.LowerID.CtrlNo, ExtGrpEntry.LowerID.ID_29,
				 ExtGrpEntry.UpperID.ID_29);
	}

}

/* Setup AF LUT */
static void SetupAFLUT(void)
{
	DEBUGOUT("Setup AF LUT... \r\n");
	Chip_CAN_SetAFLUT(LPC_CANAF, LPC_CANAF_RAM, &AFSections);
/*	PrintAFLUT();*/
}

/* Insert/Remove entries to/from AF LUT */
static void ChangeAFLUT(void)
{
#if FULL_CAN_AF_USED
	CAN_STD_ID_ENTRY_T FullEntry = {CAN_CTRL_NO, 0, 0x02};
#endif
	CAN_STD_ID_ENTRY_T StdEntry = {CAN_CTRL_NO, 0, 0xC0};
	CAN_EXT_ID_ENTRY_T ExtEntry = {CAN_CTRL_NO, ((1 << 11) | 0x0A)};
	CAN_STD_ID_RANGE_ENTRY_T StdGrpEntry = {{CAN_CTRL_NO, 0, 0x7A0}, {CAN_CTRL_NO, 0, 0x7B0}};
	CAN_EXT_ID_RANGE_ENTRY_T ExtGrpEntry = {{CAN_CTRL_NO, ((1 << 11) | 0x7A0)}, {CAN_CTRL_NO, ((1 << 11) | 0x7B0)}};

	
#if FULL_CAN_AF_USED
	/* Edit Full CAN Table */
	Chip_CAN_InsertFullCANEntry(LPC_CANAF, LPC_CANAF_RAM, &FullEntry);
	FullEntry.ID_11 = 2;
	Chip_CAN_InsertFullCANEntry(LPC_CANAF, LPC_CANAF_RAM, &FullEntry);
	FullEntry.ID_11 = 4;
	Chip_CAN_InsertFullCANEntry(LPC_CANAF, LPC_CANAF_RAM, &FullEntry);
#endif /*FULL_CAN_AF_USED*/

	/* Edit Individual STD ID Table */
	Chip_CAN_InsertSTDEntry(LPC_CANAF, LPC_CANAF_RAM, &StdEntry);
	StdEntry.ID_11 = 0x20;
	Chip_CAN_InsertSTDEntry(LPC_CANAF, LPC_CANAF_RAM, &StdEntry);
	StdEntry.ID_11 = 0x40;
	Chip_CAN_InsertSTDEntry(LPC_CANAF, LPC_CANAF_RAM, &StdEntry);

	/* Edit Individual EXT ID Table */
	Chip_CAN_InsertEXTEntry(LPC_CANAF, LPC_CANAF_RAM, &ExtEntry);
	ExtEntry.ID_29 = (1 << 11) | 0x02;
	Chip_CAN_InsertEXTEntry(LPC_CANAF, LPC_CANAF_RAM, &ExtEntry);
	ExtEntry.ID_29 = (1 << 11) | 0x04;
	Chip_CAN_InsertEXTEntry(LPC_CANAF, LPC_CANAF_RAM, &ExtEntry);

	/* Edit STD ID Group Table */
	Chip_CAN_InsertGroupSTDEntry(LPC_CANAF, LPC_CANAF_RAM, &StdGrpEntry);
	StdGrpEntry.LowerID.ID_11 = 0x200;
	StdGrpEntry.UpperID.ID_11 = 0x300;
	Chip_CAN_InsertGroupSTDEntry(LPC_CANAF, LPC_CANAF_RAM, &StdGrpEntry);
	StdGrpEntry.LowerID.ID_11 = 0x400;
	StdGrpEntry.UpperID.ID_11 = 0x500;
	Chip_CAN_InsertGroupSTDEntry(LPC_CANAF, LPC_CANAF_RAM, &StdGrpEntry);

	/* Edit EXT ID Group Table */
	Chip_CAN_InsertGroupEXTEntry(LPC_CANAF, LPC_CANAF_RAM, &ExtGrpEntry);
	ExtGrpEntry.LowerID.ID_29 = (1 << 11) | 0x200;
	ExtGrpEntry.UpperID.ID_29 = (1 << 11) | 0x300;
	Chip_CAN_InsertGroupEXTEntry(LPC_CANAF, LPC_CANAF_RAM, &ExtGrpEntry);
	ExtGrpEntry.LowerID.ID_29 = (1 << 11) | 0x400;
	ExtGrpEntry.UpperID.ID_29 = (1 << 11) | 0x500;
	Chip_CAN_InsertGroupEXTEntry(LPC_CANAF, LPC_CANAF_RAM, &ExtGrpEntry);

	PrintAFLUT();
#if REMOVE_CAN_AF_ENTRIES
	DEBUGOUT("Remove entries into the current LUT... \r\n");
	/* Remove entries from the current LUT */
#if FULL_CAN_AF_USED
	Chip_CAN_RemoveFullCANEntry(LPC_CANAF, LPC_CANAF_RAM, 0);
	Chip_CAN_RemoveFullCANEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_FULLCAN_SEC) - 1);
	Chip_CAN_RemoveFullCANEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_FULLCAN_SEC) / 2);
#endif
	Chip_CAN_RemoveSTDEntry(LPC_CANAF, LPC_CANAF_RAM, 0);
	Chip_CAN_RemoveSTDEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_SFF_SEC) - 1);
	Chip_CAN_RemoveSTDEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_SFF_SEC) / 2);
	Chip_CAN_RemoveGroupSTDEntry(LPC_CANAF, LPC_CANAF_RAM, 0);
	Chip_CAN_RemoveGroupSTDEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_SFF_GRP_SEC) - 1);
	Chip_CAN_RemoveGroupSTDEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_SFF_GRP_SEC) / 2);
	Chip_CAN_RemoveEXTEntry(LPC_CANAF, LPC_CANAF_RAM, 0);
	Chip_CAN_RemoveEXTEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_EFF_SEC) - 1);
	Chip_CAN_RemoveEXTEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_EFF_SEC) / 2);
	Chip_CAN_RemoveGroupEXTEntry(LPC_CANAF, LPC_CANAF_RAM, 0);
	Chip_CAN_RemoveGroupEXTEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_EFF_GRP_SEC) - 1);
	Chip_CAN_RemoveGroupEXTEntry(LPC_CANAF, LPC_CANAF_RAM, Chip_CAN_GetEntriesNum(LPC_CANAF, LPC_CANAF_RAM, CANAF_RAM_EFF_GRP_SEC) / 2);
	PrintAFLUT();
#endif    
}

#endif
