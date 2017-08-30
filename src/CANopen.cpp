/*
 * CANopen.cpp
 *
 *  Created on: 7 Nov 2016 г.
 *      Author: a.smirnov
 */

/** Includes *****************************************************************/
#include "config.h"

#include "CANopen.h"
#include "CO_OD.h"          // Object Dictionary declared there as external
#include "CO_EMCYSend.h"
#include "CO_RX.h"
#include "CO_SDOmaster.h"

#include "USER.h"

/** Defines and constants ****************************************************/
/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/
/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_Main oCO_Main;

/** Class methods ************************************************************/

bool cActiveClass_CO_CAN_NMTdepended::bSignalCANReceived(const sCANMsg* pCO_CanMsg)   {
  if (xQueueSendToBack(m_xQueueHandle_CANReceive, pCO_CanMsg, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive(m_xBinarySemaphore);               // give the semaphore
    return true;
  }
}//cActiveClass_CO_CAN_NMTdepended::bSignalCANReceived ------------------------

bool cActiveClass_CO_CAN_NMTdepended::bSignalCOStateChanged(const CO_NMT_internalState_t nNewState)  {

  xQueueReset(m_xQueueHandle_NMTStateChange);          // clear queue, earlier messages lost their sense
  if (xQueueSendToBack(m_xQueueHandle_NMTStateChange, &nNewState,0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive(m_xBinarySemaphore);               // give the semaphore
    return true;
  }
}//cActiveClass_CO_CAN_NMTdepended::SignalCOStateChanged ----------------------

void cActiveClass_CO_CAN_NMTdepended::vInitPartial(void)   {

  // create binary semaphore
  m_xBinarySemaphore = xSemaphoreCreateBinary();
  configASSERT( m_xBinarySemaphore );

  // create CAN receive queue
  m_xQueueHandle_CANReceive = xQueueCreate(50, sizeof(sCANMsg));
  configASSERT( m_xQueueHandle_CANReceive );

  // create CANOpen state change queue
  m_xQueueHandle_NMTStateChange = xQueueCreate(50, sizeof(CO_NMT_internalState_t));
  configASSERT( m_xQueueHandle_NMTStateChange );

  return;
}//cActiveClass_CO_CAN_NMTdepended::vInit -------------------------------------

void cCO_Main::vInit(void)  {

  uint8_t i;

  /** configure associated objects ******/
  oCO_EMCYSend.nConfigure( &oCO_NMT_EMCY, &oCO_Driver );
  oCO_HBConsumer.nConfigure( &oCO_NMT_EMCY );
  oCO_HBProducer.nConfigure( &oCO_NMT_EMCY, &oCO_Driver );
  oCO_NMT_EMCY.nConfigure( &oCO_led, &oCO_HBConsumer, &oCO_HBProducer, &oCO_EMCYSend, &oCO_SDOserver, &oCO_RPDO,
      &oCO_TPDO, &oUSER );
  oCO_RPDO.nConfigure( &oCO_NMT_EMCY, &oUSER, &oCO_OD_Interface);
  oCO_SDOserver.nConfigure( &oCO_NMT_EMCY, &oUSER, &oCO_OD_Interface, &oCO_Driver);
#if CO_NO_SDO_CLIENT > 0
  for(uint8_t i=0; i<CO_NO_SDO_CLIENT; i++)   {
    aoCO_SDOmaster[i].nConfigure( &oCO_NMT_EMCY, &oCO_Driver, OD_SDOClientParameter[i].COB_IDClientToServer );
    oCO_SDOmasterRX.bAddLink(OD_SDOClientParameter[i].COB_IDServerToClient, &aoCO_SDOmaster[i]);
  }
  oCO_RX.nConfigure( &oCO_NMT_EMCY, &oCO_HBConsumer, &oCO_SDOserver, &oCO_RPDO, &oCO_SDOmasterRX);
#else
  oCO_RX.nConfigure( &oCO_NMT_EMCY, &oCO_HBConsumer, &oCO_SDOserver, &oCO_RPDO, NULL);
#endif

  oCO_TPDO.nConfigure( &oCO_NMT_EMCY, &oCO_Driver, &oCO_OD_Interface);
  oCO_Driver.nConfigure( &oCO_RX, &oCO_NMT_EMCY, &xCO_CANFilter);

  /** fillind CAN filter struct ******/
  xCO_CANFilter.nAddCOBID(0x0000);            // NMT

#ifdef OD_SDOServerParameter                // SDO server
  for(i = 0; i < N_ELEMENTS(OD_SDOServerParameter); i++)  {
    if ( ((OD_SDOServerParameter[i].COB_IDClientToServer & 0x80000000) == 0) &&         // SDO valid ?
        ((OD_SDOServerParameter[i].COB_IDServerToClient & 0x80000000) == 0) )  {
      uint16_t   nCOBID = (uint16_t)(i == 0 ?
        OD_SDOServerParameter[i].COB_IDClientToServer + OD_CANNodeID : OD_SDOServerParameter[i].COB_IDClientToServer);
      xCO_CANFilter.nAddCOBID(nCOBID);
    }
  }
#endif

#ifdef OD_SDOClientParameter
  for(i = 0; i < N_ELEMENTS(OD_SDOClientParameter); i++)  {
    if ( ((OD_SDOClientParameter[i].COB_IDClientToServer & 0x80000000) == 0) &&         // SDO valid ?
        ((OD_SDOClientParameter[i].COB_IDServerToClient & 0x80000000) == 0) )  {
      xCO_CANFilter.nAddCOBID(OD_SDOClientParameter[i].COB_IDServerToClient);
    }
  }
#endif

#ifdef OD_consumerHeartbeatTime             // HB consumer
  for (i = 0; i < ODL_consumerHeartbeatTime_arrayLength; i++) {
    if ((OD_consumerHeartbeatTime[i] & 0xFF0000) && ((OD_consumerHeartbeatTime[i] & 0xFFFF)) &&
      ((OD_consumerHeartbeatTime[i] & 0x800000) == 0) ) {     // is channel used ?
      xCO_CANFilter.nAddCOBID( (uint16_t)((OD_consumerHeartbeatTime[i]>>16) & 0xFF) );
    }
  }
#endif

#ifdef OD_RPDOCommunicationParameter        // RPDO
  for(i = 0; i < N_ELEMENTS(OD_RPDOCommunicationParameter); i++)  {
    switch (i)  {
    case 0:
    case 1:
    case 2:
    case 3:
      xCO_CANFilter.nAddCOBID(OD_RPDOCommunicationParameter[i].COB_IDUsedByRPDO + OD_CANNodeID);
      break;
    default:
      xCO_CANFilter.nAddCOBID(OD_RPDOCommunicationParameter[i].COB_IDUsedByRPDO);
      break;
    }
  }
#endif

  /** init associated objects ******/
  oCO_EMCYSend.vInit();
  oCO_HBConsumer.vInit();
  oCO_HBProducer.vInit();
  oCO_led.vInit();
  oCO_NMT_EMCY.vInit();
  oCO_RPDO.vInit();
  oCO_RX.vInit();
  oCO_SDOserver.vInit();
#if CO_NO_SDO_CLIENT > 0
  for(uint8_t i=0; i<CO_NO_SDO_CLIENT; i++)
    aoCO_SDOmaster[i].vInit();
#endif
  oCO_TPDO.vInit();

  /** init and start CO_driver ******/
  oCO_Driver.vInit();
#if CAN_NUM == 1
  oCO_Driver.vStart(CAN_BITRATE_KB, 1);
#endif
#if CAN_NUM == 2
  oCO_Driver.vStart(CAN_BITRATE_KB, 2);
#endif

  return;
}//cCO_Main::vInit ------------------------------------------------------------

void cCO_Main::vSuspend(void)   {
  oCO_EMCYSend.vSuspend();
  oCO_HBConsumer.vSuspend();
  oCO_HBProducer.vSuspend();
  oCO_NMT_EMCY.vSuspend();
  oCO_RPDO.vSuspend();
  oCO_RX.vSuspend();
  oCO_SDOserver.vSuspend();
#if CO_NO_SDO_CLIENT > 0
  for(uint8_t i=0; i<CO_NO_SDO_CLIENT; i++)
    aoCO_SDOmaster[i].vSuspend();
#endif
  oCO_TPDO.vSuspend();
  oCO_Driver.vSuspend();
  return;
}//cCO_Main::vSuspend ---------------------------------------------------------

void cCO_Main::vResume(void)    {
  oCO_EMCYSend.vResume();
  oCO_HBConsumer.vResume();
  oCO_HBProducer.vResume();
  oCO_NMT_EMCY.vResume();
  oCO_RPDO.vResume();
  oCO_RX.vResume();
  oCO_SDOserver.vResume();
#if CO_NO_SDO_CLIENT > 0
  for(uint8_t i=0; i<CO_NO_SDO_CLIENT; i++)
    aoCO_SDOmaster[i].vResume();
#endif
  oCO_TPDO.vResume();
  oCO_Driver.vResume();
  return;
}//cCO_Main::vResume ----------------------------------------------------------

/** Exported functions *******************************************************/

uint32_t CO_getUint32(const uint8_t data[]) {
    CO_bytes_t b;
    b.u8[0] = data[0];
    b.u8[1] = data[1];
    b.u8[2] = data[2];
    b.u8[3] = data[3];
    return b.u32[0];
}

void CO_setUint32(uint8_t data[], const uint32_t value) {
    CO_bytes_t b;
    b.u32[0] = value;
    data[0] = b.u8[0];
    data[1] = b.u8[1];
    data[2] = b.u8[2];
    data[3] = b.u8[3];
}
