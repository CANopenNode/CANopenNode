/*
 * CO_HBproducer.h
 *
 *  Created on: 30 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      HB Producer
 */

#ifndef CO_HBPRODUCER_H_
#define CO_HBPRODUCER_H_

/** Preliminary class declarations *******************************************/
class cCO_HBProducer;

/** Includes *****************************************************************/
#include "CANopen.h"
#include "CO_driver.h"
#include "CO_NMT_EMCY.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 * HB Producer class
 */
class cCO_HBProducer : public cActiveClass    {
private:

  // links
  cCO_NMT_EMCY*         m_pCO_NMT_EMCY;     /**< pointer to cCO_NMT_EMCY object */
  cCO_Driver*           m_pCO_Driver;       /**< pointer to cCO_Driver object */

  QueueHandle_t       m_xQueueHandle_NMTStateChange;  // CANOpen state change queue handle

  friend void vCO_HBProducer_Task(void* pvParameters);       // task function is a friend

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

};//cCO_HBProducer ------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_HBProducer oCO_HBProducer;

/** Exported function prototypes *********************************************/

/**
 * @brief   HB producer task function
 * @param   parameters              none
 * @retval  None
 */
void vCO_HBProducer_Task(void* pvParameters);

#endif /* CO_HBPRODUCER_H_ */
