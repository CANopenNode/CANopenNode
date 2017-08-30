/*
 * CO_led.cpp
 *
 *  Created on: 23 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen led functionality (CiA DR-303-3)
 */

/** Includes *****************************************************************/
#include <stdint.h>         // for 'int8_t' to 'uint64_t'
#include "config.h"

#include "led.h"
#include "signal.h"
#include "CANopen.h"
#include "CO_led.h"

/** Defines and constants ****************************************************/
#define   TMR_TASK_INTERVAL_MS    50          // Interval of thread in ms

// Bit definition for State & error bitmap
#define   ERRST_STATE_BITS              ((uint8_t) 0x03)          // state bits
#define   ERRST_STATE_INITIALIZING      ((uint8_t) 0x00)          // INITIALIZING
#define   ERRST_STATE_PRE_OPERATIONAL   ((uint8_t) 0x01)          // PRE_OPERATIONAL
#define   ERRST_STATE_OPERATIONAL       ((uint8_t) 0x02)          // OPERATIONAL
#define   ERRST_STATE_STOPPED           ((uint8_t) 0x03)          // STOPPED

#define   ERRST_ERR_BITS                ((uint8_t) 0x3C)          // error bits
#define   ERRST_CAN_WARN                ( 1 << 2 )                // CAN warning bit
#define   ERRST_NMTHB_ERR               ( 1 << 3 )                // NMT or Heartbeat consumer error
#define   ERRST_SYNC_ERR                ( 1 << 4 )                // sync error
#define   ERRST_CAN_ERR                 ( 1 << 5 )                // CAN bus off bit
#define   ERRST_ABLSS                   ( 1 << 6 )                // Auto Baud or LSS in progress

/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/
/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_led oCO_led;

/** Class methods ************************************************************/

void cCO_led::vInit(void)   {

  TaskHandle_t                xHandle;                            // task handle

  m_xQueueHandle_StateChange = xQueueCreate(50, sizeof(CO_ledCommand_t));
  configASSERT( m_xQueueHandle_StateChange );

  if (xTaskCreate( vCO_led_Task, ( char * ) CO_led_Task_name, 200, NULL, CO_led_Task_priority, &xHandle) != pdPASS)
    for(;;);
  nAddHandle(xHandle);

  LED_Init();
  return;
}//cCO_led::vInit -------------------------------------------------------------

bool cCO_led::bSignalCOStateChanged(const CO_ledCommand_t nNewState)   {
  xQueueReset(m_xQueueHandle_StateChange);          // clear queue, earlier messages lost their sense
  if (xQueueSendToBack(m_xQueueHandle_StateChange, &nNewState, 0) == errQUEUE_FULL)
    return false;
  else  {
    return true;
  }
}//cCO_led::bSignalCOStateChanged ---------------------------------------------

/** Exported functions *******************************************************/

