/*
 * CO_HBConsumer.cpp
 *
 *  Created on: 29 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      HB Consumer
 */

/** Includes *****************************************************************/
#include "config.h"

#include "CO_OD.h"
#include "CO_HBconsumer.h"

#include "task.h"

/** Defines and constants ****************************************************/
/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/

/**
 * One monitored node
 */
typedef struct {
  uint8_t                     m_dCOB_ID;          /**< ID of the remote node */
  uint16_t                    m_dTime;            /**< Consumer heartbeat time from OD */
  bool                        m_bmonStarted;      /**< True after reception of the first Heartbeat message */
  CO_NMT_internalState_t      m_dNMTstate;        /**< NMT state of the remote node */
  uint32_t                    m_dPrevHBTime_ms;   /**< Previous heartbeat time, ms */
} CO_HBconsNode_t;

/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_HBConsumer oCO_HBConsumer;

/** Class methods ************************************************************/

CO_ReturnError_t cCO_HBConsumer::nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY)  {
  if (pCO_NMT_EMCY == NULL)   return CO_ERROR_ILLEGAL_ARGUMENT;
  m_pCO_NMT_EMCY = (cCO_NMT_EMCY*)pCO_NMT_EMCY;
  return CO_ERROR_NO;
}//cCO_HBConsumer::nConfigure -------------------------------------------------

void cCO_HBConsumer::vInit(void)    {

  TaskHandle_t                xHandle;                            // task handle

  vInitPartial();
  if (xTaskCreate( vCO_HBConsumer_Task, ( char* ) CO_HBConsumer_Task_name, 500, NULL,
      CO_HBConsumer_Task_priority, &xHandle) != pdPASS)
    for(;;);
  nAddHandle(xHandle);
  return;
}//cCO_HBConsumer::vInit ------------------------------------------------------

/** Exported functions *******************************************************/

void vCO_HBConsumer_Task(void* pvParameters) {

  CO_HBconsNode_t       aHBcons_monitoredNodes[10];               // monitored nodes array
  uint8_t               nMonitoredNodesCount = 0;

  CO_NMT_internalState_t nNMTStateSelf = CO_NMT_INITIALIZING;  // self NMT state
  CO_NMT_internalState_t nNMTPrevStateSelf = CO_NMT_INITIALIZING; // previous self NMT state
  bool bIsMonitoringNeeded = false;             // flag - need for HB monitoring
  sCANMsg               CO_CanMsg;

  uint32_t  nCurrentTime_ms;        // current time
  int32_t   nTimeDiff_ms;           // time difference
  uint8_t   i,j;

#ifdef OD_consumerHeartbeatTime
  // init monitored nodes array
  for (i = 0; i < ODL_consumerHeartbeatTime_arrayLength; i++) {
    aHBcons_monitoredNodes[nMonitoredNodesCount].m_dCOB_ID = (uint8_t)((OD_consumerHeartbeatTime[i] >> 16) & 0xFF);
    aHBcons_monitoredNodes[nMonitoredNodesCount].m_dTime = (uint16_t)((OD_consumerHeartbeatTime[i]) & 0xFFFF);
    aHBcons_monitoredNodes[nMonitoredNodesCount].m_bmonStarted = false;
    aHBcons_monitoredNodes[nMonitoredNodesCount].m_dNMTstate = CO_NMT_INITIALIZING;
    if ( ((aHBcons_monitoredNodes[nMonitoredNodesCount].m_dCOB_ID & 0x80) == 0) &&
        aHBcons_monitoredNodes[nMonitoredNodesCount].m_dCOB_ID &&
        aHBcons_monitoredNodes[nMonitoredNodesCount].m_dTime)     // is channel used ?
    {
      aHBcons_monitoredNodes[nMonitoredNodesCount].m_dCOB_ID += CO_CAN_ID_HEARTBEAT; // set right ID of expected HB message
      nMonitoredNodesCount++;
      bIsMonitoringNeeded = true;          // at least one node needs monitoring
    }
  }

  // check for there are no several times for one ID
  for (i = 0; i < nMonitoredNodesCount; i++) {
    for (j = i+1; j < nMonitoredNodesCount; j++) {
      if (aHBcons_monitoredNodes[i].m_dCOB_ID == aHBcons_monitoredNodes[j].m_dCOB_ID) {
        oCO_HBConsumer.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_HEARTBEAT_WRONG,
            CO_EMC_HEARTBEAT, aHBcons_monitoredNodes[i].m_dCOB_ID);  // send CO_EM_HEARTBEAT_WRONG
        bIsMonitoringNeeded = false;
      }
    }
  }
