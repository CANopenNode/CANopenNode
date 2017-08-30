/*
 * CO_led.h
 *
 *  Created on: 23 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen led functionality (CiA DR-303-3)
 */

#ifndef CO_LED_H_
#define CO_LED_H_

/** Preliminary class declarations *******************************************/
class cCO_led;

/** Includes *****************************************************************/
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "active_class.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 * Commands to led control task
 */
typedef enum  {

  CO_LED_COMMAND_NONE             = 0,

  CO_GREEN_INITIALIZING           = 1,          // =1, off (initializing)
  CO_GREEN_PRE_OPERATIONAL,                     // =2, blinking (pre-operational)
  CO_GREEN_OPERATIONAL,                         // =3, on (operational)
  CO_GREEN_STOPPED,                             // =4, single flash (stopped)

  CO_RED_NO_ERROR                 = 10,         // =10, no error
  CO_RED_CAN_WARNING_ON,                        // =11, single flash (CAN warning occurred)
  CO_RED_CAN_WARNING_OFF,                       // =12, CAN warning released
  CO_RED_NMTHB_ERROR_ON,                        // =13, double flash (Node guard event or Heartbeat consumer error occured)
  CO_RED_NMTHB_ERROR_OFF,                       // =14, Node guard event or Heartbeat consumer error released
  CO_RED_SYNC_ERROR_ON,                         // =15, tripple flash (sync timeout error occured)
  CO_RED_SYNC_ERROR_OFF,                        // =16, sync timeout error released
  CO_RED_CAN_ERROR_ON,                          // =17, on (CAN bus off occured)
  CO_RED_CAN_ERROR_OFF,                         // =18, CAN bus off released

  CO_AB_LSS_ON                    = 50,         // =50, both leds flickering (Auto Baud or LSS in progress)
  CO_AB_LSS_OFF                                 // =51, Auto Baud or LSS completed

} CO_ledCommand_t;

/**
 * LED control class
 */
class cCO_led : public cActiveClass   {
private:

  QueueHandle_t               m_xQueueHandle_StateChange;         // queue of CO_ledCommand_t, handle

  friend void vCO_led_Task(void* pvParameters);       // task function is a friend

public:

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

  /**
   *  @brief  Method signals to task that state was changed
   *    Tries to place a message to state change queue without timeout
   *  @param    nNewState                     new state
   *  @return   true if success, false if queue is full
   */
  bool bSignalCOStateChanged(const CO_ledCommand_t nNewState);

};//cCO_led -------------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_led oCO_led;

/** Exported function prototypes *********************************************/

/**
 * @brief   LED task function
 * @param   parameters              none
 * @retval  None
 */
void vCO_led_Task(void* pvParameters);

#endif /* CO_LED_H_ */
