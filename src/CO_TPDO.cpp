/*
 * CO_TPDO.cpp
 *
 *  Created on: 6 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen TPDO
 */

/** Includes *****************************************************************/
#include "config.h"

#include "CO_OD.h"
#include "CO_TPDO.h"

#include "task.h"

// generate error, if features are not correctly configured for this project
#ifdef OD_TPDOCommunicationParameter
#ifndef OD_TPDOMappingParameter
#error Features from CO_OD.h file are not correctly configured for this project!
#endif
#endif

/** Defines and constants ****************************************************/
/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/

/**
 * TPDO mapping and state
 * Stores accordance between valid TPDO message CAN-ID and mapping parameters index in object dictionary
 */
struct TPDO_t {

  /** Communication object identifier for transmitting message */
  uint16_t                  m_nCOB_ID;

  /** Array will contain indexes and subindexes of mapped objects
   *  index << 16 + subindex << 8
   *  (equals to OD_TPDOMappingParameter_t mappedobject with zero LS byte)
   *  To be used for finding PDO from known index+subindex
   */
  uint32_t                  m_aMappedObjects[8];

  /** number of entries in above array */
  uint8_t                   m_nMappedObjCntr;

  /** Array for direct TPDO mapping */
  uint8_t*                  m_aPDOMap[8];

  /** number of entries in above array */
  uint8_t                   m_nPDOMapCntr;

  /** request to send */
  bool                      m_bSendRequest;

  /** clear struct */
  void      vClear(void)  {
    uint8_t i;
    m_nCOB_ID = 0;
    for (i=0; i<8; i++)  m_aMappedObjects[i] = 0;
    m_nMappedObjCntr = 0;
    for (i=0; i<8; i++)  m_aPDOMap[i] = NULL;
    m_nPDOMapCntr = 0;
    m_bSendRequest = false;
  }//vClear(void)

  /** operator "=" */
  TPDO_t& operator=(const TPDO_t& CopySource) {
    if (this == &CopySource)  {
      return *this;
    }
    uint8_t i;
    m_nCOB_ID = CopySource.m_nCOB_ID;
    for (i=0; i<8; i++)  m_aMappedObjects[i] = CopySource.m_aMappedObjects[i];
    m_nMappedObjCntr = CopySource.m_nMappedObjCntr;
    for (i=0; i<8; i++)  m_aPDOMap[i] = CopySource.m_aPDOMap[i];
    m_nPDOMapCntr = CopySource.m_nPDOMapCntr;
    m_bSendRequest = CopySource.m_bSendRequest;
    return *this;
  }//TPDO_t& operator=(const TPDO_t& CopySource)

};//TPDO_t --------------------------------------------------------------------

/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_TPDO oCO_TPDO;

/** Class methods ************************************************************/

CO_ReturnError_t cCO_TPDO::nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY, cCO_Driver const * const pCO_Driver,
    cCO_OD_Interface const * const pCO_OD_Interface)  {

  if (pCO_NMT_EMCY == NULL)         return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_Driver == NULL)           return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_OD_Interface == NULL)     return CO_ERROR_ILLEGAL_ARGUMENT;

  m_pCO_NMT_EMCY = (cCO_NMT_EMCY*)pCO_NMT_EMCY;
  m_pCO_Driver = (cCO_Driver*)pCO_Driver;
  m_pCO_OD_Interface = (cCO_OD_Interface*)pCO_OD_Interface;

  return CO_ERROR_NO;
}//cCO_TPDO::nConfigure -------------------------------------------------------

void cCO_TPDO::vInit(void)  {

  TaskHandle_t                xHandle;                            // task handle

  // create binary semaphore
  m_xBinarySemaphore = xSemaphoreCreateBinary();
  configASSERT( m_xBinarySemaphore );

  // create DO change queue
  m_xQueueHandle_DOChange = xQueueCreate(50, sizeof(uint32_t));
  configASSERT( m_xQueueHandle_DOChange );

  // create CANOpen state change queue
  m_xQueueHandle_NMTStateChange = xQueueCreate(50, sizeof(CO_NMT_internalState_t));
  configASSERT( m_xQueueHandle_NMTStateChange );

  // create task
  if (xTaskCreate( vCO_TPDO_Task, ( char * ) CO_TPDO_Task_name, 500, NULL, CO_TPDO_Task_priority,
      &xHandle) != pdPASS)
    for(;;);
  nAddHandle(xHandle);

  return;
}//cCO_TPDO::vInit ------------------------------------------------------------

