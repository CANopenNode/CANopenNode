/*
 * CO_UserInterface.cpp
 *
 *  Created on: 9 Dec 2016 г.
 *      Author: a.smirnov
 *
 *      User module interface
 */

/** Includes *****************************************************************/
#include "CO_UserInterface.h"

/** Defines ******************************************************************/
/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/
/** Private functions ********************************************************/
/** Exported class methods ***************************************************/

CO_ReturnError_t cUserInterface::nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
      cCO_TPDO const * const pCO_TPDO, cCO_Driver const * const pCO_Driver)   {

  if (pCO_NMT_EMCY == NULL)   return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_TPDO == NULL)       return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_Driver == NULL)     return CO_ERROR_ILLEGAL_ARGUMENT;

  m_pCO_NMT_EMCY = (cCO_NMT_EMCY*)pCO_NMT_EMCY;
  m_pCO_TPDO = (cCO_TPDO*)pCO_TPDO;
  m_pCO_Driver = (cCO_Driver*)pCO_Driver;

  return CO_ERROR_NO;
}//cUserInterface::nConfigure -------------------------------------------------

void cUserInterface::vSignalDOChanged(void)   {
  xSemaphoreGive(m_xBinarySemaphore);
}

bool cUserInterface::bSignalStateOrCommand(const CO_NMT_command_t nNewState)  {
  if (xQueueSendToBack(m_xQueueHandle_StateOrCommand, &nNewState, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive(m_xBinarySemaphore);               // give the semaphore
    return true;
  }
}

/** Variables and objects ****************************************************/
/** Exported functions *******************************************************/
/** Interrupt handlers *******************************************************/
