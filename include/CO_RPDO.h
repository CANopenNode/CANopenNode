/*
 * CO_RPDO.h
 *
 *  Created on: 5 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen RPDO
 */

#ifndef CO_RPDO_H_
#define CO_RPDO_H_

/** Preliminary class declarations *******************************************/
class cCO_RPDO;

/** Includes *****************************************************************/
#include "CANopen.h"
#include "CO_NMT_EMCY.h"
#include "CO_ODinterface.h"
#include "CO_UserInterface.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 * RPDO class
 */
class cCO_RPDO : public cActiveClass_CO_CAN_NMTdepended   {
private:

  // links
  cCO_NMT_EMCY*         m_pCO_NMT_EMCY;     /**< pointer to cCO_NMT_EMCY object */
  cUserInterface*       m_pUserInterface;   /**< pointer to main user object */
  cCO_OD_Interface*     m_pCO_OD_Interface; /**< pointer to cCO_OD_Interface object */

  friend void vCO_RPDO_Task(void* pvParameters);       // task function is a friend

public:

  /**
   *  @brief  Configures object links
   *
   *  @param  pCO_NMT_EMCY             Pointer to cCO_NMT_EMCY object
   *  @param  pUserInterface           Pointer to main user object
   *  @param  pCO_OD_Interface         Pointer to cCO_OD_Interface object
   *  @return error state
   */
  CO_ReturnError_t nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY, cUserInterface const * const pUserInterface,
      cCO_OD_Interface const * const pCO_OD_Interface);

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

};//cCO_RPDO ------------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_RPDO oCO_RPDO;

/** Exported function prototypes *********************************************/

/**
 * @brief   RPDO task function
 * @param   parameters              none
 * @retval  None
 */
void vCO_RPDO_Task(void* pvParameters);

#endif /* CO_RPDO_H_ */
