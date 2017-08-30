/*
 * CO_EMCYSend.h
 *
 *  Created on: 31 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      EMCY Send
 *
 */

#ifndef CO_EMCYSEND_H_
#define CO_EMCYSEND_H_

/** Preliminary class declarations *******************************************/
class cCO_EMCYSend;

/** Includes *****************************************************************/
#include "CANopen.h"
#include "CO_driver.h"
#include "CO_NMT_EMCY.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 * EMCY send class
 */
class cCO_EMCYSend : public cActiveClass_CO_CAN_NMTdepended   {
private:

  // links
  cCO_NMT_EMCY*         m_pCO_NMT_EMCY;     /**< pointer to cCO_NMT_EMCY object */
  cCO_Driver*           m_pCO_Driver;       /**< pointer to cCO_Driver object */

  friend void vCO_EMCYSend_Task(void* pvParameters);       // task function is a friend

public:

  /**
   *  @brief  Configures object links
   *
   *  @param  pCO_NMT_EMCY             Pointer to cCO_NMT_EMCY object
   *  @param  pCO_Driver               Pointer to cCO_Driver object
   *  @return error state
   */
  CO_ReturnError_t nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
      cCO_Driver const * const pCO_Driver);

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

  /**
   *  @brief  Method signals to task that NMT state was changed
   *    Tries to place a message to CANOpen state change queue without timeout
   *  @param    nNewState                     new state
   *  @return   true if success, false if queue is full
   */
  bool bSignalCOStateChanged(const CO_NMT_internalState_t nNewState);

};//cCO_EMCYSend --------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_EMCYSend oCO_EMCYSend;

/** Exported function prototypes *********************************************/

/**
 * @brief   EMCY send task function
 * @param   parameters              none
 * @retval  None
 */
void vCO_EMCYSend_Task(void* pvParameters);

#endif /* CO_EMCYSEND_H_ */
