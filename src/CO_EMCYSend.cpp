/*
 * CO_EMCYSend.cpp
 *
 *  Created on: 31 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      EMCY Send
 *
 */

/** Includes *****************************************************************/
#include <math.h>             // ceil
#include "config.h"

#include "CO_OD.h"
#include "CO_EMCYSend.h"

#include "task.h"

/** Defines and constants ****************************************************/
#ifndef OD_inhibitTimeEMCY
#define OD_inhibitTimeEMCY    0
#endif

/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/
/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_EMCYSend          oCO_EMCYSend;

/** Class methods ************************************************************/

CO_ReturnError_t cCO_EMCYSend::nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
      cCO_Driver const * const pCO_Driver)    {

  if (pCO_NMT_EMCY == NULL)   return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_Driver == NULL)     return CO_ERROR_ILLEGAL_ARGUMENT;

  m_pCO_NMT_EMCY = (cCO_NMT_EMCY*)pCO_NMT_EMCY;
  m_pCO_Driver = (cCO_Driver*)pCO_Driver;

  return CO_ERROR_NO;

}//cCO_EMCYSend::nConfigure ---------------------------------------------------

void cCO_EMCYSend::vInit(void)    {

  TaskHandle_t                xHandle;                            // task handle

  vInitPartial();
  if (xTaskCreate( vCO_EMCYSend_Task, ( char * ) CO_EMCYSend_Task_name, 500, NULL,
      CO_EMCYSend_Task_priority, &xHandle) != pdPASS)
    for(;;);
  nAddHandle(xHandle);
  return;
}//cCO_EMCYSend::vInit --------------------------------------------------------

bool cCO_EMCYSend::bSignalCOStateChanged(const CO_NMT_internalState_t nNewState) {
  // xQueueReset(xQueueHandle_NMTStateChange);          // clear queue, earlier messages lost their sense
  if (xQueueSendToBack(m_xQueueHandle_NMTStateChange, &nNewState, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive(m_xBinarySemaphore);               // give the semaphore
    return true;
  }
} //cCO_EMCYSend::SignalCOStateChanged ----------------------------------------

/** Exported functions *******************************************************/

void vCO_EMCYSend_Task(void* pvParameters) {

  sCANMsg     CO_CanMsg;
  TickType_t  xLastEMCYSentTime = 0;
  TickType_t  xEMCYInhibitTicks = (OD_inhibitTimeEMCY > 0) ? // getting inhibit time in ticks
      (TickType_t) ceil((float) OD_inhibitTimeEMCY / 10 / portTICK_RATE_MS) : 0;
  CO_NMT_internalState_t      nNMTStateSelf = CO_NMT_INITIALIZING;                  // self NMT state

  for (;;) {

    xSemaphoreTake(oCO_EMCYSend.m_xBinarySemaphore, portMAX_DELAY);         // try to take semaphore forever

    if ((nNMTStateSelf == CO_NMT_PRE_OPERATIONAL) || (nNMTStateSelf == CO_NMT_OPERATIONAL))   {
      // retrieve all EMCY messages
      while (xQueueReceive(oCO_EMCYSend.m_xQueueHandle_CANReceive, &CO_CanMsg, 0) == pdTRUE)   {
        if (xTaskGetTickCount() - xLastEMCYSentTime < xEMCYInhibitTicks)  {               // EMCY inhibit time timeout not expired ?
          vTaskDelay( xEMCYInhibitTicks + xLastEMCYSentTime - xTaskGetTickCount() );        // delay
        }
        // send EMCY message
        if (!oCO_EMCYSend.m_pCO_Driver -> bCANSend(CO_CanMsg))  {                       // queue full ?
          oCO_EMCYSend.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_CAN_TX_OVERFLOW,
              CO_EMC_CAN_OVERRUN, CAN_TX_OVERFLOW_EMCYSEND);    // report CAN TX overflow
        }
        xLastEMCYSentTime = xTaskGetTickCount();
      }//while
    }

    // get state change message
    if (xQueueReceive(oCO_EMCYSend.m_xQueueHandle_NMTStateChange, &nNMTStateSelf, 0) == pdTRUE)   {
      xSemaphoreGive(oCO_EMCYSend.m_xBinarySemaphore);        // signal to re-read state change commands
    }

  }//for
}//vCO_EMCYSend_Task ----------------------------------------------------------