void vCO_led_Task(void* pvParameters)   {

  /**
   * State & error bitmap
   *
   * bit [0,1]  - CAN device state, 0..3 from @ref CO_ledCommand_t
   * bit 2      - CAN warning
   * bit 3      - NMT or Heartbeat consumer error
   * bit 4      - sync error
   * bit 5      - Auto Baud or LSS in progress
   */
  uint8_t m_nErrorFlags = 0;

  cSignal               oLed;
  TickType_t            xLastWakeTime = xTaskGetTickCount();
  CO_ledCommand_t       nCommand;                                           // received command

  for(;;) {

    // try to receive command and update state & error bitmap
    while ( xQueueReceive(oCO_led.m_xQueueHandle_StateChange, &nCommand, 0) == pdTRUE ) {
      switch (nCommand) {

      case CO_LED_COMMAND_NONE:
        break;

      case CO_GREEN_INITIALIZING:
        m_nErrorFlags &= ~ERRST_STATE_BITS;
        m_nErrorFlags |= ERRST_STATE_INITIALIZING;
        break;
      case CO_GREEN_PRE_OPERATIONAL:
        m_nErrorFlags &= ~ERRST_STATE_BITS;
        m_nErrorFlags |= ERRST_STATE_PRE_OPERATIONAL;
        break;
      case CO_GREEN_OPERATIONAL:
        m_nErrorFlags &= ~ERRST_STATE_BITS;
        m_nErrorFlags |= ERRST_STATE_OPERATIONAL;
        break;
      case CO_GREEN_STOPPED:
        m_nErrorFlags &= ~ERRST_STATE_BITS;
        m_nErrorFlags |= ERRST_STATE_STOPPED;
        break;

      case CO_RED_NO_ERROR:
        m_nErrorFlags &= ~ERRST_ERR_BITS;
        break;
      case CO_RED_CAN_WARNING_ON:
        m_nErrorFlags |= ERRST_CAN_WARN;
        break;
      case CO_RED_CAN_WARNING_OFF:
        m_nErrorFlags &= ~ERRST_CAN_WARN;
        break;
      case CO_RED_NMTHB_ERROR_ON:
        m_nErrorFlags |= ERRST_NMTHB_ERR;
        break;
      case CO_RED_NMTHB_ERROR_OFF:
        m_nErrorFlags &= ~ERRST_NMTHB_ERR;
        break;
      case CO_RED_SYNC_ERROR_ON:
        m_nErrorFlags |= ERRST_SYNC_ERR;
        break;
      case CO_RED_SYNC_ERROR_OFF:
        m_nErrorFlags &= ~ERRST_SYNC_ERR;
        break;
      case CO_RED_CAN_ERROR_ON:
        m_nErrorFlags |= ERRST_CAN_ERR;
        break;
      case CO_RED_CAN_ERROR_OFF:
        m_nErrorFlags &= ~ERRST_CAN_ERR;
        break;

      case CO_AB_LSS_ON:
        m_nErrorFlags |= ERRST_ABLSS;
        break;
      case CO_AB_LSS_OFF:
        m_nErrorFlags &= ~ERRST_ABLSS;
        break;

      }//switch (nCommand)
    }//while

    // processing state
    if (m_nErrorFlags & ERRST_ABLSS)  {                // Auto Baud or LSS in progress ?
      if (oLed.bSequenceState(SIGNAL_FLICKERING))
        GreenLEDon();
      else
        GreenLEDoff();
    }
    else  {
      switch (m_nErrorFlags & ERRST_STATE_BITS)   {
      case ERRST_STATE_INITIALIZING:
        GreenLEDoff();
        break;
      case ERRST_STATE_PRE_OPERATIONAL:
        if (oLed.bSequenceState(SIGNAL_BLINKING))
          GreenLEDon();
        else
          GreenLEDoff();
        break;
      case ERRST_STATE_OPERATIONAL:
        GreenLEDon();
        break;
      case ERRST_STATE_STOPPED:
        if (oLed.bSequenceState(SIGNAL_SINGLE_FLASH))
          GreenLEDon();
        else
          GreenLEDoff();
        break;
      }//switch (m_nErrorFlags & ERRST_STATE_BITS)
    }//if (m_nErrorFlags & ERRST_ABLSS)

    // processing errors
    if ( (m_nErrorFlags & ERRST_ERR_BITS) == 0 )   {       // no errors ?
      if (m_nErrorFlags & ERRST_ABLSS)  {                // Auto Baud or LSS in progress ?
        if (oLed.bSequenceState(SIGNAL_FLICKERING))
          RedLEDon();
        else
          RedLEDoff();
      }
      else
        RedLEDoff();
    }
    else  {
      if (m_nErrorFlags & ERRST_CAN_ERR)    {
        RedLEDon();
        break;
      }
      else if (m_nErrorFlags & ERRST_SYNC_ERR)   {
        if (oLed.bSequenceState(SIGNAL_TRIPLE_FLASH))
          RedLEDon();
        else
          RedLEDoff();
      }
      else if (m_nErrorFlags & ERRST_NMTHB_ERR)   {
        if (oLed.bSequenceState(SIGNAL_DOUBLE_FLASH))
          RedLEDon();
        else
          RedLEDoff();
      }
      else if (m_nErrorFlags & ERRST_CAN_WARN)   {
        if (oLed.bSequenceState(SIGNAL_SINGLE_FLASH))
          RedLEDon();
        else
          RedLEDoff();
      }
    }// if ( (m_nErrorFlags & ERRST_ERR_BITS) == 0 )

    vTaskDelayUntil( &xLastWakeTime, ( TMR_TASK_INTERVAL_MS / portTICK_RATE_MS  ) );
    oLed.vProcess50ms();          // update
  }//for

}//vCO_led_Task ---------------------------------------------------------------
