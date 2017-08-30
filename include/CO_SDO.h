/*
 * CO_SDO.h
 *
 *  Created on: 2 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen SDO server
 *      Block transfer NOT implemented !
 */

#ifndef CO_SDO_H_
#define CO_SDO_H_

/** Preliminary class declarations *******************************************/
class cCO_SDOserver;

/** Includes *****************************************************************/
#include "CANopen.h"
#include "CO_NMT_EMCY.h"
#include "CO_ODinterface.h"
#include "CO_driver.h"
#include "CO_UserInterface.h"

/** Defines ******************************************************************/

/**
 * SDO buffer size.
 *
 * Size of the internal SDO buffer.
 *
 * Size must be at least equal to size of largest variable in Object Dictionary.
 * If data type is domain, data length is not limited to SDO buffer size. If
 * block transfer is implemented, value should be set to 889.
 *
 * Value can be in range from 7 to 889 bytes.
 */
#ifndef CO_SDO_BUFFER_SIZE
#define CO_SDO_BUFFER_SIZE    32
#endif

/** Typedef, enumeration, class **********************************************/

/**
 * SDO server class
 */
class cCO_SDOserver : public cActiveClass_CO_CAN_NMTdepended  {
private:

  // links
  cCO_NMT_EMCY*         m_pCO_NMT_EMCY;     /**< pointer to cCO_NMT_EMCY object */
  cUserInterface*       m_pUserInterface;   /**< pointer to main user object */
  cCO_OD_Interface*     m_pCO_OD_Interface; /**< pointer to cCO_OD_Interface object */
  cCO_Driver*           m_pCO_Driver;       /**< pointer to cCO_Driver object */

  friend void vCO_SDO_Task(void* pvParameters);       // task function is a friend

public:

  /**
   *  @brief  Configures object links
   *
   *  @param  pCO_NMT_EMCY             Pointer to cCO_NMT_EMCY object
   *  @param  pUserInterface           Pointer to main user object
   *  @param  pCO_OD_Interface         Pointer to cCO_OD_Interface object
   *  @param  pCO_Driver               Pointer to cCO_Driver object
   *  @return error state
   */
  CO_ReturnError_t nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY, cUserInterface const * const pUserInterface,
      cCO_OD_Interface const * const pCO_OD_Interface, cCO_Driver const * const pCO_Driver);

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

};//cCO_SDOserver -------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_SDOserver oCO_SDOserver;

/** Exported function prototypes *********************************************/

/**
 * @brief   SDO task function
 * @param   parameters              none
 * @retval  None
 */
void vCO_SDO_Task(void* pvParameters);

#endif /* CO_SDO_H_ */
