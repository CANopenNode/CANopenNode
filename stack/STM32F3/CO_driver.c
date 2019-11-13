/*
 * CAN module object for ST STM32F334 microcontroller.
 *
 * @file        CO_driver.c
 * @author      Janez Paternoster
 * @author      Ondrej Netik
 * @author      Vijayendra
 * @author      Jan van Lienden
 * @author      Petteri Mustonen
 * @copyright   2013 Janez Paternoster
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

/* Includes ------------------------------------------------------------------*/
#include "stm32f30x.h"
#include "CO_driver.h"
#include "CO_Emergency.h"
#include <string.h>

/* Private macro -------------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private variable ----------------------------------------------------------*/
/* Private function ----------------------------------------------------------*/
static void CO_CANClkSetting (void);
static void CO_CANconfigGPIO (void);
static uint8_t CO_CANsendToModule(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer);

/*******************************************************************************
   Macro and Constants - CAN module registers
 *******************************************************************************/


/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANdriverState){
    (void) CANdriverState;
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){
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
    CAN_InitTypeDef CAN_InitStruct;
    CAN_FilterInitTypeDef CAN_FilterInitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;
    int i;
    uint8_t result;

    /* verify arguments */
    if(CANmodule==NULL || rxArray==NULL || txArray==NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CANmodule->CANdriverState = CANdriverState;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = false;
    CANmodule->bufferInhibitFlag = 0;
    CANmodule->firstCANtxMessage = 1;
    CANmodule->CANtxCount = 0;
    CANmodule->errOld = 0;
    CANmodule->em = 0;

    CAN_ITConfig(CANmodule->CANdriverState, (CAN_IT_TME | CAN_IT_FMP0), DISABLE);

    for (i = 0; i < rxSize; i++) {
        CANmodule->rxArray[i].ident = 0;
        CANmodule->rxArray[i].pFunct = 0;
    }

    for (i = 0; i < txSize; i++) {
        CANmodule->txArray[i].bufferFull = 0;
    }

    /* Setting Clock of CAN HW */
    CO_CANClkSetting();

    /* GPIO Config for CAN */
    CO_CANconfigGPIO();

    /* Init CAN controler */
    CAN_DeInit(CANmodule->CANdriverState);
    CAN_StructInit(&CAN_InitStruct);

    switch (CANbitRate) {
        case 1000: CAN_InitStruct.CAN_Prescaler = 2;
            break;
        case 500: CAN_InitStruct.CAN_Prescaler = 4;
            break;
        default:
        case 250: CAN_InitStruct.CAN_Prescaler = 8;
            break;
        case 125: CAN_InitStruct.CAN_Prescaler = 16;
            break;
        case 100: CAN_InitStruct.CAN_Prescaler = 20;
            break;
        case 50: CAN_InitStruct.CAN_Prescaler = 40;
            break;
        case 20: CAN_InitStruct.CAN_Prescaler = 100;
            break;
        case 10: CAN_InitStruct.CAN_Prescaler = 200;
            break;
    }

    CAN_InitStruct.CAN_SJW = CAN_SJW_1tq;     // changed by VJ, old value = CAN_SJW_1tq;
    CAN_InitStruct.CAN_BS1 = CAN_BS1_13tq;    // changed by VJ, old value = CAN_BS1_3tq;
    CAN_InitStruct.CAN_BS2 = CAN_BS2_2tq;     // changed by VJ, old value = CAN_BS2_2tq;
    CAN_InitStruct.CAN_NART = ENABLE;         // No Automatic retransmision

    result = CAN_Init(CANmodule->CANdriverState, &CAN_InitStruct);
    if (result == 0) {
       return CO_ERROR_TIMEOUT;  /* CO- Return Init failed */
    }

    memset(&CAN_FilterInitStruct, 0, sizeof (CAN_FilterInitStruct));
    CAN_FilterInitStruct.CAN_FilterNumber = 0;
    CAN_FilterInitStruct.CAN_FilterIdHigh = 0;
    CAN_FilterInitStruct.CAN_FilterIdLow = 0;
    CAN_FilterInitStruct.CAN_FilterMaskIdHigh = 0;
    CAN_FilterInitStruct.CAN_FilterMaskIdLow = 0;
    CAN_FilterInitStruct.CAN_FilterFIFOAssignment = 0; // pouzivame jen FIFO0
    CAN_FilterInitStruct.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStruct.CAN_FilterScale = CAN_FilterScale_32bit;
    CAN_FilterInitStruct.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&CAN_FilterInitStruct);

    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    /* interrupts from receiver */
    NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX0_INTERRUPTS;
    NVIC_Init(&NVIC_InitStructure);
    /* interrupts from transmitter */
    NVIC_InitStructure.NVIC_IRQChannel = CAN1_TX_INTERRUPTS;
    NVIC_Init(&NVIC_InitStructure);

    /* Can_init function of ST Driver puts the controller into the normal mode */

    CAN_ITConfig(CANmodule->CANdriverState, (CAN_IT_TME | CAN_IT_FMP0), ENABLE);

    return CO_ERROR_NO;
}

