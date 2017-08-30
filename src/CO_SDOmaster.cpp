/*
 * CO_SDOmaster.cpp
 *
 *  Created on: 30 Dec 2016 г.
 *      Author: a.smirnov
 *
 *      SDO master (client)
 */

/** Includes *****************************************************************/
#include "CO_SDOmaster.h"
#include "task.h"

/** Defines and constants ****************************************************/
#define CCS_DOWNLOAD_EXPEDITED        0x23
#define CCS_DOWNLOAD_SEGMENTED        0x21

#define TOGGLE_BIT_MASK               (((uint8_t) 1) << 4)        // "toggle" bit mask
#define EXP_BIT_MASK                  (((uint8_t) 1) << 1)        // "expedited" bit mask
#define SIZE_BIT_MASK                 (((uint8_t) 1) << 0)        // "size" bit mask
#define END_BIT_MASK                  (((uint8_t) 1) << 0)        // "c" bit mask

/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/
/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

#if CO_NO_SDO_CLIENT > 0
cCO_SDOmaster     aoCO_SDOmaster[CO_NO_SDO_CLIENT];               // array of SDO master objects
cCO_SDOmasterRX   oCO_SDOmasterRX;
#endif

/** Class methods ************************************************************/

cCO_SDOmaster::cCO_SDOmaster()  {
  m_pCO_NMT_EMCY = NULL;
  m_pCO_Driver = NULL;
  m_xQueueHandle_CANReceive = NULL;
  m_xBinarySemaphore_Wait = NULL;
  m_xBinarySemaphore_Task = NULL;
  m_nCOB_IDClientToServer = 0;
  m_pDataBuffer = 0;
  m_nBufferSize = 0;
  m_nTimeout_ms = 0;
  m_nIndex = 0;
  m_nSubIndex = 0;
  m_nState = SDO_MASTER_STATE_IDLE;
  m_nResult = SDO_MASTER_RESULT_OK;
  m_nBufferOffset = 0;
  m_nToggle = 0;

  m_xCANMsg.m_nDLC = 0;
  m_xCANMsg.m_nStdId = 0;
  m_xCANMsg.m_aData[0] = 0;
  m_xCANMsg.m_aData[1] = 0;
  m_xCANMsg.m_aData[2] = 0;
  m_xCANMsg.m_aData[3] = 0;
  m_xCANMsg.m_aData[4] = 0;
  m_xCANMsg.m_aData[5] = 0;
  m_xCANMsg.m_aData[6] = 0;
  m_xCANMsg.m_aData[7] = 0;
}//cCO_SDOmaster::cCO_SDOmaster -----------------------------------------------

SDO_MASTER_Error_t cCO_SDOmaster::nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
    cCO_Driver const * const pCO_Driver,
    const uint16_t nCOB_IDClientToServer)   {

  if (pCO_NMT_EMCY == NULL)               return SDO_MASTER_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_Driver == NULL)                 return SDO_MASTER_ERROR_ILLEGAL_ARGUMENT;
  if (nCOB_IDClientToServer > 0x7FF)      return SDO_MASTER_ERROR_ILLEGAL_ARGUMENT;

  m_pCO_NMT_EMCY = (cCO_NMT_EMCY*)pCO_NMT_EMCY;
  m_pCO_Driver = (cCO_Driver*)pCO_Driver;
  m_nCOB_IDClientToServer = nCOB_IDClientToServer;

  return SDO_MASTER_ERROR_NO;
}//cCO_SDOmaster::nConfigure --------------------------------------------------

