/*
 * CO_RX.cpp
 *
 *  Created on: 1 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen CAN receive
 */

/** Includes *****************************************************************/
#include "config.h"

#include "CO_OD.h"
#include "CO_RX.h"

// generate error, if features are not correctly configured for this project
#if CO_NO_SDO_SERVER != 1
#error Features from CO_OD.h file are not correctly configured for this project!
#endif

/** Defines and constants ****************************************************/
/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/

/**
 * Mapping constants
 */
typedef enum  {
  TO_NMT_EMCY        = 0,
  TO_HBCONSUMER,
  TO_SDO_SERVER,
  TO_SDO_MASTER,
  TO_RPDO,
} CANRXMapping_t;

/**
 * Mapping structure
 */
typedef struct  {
  uint16_t          m_nCAN_ID;
  CANRXMapping_t    m_nRedirect;
} CANRXMapItem_t;

/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_RX oCO_RX;

/** Class methods ************************************************************/

CO_ReturnError_t cCO_RX::nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
    cCO_HBConsumer const * const pCO_HBConsumer,
    cCO_SDOserver const * const pCO_SDOserver,
    cCO_RPDO const * const pCO_RPDO,
    cCO_SDOmasterRX const * const pCO_SDOmasterRX)    {

  if (pCO_NMT_EMCY == NULL)     return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_HBConsumer == NULL)   return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_SDOserver == NULL)    return CO_ERROR_ILLEGAL_ARGUMENT;

  m_pCO_NMT_EMCY = (cCO_NMT_EMCY*)pCO_NMT_EMCY;
  m_pCO_HBConsumer = (cCO_HBConsumer*)pCO_HBConsumer;
  m_pCO_SDOserver = (cCO_SDOserver*)pCO_SDOserver;
  m_pCO_RPDO = (cCO_RPDO*)pCO_RPDO;
  m_pCO_SDOmasterRX = (cCO_SDOmasterRX*)pCO_SDOmasterRX;

  return CO_ERROR_NO;

}//cCO_RX::nConfigure ---------------------------------------------------------

void cCO_RX::vInit(void)  {

  TaskHandle_t                xHandle;                            // task handle

  // create CAN receive queue
  m_xQueueHandle_CANReceive = xQueueCreate(50, sizeof(sCANMsg));
  configASSERT( m_xQueueHandle_CANReceive );

  // create task
  if (xTaskCreate( vCO_RX_Task, ( char * ) CO_RX_Task_name, 500, NULL, CO_RX_Task_priority, &xHandle)
      != pdPASS)
    for(;;);
  nAddHandle(xHandle);

  return;
}//cCO_RX::vInit --------------------------------------------------------------

bool cCO_RX::bSignalCANRXFromISR(sCANMsg const * const pCANMsg)  {
  if (xQueueSendToBackFromISR(m_xQueueHandle_CANReceive, pCANMsg, 0) == errQUEUE_FULL)
    return false;
  else
    return true;
}//cCO_RX::SignalCANRXFromISR -------------------------------------------------

/** Exported functions *******************************************************/

