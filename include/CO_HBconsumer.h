/*
 * CO_HBConsumer.h
 *
 *  Created on: 29 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      HB Consumer
 */

#ifndef CO_HBCONSUMER_H_
#define CO_HBCONSUMER_H_

/** Preliminary class declarations *******************************************/
class cCO_HBConsumer;

/** Includes *****************************************************************/
#include "CANopen.h"
#include "CO_NMT_EMCY.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 * HB Consumer class
 */
class cCO_HBConsumer : public cActiveClass_CO_CAN_NMTdepended   {
private:

  // links
  cCO_NMT_EMCY*         m_pCO_NMT_EMCY;     /**< pointer to cCO_NMT_EMCY object */

  friend void vCO_HBConsumer_Task(void* pvParameters);       // task function is a friend

public:

  /**
   *  @brief  Configures object links
   *
   *  @param  pCO_NMT_EMCY             Pointer to cCO_NMT_EMCY object
   *  @return error state
   */
  CO_ReturnError_t nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY);

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

};//cCO_HBConsumer ------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_HBConsumer oCO_HBConsumer;

/** Exported function prototypes *********************************************/

/**
 * @brief   HB consumer task function
 * @param   parameters              none
 * @retval  None
 */
void vCO_HBConsumer_Task(void* pvParameters);

#endif /* CO_HBCONSUMER_H_ */