#endif

  if (!bIsMonitoringNeeded)   {
    for(;;)   vTaskSuspend (NULL);              // monitoring not needed, suspend this task
  }

  // main part
  for (;;) {

    nCurrentTime_ms = xTaskGetTickCount() / portTICK_RATE_MS; // got current time

    // semaphore processing
    if ((nNMTStateSelf != CO_NMT_PRE_OPERATIONAL)
        && (nNMTStateSelf != CO_NMT_OPERATIONAL)) {
      xSemaphoreTake(oCO_HBConsumer.m_xBinarySemaphore, portMAX_DELAY);         // try to take semaphore forever
    } else {
      // calculating time, which can be used to wait for semaphore
      nTimeDiff_ms = 0x7FFFFFFF;
      for (i = 0; i < nMonitoredNodesCount; i++) { // search for minimum
        if (aHBcons_monitoredNodes[i].m_bmonStarted) {

          int32_t   nCurrTimeDiff = 0;
          nCurrTimeDiff += aHBcons_monitoredNodes[i].m_dPrevHBTime_ms;
          nCurrTimeDiff += aHBcons_monitoredNodes[i].m_dTime;
          nCurrTimeDiff -= nCurrentTime_ms;
          if (nCurrTimeDiff < nTimeDiff_ms) {
            nTimeDiff_ms = aHBcons_monitoredNodes[i].m_dPrevHBTime_ms
                + aHBcons_monitoredNodes[i].m_dTime - nCurrentTime_ms;
          }

        }
      }//for
      if (nTimeDiff_ms == 0x7FFFFFFF) {       // no nodes in active monitoring?
        xSemaphoreTake(oCO_HBConsumer.m_xBinarySemaphore, portMAX_DELAY);       // try to take semaphore forever
      } else if (nTimeDiff_ms >= 0) {
        if (nTimeDiff_ms == 0) nTimeDiff_ms++;
        xSemaphoreTake(oCO_HBConsumer.m_xBinarySemaphore,
            nTimeDiff_ms / portTICK_RATE_MS); // try to take semaphore with calculated timeout
      }
    }//if-else ((nNMTStateSelf != CO_NMT_PRE_OPERATIONAL) && (nNMTStateSelf != CO_NMT_OPERATIONAL))

    // state change processing
    nNMTPrevStateSelf = nNMTStateSelf;                // remember previous state
    while ( xQueueReceive(oCO_HBConsumer.m_xQueueHandle_NMTStateChange,
        &nNMTStateSelf, 0) == pdTRUE);                                                      // got latest state
    if ((nNMTStateSelf == CO_NMT_PRE_OPERATIONAL)
        || (nNMTStateSelf == CO_NMT_OPERATIONAL)) {
      if (((nNMTPrevStateSelf == CO_NMT_INITIALIZING)
          || (nNMTPrevStateSelf == CO_NMT_STOPPED))) {
        // if detected transfer from non-working mode to active mode, reset monitoring
        for (i = 0; i < nMonitoredNodesCount; i++) {
          aHBcons_monitoredNodes[i].m_dNMTstate = CO_NMT_INITIALIZING;
          aHBcons_monitoredNodes[i].m_bmonStarted = false;
        }
      }
    }

    // HB message processing
    while (xQueueReceive(oCO_HBConsumer.m_xQueueHandle_CANReceive, &CO_CanMsg, 0) == pdTRUE) {
      if ((CO_CanMsg.m_nDLC == 1) &&              // proper HB message ?
          ((CO_CanMsg.m_aData[0] == CO_NMT_INITIALIZING)
              || (CO_CanMsg.m_aData[0] == CO_NMT_PRE_OPERATIONAL)
              || (CO_CanMsg.m_aData[0] == CO_NMT_OPERATIONAL)
              || (CO_CanMsg.m_aData[0] == CO_NMT_STOPPED))) {
        if ((nNMTStateSelf == CO_NMT_PRE_OPERATIONAL)
            || (nNMTStateSelf == CO_NMT_PRE_OPERATIONAL)) {     // active mode ?

          for (i = 0; i < nMonitoredNodesCount; i++) {
            if (aHBcons_monitoredNodes[i].m_dCOB_ID == CO_CanMsg.m_nStdId)   {
              if ((CO_CanMsg.m_aData[0] != CO_NMT_INITIALIZING) &&
                  (!aHBcons_monitoredNodes[i].m_bmonStarted) ) {
                // received HB from remote node with state != boot
                aHBcons_monitoredNodes[i].m_bmonStarted = true;
                switch (CO_CanMsg.m_aData[0]) {
                case CO_NMT_PRE_OPERATIONAL:
                  aHBcons_monitoredNodes[i].m_dNMTstate = CO_NMT_PRE_OPERATIONAL;
                  break;
                case CO_NMT_OPERATIONAL:
                  aHBcons_monitoredNodes[i].m_dNMTstate = CO_NMT_OPERATIONAL;
                  break;
                case CO_NMT_STOPPED:
                  aHBcons_monitoredNodes[i].m_dNMTstate = CO_NMT_STOPPED;
                  break;
                }//switch
                aHBcons_monitoredNodes[i].m_dPrevHBTime_ms = nCurrentTime_ms;
              }
              if ((CO_CanMsg.m_aData[0] == CO_NMT_INITIALIZING) &&
                  (aHBcons_monitoredNodes[i].m_bmonStarted) ) {
                // receiving boot-up message from remote after received non-boot-up HB
                aHBcons_monitoredNodes[i].m_dNMTstate = CO_NMT_INITIALIZING;
                aHBcons_monitoredNodes[i].m_bmonStarted = false;
                oCO_HBConsumer.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_HB_CONSUMER_REMOTE_RESET,
                    CO_EMC_HEARTBEAT, CO_CanMsg.m_nStdId);  // send CO_EM_HB_CONSUMER_REMOTE_RESET error
              }
              break;
            }//if (vecHBcons_monitoredNodes[i].m_dCOB_ID == CO_CanMsg.StdId)
          }//for

        }// if ( (nNMTStateSelf ...
      }// if( (CO_CanRxMsg ...
    }//while

    // aHBcons_monitoredNodes processing, try to detect HB timeout
    bool  isHBError = false;
    for (i = 0; i < nMonitoredNodesCount; i++) {
      if (aHBcons_monitoredNodes[i].m_bmonStarted) {
        if (nCurrentTime_ms - aHBcons_monitoredNodes[i].m_dPrevHBTime_ms
            > aHBcons_monitoredNodes[i].m_dTime) {
          // detected HB timeout
          aHBcons_monitoredNodes[i].m_dNMTstate = CO_NMT_INITIALIZING;
          aHBcons_monitoredNodes[i].m_bmonStarted = false;
          oCO_HBConsumer.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_HEARTBEAT_CONSUMER,
              CO_EMC_HEARTBEAT, CO_CanMsg.m_nStdId);  // send CO_EM_HEARTBEAT_CONSUMER error
          isHBError = true;
        }
      }
    }
    if (!isHBError)
      oCO_HBConsumer.m_pCO_NMT_EMCY -> bSignalErrorReleased(CO_EM_HEARTBEAT_CONSUMER,
          CO_EMC_HEARTBEAT);  // send CO_EM_HEARTBEAT_CONSUMER error released

  }//for
}//vCO_HBConsumer_Task --------------------------------------------------------
