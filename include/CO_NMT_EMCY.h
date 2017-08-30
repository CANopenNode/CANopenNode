/*
 * CO_NMT_EMCY.h
 *
 *  Created on: 31 Aug 2016 г.
 *      Author: a.smirnov
 *
 *      NMT & Emergency
 *
 */

#ifndef CO_NMT_EMCY_H_
#define CO_NMT_EMCY_H_

/** Preliminary class declarations *******************************************/
class cCO_NMT_EMCY;

/** Includes *****************************************************************/
//#include <stdint.h>         // for 'int8_t' to 'uint64_t'

#include "CANopen.h"
#include "CO_led.h"
#include "CO_HBconsumer.h"
#include "CO_HBproducer.h"
#include "CO_EMCYSend.h"
#include "CO_SDO.h"
#include "CO_RPDO.h"
#include "CO_TPDO.h"
#include "CO_UserInterface.h"

/** Defines ******************************************************************/

/**
 * @defgroup CO_EM_errorCodes CANopen Error codes
 * @{
 *
 * Standard error codes according to CiA DS-301 and DS-401.
 */
#define CO_EMC_NO_ERROR                 0x0000 /**< 0x00xx, error Reset or No Error */
#define CO_EMC_GENERIC                  0x1000 /**< 0x10xx, Generic Error */
#define CO_EMC_CURRENT                  0x2000 /**< 0x20xx, Current */
#define CO_EMC_CURRENT_INPUT            0x2100 /**< 0x21xx, Current, device input side */
#define CO_EMC_CURRENT_INSIDE           0x2200 /**< 0x22xx, Current inside the device */
#define CO_EMC_CURRENT_OUTPUT           0x2300 /**< 0x23xx, Current, device output side */
#define CO_EMC_VOLTAGE                  0x3000 /**< 0x30xx, Voltage */
#define CO_EMC_VOLTAGE_MAINS            0x3100 /**< 0x31xx, Mains Voltage */
#define CO_EMC_VOLTAGE_INSIDE           0x3200 /**< 0x32xx, Voltage inside the device */
#define CO_EMC_VOLTAGE_OUTPUT           0x3300 /**< 0x33xx, Output Voltage */
#define CO_EMC_TEMPERATURE              0x4000 /**< 0x40xx, Temperature */
#define CO_EMC_TEMP_AMBIENT             0x4100 /**< 0x41xx, Ambient Temperature */
#define CO_EMC_TEMP_DEVICE              0x4200 /**< 0x42xx, Device Temperature */
#define CO_EMC_HARDWARE                 0x5000 /**< 0x50xx, Device Hardware */
#define CO_EMC_SOFTWARE_DEVICE          0x6000 /**< 0x60xx, Device Software */
#define CO_EMC_SOFTWARE_INTERNAL        0x6100 /**< 0x61xx, Internal Software */
#define CO_EMC_SOFTWARE_USER            0x6200 /**< 0x62xx, User Software */
#define CO_EMC_DATA_SET                 0x6300 /**< 0x63xx, Data Set */
#define CO_EMC_ADDITIONAL_MODUL         0x7000 /**< 0x70xx, Additional Modules */
#define CO_EMC_MONITORING               0x8000 /**< 0x80xx, Monitoring */
#define CO_EMC_COMMUNICATION            0x8100 /**< 0x81xx, Communication */
#define CO_EMC_CAN_OVERRUN              0x8110 /**< 0x8110, CAN Overrun (Objects lost) */
#define CO_EMC_CAN_PASSIVE              0x8120 /**< 0x8120, CAN in Error Passive Mode */
#define CO_EMC_HEARTBEAT                0x8130 /**< 0x8130, Life Guard Error or Heartbeat Error */
#define CO_EMC_BUS_OFF_RECOVERED        0x8140 /**< 0x8140, recovered from bus off */
#define CO_EMC_CAN_ID_COLLISION         0x8150 /**< 0x8150, CAN-ID collision */
#define CO_EMC_PROTOCOL_ERROR           0x8200 /**< 0x82xx, Protocol Error */
#define CO_EMC_PDO_LENGTH               0x8210 /**< 0x8210, PDO not processed due to length error */
#define CO_EMC_PDO_LENGTH_EXC           0x8220 /**< 0x8220, PDO length exceeded */
#define CO_EMC_DAM_MPDO                 0x8230 /**< 0x8230, DAM MPDO not processed, destination object not available */
#define CO_EMC_SYNC_DATA_LENGTH         0x8240 /**< 0x8240, Unexpected SYNC data length */
#define CO_EMC_RPDO_TIMEOUT             0x8250 /**< 0x8250, RPDO timeout */
#define CO_EMC_EXTERNAL_ERROR           0x9000 /**< 0x90xx, External Error */
#define CO_EMC_ADDITIONAL_FUNC          0xF000 /**< 0xF0xx, Additional Functions */
#define CO_EMC_DEVICE_SPECIFIC          0xFF00 /**< 0xFFxx, Device specific */

