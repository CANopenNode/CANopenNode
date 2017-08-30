/*
 * CO_NMT_EMCY.cpp
 *
 *  Created on: 31 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      NMT & Emergency
 *
 */

/** Includes *****************************************************************/
#include "config.h"

#include "CO_OD.h"
#include "CO_NMT_EMCY.h"

// generate error, if features are not correctly configured for this project
#if ODL_errorStatusBits_stringLength           < 11
#error Features from CO_OD.h file are not correctly configured for this project!
#endif

/** Defines and constants ****************************************************/
/** Private function prototypes **********************************************/

/**
 * Check specific error condition.
 *
 * @param errorBit from @ref CO_EM_errorStatusBits.
 * @return false: Error is not present.
 * @return true: Error is present.
 */
bool bIsError(const uint8_t errorBit);

/** Private typedef, enumeration, class **************************************/
/** Private functions ********************************************************/

bool bIsError(const uint8_t errorBit) {
  uint8_t index = errorBit >> 3;
  uint8_t bitmask = 1 << (errorBit & 0x7);

  if ((index < ODL_errorStatusBits_stringLength)
      && ((OD_errorStatusBits[index] & bitmask) != 0))
    return true;
  else
    return false;
}//bIsError -------------------------------------------------------------------

/** Variables and objects ****************************************************/

cCO_NMT_EMCY oCO_NMT_EMCY;

/** Class methods ************************************************************/

CO_ReturnError_t cCO_NMT_EMCY::nConfigure(cCO_led const * const pCO_led,
    cCO_HBConsumer const * const pCO_HBConsumer,
    cCO_HBProducer const * const pCO_HBProducer,
    cCO_EMCYSend const * const pCO_EMCYSend,
    cCO_SDOserver const * const pCO_SDOserver,
    cCO_RPDO const * const pCO_RPDO,
    cCO_TPDO const * const pCO_TPDO,
    cUserInterface const * const pUserInterface)  {

  if (pCO_led == NULL)          return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_HBConsumer == NULL)   return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_HBProducer == NULL)   return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_EMCYSend == NULL)     return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_SDOserver == NULL)    return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_RPDO == NULL)         return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_TPDO == NULL)         return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pUserInterface == NULL)   return CO_ERROR_ILLEGAL_ARGUMENT;

  m_pCO_led = (cCO_led*)pCO_led;
  m_pCO_HBConsumer = (cCO_HBConsumer*)pCO_HBConsumer;
  m_pCO_HBProducer = (cCO_HBProducer*)pCO_HBProducer;
  m_pCO_EMCYSend = (cCO_EMCYSend*)pCO_EMCYSend;
  m_pCO_SDOserver = (cCO_SDOserver*)pCO_SDOserver;
  m_pCO_RPDO = (cCO_RPDO*)pCO_RPDO;
  m_pCO_TPDO = (cCO_TPDO*)pCO_TPDO;
  m_pUserInterface = (cUserInterface*)pUserInterface;

  return CO_ERROR_NO;
}//cCO_NMT_EMCY::nConfigure ---------------------------------------------------

void cCO_NMT_EMCY::vInit(void)  {

  TaskHandle_t                xHandle;                            // task handle

  m_xBinarySemaphore = xSemaphoreCreateBinary();
  configASSERT( m_xBinarySemaphore );
  m_xQueueHandle_CANReceive = xQueueCreate(50, sizeof(sCANMsg));
  configASSERT( m_xQueueHandle_CANReceive );
  m_xQueueHandle_Errors = xQueueCreate(50, sizeof(CO_Error_t));
  configASSERT( m_xQueueHandle_Errors );

  if (xTaskCreate( vCO_NMT_EMCY_Task, ( char* ) CO_NMT_EMCY_Task_name, 1000, NULL,
      CO_NMT_EMCY_Task_priority, &xHandle) != pdPASS)
    for(;;);
  nAddHandle(xHandle);
  return;
}//cCO_NMT_EMCY::vInit --------------------------------------------------------

