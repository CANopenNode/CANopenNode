/*
 * CO_TPDO.h
 *
 *  Created on: 6 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen TPDO
 */

#ifndef CO_TPDO_H_
#define CO_TPDO_H_

/** Preliminary class declarations *******************************************/
class cCO_TPDO;

/** Includes *****************************************************************/
#include "CANopen.h"
#include "CO_NMT_EMCY.h"
#include "CO_ODinterface.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 * TPDO class
 */
class cCO_TPDO : public cActiveClass  {
private:

  // links
  cCO_NMT_EMCY*         m_pCO_NMT_EMCY;     /**< pointer to cCO_NMT_EMCY object */
  cCO_Driver*           m_pCO_Driver;       /**< pointer to cCO_Driver object */
  cCO_OD_Interface*     m_pCO_OD_Interface; /**< pointer to cCO_OD_Interface object */

  SemaphoreHandle_t   m_xBinarySemaphore;             // binary semaphore handle
  QueueHandle_t       m_xQueueHandle_DOChange;        // uint32_t queue handle
  QueueHandle_t       m_xQueueHandle_NMTStateChange;  // CANOpen state change queue handle

  friend void vCO_TPDO_Task(void* pvParameters);        // task function is a friend

public:

  /**
   *  @brief  Configures object links
   *
   *  @param  pCO_NMT_EMCY             Pointer to cCO_NMT_EMCY object
   *  @param  pCO_Driver               Pointer to cCO_Driver object
   *  @param  pCO_OD_Interface         Pointer to cCO_OD_Interface object
   *  @return error state
   */
  CO_ReturnError_t nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY, cCO_Driver const * const pCO_Driver,
      cCO_OD_Interface const * const pCO_OD_Interface);

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

  /**
   *  @brief  Method signals to task that data object was changed
   *    Tries to place a message to data object queue without timeout
   *  @param    object                           Index and subindex of object (index << 16 + subindex << 8)
   *  @return   true if success, false if queue is full
   */
  bool bSignalDOChanged(const uint32_t nDO);

  /**
   *  @brief  Method signals to task that NMT state was changed
   *    Tries to place a message to CANOpen state change queue without timeout
   *  @param    nNewState                     new state
   *  @return   true if success, false if queue is full
   */
  bool bSignalCOStateChanged(const CO_NMT_internalState_t nNewState);

};//cCO_TPDO ------------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_TPDO oCO_TPDO;

/** Exported function prototypes *********************************************/

/**
 * @brief   TPDO task function
 * @param   parameters              none
 * @retval  None
 */
void vCO_TPDO_Task(void* pvParameters);

#endif /* CO_TPDO_H_ */
