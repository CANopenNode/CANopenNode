/*
 * CAN module object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @copyright   2004 - 2015 Janez Paternoster
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


#include "CO_driver.h"
#include "CO_Emergency.h"
/* #include "flexcan_driver.h" */

CO_FlexCAN_config_t CAN_config;

/******************************************************************************/
/*
 * Configure mailbox for receiving and start listening for data
 *
 * This function is used only by the driver layer
 *
 * @param   instance           A FlexCAN instance number
 * @param   data               The FlexCAN receive message structure
 */
static void FlexCAN_RxMailbox_Config(uint8_t instance, flexcan_msgbuff_t *data)
{
    flexcan_data_info_t dataInfo =
    {
        .data_length = 8U,
        .msg_id_type = FLEXCAN_MSG_ID_STD,
        .enable_brs  = false,
        .fd_enable   = false,
        .fd_padding  = 0U
    };

    /* Configure RX message buffer with index RX_MSG_ID and RX_MAILBOX */
    FLEXCAN_DRV_ConfigRxMb(instance, RX_MAILBOXID, &dataInfo, RX_MESSAGEID);

    /* Start receiving data in RX_MAILBOX. */
    FLEXCAN_DRV_Receive(instance, RX_MAILBOXID, data);
}


/******************************************************************************/
CO_ReturnError_t CO_FLEXCAN_Init(
        uint8_t                         instance,
        flexcan_state_t                *state,
        const flexcan_user_config_t    *data,
        uint16_t                        nodeID)
{
    if((state != NULL) && (data != NULL))
    {
        CAN_config.CAN_instance = instance;
        CAN_config.CAN_state = state;
        CAN_config.CAN_user_config = data;
        CAN_config.nodeID = nodeID;
    }
    else
    {
        return CO_ERROR_PARAMETERS;
    }
    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANsetConfigurationMode(int32_t CANbaseAddress){
    /* Put CAN module in configuration mode */
    (void)CANbaseAddress;/* suppress unused variable warning */

    /* Configure the FlexCAN module with the user FlexCAN settings */
    if((CAN_config.CAN_state != NULL) && (CAN_config.CAN_user_config != NULL))
    {
        FLEXCAN_DRV_Init(CAN_config.CAN_instance, CAN_config.CAN_state, CAN_config.CAN_user_config);
    }
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){
    /* Put CAN module in normal mode */

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
    (void)CANbitRate;/*suppress unused warning as bit rate is configured in FlexCAN component*/
    
    /* verify arguments */
    if(CANmodule==NULL || rxArray==NULL || txArray==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->INST_CANCOM = CAN_config.CAN_instance;
    CANmodule->canCom_State = CAN_config.CAN_state;
    CANmodule->canCom_InitConfig = CAN_config.CAN_user_config;
    CANmodule->nodeID = CAN_config.nodeID;

    CANmodule->CANbaseAddress = CANbaseAddress;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANnormal = false;
    /* microcontroller dependent, this version does not support RxFIFO or filters */
    CANmodule->useCANrxFilters = CANmodule->canCom_InitConfig->is_rx_fifo_needed;
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
    /* Configure CAN timing */
    /* Configure CAN module hardware filters */
    /* These configurations of CAN module are already done using FLEXCAN_DRV_Init() */

    /* Configure CAN interrupt registers */
    /* Install CallBack function for FlexCAN interrupts */
    FLEXCAN_DRV_InstallEventCallback(CANmodule->INST_CANCOM, &CO_CANinterrupt, CANmodule);

    /* Disable mailboxID filtering */
    FLEXCAN_DRV_SetRxMaskType(CANmodule->INST_CANCOM,FLEXCAN_RX_MASK_GLOBAL);
    FLEXCAN_DRV_SetRxMbGlobalMask(CANmodule->INST_CANCOM,FLEXCAN_MSG_ID_STD,0x00U);
    FLEXCAN_DRV_SetRxMb14Mask(CANmodule->INST_CANCOM,FLEXCAN_MSG_ID_STD,0x00U);
    FLEXCAN_DRV_SetRxMb15Mask(CANmodule->INST_CANCOM,FLEXCAN_MSG_ID_STD,0x00U);

    /* Configure RX message buffer */
    FlexCAN_RxMailbox_Config(CANmodule->INST_CANCOM, &CANmodule->rxBuffer);

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule){
    /* turn off the module */
    FLEXCAN_DRV_Deinit(CANmodule->INST_CANCOM);
    CANmodule->CANnormal = false;
}


/******************************************************************************/
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg){
    return (uint16_t) rxMsg->messageId;
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
    (void)rtr;
    if((CANmodule != NULL) && (index < CANmodule->txSize)){
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

        /* 
         * CAN identifier aligned with CAN module transmit buffer.
         * Microcontroller specific.
         */
        buffer->ident = ((uint32_t)ident & 0x07FFU);

        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;

        buffer->dataInfo.data_length = noOfBytes;
        buffer->dataInfo.msg_id_type = FLEXCAN_MSG_ID_STD;
        buffer->dataInfo.enable_brs  = false;
        buffer->dataInfo.fd_enable   = false;
        buffer->dataInfo.fd_padding  = 0U;
    }
    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;

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
    status_t status = FLEXCAN_DRV_GetTransferStatus(CANmodule->INST_CANCOM, TX_MAILBOXID);
    if((status == STATUS_SUCCESS) && (CANmodule->CANtxCount == 0)){
        CANmodule->bufferInhibitFlag = buffer->syncFlag;

        /* Configure TX message buffer with index TX_MSG_ID and TX_MAILBOX*/
        FLEXCAN_DRV_ConfigTxMb(CANmodule->INST_CANCOM, TX_MAILBOXID, &buffer->dataInfo, buffer->ident);

        /* Execute send non-blocking */
        FLEXCAN_DRV_Send(CANmodule->CANbaseAddress, TX_MAILBOXID, &buffer->dataInfo, buffer->ident, buffer->data);
    }
    /* if no buffer is free, message will be sent by the interrupt routine */
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
    status_t status = FLEXCAN_DRV_GetTransferStatus(CANmodule->INST_CANCOM, TX_MAILBOXID);
    if((status != STATUS_SUCCESS) && CANmodule->bufferInhibitFlag)
    {
        status = FLEXCAN_DRV_AbortTransfer(CANmodule->INST_CANCOM, TX_MAILBOXID);
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

    /* get error counters from module. Id possible, function may use different way to determine errors. */
    CAN_Type *CAN_Base[] = CAN_BASE_PTRS;
    rxErrors = (uint16_t)((CAN_Base[CANmodule->INST_CANCOM]->ECR & CAN_ECR_RXERRCNT_MASK) >> CAN_ECR_RXERRCNT_SHIFT);
    txErrors = (uint16_t)((CAN_Base[CANmodule->INST_CANCOM]->ECR & CAN_ECR_TXERRCNT_MASK) >> CAN_ECR_TXERRCNT_SHIFT);
    overflow = (uint16_t)(((CAN_Base[CANmodule->INST_CANCOM]->ESR1 & CAN_ESR1_ERROVR_MASK) >> CAN_ESR1_ERROVR_SHIFT) & 1u);

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
void CO_CANinterrupt(uint8_t                        instance,
                     flexcan_event_type_t           eventType,
                     flexcan_state_t               *flexcanState)
{
    /* Get the CANmodule object */
    CO_CANmodule_t *CANmodule = flexcanState->callbackParam;

    /* Find the mailbox that generated the interrupt */
    uint16_t mailboxId = 0;

    for(; mailboxId < MAILBOX_NR; mailboxId++)
        if((flexcanState->mbs[mailboxId].state == FLEXCAN_MB_IDLE)
            && (flexcanState->mbs[mailboxId].mb_message != NULL))
            break;

    /* receive interrupt */
    if((eventType == FLEXCAN_EVENT_RX_COMPLETE) && (mailboxId < MAILBOX_NR))
    {
        CO_CANrxMsg_t rcvMsgBuff;
        CO_CANrxMsg_t *rcvMsg;      /* pointer to received message in CAN module */
        uint16_t index;             /* index of received message */
        uint32_t rcvMsgIdent;       /* identifier of the received message */
        CO_CANrx_t *buffer = CANmodule->rxArray;  /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;

        /* get message from module here */
        rcvMsg = &rcvMsgBuff;
        rcvMsg->data = flexcanState->mbs[mailboxId].mb_message->data;
        rcvMsg->DLC = flexcanState->mbs[mailboxId].mb_message->dataLen;
        rcvMsg->messageId = flexcanState->mbs[mailboxId].mb_message->msgId;
        rcvMsg->ident = CO_CANrxMsg_readIdent(rcvMsg);
        rcvMsgIdent = CO_CANrxMsg_readIdent(rcvMsg);

        /* 
         * CAN module filters are not used, message with any standard 11-bit identifier
         * has been received. Search rxArray from CANmodule for the same CAN-ID.
         */
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

        /* Interrupt flag is cleared in the FlexCAN IRQ handler*/
        /* Restart listening */
        FLEXCAN_DRV_Receive(instance, mailboxId, &CANmodule->rxBuffer);
    }
    /* transmit interrupt */
    else if(eventType == FLEXCAN_EVENT_TX_COMPLETE)
    {
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
                    CO_CANsend(CANmodule, buffer);
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
