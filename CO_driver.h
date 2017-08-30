/*
 * CO_driver.h
 *
 *  Created on: 8 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen CAN driver
 */

#ifndef CO_DRIVER_H
#define CO_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

/** Preliminary class declarations *******************************************/
class cCO_Driver;

/** Includes *****************************************************************/
#include "config.h"
#include "can.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

/** Defines ******************************************************************/

// generate error, if features are not correctly configured for this project
#ifndef UNI_VERSION
#error UNI module version not defined!
#endif
#if (UNI_VERSION != 33) && (UNI_VERSION != 4) && (UNI_VERSION != 400)
#error Unknown UNI module version!
#endif
#ifndef CAN_NUM
#error CAN base adress not defined!
#endif
#if (CAN_NUM != 1) && (CAN_NUM != 2)
#error Illegal CAN number!
#endif

/** Typedef, enumeration, class **********************************************/

/**
 *  CANOpen CAN driver class
 */
class cCO_Driver : public acCANDriver   {
private:

  /**
   * Veryfing CAN module errors
   * Method must be called cyclically with CAN_ERROR_CHECK_PERIOD_MS timeout
   * To not be called from interrupt!
   */
  void vCANVerifyErrors(void) const;

  /**
   *  @brief  Stop CAN
   *
   *  @param    none
   *  @return   none
   */
  void vStop(void);

  friend void vCO_Driver_Task(void* pvParameters);

#if CAN_NUM == 1
  friend void CAN1_TX_IRQHandler(void);
  friend void CAN1_RX0_IRQHandler(void);
  friend void CAN1_SCE_IRQHandler(void);
#endif
#if CAN_NUM == 2
  friend void CAN2_TX_IRQHandler(void);
  friend void CAN2_RX0_IRQHandler(void);
  friend void CAN2_SCE_IRQHandler(void);
#endif

public:

  /**
   *  @brief  Init function. Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

  /**
   *  @brief  Configures hardware, start/reset CAN module and all internal/assosiated OS objects and tasks
   *
   *  @param    nCANbitRateKB             CAN bitrate in kBod.
   *  Valid values are (in kbps): 10, 20, 50, 125, 250, 500, 800, 1000.
   *
   *  @param    nCANNum                   CAN num
   *  @return   error state
   */
  eCANReturn vStart(const uint16_t nCANbitRateKB, const uint8_t nCANNum);

  /**
   *  @brief  Method puts a message to CAN send queue without timeout
   *
   *  @param    xCANMsg                       CAN message
   *  @return   true if success, false if queue is full or CAN is off
   */
  bool bCANSend(sCANMsg xCANMsg);

};//cCO_Driver ----------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern    cCO_Driver        oCO_Driver;
extern    sCANFilter        xCO_CANFilter;

/** Exported function prototypes *********************************************/

/**
 * @brief   CAN driver task function
 * @param   parameters              none
 * @retval  None
 */
void vCO_Driver_Task(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif    // CO_DRIVER_H
