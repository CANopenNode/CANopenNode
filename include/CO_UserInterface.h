/*
 * CO_UserInterface.h
 *
 *  Created on: 12 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      User module interface
 */

#ifndef CO_USERINTERFACE_H_
#define CO_USERINTERFACE_H_

/** Preliminary class declarations *******************************************/
class cUserInterface;

/** Includes *****************************************************************/
#include "CANopen.h"
#include "CO_NMT_EMCY.h"
#include "CO_TPDO.h"
#include "CO_driver.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 * User interface class
 */
class cUserInterface : public cActiveClass   {
protected:

  /** links ******/
  cCO_NMT_EMCY*         m_pCO_NMT_EMCY;                 // pointer to cCO_NMT_EMCY object
  cCO_TPDO*             m_pCO_TPDO;                     // pointer to cCO_TPDO object
  cCO_Driver*           m_pCO_Driver;                   // pointer to cCO_Driver object

  /** OS objects ******/
  SemaphoreHandle_t   m_xBinarySemaphore;                 // binary semaphore handle
  QueueHandle_t       m_xQueueHandle_StateOrCommand;      // state change&command queue handle

public:

  /**
   *  @brief  Configures object links
   *
   *  @param  pCO_NMT_EMCY              Pointer to cCO_NMT_EMCY object
   *  @param  pCO_TPDO                  Pointer to сCO_TPDO object
   *  @param  pCO_Driver                Pointer to cCO_Driver object
   *  @return error state
   */
  CO_ReturnError_t nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
      cCO_TPDO const * const pCO_TPDO, cCO_Driver const * const pCO_Driver);

  /**
   *  @brief  Method signals to task that data object was changed
   *    Gives binary semaphore
   *
   *  @return   none
   */
  void                vSignalDOChanged(void);

  /**
   *  @brief  Method signals to task that NMT state was changed or command received
   *    Tries to place a message to StateOrCommand queue without timeout
   *  @param    nNewState                     new state
   *  @return   true if success, false if queue is full
   */
  bool                bSignalStateOrCommand(const CO_NMT_command_t nNewState);
};//cUserInterface ------------------------------------------------------------

/** Exported variables and objects *******************************************/
/** Exported function prototypes *********************************************/

/**
 * @brief   User task function
 * @param   parameters              none
 * @retval  None
 */
void vUser_Task(void* pvParameters);

#endif /* CO_USERINTERFACE_H_ */
