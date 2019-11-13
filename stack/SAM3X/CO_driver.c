/*
 * CAN module object for the Atmel SAM3X microcontroller.
 *
 * @file        CO_driver.c
 * @author      Janez Paternoster
 * @author      Olof Larsson
 * @copyright   2014 Janez Paternoster
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

#include "asf.h"
#include "htc.h"

#include "sn65hvd234.h"

#define CO_UNUSED(v)  (void)(v)
#define CANMB_TX      (CANMB_NUMBER - 1)

void reset_mailbox_conf(can_mb_conf_t *p_mailbox)
{
  p_mailbox->ul_mb_idx = 0;
  p_mailbox->uc_obj_type = 0;
  p_mailbox->uc_id_ver = 0;
  p_mailbox->uc_length = 0;
  p_mailbox->uc_tx_prio = 0;
  p_mailbox->ul_status = 0;
  p_mailbox->ul_id_msk = 0;
  p_mailbox->ul_id = 0;
  p_mailbox->ul_fid = 0;
  p_mailbox->ul_datal = 0;
  p_mailbox->ul_datah = 0;
}


/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANdriverState)
{
  CO_UNUSED(CANdriverState);
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule)
{
  CO_UNUSED(CANmodule->CANdriverState);

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
  uint32_t ul_sysclk;

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

  for(i=0U; i<rxSize; i++)
  {
    rxArray[i].ident = 0U;
    rxArray[i].pFunct = NULL;
  }
  for(i=0U; i<txSize; i++)
  {
    txArray[i].bufferFull = false;
  }

  /* Configure CAN module registers */

  if (((uintptr_t) CANmodule->CANdriverState) == CAN0)
  {
    /* CAN0 Transceiver */
    static sn65hvd234_ctrl_t can0_transceiver;

    /* Initialize CAN0 Transceiver. */
    sn65hvd234_set_rs(&can0_transceiver, PIN_CAN0_TR_RS_IDX);
    sn65hvd234_set_en(&can0_transceiver, PIN_CAN0_TR_EN_IDX);

    /* Low power mode is listening only */
    sn65hvd234_disable_low_power(&can0_transceiver);

    /* Enable CAN0 Transceiver */
    sn65hvd234_enable(&can0_transceiver);

    /* Configure CAN timing */

    /* Enable CAN0 & CAN0 clock. */
    pmc_enable_periph_clk(ID_CAN0);

    ul_sysclk = sysclk_get_cpu_hz();

    if (can_init(CAN0, ul_sysclk, CANbitRate))
    {
      printf("CAN0 initialization is completed\n\r");

      /* Disable all CAN0 interrupts */
      can_disable_interrupt(CAN0, CAN_DISABLE_ALL_INTERRUPT_MASK);

      /* Configure and enable interrupt of CAN0 */
      NVIC_EnableIRQ(CAN0_IRQn);
    }

    can_reset_all_mailbox(CANmodule->CANdriverState);

    /* CAN module filters are not used, all messages with standard 11-bit */
    /* identifier will be received */
    /* Configure mask 0 so that all messages with standard identifier are accepted */

    /* Init CAN0 mailbox 0-6 as reception mailboxes */
    for (i = 0; i <= (CANMB_NUMBER-2); i++)
    {
      reset_mailbox_conf(&CANmodule->rxMbConf[i]);
      CANmodule->rxMbConf[i].ul_mb_idx = i;
      CANmodule->rxMbConf[i].uc_obj_type = CAN_MB_RX_MODE;

      if (i == (CANMB_NUMBER-2))
        CANmodule->rxMbConf[i].uc_obj_type = CAN_MB_RX_OVER_WR_MODE;

      /* Standard mode only, not extended mode */
      CANmodule->rxMbConf[i].ul_id_msk = CAN_MAM_MIDvA_Msk;
      CANmodule->rxMbConf[i].ul_id = CAN_MID_MIDvA(0);
      can_mailbox_init(CANmodule->CANdriverState, &CANmodule->rxMbConf[i]);

      /* Enable CAN0 mailbox number i interrupt. */
      can_enable_interrupt(CANmodule->CANdriverState, (0x1u << i));
    }

    /* Init last CAN0 mailbox, number 7, as transmit mailbox */
    reset_mailbox_conf(&CANmodule->txMbConf);
    CANmodule->txMbConf.ul_mb_idx = (CANMB_TX);
    CANmodule->txMbConf.uc_obj_type = CAN_MB_TX_MODE;
    CANmodule->txMbConf.uc_tx_prio = 14;
    CANmodule->txMbConf.uc_id_ver = 0;
    CANmodule->txMbConf.ul_id_msk = 0;
    can_mailbox_init(CANmodule->CANdriverState, &CANmodule->txMbConf);
  }

  if (CANmodule->CANdriverState == CAN1)
  {
    /* CAN1 Transceiver */
    static sn65hvd234_ctrl_t can1_transceiver;

    /* Initialize CAN1 Transceiver */
    sn65hvd234_set_rs(&can1_transceiver, PIN_CAN1_TR_RS_IDX);
    sn65hvd234_set_en(&can1_transceiver, PIN_CAN1_TR_EN_IDX);

    /* Enable CAN1 Transceiver */
    sn65hvd234_disable_low_power(&can1_transceiver); //Low power mode == listening only mode
    sn65hvd234_enable(&can1_transceiver);

    /* Configure CAN timing */

    /* Enable CAN1 & CAN1 clock */
    pmc_enable_periph_clk(ID_CAN1);

    ul_sysclk = sysclk_get_cpu_hz();

    if (can_init(CAN1, ul_sysclk, CAN_BPS_250K))
    {
      printf("CAN1 initialization is completed\n\r");

      /* Disable all CAN1 interrupts */
      can_disable_interrupt(CAN1, CAN_DISABLE_ALL_INTERRUPT_MASK);

      /* Configure and enable interrupt of CAN1 */
      NVIC_EnableIRQ(CAN1_IRQn);
    }

    can_reset_all_mailbox(CANmodule->CANdriverState);

    /* CAN module filters are not used, all messages with standard 11-bit */
    /* identifier will be received */
    /* Configure mask 0 so that all messages with standard identifier are accepted */

    /* Init CAN1 mailbox 0-6 as reception mailboxes */

    for (i = 0; i <= (CANMB_NUMBER-2); i++)
    {
      reset_mailbox_conf(&CANmodule->rxMbConf[i]);
      CANmodule->rxMbConf[i].ul_mb_idx = i;
      CANmodule->rxMbConf[i].uc_obj_type = CAN_MB_RX_MODE;

      if (i == (CANMB_NUMBER-2))
        CANmodule->rxMbConf[i].uc_obj_type = CAN_MB_RX_OVER_WR_MODE;

      /* Standard mode only, not extended mode */
      CANmodule->rxMbConf[i].ul_id_msk = CAN_MAM_MIDvA_Msk;
      CANmodule->rxMbConf[i].ul_id = CAN_MID_MIDvA(0);
      can_mailbox_init(CANmodule->CANdriverState, &CANmodule->rxMbConf[i]);

      /* Enable CAN1 mailbox number i interrupt. */
      can_enable_interrupt(CANmodule->CANdriverState, (0x1u << i));
    }

    /* Init last CAN1 mailbox, number 7, as transmit mailbox */
    reset_mailbox_conf(&CANmodule->txMbConf);
    CANmodule->txMbConf.ul_mb_idx = (CANMB_TX);
    CANmodule->txMbConf.uc_obj_type = CAN_MB_TX_MODE;
    CANmodule->txMbConf.uc_tx_prio = 14;
    CANmodule->txMbConf.uc_id_ver = 0;
    CANmodule->txMbConf.ul_id_msk = 0;
    can_mailbox_init(CANmodule->CANdriverState, &CANmodule->txMbConf);
  }

  return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
  /* Turn the module off  */
  can_disable(CANmodule->CANdriverState);
}