#define CO_EMC401_OUT_CUR_HI            0x2310 /**< 0x2310, DS401, Current at outputs too high (overload) */
#define CO_EMC401_OUT_SHORTED           0x2320 /**< 0x2320, DS401, Short circuit at outputs */
#define CO_EMC401_OUT_LOAD_DUMP         0x2330 /**< 0x2330, DS401, Load dump at outputs */
#define CO_EMC401_IN_VOLT_HI            0x3110 /**< 0x3110, DS401, Input voltage too high */
#define CO_EMC401_IN_VOLT_LOW           0x3120 /**< 0x3120, DS401, Input voltage too low */
#define CO_EMC401_INTERN_VOLT_HI        0x3210 /**< 0x3210, DS401, Internal voltage too high */
#define CO_EMC401_INTERN_VOLT_LO        0x3220 /**< 0x3220, DS401, Internal voltage too low */
#define CO_EMC401_OUT_VOLT_HIGH         0x3310 /**< 0x3310, DS401, Output voltage too high */
#define CO_EMC401_OUT_VOLT_LOW          0x3320 /**< 0x3320, DS401, Output voltage too low */
/** @} */


/**
 * @defgroup CO_EM_errorStatusBits Error status bits
 * @{
 *
 * Internal indication of the error condition.
 *
 * Each error condition is specified by unique index from 0x00 up to 0xFF.
 * Variable  (from manufacturer section in the Object
 * Dictionary) contains up to 0xFF bits (32bytes) for the identification of the
 * specific error condition. (Type of the variable is CANopen OCTET_STRING.)
 *
 * Stack uses first 6 bytes of the _Error Status Bits_ variable. Device profile
 * or application may define own macros for Error status bits. Note that
 * _Error Status Bits_ must be large enough (up to 32 bytes).
 */
#define CO_EM_NO_ERROR                  0x00 /**< 0x00, Error Reset or No Error */
#define CO_EM_CAN_BUS_WARNING           0x01 /**< 0x01, communication, info, CAN bus warning limit reached */
#define CO_EM_RXMSG_WRONG_LENGTH        0x02 /**< 0x02, communication, info, Wrong data length of the received CAN message */
#define CO_EM_RXMSG_OVERFLOW            0x03 /**< 0x03, communication, info, Previous received CAN message wasn't processed yet */
#define CO_EM_RPDO_WRONG_LENGTH         0x04 /**< 0x04, communication, info, Wrong data length of received PDO */
#define CO_EM_RPDO_OVERFLOW             0x05 /**< 0x05, communication, info, Previous received PDO wasn't processed yet */
#define CO_EM_CAN_RX_BUS_PASSIVE        0x06 /**< 0x06, communication, info, CAN receive bus is passive */
#define CO_EM_CAN_TX_BUS_PASSIVE        0x07 /**< 0x07, communication, info, CAN transmit bus is passive */
#define CO_EM_NMT_WRONG_COMMAND         0x08 /**< 0x08, communication, info, Wrong NMT command received */
#define CO_EM_09_unused                 0x09 /**< 0x09, (unused) */
#define CO_EM_0A_unused                 0x0A /**< 0x0A, (unused) */
#define CO_EM_0B_unused                 0x0B /**< 0x0B, (unused) */
#define CO_EM_0C_unused                 0x0C /**< 0x0C, (unused) */
#define CO_EM_0D_unused                 0x0D /**< 0x0D, (unused) */
#define CO_EM_0E_unused                 0x0E /**< 0x0E, (unused) */
#define CO_EM_0F_unused                 0x0F /**< 0x0F, (unused) */

