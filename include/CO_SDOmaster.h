/*
 * CO_SDOmaster.h
 *
 *  Created on: 30 Dec 2016 г.
 *      Author: a.smirnov
 *
 *      SDO master (client)
 */

#ifndef CO_SDOMASTER_H_
#define CO_SDOMASTER_H_

/** Preliminary class declarations *******************************************/
class cCO_SDOmaster;
class cCO_SDOmasterRX;

/** Includes *****************************************************************/
#include "CANopen.h"
#include "CO_NMT_EMCY.h"
#include "CO_driver.h"
#include "CO_OD.h"

/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 *  Return values
 */
typedef enum  {
  SDO_MASTER_ERROR_NO                     = 0,            // success
  SDO_MASTER_ERROR_ILLEGAL_ARGUMENT       = -1,           // illegal argument
  SDO_MASTER_ERROR_COMM_REFUSED           = -2,           // command refused
  SDO_MASTER_ERROR_INT_SOFT               = -3,           // internal software error
} SDO_MASTER_Error_t;


/**
 *  SDO master states
 */
typedef enum  {
  SDO_MASTER_STATE_IDLE                   = 0,
  SDO_MASTER_STATE_ABORT                  = 1,

  SDO_MASTER_STATE_DOWNLOAD_INITIATE      = 10,
  SDO_MASTER_STATE_DOWNLOAD_REQUEST       = 11,
  SDO_MASTER_STATE_DOWNLOAD_RESPONSE      = 12,

  SDO_MASTER_STATE_UPLOAD_INITIATE        = 20,
  SDO_MASTER_STATE_UPLOAD_REQUEST         = 21,
  SDO_MASTER_STATE_UPLOAD_RESPONSE        = 22,
} SDO_MASTER_State_t;


/**
 *  SDO transfer result
 */
typedef enum  {
  SDO_MASTER_RESULT_OK                    = 0,              // transfer comleted succesfully
  SDO_MASTER_RESULT_TIMEOUT               = -1,             // transfer timeout
  SDO_MASTER_RESULT_SERVER_ABORT          = -2,             // transfer aborted by remote node
  SDO_MASTER_RESULT_CLIENT_ABORT          = -3,             // transfer aborted by application
  SDO_MASTER_RESULT_ERROR_INT_SOFT        = -4,             // transfer aborted, internal soft error
  SDO_MASTER_RESULT_REC_BUFF_SMALL        = -5,             // upload transfer aborted, receive buffer too small
} SDO_MASTER_Result_t;


/**
 *  SDO master class
 */
class cCO_SDOmaster : public cActiveClass  {
private:

  /** links */
  cCO_NMT_EMCY*         m_pCO_NMT_EMCY;     // pointer to cCO_NMT_EMCY object
  cCO_Driver*           m_pCO_Driver;       // pointer to cCO_Driver object

  /** OS objects */
  QueueHandle_t         m_xQueueHandle_CANReceive;      // CAN receive queue handle
  SemaphoreHandle_t     m_xBinarySemaphore_Wait;        // binary semaphore handle for waiting from outer module
  SemaphoreHandle_t     m_xBinarySemaphore_Task;        // binary semaphore handle for send signal to task

  // COB_ID Client-to-Server
  uint16_t              m_nCOB_IDClientToServer;

  sCANMsg               m_xCANMsg;                      // CAN message
  SDO_MASTER_Result_t   m_nResult;                      // result of last transfer

  /** transfer data */

  uint8_t*              m_pDataBuffer;                  // pointer to data buffer supplied by user

  // By download application indicates data size in buffer. By upload application indicates buffer size
  uint32_t              m_nBufferSize;

  uint16_t              m_nTimeout_ms;                  // Timeout timer for SDO communication, ms
  uint16_t              m_nIndex;                       // Index of current object in Object Dictionary
  uint8_t               m_nSubIndex;                    // Subindex of current object in Object Dictionary

  /** operational data */

  SDO_MASTER_State_t    m_nState;                       // internal state

  // Offset in buffer of next data segment being read/written
  uint16_t              m_nBufferOffset;

  // Toggle bit in segmented transfer or block sequence in block transfer
  uint8_t               m_nToggle;

  /**
   *  @brief  Send CAN message
   *
   *  @return   true if success, false if queue is full or CAN is off
   */
  bool bCANSend(void);

  friend void vCO_SDOmaster_Task(void* pvParameters);       // task function as a friend

public:

  /**
   *  @brief  Main constructor
   *  @param  none
   *  @return none
   */
  cCO_SDOmaster();