/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    CAN_DeInit(CANmodule->CANdriverState);
}

/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        int8_t                  rtr,
        void                   *object,
        void                  (*pFunct)(void *object, const CO_CANrxMsg_t *message))
{
    CO_CANrx_t *rxBuffer;
    uint16_t RXF, RXM;

    //safety
    if (!CANmodule || !object || !pFunct || index >= CANmodule->rxSize) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* buffer, which will be configured */
    rxBuffer = CANmodule->rxArray + index;

    /* Configure object variables */
    rxBuffer->object = object;
    rxBuffer->pFunct = pFunct;

    /* CAN identifier and CAN mask, bit aligned with CAN module registers */
    RXF = (ident & 0x07FF) << 2;
    if (rtr) RXF |= 0x02;
    RXM = (mask & 0x07FF) << 2;
    RXM |= 0x02;

    /* configure filter and mask */
    if (RXF != rxBuffer->ident || RXM != rxBuffer->mask) {
        rxBuffer->ident = RXF;
        rxBuffer->mask = RXM;
    }

    return CO_ERROR_NO;
}

/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        int8_t                  rtr,
        uint8_t                 noOfBytes,
        int8_t                  syncFlag)
{
    uint32_t TXF;
    CO_CANtx_t *buffer;

    /* safety */
    if (!CANmodule || CANmodule->txSize <= index) return 0;

    /* get specific buffer */
    buffer = &CANmodule->txArray[index];

    /* CAN identifier, bit aligned with CAN module registers */
    TXF = ident << 21;
    TXF &= 0xFFE00000;
    if (rtr) TXF |= 0x02;

    /* write to buffer */
    buffer->ident = TXF;
    buffer->DLC = noOfBytes;
    buffer->bufferFull = 0;
    buffer->syncFlag = syncFlag ? 1 : 0;

    return buffer;
}

/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t err = CO_ERROR_NO;
    uint8_t txBuff;

    /* Verify overflow */
    if (buffer->bufferFull) {
        if(!CANmodule->firstCANtxMessage) /* don't set error, if bootup message is still on buffers */
            CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, 0);
        err = CO_ERROR_TX_OVERFLOW;
    }

    CO_LOCK_CAN_SEND();

    /* First try to transmit the message immediately if mailbox is free.
     * Only one TX mailbox is used of the three available in the hardware */
    CANmodule->bufferInhibitFlag = buffer->syncFlag;
    txBuff = CO_CANsendToModule(CANmodule, buffer);

    /* No free mailbox -> use interrupt for transmission */
    if (txBuff == CAN_TxStatus_NoMailBox) {
        buffer->bufferFull = 1;
        CANmodule->CANtxCount++;
    }
    CO_UNLOCK_CAN_SEND();

    return err;
}