bool cCO_TPDO::bSignalDOChanged(const uint32_t nDO)  {
  if (xQueueSendToBack(m_xQueueHandle_DOChange, &nDO, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive(m_xBinarySemaphore);               // give the semaphore
    return true;
  }
}//cCO_TPDO::bSignalDOChanged -------------------------------------------------

bool cCO_TPDO::bSignalCOStateChanged(const CO_NMT_internalState_t nNewState) {
  xQueueReset(m_xQueueHandle_NMTStateChange);          // clear queue, earlier messages lost their sense
  if (xQueueSendToBack(m_xQueueHandle_NMTStateChange, &nNewState, 0) == errQUEUE_FULL)
    return false;
  else {
    xSemaphoreGive(m_xBinarySemaphore);               // give the semaphore
    return true;
  }
}//cCO_TPDO::bSignalCOStateChanged --------------------------------------------

/** Exported functions *******************************************************/

void vCO_TPDO_Task(void* pvParameters)  {

#ifndef OD_TPDOCommunicationParameter
  for(;;) vTaskSuspend (NULL);              // TPDO not used, suspend this task
#else

  CO_NMT_internalState_t          nNMTStateSelf = CO_NMT_INITIALIZING;        // self NMT state
  CO_NMT_internalState_t          nNMTStateSelfPrev = CO_NMT_INITIALIZING;    // previous NMT state
  sCANMsg                         CO_CanMsg;
  uint8_t                         i,j;

  TPDO_t                          xTPDO_Item;             // working variable
  TPDO_t                          xTPDO[20];              // TPDO mapping array

  /** checking OD is correct ******/
  if ( N_ELEMENTS(OD_TPDOCommunicationParameter) > N_ELEMENTS(OD_TPDOMappingParameter) )  {
    oCO_TPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, 0);
    for(;;) vTaskSuspend(NULL);
  }

  /** init transmit mapping ******/
  for (i=0; i<20; i++)  xTPDO[i].vClear();          // clear TPDO mapping array
  for(i = 0; i < N_ELEMENTS(OD_TPDOCommunicationParameter); i++)  {
    if ( (OD_TPDOCommunicationParameter[i].COB_IDUsedByTPDO & PDO_VALID_MASK) == 0) {          // RPDO valid ?

      bool        bMappingPossible = true;
      uint8_t     nPDOlength = 0;
      uint16_t    index;
      uint8_t     subIndex;
      uint8_t     dataLen;
      uint16_t    entryNo;
      uint8_t     attr;

      xTPDO_Item.vClear();                // clear structure

      for(j = 0;
          j < (OD_TPDOMappingParameter[i].numberOfMappedObjects <= 8 ? OD_TPDOMappingParameter[i].numberOfMappedObjects : 8);
          j++)
      {

        uint32_t     pMapPointer;
        switch (j) {
        case 0:
          pMapPointer = OD_TPDOMappingParameter[i].mappedObject1;
          break;
        case 1:
          pMapPointer = OD_TPDOMappingParameter[i].mappedObject2;
          break;
        case 2:
          pMapPointer = OD_TPDOMappingParameter[i].mappedObject3;
          break;
        case 3:
          pMapPointer = OD_TPDOMappingParameter[i].mappedObject4;
          break;
        case 4:
          pMapPointer = OD_TPDOMappingParameter[i].mappedObject5;
          break;
        case 5:
          pMapPointer = OD_TPDOMappingParameter[i].mappedObject6;
          break;
        case 6:
          pMapPointer = OD_TPDOMappingParameter[i].mappedObject7;
          break;
        case 7:
          pMapPointer = OD_TPDOMappingParameter[i].mappedObject8;
          break;
        }//switch
        index       = (uint16_t) (pMapPointer >> 16);
        subIndex    = (uint8_t) (pMapPointer >> 8);
        dataLen     = (uint8_t) (pMapPointer);

        if (dataLen&0x07)   {           // wrong data length ? report & abort
          oCO_TPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
          bMappingPossible = false;
          break;  //for(j)
        }

        dataLen >>= 3;                // got data length in bytes
        nPDOlength += dataLen;
        if (nPDOlength > 8)  {       // total RPDO length exceeded ?
          oCO_TPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
          bMappingPossible = false;
          break;  //for(j)
        }

        entryNo = oCO_TPDO.m_pCO_OD_Interface -> nCO_OD_Find(index);        // trying to find object in Object Dictionary
        if ( (entryNo == 0xFFFF) ||
            (subIndex > oCO_TPDO.m_pCO_OD_Interface -> nCO_OD_GetMaxSubindex(entryNo)))  { // doesn't object exist in OD?
          oCO_TPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
          bMappingPossible = false;
          break;  //for(j)
        }
        attr = oCO_TPDO.m_pCO_OD_Interface -> nCO_OD_GetAttribute(entryNo,subIndex);                      // got attributes
        if (!((attr & CO_ODA_TPDO_MAPABLE) && (attr & CO_ODA_READABLE)))  {                 // isn't object Mappable for TPDO?
          oCO_TPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
          bMappingPossible = false;
          break;  //for(j)
        }

        // filling structure
        uint8_t*    pData = (uint8_t*) oCO_TPDO.m_pCO_OD_Interface ->
            pCO_OD_GetDataPointer(entryNo,subIndex);    // got pointer to data
        while (dataLen > 0)  {
          xTPDO_Item.m_aPDOMap[xTPDO_Item.m_nPDOMapCntr] = pData;
          xTPDO_Item.m_nPDOMapCntr++;
          pData++;
          dataLen--;
        }
        xTPDO_Item.m_aMappedObjects[xTPDO_Item.m_nMappedObjCntr] = pMapPointer & 0xFFFFFF00;
        xTPDO_Item.m_nMappedObjCntr++;

      }//for(j)

      if (bMappingPossible) {                   // at last mapping check complete, is it all right ? adding record to vector
        switch (i)  {
        case 0:
        case 1:
        case 2:
        case 3:
          xTPDO_Item.m_nCOB_ID = OD_TPDOCommunicationParameter[i].COB_IDUsedByTPDO + OD_CANNodeID;
          break;
        default:
          xTPDO_Item.m_nCOB_ID = OD_TPDOCommunicationParameter[i].COB_IDUsedByTPDO;
          break;
        }//switch
        xTPDO_Item.m_bSendRequest = false;
        j=0;
        while ((xTPDO[j].m_nCOB_ID != 0) && (j<20))   j++;
        if (j >= 20)  j=19;
        xTPDO[j] = xTPDO_Item;                                        // store mapping record
      }//if (bMappingPossible)

    }//if ( (OD_TPDOCommunicationParameter[i].COB_IDUsedByRPDO & PDO_VALID_MASK) == 0)
  }//for(i)

  /** get number of entries in map ******/
  uint8_t nPDOMapCntr=0;
  while ((xTPDO[nPDOMapCntr].m_nCOB_ID != 0) && (nPDOMapCntr<20))   nPDOMapCntr++;

  /** main part ******/
  for(;;)   {

    xSemaphoreTake(oCO_TPDO.m_xBinarySemaphore,portMAX_DELAY);         // try to take semaphore forever

    // retreving all state change messages
    while (xQueueReceive(oCO_TPDO.m_xQueueHandle_NMTStateChange, &nNMTStateSelf, 0) == pdTRUE);

    // force sending all TPDO if entering "operational"
    if ((nNMTStateSelf == CO_NMT_OPERATIONAL) && (nNMTStateSelf != nNMTStateSelfPrev))
      for (i = 0; i < nPDOMapCntr; i++)
        xTPDO[i].m_bSendRequest = true;

    nNMTStateSelfPrev = nNMTStateSelf;              // remember state

    // main state mashine
    if (nNMTStateSelf != CO_NMT_OPERATIONAL)                                // not in operational state ?
      xQueueReset(oCO_TPDO.m_xQueueHandle_DOChange);       // delete all "DO change" messages
    else  {

      uint32_t  nDO;

      // finding TPDO to be sent
      while (xQueueReceive(oCO_TPDO.m_xQueueHandle_DOChange, &nDO, 0) == pdTRUE)   {   // try to get new "DO change" message
        for(i = 0; i < nPDOMapCntr; i++) {
          for(j = 0; j < xTPDO[i].m_nMappedObjCntr; j++)  {
            if (xTPDO[i].m_aMappedObjects[j] == nDO) {
              xTPDO[i].m_bSendRequest = true;
              break;
            }
          }
        }
      }//while
    }//if-else (nNMTStateSelf != CO_NMT_OPERATIONAL)

    // send TPDO
    for (i = 0; i < nPDOMapCntr; i++)   {
      if (xTPDO[i].m_bSendRequest == true)  {

        CO_CanMsg.m_nStdId = xTPDO[i].m_nCOB_ID;
        CO_CanMsg.m_nDLC = xTPDO[i].m_nPDOMapCntr;
        for (j = 0; j < xTPDO[i].m_nPDOMapCntr; j++) {
          CO_CanMsg.m_aData[j] = *((xTPDO[i]).m_aPDOMap[j]);
        }
        // send TPDO message
        if (!oCO_TPDO.m_pCO_Driver->bCANSend(CO_CanMsg)) {                                         // queue full ?
          oCO_TPDO.m_pCO_NMT_EMCY->bSignalErrorOccured(CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, CAN_TX_OVERFLOW_TPDO);  // report CAN TX overflow
        }
        xTPDO[i].m_bSendRequest = false;
      }//if (xTPDO[i].m_bSendRequest == true)
    }//for (i = 0; i < nPDOMapCntr; i++)

  }//for
#endif
}//vCO_TPDO_Task ------------------------------------------------------------------------