void vCO_RX_Task(void* pvParameters)  {

  sCANMsg                         CO_CanMsg;                      // buffer for incoming NMT command and outcoming EMCY message
  uint8_t                         i;
  CANRXMapItem_t                  aCANRX_Map[20];
  uint8_t                         nMapCount = 0;                  // nimber of entries in aCANRX_Map

  /** init CAN mapping ******/
  aCANRX_Map[0].m_nCAN_ID = 0;
  aCANRX_Map[0].m_nRedirect = TO_NMT_EMCY;
  nMapCount = 1;

#ifdef OD_consumerHeartbeatTime                     // HB messages redirection to HB consumer task
  // HB consume
  for (i = 0; i < ODL_consumerHeartbeatTime_arrayLength; i++) {
    if ((OD_consumerHeartbeatTime[i] & 0xFF0000) && ((OD_consumerHeartbeatTime[i] & 0xFFFF)) &&
      ((OD_consumerHeartbeatTime[i] & 0x800000) == 0) ) {     // is channel used ?
      aCANRX_Map[nMapCount].m_nCAN_ID = (uint16_t)(OD_consumerHeartbeatTime[i] >> 16) + CO_CAN_ID_HEARTBEAT;
      aCANRX_Map[nMapCount].m_nRedirect = TO_HBCONSUMER;
      nMapCount++;
    }
  }
#endif

  // SDO server
  for(i = 0; i < N_ELEMENTS(OD_SDOServerParameter); i++)  {             // SDO messages redirection to SDO server task
    aCANRX_Map[nMapCount].m_nCAN_ID = (i == 0 ?
        OD_SDOServerParameter[i].COB_IDClientToServer + OD_CANNodeID : OD_SDOServerParameter[i].COB_IDClientToServer);
    aCANRX_Map[nMapCount].m_nRedirect = TO_SDO_SERVER;
    nMapCount++;
  }

#ifdef OD_SDOClientParameter
  // SDO master
  for(i = 0; i < N_ELEMENTS(OD_SDOClientParameter); i++)  {             // SDO messages redirection to SDO master task
    aCANRX_Map[nMapCount].m_nCAN_ID = OD_SDOClientParameter[i].COB_IDServerToClient;
    aCANRX_Map[nMapCount].m_nRedirect = TO_SDO_MASTER;
    nMapCount++;
  }
#endif

#ifdef OD_RPDOCommunicationParameter
  // RPDO
  for(i = 0; i < N_ELEMENTS(OD_RPDOCommunicationParameter); i++)  {             // RPDO messages redirection to RPDO task
    switch (i)  {
    case 0:
    case 1:
    case 2:
    case 3:
      aCANRX_Map[nMapCount].m_nCAN_ID = OD_RPDOCommunicationParameter[i].COB_IDUsedByRPDO + OD_CANNodeID;
      break;
    default:
      aCANRX_Map[nMapCount].m_nCAN_ID = OD_RPDOCommunicationParameter[i].COB_IDUsedByRPDO;
      break;
    }
    aCANRX_Map[nMapCount].m_nRedirect = TO_RPDO;
    nMapCount++;
  }
#endif

  // main part
  for(;;)   {

    xQueueReceive(oCO_RX.m_xQueueHandle_CANReceive, &CO_CanMsg,
        portMAX_DELAY);                           // try to receive message without timeout (forever)
    i=0;
    while (i<nMapCount) {
      if (CO_CanMsg.m_nStdId == aCANRX_Map[i].m_nCAN_ID)
        break;
      i++;
    }//while

    if (i<nMapCount)  {
      switch (aCANRX_Map[i].m_nRedirect) {
      case TO_NMT_EMCY:
        configASSERT( oCO_RX.m_pCO_NMT_EMCY );
        if ( !oCO_RX.m_pCO_NMT_EMCY -> bSignalCANReceived(&CO_CanMsg) )              // queue full ?
          oCO_RX.m_pCO_NMT_EMCY->
            bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL, CO_EMC_SOFTWARE_INTERNAL,
                QUEUE_FULL_NMTEMCY_1);    // report internal soft error
        break;
      case TO_HBCONSUMER:
        configASSERT( oCO_RX.m_pCO_HBConsumer );
        if ( !oCO_RX.m_pCO_HBConsumer -> bSignalCANReceived(&CO_CanMsg) )              // queue full ?
          oCO_RX.m_pCO_NMT_EMCY->
            bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL, CO_EMC_SOFTWARE_INTERNAL,
                QUEUE_FULL_HB_CONSUMER_2);    // report internal soft error
        break;
      case TO_SDO_SERVER:
        configASSERT( oCO_RX.m_pCO_SDOserver );
        if ( !oCO_RX.m_pCO_SDOserver -> bSignalCANReceived(&CO_CanMsg) )              // queue full ?
          oCO_RX.m_pCO_NMT_EMCY->
            bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL, CO_EMC_SOFTWARE_INTERNAL,
                QUEUE_FULL_SDO_2);    // report internal soft error
        break;
      case TO_SDO_MASTER:
        configASSERT( oCO_RX.m_pCO_SDOmasterRX );
        if ( !oCO_RX.m_pCO_SDOmasterRX -> bSignalCANReceived(&CO_CanMsg) )              // error ?
          oCO_RX.m_pCO_NMT_EMCY->
            bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL, CO_EMC_SOFTWARE_INTERNAL,
                QUEUE_FULL_SDO_CLIENT_1);     // report internal soft error
        break;
      case TO_RPDO:
        configASSERT( oCO_RX.m_pCO_RPDO );
        if ( !oCO_RX.m_pCO_RPDO -> bSignalCANReceived(&CO_CanMsg) )              // queue full ?
          oCO_RX.m_pCO_NMT_EMCY->
            bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL, CO_EMC_SOFTWARE_INTERNAL,
                QUEUE_FULL_RPDO_2);    // report internal soft error
        break;
      }//switch
    }//if (i<nMapCount)

  }//for
}//vCO_RX_Task ----------------------------------------------------------------