void cCO_SDOmaster::vInit(void)   {

  TaskHandle_t                xHandle;                            // task handle

  /** create OS objects ******/
  // create CAN receive queue handle
  m_xQueueHandle_CANReceive = xQueueCreate(50, sizeof(sCANMsg));
  configASSERT( m_xQueueHandle_CANReceive );

  // create semaphore for waiting from outer module
  m_xBinarySemaphore_Wait = xSemaphoreCreateBinary();
  configASSERT( m_xBinarySemaphore_Wait );

  // create semaphore for for send signal to task
  m_xBinarySemaphore_Task = xSemaphoreCreateBinary();
  configASSERT( m_xBinarySemaphore_Task );

  /** create task ******/
  if (xTaskCreate( vCO_SDOmaster_Task, ( char * ) CO_SDOMaster_Task_name, 500, this, CO_SDO_Task_priority, &xHandle)
      != pdPASS)
    for(;;);
  nAddHandle(xHandle);

  /** init internal data ******/
  m_pDataBuffer = 0;
  m_nBufferSize = 0;
  m_nTimeout_ms = 0;
  m_nIndex = 0;
  m_nSubIndex = 0;
  m_nState = SDO_MASTER_STATE_IDLE;
  m_nResult = SDO_MASTER_RESULT_OK;
  m_nBufferOffset = 0;
  m_nToggle = 0;

  m_xCANMsg.m_nDLC = 0;
  m_xCANMsg.m_nStdId = 0;
  m_xCANMsg.m_aData[0] = 0;
  m_xCANMsg.m_aData[1] = 0;
  m_xCANMsg.m_aData[2] = 0;
  m_xCANMsg.m_aData[3] = 0;
  m_xCANMsg.m_aData[4] = 0;
  m_xCANMsg.m_aData[5] = 0;
  m_xCANMsg.m_aData[6] = 0;
  m_xCANMsg.m_aData[7] = 0;

  return;
}//cCO_SDOmaster::vInit -------------------------------------------------------

bool cCO_SDOmaster::bSignalCANReceived(const sCANMsg* pCO_CanMsg)   {
  if (xQueueSendToBack(m_xQueueHandle_CANReceive, pCO_CanMsg, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive( m_xBinarySemaphore_Task );               // give the semaphore
    return true;
  }
}//cCO_SDOmaster::bSignalCANReceived ------------------------------------------

SDO_MASTER_State_t cCO_SDOmaster::nGetState(void)   {
  return m_nState;
}//cCO_SDOmaster::nGetState ---------------------------------------------------

SDO_MASTER_Result_t cCO_SDOmaster::nGetResult(void)   {
  return m_nResult;
}//cCO_SDOmaster::nGetResult --------------------------------------------------

SDO_MASTER_Error_t cCO_SDOmaster::nClientDownloadInitiate(
    uint8_t*                pDataTx,
    uint32_t                nDataSize,
    uint16_t                nTimeout_ms,
    uint16_t                nIndex,
    uint8_t                 nSubIndex)  {

  if (m_nState != SDO_MASTER_STATE_IDLE)  return  SDO_MASTER_ERROR_COMM_REFUSED;    // verify state
  if (pDataTx == NULL || nDataSize == 0 || nTimeout_ms == 0)                        // verify parameters
    return SDO_MASTER_ERROR_ILLEGAL_ARGUMENT;

  // store parameters
  m_pDataBuffer = pDataTx;
  m_nBufferSize = nDataSize;
  m_nTimeout_ms = nTimeout_ms;
  m_nIndex = nIndex;
  m_nSubIndex = nSubIndex;

  m_nState = SDO_MASTER_STATE_DOWNLOAD_INITIATE;                    // change state

  // prepare message partially
  m_xCANMsg.m_nDLC = 8;
  m_xCANMsg.m_nStdId = m_nCOB_IDClientToServer;
  m_xCANMsg.m_aData[1] = (uint8_t)(m_nIndex & 0xFF);
  m_xCANMsg.m_aData[2] = (uint8_t)(m_nIndex >> 8);
  m_xCANMsg.m_aData[3] = m_nSubIndex;

  if (nDataSize <= 4)     {                                     // is expedited transfer possible ?
    m_xCANMsg.m_aData[0] = CCS_DOWNLOAD_EXPEDITED | ((4-nDataSize) << 2);     // prepare first byte
    for(uint8_t i=4; i < 4+nDataSize; i++)
      m_xCANMsg.m_aData[i] = pDataTx[i-4];
  }
  else  {                                                       // segmented transfer
    m_xCANMsg.m_aData[0] = CCS_DOWNLOAD_SEGMENTED;                 // prepare first byte
    // prepare length
    m_xCANMsg.m_aData[4] = (uint8_t) (nDataSize & 0xFF);
    m_xCANMsg.m_aData[5] = (uint8_t) ((nDataSize >> 8) & 0xFF);
    m_xCANMsg.m_aData[6] = (uint8_t) ((nDataSize >> 16) & 0xFF);
    m_xCANMsg.m_aData[7] = (uint8_t) ((nDataSize >> 24) & 0xFF);
  }//if (nDataSize <= 4)

  // start transfer
  if (!m_pCO_Driver -> bCANSend(m_xCANMsg))   {                                      // queue full ?
    m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN,
        CAN_TX_OVERFLOW_SDO_MASTER);    // report CAN TX overflow
    return SDO_MASTER_ERROR_INT_SOFT;
  }
  else  {
    xSemaphoreGive( m_xBinarySemaphore_Task );                                        // signal to task
    xSemaphoreTake( m_xBinarySemaphore_Wait, 0);                                      // clear waiting semaphore
    return SDO_MASTER_ERROR_NO;
  }
}//cCO_SDOmaster::nClientDownloadInitiate -------------------------------------