  /**
   *  @brief  Configures object links
   *
   *  @param  pCO_NMT_EMCY             Pointer to cCO_NMT_EMCY object
   *  @param  pCO_Driver               Pointer to cCO_Driver object
   *  @param  nCOB_IDClientToServer    COB_ID Client-to-Server
   *  @return error state
   */
  SDO_MASTER_Error_t nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
      cCO_Driver const * const pCO_Driver,
      const uint16_t nCOB_IDClientToServer);

  /**
   *  @brief  Init function
   *  Creates and inits all internal/assosiated OS objects and tasks
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

  /** access methods */
  SDO_MASTER_State_t    nGetState(void);
  SDO_MASTER_Result_t   nGetResult(void);

  /**
   *  @brief  Initiate SDO download communication
   *
   *  @param  *pDataTx    Pointer to data to be written. Data must be valid until end of communication
   *  @param  nDataSize   Size of data, bytes
   *  @param  nTimeout_ms Timeout time for SDO communication in milliseconds
   *  @param  nIndex      Index of object in object dictionary in remote node
   *  @param  nSubIndex   Subindex of object in object dictionary in remote node
   *
   *  @return   error state
   */
  SDO_MASTER_Error_t nClientDownloadInitiate(
      uint8_t*                pDataTx,
      uint32_t                nDataSize,
      uint16_t                nTimeout_ms,
      uint16_t                nIndex,
      uint8_t                 nSubIndex);

  /**
   *  @brief  Initiate SDO upload communication
   *
   *  @param  *pDataRx    Pointer to data buffer, into which received data will be written.
   *    Buffer must be valid until end of communication
   *  @param  nDataSize   Size of data, bytes
   *  @param  nTimeout_ms Timeout time for SDO communication in milliseconds
   *  @param  nIndex      Index of object in object dictionary in remote node
   *  @param  nSubIndex   Subindex of object in object dictionary in remote node
   *
   *  @return   error state
   */
  SDO_MASTER_Error_t nClientUploadInitiate(
      uint8_t*                pDataRx,
      uint32_t                nDataSize,
      uint16_t                nTimeout_ms,
      uint16_t                nIndex,
      uint8_t                 nSubIndex);

  /**
   *  @brief  Abort SDO communication
   *
   *  @param    nCode     Abort code
   *  @return   none
   */
  void vClientAbort(CO_SDO_abortCode_t nCode);

  /**
   *  @brief  Wait for ongoing transfed is completed
   *  If SDO master is idle, returns silently
   *  @param  none
   *  @return error state
   */
  SDO_MASTER_Error_t nWaitTransferCompleted(void);

};//cCO_SDOmaster -------------------------------------------------------------


/**
 *  SDO master RX dispatcher class
 */
class cCO_SDOmasterRX   {
private:

#if CO_NO_SDO_CLIENT > 0

  /** links */
  uint16_t          m_anCAN_ID[CO_NO_SDO_CLIENT];             // array of incoming CAN_IDs
  cCO_SDOmaster*    m_apCO_SDOmaster[CO_NO_SDO_CLIENT];       // array of pointers to SDOmaster objects

  uint8_t           m_nLinkNum;                               // actual number of links

#endif

public:

  /**
   *  @brief  Main constructor
   *  @param  none
   *  @return none
   */
  cCO_SDOmasterRX();

  /**
   *  @brief  Add link
   *
   *  @param    nCAN_ID             Incoming message CAN_ID
   *  @param    pCO_SDOmaster       Pointer to cCO_SDOmaster object
   *  @return   true if success, false otherwise
   */
  bool bAddLink(const uint16_t nCAN_ID, cCO_SDOmaster const * const pCO_SDOmaster);

  /**
   *  @brief  Method signals to appropriate SDO master object that CAN message has been received
   *    Tries to place a message to CAN receive queue without timeout
   *  @param    pCO_CanMsg                     Pointer to message been received
   *  @return   true if success, false otherwise
   */
  bool bSignalCANReceived(const sCANMsg* pCO_CanMsg);

};//cCO_SDOmasterRX -----------------------------------------------------------

/** Exported variables and objects *******************************************/

#if CO_NO_SDO_CLIENT > 0
extern  cCO_SDOmaster     aoCO_SDOmaster[CO_NO_SDO_CLIENT];               // array of SDO master objects
extern  cCO_SDOmasterRX   oCO_SDOmasterRX;
#endif

/** Exported function prototypes *********************************************/

/**
 * @brief   SDO task function
 * @param   parameters              Pointer to cCO_SDOmaster object
 * @retval  None
 */
void vCO_SDOmaster_Task(void* pvParameters);

#endif /* CO_SDOMASTER_H_ */