bool cCO_NMT_EMCY::bSignalCANReceived(const sCANMsg* pCO_CanMsg)   {
  if (xQueueSendToBack(m_xQueueHandle_CANReceive, pCO_CanMsg, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive(m_xBinarySemaphore);               // give the semaphore
    return true;
  }
}//cCO_NMT_EMCY::bSignalCANReceived -------------------------------------------

bool cCO_NMT_EMCY::bSignalErrorOccured(const uint8_t nErrorBit,
    const uint16_t nErrorCode, const uint32_t nInfoCode) {
  CO_Error_t CO_Error;

  CO_Error.m_nErrorBit = nErrorBit;
  CO_Error.m_nErrorCode = nErrorCode;
  CO_Error.m_nInfoCode = nInfoCode;

  if (xQueueSendToBack(m_xQueueHandle_Errors, &CO_Error, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive(m_xBinarySemaphore);               // give the semaphore
    return true;
  }
}//cCO_NMT_EMCY::bSignalErrorOccured ------------------------------------------

bool cCO_NMT_EMCY::bSignalErrorOccuredFromISR(const uint8_t nErrorBit, const uint16_t nErrorCode,
    const uint32_t nInfoCode) {

  CO_Error_t          CO_Error;
  BaseType_t          xTaskWokenByReceive = pdFALSE;

  CO_Error.m_nErrorBit = nErrorBit;
  CO_Error.m_nErrorCode = nErrorCode;
  CO_Error.m_nInfoCode = nInfoCode;

  if (xQueueSendToBackFromISR(m_xQueueHandle_Errors, &CO_Error, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGiveFromISR(m_xBinarySemaphore, &xTaskWokenByReceive);        // give the semaphore
    return true;
  }
}//cCO_NMT_EMCY::bSignalErrorOccuredFromISR -----------------------------------

bool cCO_NMT_EMCY::bSignalErrorReleased(const uint8_t nErrorBit, const uint32_t nInfoCode) {

  CO_Error_t CO_Error;

  CO_Error.m_nErrorBit = nErrorBit;
  CO_Error.m_nErrorCode = 0;
  CO_Error.m_nInfoCode = nInfoCode;

  if (xQueueSendToBack(m_xQueueHandle_Errors, &CO_Error, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive(m_xBinarySemaphore);               // give the semaphore
    return true;
  }
}//cCO_NMT_EMCY::bSignalErrorReleased -----------------------------------------

bool cCO_NMT_EMCY::bSignalErrorReleasedFromISR(const uint8_t nErrorBit, const uint32_t nInfoCode) {

  CO_Error_t          CO_Error;
  BaseType_t          xTaskWokenByReceive = pdFALSE;

  CO_Error.m_nErrorBit = nErrorBit;
  CO_Error.m_nErrorCode = 0;
  CO_Error.m_nInfoCode = nInfoCode;

  if (xQueueSendToBackFromISR(m_xQueueHandle_Errors, &CO_Error,
      0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGiveFromISR(m_xBinarySemaphore, &xTaskWokenByReceive);        // give the semaphore
    return true;
  }
}//cCO_NMT_EMCY::bSignalErrorReleasedFromISR ----------------------------------

bool cCO_NMT_EMCY::bSignalCANErrorFromISR(const eCANError error, const uint32_t nInfoCode)  {

  bool bResult;

  switch(error)   {
  case CAN_ERROR_BUS_WARNING:          // CAN bus warning limit reached
    bResult = bSignalErrorOccuredFromISR(CO_EM_CAN_BUS_WARNING, CO_EMC_COMMUNICATION, nInfoCode);
    break;
  case CAN_ERROR_RX_BUS_PASSIVE:       // CAN RX bus enter error passive state
    bResult = bSignalErrorOccuredFromISR(CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, nInfoCode);
    break;
  case CAN_ERROR_TX_BUS_PASSIVE:       // CAN TX bus enter error passive state
    bResult = bSignalErrorOccuredFromISR(CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, nInfoCode);
    break;
  case CAN_ERROR_TX_OFF:               // CAN TX off
    bResult = bSignalErrorOccuredFromISR(CO_EM_CAN_TX_BUS_OFF, CO_EMC_COMMUNICATION, nInfoCode);
    break;
  case CAN_ERROR_RX_OVERFLOW:          // CAN hardware RX buffer overflowed
    bResult = bSignalErrorOccuredFromISR(CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, nInfoCode);
    break;
  case CAN_ERROR_RX_FORWARD:           // error during forwarding CAN message to RX object
    bResult = bSignalErrorOccuredFromISR(CO_EM_INT_SOFT_CRITICAL, CO_EMC_SOFTWARE_INTERNAL, QUEUE_CAN_RX_OVERFLOW);
    break;
  }//switch(error)

  return bResult;
}//cCO_NMT_EMCY::bSignalCANErrorFromISR ---------------------------------------

bool cCO_NMT_EMCY::bSignalCANError(const eCANError error, const uint32_t nInfoCode)   {

  bool bResult;

  switch(error)   {
  case CAN_ERROR_BUS_WARNING:          // CAN bus warning limit reached
    bResult = bSignalErrorOccured(CO_EM_CAN_BUS_WARNING, CO_EMC_COMMUNICATION, nInfoCode);
    break;
  case CAN_ERROR_RX_BUS_PASSIVE:       // CAN RX bus enter error passive state
    bResult = bSignalErrorOccured(CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, nInfoCode);
    break;
  case CAN_ERROR_TX_BUS_PASSIVE:       // CAN TX bus enter error passive state
    bResult = bSignalErrorOccured(CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, nInfoCode);
    break;
  case CAN_ERROR_TX_OFF:               // CAN TX off
    bResult = bSignalErrorOccured(CO_EM_CAN_TX_BUS_OFF, CO_EMC_COMMUNICATION, nInfoCode);
    break;
  case CAN_ERROR_RX_OVERFLOW:          // CAN hardware RX buffer overflowed
    bResult = bSignalErrorOccured(CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, nInfoCode);
    break;
  case CAN_ERROR_RX_FORWARD:           // error during forwarding CAN message to RX object
    bResult = bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL, CO_EMC_SOFTWARE_INTERNAL, QUEUE_CAN_RX_OVERFLOW);
    break;
  }//switch(error)

  return bResult;
}//cCO_NMT_EMCY::bSignalCANError ----------------------------------------------

bool cCO_NMT_EMCY::bSignalCANErrorReleased(const eCANError error, const uint32_t nInfoCode)   {

  bool bResult;

  switch(error)   {
  case CAN_ERROR_BUS_WARNING:          // CAN bus warning limit reached
    bResult = bSignalErrorReleased(CO_EM_CAN_BUS_WARNING,  nInfoCode);
    break;
  case CAN_ERROR_RX_BUS_PASSIVE:       // CAN RX bus enter error passive state
    bResult = bSignalErrorReleased(CO_EM_CAN_RX_BUS_PASSIVE, nInfoCode);
    break;
  case CAN_ERROR_TX_BUS_PASSIVE:       // CAN TX bus enter error passive state
    bResult = bSignalErrorReleased(CO_EM_CAN_TX_BUS_PASSIVE, nInfoCode);
    break;
  case CAN_ERROR_TX_OFF:               // CAN TX off
    bResult = bSignalErrorReleased(CO_EM_CAN_TX_BUS_OFF, nInfoCode);
    break;
  case CAN_ERROR_RX_OVERFLOW:          // CAN hardware RX buffer overflowed
    bResult = bSignalErrorReleased(CO_EM_CAN_RXB_OVERFLOW, nInfoCode);
    break;
  case CAN_ERROR_RX_FORWARD:           // error during forwarding CAN message to RX object
    bResult = bSignalErrorReleased(CO_EM_INT_SOFT_CRITICAL, QUEUE_CAN_RX_OVERFLOW);
    break;
  }//switch(error)

  return bResult;
}//cCO_NMT_EMCY::bSignalCANErrorReleased --------------------------------------

/** Exported functions *******************************************************/

void vCO_NMT_EMCY_Task(void* pvParameters) {

  sCANMsg CO_CanMsg; // buffer for incoming NMT command and outcoming EMCY message
  CO_Error_t CO_Error;                         // error message
  CO_ledCommand_t CO_ledCommand;
  CO_NMT_internalState_t nNMTStateSelf = CO_NMT_INITIALIZING;  // self NMT state
  CO_NMT_internalState_t nNMTPrevStateSelf = CO_NMT_INITIALIZING; // previous self NMT state

#ifdef OD_preDefinedErrorField
  uint8_t preDefErrNoOfErrors = 0; /**< Number of active errors in preDefErr */
#endif

  // automatic transfer to pre-operational state
  nNMTStateSelf         = CO_NMT_PRE_OPERATIONAL;
  xSemaphoreGive(oCO_NMT_EMCY.m_xBinarySemaphore);          // self-giving semaphore - signal to execute main loop one time

  for (;;) {

    xSemaphoreTake(oCO_NMT_EMCY.m_xBinarySemaphore, portMAX_DELAY);         // try to take semaphore forever

    // NMT message processing
    while (xQueueReceive(oCO_NMT_EMCY.m_xQueueHandle_CANReceive, &CO_CanMsg, 0) == pdTRUE) {
      if ((CO_CanMsg.m_nDLC == 2)
          && ((CO_CanMsg.m_aData[1] == 0) || (CO_CanMsg.m_aData[1] == OD_CANNodeID))) { // proper NMT message ?

        switch (CO_CanMsg.m_aData[0]) {
        case CO_NMT_ENTER_OPERATIONAL:
          if (OD_errorRegister == 0)
            nNMTStateSelf = CO_NMT_OPERATIONAL;
          break;
        case CO_NMT_ENTER_STOPPED:
          nNMTStateSelf = CO_NMT_STOPPED;
          break;
        case CO_NMT_ENTER_PRE_OPERATIONAL:
          nNMTStateSelf = CO_NMT_PRE_OPERATIONAL;
          break;
        case CO_NMT_RESET_NODE:
          if (!oCO_NMT_EMCY.m_pUserInterface -> bSignalStateOrCommand(CO_NMT_RESET_NODE))          // queue full ?
            oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL,
                CO_EMC_SOFTWARE_INTERNAL, QUEUE_FULL_USER_1);    // report internal soft error
          break;
        case CO_NMT_RESET_COMMUNICATION:
          if (!oCO_NMT_EMCY.m_pUserInterface -> bSignalStateOrCommand(CO_NMT_RESET_COMMUNICATION))          // queue full ?
            oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL,
                CO_EMC_SOFTWARE_INTERNAL, QUEUE_FULL_USER_1);    // report internal soft error
          break;
        } //switch

      } // if( (CO_CanRxMsg.DLC == 2) ...
    } //while ... (NMT message processing)

    // error message processing
    while (xQueueReceive(oCO_NMT_EMCY.m_xQueueHandle_Errors, &CO_Error, 0) == pdTRUE) {

      uint8_t index = CO_Error.m_nErrorBit >> 3;
      uint8_t bitmask = 1 << (CO_Error.m_nErrorBit & 0x7);
      uint8_t* errorStatusBits;
      bool bNeedToSendEmergency = false;       // flag: need to send EMCY to CAN

      if (index >= ODL_errorStatusBits_stringLength) {
        oCO_NMT_EMCY.                //if errorBit value not supported, send emergency 'CO_EM_WRONG_ERROR_REPORT'
        bSignalErrorOccured(CO_EM_WRONG_ERROR_REPORT, CO_EMC_SOFTWARE_INTERNAL, CO_Error.m_nErrorBit);
      } else {

        errorStatusBits = &OD_errorStatusBits[index];
        CO_ledCommand = CO_LED_COMMAND_NONE;
        if (((*errorStatusBits & bitmask) == 0)
            && (CO_Error.m_nErrorCode != 0)) { // error occurence wasn't reported ?
          *errorStatusBits |= bitmask;        // set error bit
          bNeedToSendEmergency = true;
          if (CO_Error.m_nErrorBit == CO_EM_CAN_BUS_WARNING) // CAN warning error occured ?
            CO_ledCommand = CO_RED_CAN_WARNING_ON;
          if ((CO_Error.m_nErrorBit == CO_EM_HEARTBEAT_CONSUMER) || // Node guard event or Heartbeat consumer error occured ?
              (CO_Error.m_nErrorBit == CO_EM_HB_CONSUMER_REMOTE_RESET))
            CO_ledCommand = CO_RED_NMTHB_ERROR_ON;
          if (CO_Error.m_nErrorBit == CO_EM_SYNC_TIME_OUT) // sync timeout error occured ?
            CO_ledCommand = CO_RED_SYNC_ERROR_ON;
          if (CO_Error.m_nErrorBit == CO_EM_CAN_TX_BUS_OFF) // CAN bus off occured ?
            CO_ledCommand = CO_RED_CAN_ERROR_ON;
        }
        if (((*errorStatusBits & bitmask) != 0)
            && (CO_Error.m_nErrorCode == 0)) { // error release wasn't reported ?
          *errorStatusBits &= ~bitmask;       // clear error bit
          bNeedToSendEmergency = true;
          if (CO_Error.m_nErrorBit == CO_EM_CAN_BUS_WARNING) // CAN warning error released ?
            CO_ledCommand = CO_RED_CAN_WARNING_OFF;
          if ((CO_Error.m_nErrorBit == CO_EM_HEARTBEAT_CONSUMER) || // Node guard event or Heartbeat consumer error released ?
              (CO_Error.m_nErrorBit == CO_EM_HB_CONSUMER_REMOTE_RESET))
            CO_ledCommand = CO_RED_NMTHB_ERROR_OFF;
          if (CO_Error.m_nErrorBit == CO_EM_SYNC_TIME_OUT) // sync timeout error released ?
            CO_ledCommand = CO_RED_SYNC_ERROR_OFF;
          if (CO_Error.m_nErrorBit == CO_EM_CAN_TX_BUS_OFF) // CAN bus off released ?
            CO_ledCommand = CO_RED_CAN_ERROR_OFF;
        }
        if (CO_ledCommand != CO_LED_COMMAND_NONE) {                    // new info for leds task ?
          if (!oCO_NMT_EMCY.m_pCO_led -> bSignalCOStateChanged(CO_ledCommand)) // queue full ?
            oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_INFO,
                CO_EMC_SOFTWARE_INTERNAL, LED_TASK_QUEUE_OVERFLOW);    // report internal soft error
        }

        // calculating error register
        OD_errorRegister = 0;
        if (OD_errorStatusBits[CO_EM_GENERIC_CRITICAL_BYTE1_INDEX])
          OD_errorRegister |= CO_ERR_REG_GENERIC_ERR;   // set generic error bit
        if (OD_errorStatusBits[CO_EM_COMM_CRITICAL_BYTE1_INDEX]
            || OD_errorStatusBits[CO_EM_COMM_CRITICAL_BYTE2_INDEX])
          OD_errorRegister |= CO_ERR_REG_COMM_ERR; // set communication error bit
        if (OD_errorStatusBits[CO_EM_INT_MANUF_CRITICAL_BYTE1_INDEX])
          OD_errorRegister |= CO_ERR_REG_MANUFACTURER_CRIT;   // set manufacturer critical error bit
        if (OD_errorStatusBits[CO_EM_INT_MANUF_OTHER_BYTE1_INDEX])
          OD_errorRegister |= CO_ERR_REG_MANUFACTURER_OTHER;  // set manufacturer other error bit
        if (OD_errorStatusBits[CO_EM_INT_MANUF_INFO_BYTE1_INDEX])
          OD_errorRegister |= CO_ERR_REG_MANUFACTURER_INFO;   // set manufacturer infp error bit

        // send emergence message to EMCYSend task and write 'pre-defined error field' (object dictionary index 0x1003)
        if (bNeedToSendEmergency) {
#ifdef OD_COB_ID_EMCY
          CO_CanMsg.m_nStdId = OD_COB_ID_EMCY + OD_CANNodeID;
#else
          CO_CanMsg.m_nStdId = CO_CAN_ID_EMERGENCY + OD_CANNodeID;
#endif
          CO_CanMsg.m_aData[0] = (uint8_t)(CO_Error.m_nErrorCode & 0xFF); // lower byte
          CO_CanMsg.m_aData[1] = (uint8_t)(CO_Error.m_nErrorCode >> 8); // upper byte
          CO_CanMsg.m_aData[2] = OD_errorRegister;
          CO_CanMsg.m_aData[3] = (uint8_t)(CO_Error.m_nInfoCode & 0xFF); // 1st byte
          CO_CanMsg.m_aData[4] = (uint8_t)((CO_Error.m_nInfoCode >> 8) & 0xFF); // 2nd byte
          CO_CanMsg.m_aData[5] = (uint8_t)((CO_Error.m_nInfoCode >> 16) & 0xFF); // 3d byte
          CO_CanMsg.m_aData[6] = (uint8_t)((CO_Error.m_nInfoCode >> 24) & 0xFF); // 4h byte
          CO_CanMsg.m_aData[7] = 0;
          CO_CanMsg.m_nDLC = 8;
          if (!oCO_NMT_EMCY.m_pCO_EMCYSend -> bSignalCANReceived(&CO_CanMsg))          // EMCYSend queue full?
            oCO_NMT_EMCY.bSignalErrorOccured(
                CO_EM_EMERGENCY_BUFFER_FULL, CO_EMC_SOFTWARE_INTERNAL, 1); // report internal soft error

#ifdef OD_preDefinedErrorField                                              // store error history to OD #1003
          if (preDefErrNoOfErrors < ODL_preDefinedErrorField_arrayLength - 1)
            preDefErrNoOfErrors++;
          for (uint8_t i = preDefErrNoOfErrors - 1; i > 1; i--)
            OD_preDefinedErrorField[i] = OD_preDefinedErrorField[i-1];
          OD_preDefinedErrorField[1] = CO_Error.m_nInfoCode << 16 | CO_Error.m_nErrorCode;
          OD_preDefinedErrorField[0] = preDefErrNoOfErrors;
#endif//ifdef OD_preDefinedErrorField

        }//if (bNeedToSendEmergency)
      }//if (index >= ODL_errorStatusBits_stringLength)
    }//while ... (error message processing)

    // error behaviour
#ifdef ODA_errorBehavior_communication
    {
      bool bEnterPreOpRequested = false;        // flag to enter pre-operational
      bool bEnterStopRequested = false;                // flag to enter stop

      #ifdef ODA_errorBehavior_communication
      if (OD_errorRegister & CO_ERR_REG_COMM_ERR) {
        switch (OD_errorBehavior[ODA_errorBehavior_communication]) {
        case 0:
          bEnterPreOpRequested = true;
          break;
        case 2:
          bEnterStopRequested = true;
          break;
        }
      }
#endif //#ifdef ODA_errorBehavior_communication

#ifdef ODA_errorBehavior_communicationPassive
      if (bIsError(CO_EM_CAN_RX_BUS_PASSIVE)
          || bIsError(CO_EM_CAN_TX_BUS_PASSIVE)) {
        switch (OD_errorBehavior[ODA_errorBehavior_communicationPassive]) {
        case 0:
          bEnterPreOpRequested = true;
          break;
        case 2:
          bEnterStopRequested = true;
          break;
        }
      }
#endif//#ifdef ODA_errorBehavior_communicationPassive

#ifdef ODA_errorBehavior_generic
      if (OD_errorRegister & CO_ERR_REG_GENERIC_ERR) {
        switch (OD_errorBehavior[ODA_errorBehavior_generic]) {
        case 0:
          bEnterPreOpRequested = true;
          break;
        case 2:
          bEnterStopRequested = true;
          break;
        }
      }
#endif //#ifdef ODA_errorBehavior_generic

#ifdef ODA_errorBehavior_manufacturerOther
      if (OD_errorRegister & CO_ERR_REG_MANUFACTURER_OTHER) {
        switch (OD_errorBehavior[ODA_errorBehavior_manufacturerOther]) {
        case 0:
          bEnterPreOpRequested = true;
          break;
        case 2:
          bEnterStopRequested = true;
          break;
        }
      }
#endif //#ifdef ODA_errorBehavior_deviceProfile

#ifdef ODA_errorBehavior_manufacturerCritical
      if (OD_errorRegister & CO_ERR_REG_MANUFACTURER_CRIT) {
        switch (OD_errorBehavior[ODA_errorBehavior_manufacturerCritical]) {
        case 0:
          bEnterPreOpRequested = true;
          break;
        case 2:
          bEnterStopRequested = true;
          break;
        }
      }
#endif //#ifdef ODA_errorBehavior_manufacturerCritical

#ifdef ODA_errorBehavior_manufacturerInfo
      if (OD_errorRegister & CO_ERR_REG_MANUFACTURER_INFO) {
        switch (OD_errorBehavior[ODA_errorBehavior_manufacturerInfo]) {
        case 0:
          bEnterPreOpRequested = true;
          break;
        case 2:
          bEnterStopRequested = true;
          break;
        }
      }
#endif//#ifdef ODA_errorBehavior_manufacturerInfo

      // state change
      if ((nNMTStateSelf == CO_NMT_OPERATIONAL) && (bEnterPreOpRequested))
        nNMTStateSelf = CO_NMT_PRE_OPERATIONAL;
      if (bEnterStopRequested)
        nNMTStateSelf = CO_NMT_STOPPED;
    }
    #endif//ifdef OD_errorBehavior

    // state change signalling
    if (nNMTStateSelf != nNMTPrevStateSelf) { // state changed (need for signalling) ?

      // signal to leds
      CO_ledCommand = CO_LED_COMMAND_NONE;
      switch (nNMTStateSelf) {
      case CO_NMT_INITIALIZING:
        break;
      case CO_NMT_OPERATIONAL:
        CO_ledCommand = CO_GREEN_OPERATIONAL;
        break;
      case CO_NMT_STOPPED:
        CO_ledCommand = CO_GREEN_STOPPED;
        break;
      case CO_NMT_PRE_OPERATIONAL:
        CO_ledCommand = CO_GREEN_PRE_OPERATIONAL;
        break;
      }
      if (CO_ledCommand != CO_LED_COMMAND_NONE) {
        if (!oCO_NMT_EMCY.m_pCO_led -> bSignalCOStateChanged(CO_ledCommand)) // queue full ?
          oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_INFO,
              CO_EMC_SOFTWARE_INTERNAL, LED_TASK_QUEUE_OVERFLOW);    // report internal soft error
      }

      // signals to FreeRTOS system tasks
      if (!oCO_NMT_EMCY.m_pCO_HBConsumer -> bSignalCOStateChanged(nNMTStateSelf)) // queue full ?
        oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL,
            CO_EMC_SOFTWARE_INTERNAL, QUEUE_FULL_HB_CONSUMER_1);    // report internal soft error
      if (!oCO_NMT_EMCY.m_pCO_HBProducer -> bSignalCOStateChanged(nNMTStateSelf)) // queue full ?
        oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL,
            CO_EMC_SOFTWARE_INTERNAL, QUEUE_FULL_HB_PRODUCER_1);    // report internal soft error
      if (!oCO_NMT_EMCY.m_pCO_EMCYSend -> bSignalCOStateChanged(nNMTStateSelf)) // queue full ?
        oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL,
            CO_EMC_SOFTWARE_INTERNAL, QUEUE_FULL_EMCYSEND_1);    // report internal soft error
      if (!oCO_NMT_EMCY.m_pCO_SDOserver -> bSignalCOStateChanged(nNMTStateSelf)) // queue full ?
        oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL,
            CO_EMC_SOFTWARE_INTERNAL, QUEUE_FULL_SDO_1);    // report internal soft error
      if (!oCO_NMT_EMCY.m_pCO_RPDO -> bSignalCOStateChanged(nNMTStateSelf)) // queue full ?
        oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL,
            CO_EMC_SOFTWARE_INTERNAL, QUEUE_FULL_RPDO_1);    // report internal soft error
      if (!oCO_NMT_EMCY.m_pCO_TPDO -> bSignalCOStateChanged(nNMTStateSelf)) // queue full ?
        oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL,
            CO_EMC_SOFTWARE_INTERNAL, QUEUE_FULL_TPDO_1);    // report internal soft error

      // signal to user task
      CO_NMT_command_t        nSignalToUserTask = CO_NMT_RESET_NODE;
      switch (nNMTStateSelf)  {
      case CO_NMT_INITIALIZING:
        break;
      case CO_NMT_PRE_OPERATIONAL:
        nSignalToUserTask = CO_NMT_ENTER_PRE_OPERATIONAL;
        break;
      case CO_NMT_OPERATIONAL:
        nSignalToUserTask = CO_NMT_ENTER_OPERATIONAL;
        break;
      case CO_NMT_STOPPED:
        nSignalToUserTask = CO_NMT_ENTER_STOPPED;
        break;
      }
      if (!oCO_NMT_EMCY.m_pUserInterface -> bSignalStateOrCommand(nSignalToUserTask))          // queue full ?
        oCO_NMT_EMCY.bSignalErrorOccured(CO_EM_INT_SOFT_CRITICAL,
            CO_EMC_SOFTWARE_INTERNAL, QUEUE_FULL_USER_1);    // report internal soft error

      nNMTPrevStateSelf = nNMTStateSelf;
    }//if (nNMTStateSelf != nNMTPrevStateSelf)
  }//for
}//vCO_NMT_EMCY_Task ----------------------------------------------------------
