/*
 * CO_RX.h
 *
 *  Created on: 1 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen CAN receive
 */

#ifndef CO_RX_H_
#define CO_RX_H_

/** Preliminary class declarations *******************************************/
class cCO_RX;

/** Includes *****************************************************************/
#include "CANopen.h"
#include "CO_NMT_EMCY.h"
#include "CO_HBconsumer.h"
#include "CO_SDO.h"
#include "CO_SDOmaster.h"
#include "CO_RPDO.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 * CAN receive class
 */
class cCO_RX : public acCANRX    {
private:

  //links
  cCO_NMT_EMCY*         m_pCO_NMT_EMCY;     /**< pointer to cCO_NMT_EMCY object */
  cCO_HBConsumer*       m_pCO_HBConsumer;   /**< pointer to cCO_HBConsumer object */
  cCO_SDOserver*        m_pCO_SDOserver;    /**< pointer to cSDOserver object */
  cCO_RPDO*             m_pCO_RPDO;         /**< pointer to cCO_RPDO object */
  cCO_SDOmasterRX*      m_pCO_SDOmasterRX;  /**< pointer to cCO_SDOmasterRX object */

  friend void vCO_RX_Task(void* pvParameters);       // task function is a friend

public:

  /**
   *  @brief  Configures object links
   *
   *  @param  pCO_NMT_EMCY             Pointer to cCO_NMT_EMCY object
   *  @param  pCO_HBConsumer           Pointer to cCO_HBConsumer object
   *  @param  pCO_SDOserver            Pointer to cCO_SDOserver object
   *  @param  pCO_RPDO                 Pointer to cCO_RPDO object
   *  @param  pCO_SDOmasterRX          Pointer to cCO_SDOmasterRX object
   *  @return error state
   */
  CO_ReturnError_t nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
      cCO_HBConsumer const * const pCO_HBConsumer,
      cCO_SDOserver const * const pCO_SDOserver,
      cCO_RPDO const * const pCO_RPDO,
      cCO_SDOmasterRX const * const pCO_SDOmasterRX);

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

  /**
   *  @brief  Method signals to task that CAN message been received
   *    Tries to place a message to CAN receive queue without timeout
   *
   *  @param    pCANMsg                     Pointer to message been received
   *  @return   true if success, false otherwise
   *
   *  Function must be called only from interrupt !!!
   */
  bool bSignalCANRXFromISR(sCANMsg const * const pCANMsg);

};//cCO_RX --------------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_RX oCO_RX;

/** Exported function prototypes *********************************************/

/**
 * @brief   CAN RX task function
 * @param   parameters              pointer to cCO_RX_Task_Param_t object
 * @retval  None
 */
void vCO_RX_Task(void* pvParameters);

#endif /* CO_RX_H_ */