/******************************************************************************/
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg)
{
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

  if((CANmodule!=NULL) && (object!=NULL) && (pFunct!=NULL) && (index < CANmodule->rxSize))
  {
    /* Buffer, which will be configured */
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
    if(CANmodule->useCANrxFilters)
    {
    }
  }
  else
  {
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

  if((CANmodule != NULL) && (index < CANmodule->txSize))
  {
    /* Get specific buffer */
    buffer = &CANmodule->txArray[index];

    /* CAN identifier, DLC and rtr, bit aligned with CAN module transmit buffer. */
    buffer->ident = ident;
    buffer->rtr = rtr;

    buffer->bufferFull = false;
    buffer->syncFlag = syncFlag;
    buffer->DLC = noOfBytes;
  }

  return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
  CO_ReturnError_t err = CO_ERROR_NO;

  /* Verify overflow */
  if(buffer->bufferFull){
    if(!CANmodule->firstCANtxMessage){
      /* Don't set error, if bootup message is still on buffers */
      CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, buffer->ident);
    }
    err = CO_ERROR_TX_OVERFLOW;
  }

  CO_LOCK_CAN_SEND();

  /* If CAN TX buffer is free, copy message to it */
  if (((can_mailbox_get_status(CANmodule->CANdriverState, CANMB_TX) & CAN_MSR_MRDY) == CAN_MSR_MRDY) && (CANmodule->CANtxCount == 0))
  {
    CANmodule->bufferInhibitFlag = buffer->syncFlag;

    /* Copy message and txRequest */
    CANmodule->txMbConf.ul_id = CAN_MID_MIDvA(buffer->ident);
    CANmodule->txMbConf.ul_datal = *((uint32_t *) &(buffer->data[0]));
    CANmodule->txMbConf.ul_datah = *((uint32_t *) &(buffer->data[4]));
    CANmodule->txMbConf.uc_length = buffer->DLC;

    if (buffer->rtr)
      can_mailbox_tx_remote_frame(CANmodule->CANdriverState, &CANmodule->txMbConf);
    else
      can_mailbox_write(CANmodule->CANdriverState, &CANmodule->txMbConf);

    can_global_send_transfer_cmd(CANmodule->CANdriverState, 1 << CANMB_TX);
  }
  else /* If no buffer is free, message will be sent by interrupt */
  {
    buffer->bufferFull = true;
    CANmodule->CANtxCount++;
  }
  can_enable_interrupt(CANmodule->CANdriverState, 0x1u << CANMB_TX);
  CO_UNLOCK_CAN_SEND();

  return err;
}


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
  uint32_t tpdoDeleted = 0U;

  CO_LOCK_CAN_SEND();
  /* Abort message from CAN module, if there is synchronous TPDO.
  * Take special care with this functionality. */
  if(/*messageIsOnCanBuffer && */CANmodule->bufferInhibitFlag){
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
  uint16_t rxErrors = 0, txErrors = 0, overflow = 0;
  CO_EM_t* em = (CO_EM_t*)CANmodule->em;
  uint32_t err;

  /* get error counters from module. Id possible, function may use different way to
  * determine errors. */

  rxErrors = can_get_rx_error_cnt(CANmodule->CANdriverState);

  txErrors = can_get_tx_error_cnt(CANmodule->CANdriverState);

  //overflow = CANmodule->txSize;

  err = ((uint32_t)txErrors << 16) | ((uint32_t)rxErrors << 8) | overflow;

  if(CANmodule->errOld != err)
  {
    CANmodule->errOld = err;

    if(txErrors >= 256U)   /* bus off */
    {
      CO_errorReport(em, CO_EM_CAN_TX_BUS_OFF, CO_EMC_BUS_OFF_RECOVERED, err);
    }
    else  /* not bus off */
    {
      CO_errorReset(em, CO_EM_CAN_TX_BUS_OFF, err);

      if((rxErrors >= 96U) || (txErrors >= 96U))
      {     /* bus warning */
        CO_errorReport(em, CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, err);
      }

      if(rxErrors >= 128U) /* RX bus passive */
      {
        CO_errorReport(em, CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
      }
      else
      {
        CO_errorReset(em, CO_EM_CAN_RX_BUS_PASSIVE, err);
      }

      if(txErrors >= 128U) /* TX bus passive */
      {
        if(!CANmodule->firstCANtxMessage)
        {
          CO_errorReport(em, CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
        }
      }
      else
      {
        bool_t isError = CO_isError(em, CO_EM_CAN_TX_BUS_PASSIVE);
        if(isError)
        {
          CO_errorReset(em, CO_EM_CAN_TX_BUS_PASSIVE, err);
          CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
        }
      }

      if((rxErrors < 96U) && (txErrors < 96U)) /* no error */
      {
        CO_errorReset(em, CO_EM_CAN_BUS_WARNING, err);
      }
    }

    if(overflow != 0U) /* CAN RX bus overflow */
    {
      CO_errorReport(em, CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, err);
    }
  }
}


/******************************************************************************/
void CO_CANinterrupt(CO_CANmodule_t *CANmodule)
{
  uint32_t ul_status;

  ul_status = can_get_status(CANmodule->CANdriverState);
  if (ul_status & GLOBAL_MAILBOX_MASK)
  {
    for (uint8_t i = 0; i < CANMB_NUMBER; i++)
    {
      ul_status = can_mailbox_get_status(CANmodule->CANdriverState, i);
      if ((ul_status & CAN_MSR_MRDY) == CAN_MSR_MRDY) //Mailbox Ready Bit
      {
        //Handle interrupt
        if (i != CANMB_TX)
        {
          //Receive interrupt
          CO_CANrxMsg_t *rcvMsg;      /* pointer to received message in CAN module */
          CO_CANrxMsg_t rcvMsgBuf;
          uint16_t index;             /* index of received message */
          uint32_t rcvMsgIdent;       /* identifier of the received message */
          CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */
          bool_t msgMatched = false;

          CANmodule->rxMbConf[i].ul_mb_idx = i;
          CANmodule->rxMbConf[i].ul_status = ul_status;
          can_mailbox_read(CANmodule->CANdriverState, &CANmodule->rxMbConf[i]);


          /* Get message from module here */
          memset(rcvMsgBuf.data, 0, 8);
          memcpy(rcvMsgBuf.data, &CANmodule->rxMbConf[i].ul_datal, CANmodule->rxMbConf[i].uc_length);
          rcvMsgBuf.ident = CANmodule->rxMbConf[i].ul_id;
          rcvMsgBuf.DLC = CANmodule->rxMbConf[i].uc_length;

          rcvMsg = &rcvMsgBuf;

          rcvMsgIdent = rcvMsg->ident;
          if(CANmodule->useCANrxFilters)
          {
            /* CAN module filters are used. Message with known 11-bit identifier has */
            /* been received */
            index = 0;  /* get index of the received message here. Or something similar */
            if(index < CANmodule->rxSize)
            {
              buffer = &CANmodule->rxArray[index];
              /* verify also RTR */
              if(((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U)
              {
                msgMatched = true;
              }
            }
          }
          else
          {
            /* CAN module filters are not used, message with any standard 11-bit identifier */
            /* has been received. Search rxArray from CANmodule for the same CAN-ID. */
            buffer = &CANmodule->rxArray[0];
            for(index = CANmodule->rxSize; index > 0U; index--)
            {
              if(((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U)
              {
                msgMatched = true;
                break;
              }
              buffer++;
            }
          }

          /* Call specific function, which will process the message */
          if(msgMatched && (buffer != NULL) && (buffer->pFunct != NULL))
          {
            buffer->pFunct(buffer->object, rcvMsg);
          }
        }
        else
        {
          /* First CAN message (bootup) was sent successfully */
          CANmodule->firstCANtxMessage = false;
          /* Clear flag from previous message */
          CANmodule->bufferInhibitFlag = false;
          /* Are there any new messages waiting to be send */
          if(CANmodule->CANtxCount > 0U)
          {
            uint16_t j;    /* Index of transmitting message */

            /* First buffer */
            CO_CANtx_t *buffer = &CANmodule->txArray[0];
            /* Search through whole array of pointers to transmit message buffers. */
            for(j = CANmodule->txSize; j > 0U; j--)
            {
              /* If message buffer is full, send it. */
              if(buffer->bufferFull)
              {
                buffer->bufferFull = false;
                CANmodule->CANtxCount--;

                /* Copy message to CAN buffer */
                CANmodule->txMbConf.ul_datal = *((uint32_t *) &(buffer->data[0]));
                CANmodule->txMbConf.ul_datah = *((uint32_t *) &(buffer->data[4]));

                /* Write transmit information into mailbox. */
                CANmodule->txMbConf.ul_id = CAN_MID_MIDvA(buffer->ident);
                CANmodule->txMbConf.uc_length = buffer->DLC;

                if (buffer->rtr)
                  can_mailbox_tx_remote_frame(CANmodule->CANdriverState, &CANmodule->txMbConf);
                else
                  can_mailbox_write(CANmodule->CANdriverState, &CANmodule->txMbConf);

                can_global_send_transfer_cmd(CANmodule->CANdriverState, 1 << CANMB_TX);

                break; /* Exit for loop */
              }
              buffer++;
            } /* End of for loop */

            /* Clear counter if no more messages */
            if(j == 0U){
              CANmodule->CANtxCount = 0U;
            }
          }
          else
          {
            /* Nothing more to send */
            can_disable_interrupt(CANmodule->CANdriverState, 0x1u << CANMB_TX);
          }
        }
        break;
      }
    }
  }
  else
  {
    if (ul_status & CAN_SR_ERRA);   //error active
    if (ul_status & CAN_SR_WARN);   //warning limit
    //CO_EM_CAN_BUS_WARNING
    if (ul_status & CAN_SR_ERRP);   //error passive
    //CO_EM_CAN_TX_BUS_PASSIVE
    if (ul_status & CAN_SR_BOFF);   //bus off
    //CO_EM_CAN_TX_BUS_OFF
    if (ul_status & CAN_SR_SLEEP);  //controller in sleep mode
    if (ul_status & CAN_SR_WAKEUP); //controller woke up
    if (ul_status & CAN_SR_TOVF);   //timer overflow
    if (ul_status & CAN_SR_TSTP);   //timestamp - start or end of frame
    if (ul_status & CAN_SR_CERR);   //CRC error in mailbox
    if (ul_status & CAN_SR_SERR);   //stuffing error in mailbox
    if (ul_status & CAN_SR_AERR);   //ack error
    if (ul_status & CAN_SR_FERR);   //form error
    if (ul_status & CAN_SR_BERR);   //bit error
  }
}