#define CO_EM_10_unused                 0x10 /**< 0x10, (unused) */
#define CO_EM_11_unused                 0x11 /**< 0x11, (unused) */
#define CO_EM_CAN_TX_BUS_OFF            0x12 /**< 0x12, communication, critical, CAN transmit bus is off */
#define CO_EM_CAN_RXB_OVERFLOW          0x13 /**< 0x13, communication, critical, CAN module receive buffer has overflowed */
#define CO_EM_CAN_TX_OVERFLOW           0x14 /**< 0x14, communication, critical, CAN transmit buffer has overflowed */
#define CO_EM_TPDO_OUTSIDE_WINDOW       0x15 /**< 0x15, communication, critical, TPDO is outside SYNC window */
#define CO_EM_16_unused                 0x16 /**< 0x16, (unused) */
#define CO_EM_17_unused                 0x17 /**< 0x17, (unused) */
#define CO_EM_SYNC_TIME_OUT             0x18 /**< 0x18, communication, critical, SYNC message timeout */
#define CO_EM_SYNC_LENGTH               0x19 /**< 0x19, communication, critical, Unexpected SYNC data length */
#define CO_EM_PDO_WRONG_MAPPING         0x1A /**< 0x1A, communication, critical, Error with PDO mapping */
#define CO_EM_HEARTBEAT_CONSUMER        0x1B /**< 0x1B, communication, critical, Heartbeat consumer timeout */
#define CO_EM_HB_CONSUMER_REMOTE_RESET  0x1C /**< 0x1C, communication, critical, Heartbeat consumer detected remote node reset */
#define CO_EM_HEARTBEAT_WRONG           0x1D /**< 0x1D, communication, critical, Error in OD#1016 (Consumer HB time) */
#define CO_EM_1E_unused                 0x1E /**< 0x1E, (unused) */
#define CO_EM_1F_unused                 0x1F /**< 0x1F, (unused) */

#define CO_EM_EMERGENCY_BUFFER_FULL     0x20 /**< 0x20, generic, info, Emergency buffer is full, Emergency message wasn't sent */
#define CO_EM_NON_VOLATILE_MEMORY       0x21 /**< 0x21, generic, info, Error with access to non volatile device memory */
#define CO_EM_MICROCONTROLLER_RESET     0x22 /**< 0x22, generic, info, Microcontroller has just started */
#define CO_EM_23_unused                 0x23 /**< 0x23, (unused) */
#define CO_EM_24_unused                 0x24 /**< 0x24, (unused) */
#define CO_EM_25_unused                 0x25 /**< 0x25, (unused) */
#define CO_EM_26_unused                 0x26 /**< 0x26, (unused) */
#define CO_EM_27_unused                 0x27 /**< 0x27, (unused) */

#define CO_EM_WRONG_ERROR_REPORT        0x28 /**< 0x28, generic, critical, Wrong parameters to CO_errorReport() function */
#define CO_EM_ISR_TIMER_OVERFLOW        0x29 /**< 0x29, generic, critical, Timer task has overflowed */
#define CO_EM_MEMORY_ALLOCATION_ERROR   0x2A /**< 0x2A, generic, critical, Unable to allocate memory for objects */
#define CO_EM_GENERIC_ERROR             0x2B /**< 0x2B, generic, critical, Generic error, test usage */
#define CO_EM_GENERIC_SOFTWARE_ERROR    0x2C /**< 0x2C, generic, critical, Software error */
#define CO_EM_INCONSISTENT_OBJECT_DICT  0x2D /**< 0x2D, generic, critical, Object dictionary does not match the software */
#define CO_EM_CALCULATION_OF_PARAMETERS 0x2E /**< 0x2E, generic, critical, Error in calculation of device parameters */

#define CO_EM_INT_SOFT_INFO             0x38 /**< 0x38, internal soft, info */

#define CO_EM_INT_SOFT_CRITICAL         0x40 /**< 0x40, internal soft, critical */
#define CO_EM_USER_SOFT_CRITICAL        0x41 /**< 0x41, user soft, critical */
#define CO_EM_HARDWARE_CRITICAL         0x42 /**< 0x42, hardware, critical */

#define CO_EM_HARDWARE_INFO             0x48 /**< 0x48, hardware, info */
#define CO_EM_EXTERNAL_OTHER            0x49 /**< 0x48, external, other */

#define CO_EM_EXTERNAL_INFO             0x50 /**< 0x50, external, info */
/** @} */


/**
 * @defgroup CO_EM_errorStatusBytes Error status bytes indexes
 * @{
 *
 */