SDO_MASTER_Error_t cCO_SDOmaster::nClientUploadInitiate(
    uint8_t*                pDataRx,
    uint32_t                nDataSize,
    uint16_t                nTimeout_ms,
    uint16_t                nIndex,
    uint8_t                 nSubIndex)    {

  if (m_nState != SDO_MASTER_STATE_IDLE)  return  SDO_MASTER_ERROR_COMM_REFUSED;    // verify state
  if (pDataRx == NULL || nDataSize < 4 || nTimeout_ms == 0)                        // verify parameters
    return SDO_MASTER_ERROR_ILLEGAL_ARGUMENT;

  // store parameters
  m_pDataBuffer = pDataRx;
  m_nBufferSize = nDataSize;
  m_nTimeout_ms = nTimeout_ms;
  m_nIndex = nIndex;
  m_nSubIndex = nSubIndex;

  m_nState = SDO_MASTER_STATE_UPLOAD_INITIATE;                      // change state

  // prepare message
  m_xCANMsg.m_nDLC = 8;
  m_xCANMsg.m_nStdId = m_nCOB_IDClientToServer;
  m_xCANMsg.m_aData[0] = CCS_UPLOAD_INITIATE << 5;
  m_xCANMsg.m_aData[1] = (uint8_t)(m_nIndex & 0xFF);
  m_xCANMsg.m_aData[2] = (uint8_t)(m_nIndex >> 8);
  m_xCANMsg.m_aData[3] = m_nSubIndex;
  m_xCANMsg.m_aData[4] = 0;
  m_xCANMsg.m_aData[5] = 0;
  m_xCANMsg.m_aData[6] = 0;
  m_xCANMsg.m_aData[7] = 0;

  // start transfer
  if (!bCANSend())   {
    return SDO_MASTER_ERROR_INT_SOFT;
  }
  else  {
    xSemaphoreGive( m_xBinarySemaphore_Task );                                        // signal to task
    xSemaphoreTake( m_xBinarySemaphore_Wait, 0);                                      // clear waiting semaphore
    return SDO_MASTER_ERROR_NO;
  }
}//cCO_SDOmaster::nClientUploadInitiate ---------------------------------------

void cCO_SDOmaster::vClientAbort(CO_SDO_abortCode_t nCode)   {

  // prepare message
  m_xCANMsg.m_aData[4] = (uint8_t) (nCode & 0xFF);
  m_xCANMsg.m_aData[5] = (uint8_t) ((nCode >> 8) & 0xFF);
  m_xCANMsg.m_aData[6] = (uint8_t) ((nCode >> 16) & 0xFF);
  m_xCANMsg.m_aData[7] = (uint8_t) ((nCode >> 24) & 0xFF);

  m_nState = SDO_MASTER_STATE_ABORT;                          // change state
  xSemaphoreGive( m_xBinarySemaphore_Task );                  // signal to task

  return;
}//cCO_SDOmaster::vClientAbort ------------------------------------------------

SDO_MASTER_Error_t cCO_SDOmaster::nWaitTransferCompleted(void)    {
  if (m_nState == SDO_MASTER_STATE_IDLE)  return SDO_MASTER_ERROR_COMM_REFUSED;
  xSemaphoreTake(m_xBinarySemaphore_Wait, portMAX_DELAY);     // waiting for "wait" semaphore forever
  return SDO_MASTER_ERROR_NO;
}//cCO_SDOmaster::nWaitTransferCompleted --------------------------------------

