/*
 * CO_HBproducer.cpp
 *
 *  Created on: 30 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      HB Producer
 *
 */

/** Includes *****************************************************************/
#include "config.h"

#include "CO_OD.h"
#include "CO_HBproducer.h"

#include "task.h"

// generate error, if features are not correctly configured for this project
#ifndef OD_producerHeartbeatTime
#error Features from CO_OD.h file are not correctly configured for this project!
#endif

/** Defines and constants ****************************************************/
/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/
/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_HBProducer oCO_HBProducer;

/** Class methods ************************************************************/

CO_ReturnError_t cCO_HBProducer::nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
    cCO_Driver const * const pCO_Driver)    {

  if (pCO_NMT_EMCY == NULL)   return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_Driver == NULL)     return CO_ERROR_ILLEGAL_ARGUMENT;

  m_pCO_NMT_EMCY = (cCO_NMT_EMCY*)pCO_NMT_EMCY;
  m_pCO_Driver = (cCO_Driver*)pCO_Driver;

  return CO_ERROR_NO;

}//cCO_HBProducer::nConfigure -------------------------------------------------

void cCO_HBProducer::vInit(void)    {

  TaskHandle_t                xHandle;                            // task handle

  /** create OS objects ******/
  // create CANOpen state change queue
  m_xQueueHandle_NMTStateChange = xQueueCreate(50, sizeof(CO_NMT_internalState_t));
  configASSERT( m_xQueueHandle_NMTStateChange );

  // create task
  if (xTaskCreate( vCO_HBProducer_Task, ( char * ) CO_HBProducer_Task_name, 500, NULL,
      CO_HBProducer_Task_priority, &xHandle) != pdPASS)
    for(;;);
  nAddHandle(xHandle);
  return;
}//cCO_HBProducer::vInit ------------------------------------------------------

bool cCO_HBProducer::bSignalCOStateChanged(const CO_NMT_internalState_t nNewState) {
  xQueueReset(m_xQueueHandle_NMTStateChange);          // clear queue, earlier messages lost their sense
  if (xQueueSendToBack(m_xQueueHandle_NMTStateChange, &nNewState, 0) == errQUEUE_FULL)
    return false;
  else  {
    return true;
  }
}//cCO_HBProducer::SignalCOStateChanged ---------------------------------------

void vCO_HBProducer_Task(void* pvParameters)    {

  sCANMsg                     CO_CanMsg;
  TickType_t                  xLastHBSentTime;
  CO_NMT_internalState_t      nNMTStateSelf = CO_NMT_INITIALIZING;                  // self NMT state
  CO_NMT_internalState_t      nNMTPrevStateSelf = CO_NMT_INITIALIZING;              // previous self NMT state
  uint32_t                    nTickCount;
  uint32_t                    nDelayTime;

  // init message buffer
  CO_CanMsg.m_nStdId = OD_CANNodeID + CO_CAN_ID_HEARTBEAT;
  CO_CanMsg.m_nDLC = 1;

  // send boot-up message
  CO_CanMsg.m_aData[0] = CO_NMT_INITIALIZING;
  if (!oCO_HBProducer.m_pCO_Driver -> bCANSend(CO_CanMsg))   {                                         // queue full ?
    oCO_HBProducer.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN,
        CAN_TX_OVERFLOW_HBPRODUCER);    // report CAN TX overflow
  }
  xLastHBSentTime = xTaskGetTickCount();

  if (OD_producerHeartbeatTime == 0)
    for(;;) vTaskSuspend(NULL);                     // HB producing disabled, suspend this task

  // main part
  for(;;)   {

    if ( nNMTStateSelf == CO_NMT_INITIALIZING )   {
      xQueueReceive(oCO_HBProducer.m_xQueueHandle_NMTStateChange,
              &nNMTStateSelf, portMAX_DELAY);                                       // try to receive message forever
    }
    else {
      // try to receive message with timeout
      nTickCount = xTaskGetTickCount();
      nDelayTime = (OD_producerHeartbeatTime / portTICK_RATE_MS + xLastHBSentTime - nTickCount > 0) ?
          (OD_producerHeartbeatTime / portTICK_RATE_MS + xLastHBSentTime - nTickCount) : 0;
      if (xQueueReceive(oCO_HBProducer.m_xQueueHandle_NMTStateChange, &nNMTStateSelf, nDelayTime) == pdTRUE)   {
        // new state received
        if ( (nNMTPrevStateSelf == CO_NMT_OPERATIONAL) && (nNMTStateSelf != CO_NMT_OPERATIONAL) )   {       // op state lost ?
          // send HB immediately
          CO_CanMsg.m_aData[0] = nNMTStateSelf;
          if (!oCO_HBProducer.m_pCO_Driver -> bCANSend(CO_CanMsg))   {                                         // queue full ?
            oCO_HBProducer.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN,
                CAN_TX_OVERFLOW_HBPRODUCER);    // report CAN TX overflow
          }
          xLastHBSentTime = xTaskGetTickCount();
        }
        nNMTPrevStateSelf = nNMTStateSelf;
      }
      else  {           // timeout expired
        CO_CanMsg.m_aData[0] = nNMTStateSelf;
        // send HB message
        if (!oCO_HBProducer.m_pCO_Driver -> bCANSend(CO_CanMsg))   {                                         // queue full ?
          oCO_HBProducer.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN,
              CAN_TX_OVERFLOW_HBPRODUCER);    // report CAN TX overflow
        }
        xLastHBSentTime = xTaskGetTickCount();
      }
    }

  }//for
}//void vCO_HBProducer_Task ---------------------------------------------------