#define CO_EM_COMM_INFO_BYTE1_INDEX             0
#define CO_EM_COMM_INFO_BYTE2_INDEX             1
#define CO_EM_COMM_CRITICAL_BYTE1_INDEX         2
#define CO_EM_COMM_CRITICAL_BYTE2_INDEX         3
#define CO_EM_GENERIC_INFO_BYTE1_INDEX          4
#define CO_EM_GENERIC_CRITICAL_BYTE1_INDEX      5
#define CO_EM_INT_SOFT_INFO_BYTE1_INDEX         7
#define CO_EM_INT_MANUF_CRITICAL_BYTE1_INDEX    8
#define CO_EM_INT_MANUF_OTHER_BYTE1_INDEX       9
#define CO_EM_INT_MANUF_INFO_BYTE1_INDEX        10
/** @} */

/** Typedef, enumeration, class **********************************************/

/**
 * CANopen Error register bitmask
 *
 * In object dictionary on index 0x1001.
 *
 * Error register is calculated from critical internal @ref CO_EM_errorStatusBits.
 * Generic and communication bits are calculated in CO_EM_process
 * function, device profile or manufacturer specific bits may be calculated
 * inside the application.
 *
 * Internal errors may prevent device to stay in NMT Operational state. Details
 * are described in _Error Behavior_ object in Object Dictionary at index 0x1029.
 */
typedef enum  {
    CO_ERR_REG_GENERIC_ERR          = 0x01U, /**< bit 0, generic error */
    CO_ERR_REG_CURRENT              = 0x02U, /**< bit 1, current */
    CO_ERR_REG_VOLTAGE              = 0x04U, /**< bit 2, voltage */
    CO_ERR_ERG_TEMPERATUR           = 0x08U, /**< bit 3, temperature */
    CO_ERR_REG_COMM_ERR             = 0x10U, /**< bit 4, communication error (overrun, error state) */
    CO_ERR_REG_MANUFACTURER_OTHER   = 0x20U, /**< bit 5, manufacturer specific other */
    CO_ERR_REG_MANUFACTURER_INFO    = 0x40U, /**< bit 6, manufacturer specific info */
    CO_ERR_REG_MANUFACTURER_CRIT    = 0x80U  /**< bit 7, manufacturer specific critical */
} CO_errorRegisterBitmask_t;


/**
 * Error message
 */
typedef struct  {
  uint8_t                           m_nErrorBit;        /**< from @ref CO_EM_errorStatusBits  */
  uint16_t                          m_nErrorCode;       /**< from @ref CO_EM_errorCodes, 0 if error released */
  uint32_t                          m_nInfoCode;        /**< 32 bit value optional info */
} CO_Error_t;


/**
 *  NMT & Emergency class
 */
class cCO_NMT_EMCY : public acCANError    {
private:

  // links
  cCO_led*                m_pCO_led;                  /**< pointer to cCO_led object */
  cCO_HBConsumer*         m_pCO_HBConsumer;           /**< pointer to cCO_HBConsumer object */
  cCO_HBProducer*         m_pCO_HBProducer;           /**< pointer to cCO_HBProducer object */
  cCO_EMCYSend*           m_pCO_EMCYSend;             /**< pointer to cCO_EMCYSend object */
  cCO_SDOserver*          m_pCO_SDOserver;            /**< pointer to ccCO_SDOserver object */
  cCO_RPDO*               m_pCO_RPDO;                 /**< pointer to cCO_RPDO object */
  cCO_TPDO*               m_pCO_TPDO;                 /**< pointer to cCO_TPDO object */
  cUserInterface*         m_pUserInterface;           /**< pointer to main user object */

  SemaphoreHandle_t   m_xBinarySemaphore;             // binary semaphore handle
  QueueHandle_t       m_xQueueHandle_CANReceive;      // CAN receive queue handle
  QueueHandle_t       m_xQueueHandle_Errors;          // queue of CO_Error_t, handle

  friend void vCO_NMT_EMCY_Task(void* pvParameters);        // task function is a friend

public:

  /**
   *  @brief  Configures object links
   *
   *  @return error state
   */
  CO_ReturnError_t nConfigure(cCO_led const * const pCO_led,
      cCO_HBConsumer const * const pCO_HBConsumer,
      cCO_HBProducer const * const pCO_HBProducer,
      cCO_EMCYSend const * const pCO_EMCYSend,
      cCO_SDOserver const * const pCO_SDOserver,
      cCO_RPDO const * const pCO_RPDO,
      cCO_TPDO const * const pCO_TPDO,
      cUserInterface const * const pUserInterface);

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