bool cCO_SDOmaster::bCANSend(void)   {
  if (!m_pCO_Driver -> bCANSend(m_xCANMsg))   {                                      // queue full ?
    m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN,
        CAN_TX_OVERFLOW_SDO_MASTER);    // report CAN TX overflow
    return false;
  }
  else
    return true;
}//cCO_SDOmaster::bCANSend ----------------------------------------------------

cCO_SDOmasterRX::cCO_SDOmasterRX()  {
#if CO_NO_SDO_CLIENT > 0
  for(uint8_t i=0; i<CO_NO_SDO_CLIENT; i++)   {
    m_anCAN_ID[i] = 0;
    m_apCO_SDOmaster[i] = NULL;
  }
  m_nLinkNum = 0;
#endif
}//cCO_SDOmasterRX::cCO_SDOmasterRX -------------------------------------------

bool cCO_SDOmasterRX::bAddLink(const uint16_t nCAN_ID, cCO_SDOmaster const * const pCO_SDOmaster)   {
#if CO_NO_SDO_CLIENT > 0
  if (m_nLinkNum < CO_NO_SDO_CLIENT)  {
    m_anCAN_ID[m_nLinkNum] = nCAN_ID;
    m_apCO_SDOmaster[m_nLinkNum] = (cCO_SDOmaster*)pCO_SDOmaster;
    m_nLinkNum++;
    return true;
  }
  else
    return false;
#else
  return false;
#endif
}//cCO_SDOmasterRX::bAddLink --------------------------------------------------

bool cCO_SDOmasterRX::bSignalCANReceived(const sCANMsg* pCO_CanMsg)   {
#if CO_NO_SDO_CLIENT > 0

  // find link
  for(uint8_t i=0; i<CO_NO_SDO_CLIENT; i++)     {
    if (pCO_CanMsg -> m_nStdId == m_anCAN_ID[i])   {
      return m_apCO_SDOmaster[i]->bSignalCANReceived(pCO_CanMsg);
    }
  }

  return false;   // link not found
#else
  return false;
#endif
}//cCO_SDOmasterRX::bSignalCANReceived ----------------------------------------

/** Exported functions *******************************************************/