/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    uint32_t tpdoDeleted = 0U;
    uint8_t state = 0;

    CO_LOCK_CAN_SEND();
    /* Abort message from CAN module, if there is synchronous TPDO. */
    state = CAN_TransmitStatus(CANmodule->CANdriverState, CO_CAN_TXMAILBOX);
    if((state == CAN_TxStatus_Pending) && (CANmodule->bufferInhibitFlag)) {
        CAN_CancelTransmit(CANmodule->CANdriverState, CO_CAN_TXMAILBOX);
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
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule)
{
    uint32_t err;
    CO_EM_t* em = (CO_EM_t*)CANmodule->em;

    err = CANmodule->CANdriverState->ESR;

    if(CANmodule->errOld != err) {
        CANmodule->errOld = err;

        /* CAN RX bus overflow */
        if(CANmodule->CANdriverState->RF0R & 0x10) {
            CO_errorReport(em, CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, err);
            CANmodule->CANdriverState->RF0R &=~0x10;//clear bits
        }

        /* CAN TX bus off */
        if(err & 0x04) {
            CO_errorReport(em, CO_EM_CAN_TX_BUS_OFF, CO_EMC_BUS_OFF_RECOVERED, err);
        }
        else {
            CO_errorReset(em, CO_EM_CAN_TX_BUS_OFF, err);
        }

        /* CAN TX or RX bus passive */
        if(err & 0x02) {
            if(!CANmodule->firstCANtxMessage) CO_errorReport(em, CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
        }
        else {
        // int16_t wasCleared;
        /* wasCleared = */CO_errorReset(em, CO_EM_CAN_TX_BUS_PASSIVE, err);
        /* if(wasCleared == 1) */CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
        }


        /* CAN TX or RX bus warning */
        if(err & 0x01) {
            CO_errorReport(em, CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, err);
        }
        else {
            CO_errorReset(em, CO_EM_CAN_BUS_WARNING, err);
        }
    }
}

/******************************************************************************/
/* Interrupt from receiver */
void CO_CANinterrupt_Rx(CO_CANmodule_t *CANmodule)
{
    CanRxMsg CAN1_RxMsg;
    uint16_t index;
    uint8_t msgMatched = 0;
    CO_CANrx_t *msgBuff = CANmodule->rxArray;

	CAN_Receive(CANmodule->CANdriverState, CAN_FilterFIFO0, &CAN1_RxMsg);

    for (index = 0; index < CANmodule->rxSize; index++) {
        uint16_t msg = (CAN1_RxMsg.StdId << 2) | (CAN1_RxMsg.RTR ? 2 : 0);

        if (((msg ^ msgBuff->ident) & msgBuff->mask) == 0) {
            msgMatched = 1;
            break;
        }
        msgBuff++;
    }

    /* Call specific function, which will process the message */
    if (msgMatched && msgBuff->pFunct) {
        msgBuff->pFunct(msgBuff->object, (CO_CANrxMsg_t*) &CAN1_RxMsg);
    }
}

/******************************************************************************/
/* Interrupt from trasmitter */
void CO_CANinterrupt_Tx(CO_CANmodule_t *CANmodule)
{
    /* First CAN message (bootup) was sent successfully */
    CANmodule->firstCANtxMessage = 0;

    /* clear flag from previous message */
    CANmodule->bufferInhibitFlag = 0;

    /* Are there any new messages waiting to be send */
    if (CANmodule->CANtxCount > 0) {
        uint16_t i;             /* index of transmitting message */

        /* first buffer */
        CO_CANtx_t *buffer = CANmodule->txArray;
        /* search through whole array of pointers to transmit message buffers. */
        for(i = CANmodule->txSize; i > 0; i--) {
            /* if message buffer is full, send it. */
            if(buffer->bufferFull) {
                buffer->bufferFull = 0;
                CANmodule->CANtxCount--;

                /* Copy message to CAN buffer */
                CANmodule->bufferInhibitFlag = buffer->syncFlag;
                CO_CANsendToModule(CANmodule, buffer);
                break;                      /* exit for loop */
            }
            buffer++;
        }/* end of for loop */

        /* Clear counter if no more messages */
        if(i == 0) CANmodule->CANtxCount = 0;
    }
}

/******************************************************************************/
static uint8_t CO_CANsendToModule(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CAN_TxMailBox_TypeDef* txMbox;

    /* Checks if the transmit mailbox is available */
    if ((CANmodule->CANdriverState->TSR & CAN_TSR_TME0) == CAN_TSR_TME0) {
        txMbox = &CANmodule->CANdriverState->sTxMailBox[CO_CAN_TXMAILBOX];
    }
    else {
        return CAN_TxStatus_NoMailBox;
    }

    /* ID: always assuming standard 11-bit ID */
    txMbox->TIR &= 1;
    txMbox->TIR |= ((buffer->ident) | CAN_RTR_DATA);

    /* DLC */
    buffer->DLC &= (uint8_t)0x0000000F;
    txMbox->TDTR &= (uint32_t)0xFFFFFFF0;
    txMbox->TDTR |= buffer->DLC;

    /* Data field */
    txMbox->TDLR = (((uint32_t)buffer->data[3] << 24) |
                    ((uint32_t)buffer->data[2] << 16) |
                    ((uint32_t)buffer->data[1] << 8) |
                    ((uint32_t)buffer->data[0]));

    txMbox->TDHR = (((uint32_t)buffer->data[7] << 24) |
                    ((uint32_t)buffer->data[6] << 16) |
                    ((uint32_t)buffer->data[5] << 8) |
                    ((uint32_t)buffer->data[4]));

    /* Request transmission */
    txMbox->TIR |= 1;

    return 0;
}

/******************************************************************************/
static void CO_CANClkSetting (void)
{
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
}

/******************************************************************************/
static void CO_CANconfigGPIO (void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_CAN_RX | GPIO_Pin_CAN_TX;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;

	GPIO_Init(GPIO_CAN, &GPIO_InitStruct);

    /* Map to correct alternative function (9 == CAN) */
	GPIO_PinAFConfig(GPIO_CAN, GPIO_PinSource_CAN_RX, GPIO_AF_9);
	GPIO_PinAFConfig(GPIO_CAN, GPIO_PinSource_CAN_TX, GPIO_AF_9);
}