  /**
   *  @brief  Method signals that appropriate CAN message been received
   *    Tries to place a message to CAN receive queue without timeout
   *  @param    pCO_CanMsg                     Pointer to message been received
   *  @return   true if success, false if queue is full
   */
  bool bSignalCANReceived(const sCANMsg* pCO_CanMsg);

  /**
   *  @brief  Report error condition from task
   *
   * Function is called on any error condition inside CANopen stack and may also
   * be called by application on custom error condition. Emergency message is sent
   * after the first occurance of specific error. In case of critical error, device
   * will not be able to stay in NMT_OPERATIONAL state.
   *
   * Function must be used only from task !!!
   *
   * @param nErrorBit from @ref CO_EM_errorStatusBits.
   * @param nErrorCode from @ref CO_EM_errorCodes.
   * @param nInfoCode 32 bit value is passed to bytes 4...7 of the Emergency message.
   * It contains optional additional information inside emergency message.
   *
   *  @return   true if success, false if queue is full
   */
  bool bSignalErrorOccured(const uint8_t nErrorBit, const uint16_t nErrorCode, const uint32_t nInfoCode);

  /**
   *  @brief  Report error condition from interrupt
   *
   * Function is called on any error condition inside CANopen stack and may also
   * be called by application on custom error condition. Emergency message is sent
   * after the first occurance of specific error. In case of critical error, device
   * will not be able to stay in NMT_OPERATIONAL state.
   *
   * Function must be used only from interrupt !!!
   *
   * @param nErrorBit from @ref CO_EM_errorStatusBits.
   * @param nErrorCode from @ref CO_EM_errorCodes.
   * @param nInfoCode 32 bit value is passed to bytes 4...7 of the Emergency message.
   * It contains optional additional information inside emergency message.
   *
   *  @return   true if success, false if queue is full
   */
  bool bSignalErrorOccuredFromISR(const uint8_t nErrorBit, const uint16_t nErrorCode, const uint32_t nInfoCode);

  /**
   *  @brief  Reset error condition from task
   *
   * Function is called if any error condition is solved. Emergency message is sent
   * with @ref CO_EM_errorCodes 0x0000.
   *
   * Function must be used only from task !!!
   *
   * @param nErrorBit from @ref CO_EM_errorStatusBits.
   * @param nInfoCode 32 bit value is passed to bytes 4...7 of the Emergency message.
   *
   *  @return   true if success, false if queue is full
   */
  bool bSignalErrorReleased(const uint8_t nErrorBit, const uint32_t nInfoCode);

  /**
   *  @brief  Reset error condition from interrupt
   *
   * Function is called if any error condition is solved. Emergency message is sent
   * with @ref CO_EM_errorCodes 0x0000.
   *
   * Function must be used only from interrupt !!!
   *
   * @param nErrorBit from @ref CO_EM_errorStatusBits.
   * @param nInfoCode 32 bit value is passed to bytes 4...7 of the Emergency message.
   *
   *  @return   true if success, false if queue is full
   */
  bool bSignalErrorReleasedFromISR(const uint8_t nErrorBit, const uint32_t nInfoCode);

  /**
   *  @brief  Report CAN error condition from interrupt
   *
   *  Function must be used only from interrupt !!!
   *
   *  @param    error                       CAN error
   *  @param    nInfoCode                   Additional info
   *  @return   true if success, false otherwise
   */
  bool bSignalCANErrorFromISR(const eCANError error, const uint32_t nInfoCode);

  /**
   *  @brief  Report CAN error condition from task
   *
   *  Function must be used only from task !!!
   *
   *  @param    error                       CAN error
   *  @param    nInfoCode                   Additional info
   *  @return   true if success, false otherwise
   */
  bool bSignalCANError(const eCANError error, const uint32_t nInfoCode);

  /**
   *  @brief  Reset error condition from task
   *
   *  Function must be used only from task !!!
   *
   *  @param    error                       CAN error
   *  @param    nInfoCode                   Additional info
   *  @return   true if success, false otherwise
   */
  bool bSignalCANErrorReleased(const eCANError error, const uint32_t nInfoCode);

};//cCO_NMT_EMCY --------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_NMT_EMCY oCO_NMT_EMCY;

/** Exported function prototypes *********************************************/

/**
 * @brief   NMT&EMCY task function
 * @param   parameters              none
 * @retval  None
 */
void vCO_NMT_EMCY_Task(void* pvParameters);

#endif /* CO_NMT_EMCY_H_ */