void vCO_SDOmaster_Task(void* pvParameters)   {

  cCO_SDOmaster*      pCO_SDOmaster = (cCO_SDOmaster*) pvParameters;
  uint8_t             nSCS;                                             // server command specifier
  uint8_t             i, j, nSize;

  // main part
  for(;;)   {

    switch (pCO_SDOmaster->m_nState)  {

    case SDO_MASTER_STATE_IDLE:
      xSemaphoreTake(pCO_SDOmaster->m_xBinarySemaphore_Task, portMAX_DELAY);      // wait for "task" semaphore forever
      break;

    case SDO_MASTER_STATE_DOWNLOAD_INITIATE:

      if (xSemaphoreTake(pCO_SDOmaster->m_xBinarySemaphore_Task, (pCO_SDOmaster->m_nTimeout_ms) / portTICK_RATE_MS) ==
          pdFALSE)  {                 // timeout expired ?
        // prepare abort code
        pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_TIMEOUT & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 8) & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 16) & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 24) & 0xFF);
        pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_TIMEOUT;
        pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
      }//if (xSemaphoreTake(pCO_SDOmaster->m_xBinarySemaphore_Task ...

      if (pCO_SDOmaster->m_nState == SDO_MASTER_STATE_DOWNLOAD_INITIATE)     {      // is the state same ?
        if (xQueueReceive(pCO_SDOmaster->m_xQueueHandle_CANReceive, &pCO_SDOmaster->m_xCANMsg, 0) == pdTRUE)  {

          nSCS = (pCO_SDOmaster->m_xCANMsg).m_aData[0] >> 5;         // get server command specifier

          switch (nSCS)   {
          case SCS_ABORT:                               // abort from server
            pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;                           // change state
            pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_SERVER_ABORT;
            xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
            break;//case SCS_ABORT

          case SCS_DOWNLOAD_INITIATE:
            if (pCO_SDOmaster->m_nBufferSize <= 4)  {               // expedited transfer ?
              pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;
              pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_OK;
              xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
            }
            else  {
              // segmented transfer - prepare the first segment
              pCO_SDOmaster -> m_nBufferOffset = 0;
              pCO_SDOmaster -> m_nToggle = 0;
              pCO_SDOmaster -> m_nState = SDO_MASTER_STATE_DOWNLOAD_REQUEST;
            }//if-else (pCO_SDOmaster->m_nBufferSize <= 4)
            break;//case SCS_DOWNLOAD_INITIATE

          default:
            // prepare abort code
            pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_CMD & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_CMD >> 8) & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_CMD >> 16) & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_CMD >> 24) & 0xFF);
            pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_CLIENT_ABORT;
            pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
            break;
          }//switch (nSCS)

        }//if (xQueueReceive(pCO_SDOmaster->m_xQueueHandle_CANReceive, &pCO_SDOmaster->m_xCANMsg, 0) == pdTRUE)
      }//if (pCO_SDOmaster->m_nState == SDO_MASTER_STATE_DOWNLOAD_INITIATE)

      break;//case SDO_MASTER_STATE_DOWNLOAD_INITIATE

    case SDO_MASTER_STATE_DOWNLOAD_REQUEST:

      // calculate length to be sent
      j = pCO_SDOmaster->m_nBufferSize - pCO_SDOmaster->m_nBufferOffset;
      if (j>7) j=7;

      // fill data bytes
      for(i=0; i<j; i++)
        pCO_SDOmaster -> m_xCANMsg.m_aData[i+1] = pCO_SDOmaster -> m_pDataBuffer[pCO_SDOmaster->m_nBufferOffset + i];
      for(i=j; i<7; i++)
        pCO_SDOmaster->m_xCANMsg.m_aData[i+1] = 0;

      pCO_SDOmaster->m_nBufferOffset += j;                // modify buffer offset
      pCO_SDOmaster->m_xCANMsg.m_aData[0] = CCS_DOWNLOAD_SEGMENT | ((pCO_SDOmaster->m_nToggle) << 4) |
          ((7-j) << 1);                                   // set client command specifier
      if (pCO_SDOmaster->m_nBufferOffset == pCO_SDOmaster->m_nBufferSize)     // is end of transfer?
        pCO_SDOmaster->m_xCANMsg.m_aData[0] |= END_BIT_MASK;                     // set "c" bit

      // prepare the rest part of message
      pCO_SDOmaster->m_xCANMsg.m_nDLC = 8;
      pCO_SDOmaster->m_xCANMsg.m_nStdId = pCO_SDOmaster->m_nCOB_IDClientToServer;

      // send message
      if (!pCO_SDOmaster->bCANSend())   {
        pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;
        pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_ERROR_INT_SOFT;
        xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
      }
      else
        pCO_SDOmaster->m_nState = SDO_MASTER_STATE_DOWNLOAD_RESPONSE;   // next state

      break;//case SDO_MASTER_STATE_DOWNLOAD_REQUEST

    case SDO_MASTER_STATE_DOWNLOAD_RESPONSE:

      if (xSemaphoreTake(pCO_SDOmaster->m_xBinarySemaphore_Task, (pCO_SDOmaster->m_nTimeout_ms) / portTICK_RATE_MS) ==
          pdFALSE)  {                 // timeout expired ?
        // prepare abort code
        pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_TIMEOUT & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 8) & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 16) & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 24) & 0xFF);
        pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_TIMEOUT;
        pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
      }//if (xSemaphoreTake(pCO_SDOmaster->m_xBinarySemaphore_Task ...

      if (pCO_SDOmaster->m_nState == SDO_MASTER_STATE_DOWNLOAD_RESPONSE)     {   // is the state same ?
        if (xQueueReceive(pCO_SDOmaster->m_xQueueHandle_CANReceive, &pCO_SDOmaster->m_xCANMsg, 0) == pdTRUE)  {

          nSCS = (pCO_SDOmaster->m_xCANMsg).m_aData[0] >> 5;         // get server command specifier

          switch (nSCS)   {
          case SCS_ABORT:                               // abort from server
            pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;                           // change state
            pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_SERVER_ABORT;
            xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
            break;//case SCS_ABORT

          case SCS_DOWNLOAD_SEGMENT:
            // verify toggle bit
            if (((pCO_SDOmaster->m_xCANMsg).m_aData[0] & TOGGLE_BIT_MASK) != (pCO_SDOmaster->m_nToggle << 4))  {
              // prepare abort code
              pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_TOGGLE_BIT & 0xFF);
              pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_TOGGLE_BIT >> 8) & 0xFF);
              pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_TOGGLE_BIT >> 16) & 0xFF);
              pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_TOGGLE_BIT >> 24) & 0xFF);
              pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_CLIENT_ABORT;
              pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
              break;//case SCS_DOWNLOAD_SEGMENT
            }
            // alternate toggle bit
            if (pCO_SDOmaster->m_nToggle == 0)
              pCO_SDOmaster->m_nToggle = 1;
            else
              pCO_SDOmaster->m_nToggle = 0;
            if (pCO_SDOmaster->m_nBufferOffset == pCO_SDOmaster->m_nBufferSize)  {   // is end of transfer?
              pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;
              pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_OK;
              xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
            }
            else
              pCO_SDOmaster->m_nState = SDO_MASTER_STATE_DOWNLOAD_REQUEST;

            break;//case SCS_DOWNLOAD_SEGMENT

          default:
            // prepare abort code
            pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_CMD & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_CMD >> 8) & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_CMD >> 16) & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_CMD >> 24) & 0xFF);
            pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_CLIENT_ABORT;
            pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
            break;
          }//switch (nSCS)

        }//if (xQueueReceive(pCO_SDOmaster->m_xQueueHandle_CANReceive, &pCO_SDOmaster->m_xCANMsg, 0) == pdTRUE)
      }//if (pCO_SDOmaster->m_nState == SDO_MASTER_STATE_DOWNLOAD_RESPONSE)

      break;//case SDO_MASTER_STATE_DOWNLOAD_RESPONSE

    case SDO_MASTER_STATE_UPLOAD_INITIATE:

      if (xSemaphoreTake(pCO_SDOmaster->m_xBinarySemaphore_Task, (pCO_SDOmaster->m_nTimeout_ms) / portTICK_RATE_MS) ==
          pdFALSE)  {                 // timeout expired ?
        // prepare abort code
        pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_TIMEOUT & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 8) & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 16) & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 24) & 0xFF);
        pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_TIMEOUT;
        pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
      }//if (xSemaphoreTake(pCO_SDOmaster->m_xBinarySemaphore_Task ...

      if (pCO_SDOmaster->m_nState == SDO_MASTER_STATE_UPLOAD_INITIATE)     {      // is the state same ?
        if (xQueueReceive(pCO_SDOmaster->m_xQueueHandle_CANReceive, &pCO_SDOmaster->m_xCANMsg, 0) == pdTRUE)  {

          nSCS = (pCO_SDOmaster->m_xCANMsg).m_aData[0] >> 5;         // get server command specifier

          switch (nSCS)   {
          case SCS_ABORT:                               // abort from server
            pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;                           // change state
            pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_SERVER_ABORT;
            xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
            break;//case SCS_ABORT

          case SCS_UPLOAD_INITIATE:
            if ((pCO_SDOmaster->m_xCANMsg).m_aData[0] & EXP_BIT_MASK)  {       // expeditad transfer ?
              if ((pCO_SDOmaster->m_xCANMsg).m_aData[0] & SIZE_BIT_MASK)       // is size indicated ?
                nSize = 4 - ((pCO_SDOmaster -> m_xCANMsg.m_aData[0] >> 2) & 0x03);       // get indicated size
              else
                nSize = 4;
              while (nSize--)                                               // copy data
                pCO_SDOmaster->m_pDataBuffer[nSize] = pCO_SDOmaster->m_xCANMsg.m_aData[4+nSize];
              pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;
              pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_OK;
              xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
            }//if ((pCO_SDOmaster->m_xCANMsg).Data[0] & EXP_BIT_MASK)
            else  {
              // segmented transfer - prepare the first segment
              pCO_SDOmaster -> m_nBufferOffset = 0;
              pCO_SDOmaster -> m_nToggle = 0;
              pCO_SDOmaster -> m_nState = SDO_MASTER_STATE_UPLOAD_REQUEST;
            }//if-else ((pCO_SDOmaster->m_xCANMsg).Data[0] & EXP_BIT_MASK)
            break;//case SCS_UPLOAD_INITIATE

          default:
            // prepare abort code
            pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_CMD & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_CMD >> 8) & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_CMD >> 16) & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_CMD >> 24) & 0xFF);
            pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_CLIENT_ABORT;
            pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
            break;
          }//switch (nSCS)

        }//if (xQueueReceive(pCO_SDOmaster->m_xQueueHandle_CANReceive, &pCO_SDOmaster->m_xCANMsg, 0) == pdTRUE)
      }//if (pCO_SDOmaster->m_nState == SDO_MASTER_STATE_UPLOAD_INITIATE)

      break;//SDO_MASTER_STATE_UPLOAD_INITIATE

    case SDO_MASTER_STATE_UPLOAD_REQUEST:

      // prepare message
      pCO_SDOmaster->m_xCANMsg.m_nDLC = 8;
      pCO_SDOmaster->m_xCANMsg.m_nStdId = pCO_SDOmaster->m_nCOB_IDClientToServer;
      pCO_SDOmaster->m_xCANMsg.m_aData[0] = (CCS_UPLOAD_SEGMENT << 5) | (pCO_SDOmaster->m_nToggle & TOGGLE_BIT_MASK);
      pCO_SDOmaster->m_xCANMsg.m_aData[1] = 0;
      pCO_SDOmaster->m_xCANMsg.m_aData[2] = 0;
      pCO_SDOmaster->m_xCANMsg.m_aData[3] = 0;
      pCO_SDOmaster->m_xCANMsg.m_aData[4] = 0;
      pCO_SDOmaster->m_xCANMsg.m_aData[5] = 0;
      pCO_SDOmaster->m_xCANMsg.m_aData[6] = 0;
      pCO_SDOmaster->m_xCANMsg.m_aData[7] = 0;

      // send message
      if (!pCO_SDOmaster->bCANSend())   {
        pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;
        pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_ERROR_INT_SOFT;
        xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
      }
      else
        pCO_SDOmaster->m_nState = SDO_MASTER_STATE_UPLOAD_RESPONSE;   // next state

      break;//case SDO_MASTER_STATE_UPLOAD_REQUEST

    case SDO_MASTER_STATE_UPLOAD_RESPONSE:

      if (xSemaphoreTake(pCO_SDOmaster->m_xBinarySemaphore_Task, (pCO_SDOmaster->m_nTimeout_ms) / portTICK_RATE_MS) ==
          pdFALSE)  {                 // timeout expired ?
        // prepare abort code
        pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_TIMEOUT & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 8) & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 16) & 0xFF);
        pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_TIMEOUT >> 24) & 0xFF);
        pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_TIMEOUT;
        pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
      }//if (xSemaphoreTake(pCO_SDOmaster->m_xBinarySemaphore_Task ...

      if (pCO_SDOmaster->m_nState == SDO_MASTER_STATE_UPLOAD_RESPONSE)     {   // is the state same ?
        if (xQueueReceive(pCO_SDOmaster->m_xQueueHandle_CANReceive, &pCO_SDOmaster->m_xCANMsg, 0) == pdTRUE)  {

          nSCS = (pCO_SDOmaster->m_xCANMsg).m_aData[0] >> 5;         // get server command specifier

          switch (nSCS)   {
          case SCS_ABORT:                               // abort from server
            pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;                           // change state
            pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_SERVER_ABORT;
            xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
            break;//case SCS_ABORT

          case SCS_UPLOAD_SEGMENT:
            if (((pCO_SDOmaster->m_xCANMsg).m_aData[0] & TOGGLE_BIT_MASK) != (pCO_SDOmaster->m_nToggle << 4))  {
              // prepare abort code
              pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_TOGGLE_BIT & 0xFF);
              pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_TOGGLE_BIT >> 8) & 0xFF);
              pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_TOGGLE_BIT >> 16) & 0xFF);
              pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_TOGGLE_BIT >> 24) & 0xFF);
              pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_CLIENT_ABORT;
              pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
              break;//case SCS_UPLOAD_SEGMENT
            }
            // alternate toggle bit
            if (pCO_SDOmaster->m_nToggle == 0)
              pCO_SDOmaster->m_nToggle = 1;
            else
              pCO_SDOmaster->m_nToggle = 0;
            nSize = ((pCO_SDOmaster->m_xCANMsg.m_aData[0]) >> 1) & 0x07;                      // get size
            // verify length
            if ((pCO_SDOmaster->m_nBufferOffset + nSize) > pCO_SDOmaster->m_nBufferSize )   {       // out of memory ?
              // prepare abort code
              pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_OUT_OF_MEM & 0xFF);
              pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_OUT_OF_MEM >> 8) & 0xFF);
              pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_OUT_OF_MEM >> 16) & 0xFF);
              pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_OUT_OF_MEM >> 24) & 0xFF);
              pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_REC_BUFF_SMALL;
              pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
              break;//case SCS_UPLOAD_SEGMENT
            }//if ((pCO_SDOmaster->m_nBufferOffset + nSize) > pCO_SDOmaster->m_nBufferSize )
            // copy data to buffer
            for(i=0; i<nSize; i++)
              pCO_SDOmaster->m_pDataBuffer[pCO_SDOmaster->m_nBufferOffset + i] = pCO_SDOmaster->m_xCANMsg.m_aData[i+1];
            pCO_SDOmaster->m_nBufferOffset += nSize;
            // if no more segments to be uploaded, finish communication
            if (pCO_SDOmaster->m_xCANMsg.m_aData[0] & END_BIT_MASK)   {
              pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;
              pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_OK;
              xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task
            }
            else  {
              pCO_SDOmaster->m_nState = SDO_MASTER_STATE_UPLOAD_REQUEST;
            }

            break;//case SCS_UPLOAD_SEGMENT

          default:
            // prepare abort code
            pCO_SDOmaster->m_xCANMsg.m_aData[4] = (uint8_t) (CO_SDO_AB_CMD & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[5] = (uint8_t) ((CO_SDO_AB_CMD >> 8) & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[6] = (uint8_t) ((CO_SDO_AB_CMD >> 16) & 0xFF);
            pCO_SDOmaster->m_xCANMsg.m_aData[7] = (uint8_t) ((CO_SDO_AB_CMD >> 24) & 0xFF);
            pCO_SDOmaster->m_nResult = SDO_MASTER_RESULT_CLIENT_ABORT;
            pCO_SDOmaster->m_nState = SDO_MASTER_STATE_ABORT;                           // change state
            break;
          }//switch (nSCS)
        }//if (xQueueReceive(pCO_SDOmaster->m_xQueueHandle_CANReceive, &pCO_SDOmaster->m_xCANMsg, 0) == pdTRUE)
      }//if (pCO_SDOmaster->m_nState == SDO_MASTER_STATE_UPLOAD_RESPONSE)

      break;//case SDO_MASTER_STATE_UPLOAD_RESPONSE

    case SDO_MASTER_STATE_ABORT:

      // prepare rest part of the message, code is prepared earlier
      pCO_SDOmaster->m_xCANMsg.m_nDLC = 8;
      pCO_SDOmaster->m_xCANMsg.m_nStdId = pCO_SDOmaster->m_nCOB_IDClientToServer;
      pCO_SDOmaster->m_xCANMsg.m_aData[0] = CCS_ABORT;
      pCO_SDOmaster->m_xCANMsg.m_aData[1] = (uint8_t)(pCO_SDOmaster->m_nIndex & 0xFF);
      pCO_SDOmaster->m_xCANMsg.m_aData[2] = (uint8_t)(pCO_SDOmaster->m_nIndex >> 8);
      pCO_SDOmaster->m_xCANMsg.m_aData[3] = pCO_SDOmaster->m_nSubIndex;

      // send message
      pCO_SDOmaster->bCANSend();
      pCO_SDOmaster->m_nState = SDO_MASTER_STATE_IDLE;
      xSemaphoreGive( pCO_SDOmaster->m_xBinarySemaphore_Wait );     // signal to outer task

      break;//SDO_MASTER_STATE_ABORT

    }//switch (pCO_SDOmaster->m_nState)
  }//for
}//vCO_SDOmaster_Task ---------------------------------------------------------
