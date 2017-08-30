/*
 * CO_RPDO.cpp
 *
 *  Created on: 5 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen RPDO
 */

/** Includes *****************************************************************/
#include "config.h"

#include "CO_OD.h"
#include "CO_RPDO.h"

#include "task.h"

// generate error, if features are not correctly configured for this project
#ifdef OD_RPDOCommunicationParameter
#ifndef OD_RPDOMappingParameter
#error Features from CO_OD.h file are not correctly configured for this project!
#endif
#endif  //ifdef OD_RPDOCommunicationParameter

/** Defines and constants ****************************************************/
/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/

/**
 * Structure for direct PDO mapping
 */
typedef struct {
  uint16_t        m_nCOB_ID;            // CAN ID
  uint8_t         m_nSize;              // size of m_aPDOMap[]
  uint8_t*        m_aPDOMap[8];         // array of pointers to OD entries
} PDOMap_t;

/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_RPDO oCO_RPDO;

/** Class methods ************************************************************/

CO_ReturnError_t cCO_RPDO::nConfigure(cCO_NMT_EMCY const * const pCO_NMT_EMCY,
    cUserInterface const * const pUserInterface, cCO_OD_Interface const * const pCO_OD_Interface)   {

  if (pCO_NMT_EMCY == NULL)         return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pUserInterface == NULL)       return CO_ERROR_ILLEGAL_ARGUMENT;
  if (pCO_OD_Interface == NULL)     return CO_ERROR_ILLEGAL_ARGUMENT;

  m_pCO_NMT_EMCY = (cCO_NMT_EMCY*)pCO_NMT_EMCY;
  m_pUserInterface = (cUserInterface*)pUserInterface;
  m_pCO_OD_Interface = (cCO_OD_Interface*)pCO_OD_Interface;

  return CO_ERROR_NO;

}//cCO_RPDO::nConfigure -------------------------------------------------------

void cCO_RPDO::vInit(void)  {

  TaskHandle_t                xHandle;                            // task handle

  vInitPartial();
  if (xTaskCreate( vCO_RPDO_Task, ( char * ) CO_RPDO_Task_name, 500, NULL, CO_RPDO_Task_priority,
      &xHandle) != pdPASS)
    for(;;);
  nAddHandle(xHandle);
  return;
}//cCO_RPDO::vInit ------------------------------------------------------------

/** Exported functions *******************************************************/

void vCO_RPDO_Task(void* pvParameters)    {

#ifndef OD_RPDOCommunicationParameter
  for(;;) vTaskSuspend (NULL);              // RPDO not used, suspend this task
#endif

  CO_NMT_internalState_t          nNMTStateSelf = CO_NMT_INITIALIZING;  // self NMT state
  sCANMsg                         CO_CanMsg;
  uint8_t                         i,j;

  uint8_t*        aPDOMap[8];                 // Structure for direct PDO mapping
  uint8_t         nPDOMapCntr = 0;

  // RPDO mapping array
  PDOMap_t        xRPDO_Map[20] = {
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} } ,
      {0 , 0 ,{NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL} }
  };

  // checking OD is correct
  if (N_ELEMENTS(OD_RPDOCommunicationParameter) > N_ELEMENTS(OD_RPDOMappingParameter))   {
    oCO_RPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, 0);
    for(;;) vTaskSuspend(NULL);
  }

  // init receive mapping
  for(i = 0; i < N_ELEMENTS(OD_RPDOCommunicationParameter); i++)  {
    if ( (OD_RPDOCommunicationParameter[i].COB_IDUsedByRPDO & PDO_VALID_MASK) == 0) {          // RPDO valid ?

      bool        bMappingPossible = true;
      uint8_t     nPDOlength = 0;
      uint16_t    index;
      uint8_t     subIndex;
      uint8_t     dataLen;
      uint16_t    entryNo;
      uint8_t     attr;

      // clear array
      for(j=0; j<8; j++)  aPDOMap[j] = NULL;
      nPDOMapCntr = 0;

      for(j = 0;
          j < (OD_RPDOMappingParameter[i].numberOfMappedObjects <= 8 ? OD_RPDOMappingParameter[i].numberOfMappedObjects : 8);
          j++)
      {

        uint32_t     pMapPointer;
        switch (j) {
        case 0:
          pMapPointer = OD_RPDOMappingParameter[i].mappedObject1;
          break;
        case 1:
          pMapPointer = OD_RPDOMappingParameter[i].mappedObject2;
          break;
        case 2:
          pMapPointer = OD_RPDOMappingParameter[i].mappedObject3;
          break;
        case 3:
          pMapPointer = OD_RPDOMappingParameter[i].mappedObject4;
          break;
        case 4:
          pMapPointer = OD_RPDOMappingParameter[i].mappedObject5;
          break;
        case 5:
          pMapPointer = OD_RPDOMappingParameter[i].mappedObject6;
          break;
        case 6:
          pMapPointer = OD_RPDOMappingParameter[i].mappedObject7;
          break;
        case 7:
          pMapPointer = OD_RPDOMappingParameter[i].mappedObject8;
          break;
        }//switch
        index       = (uint16_t) (pMapPointer >> 16);
        subIndex    = (uint8_t) (pMapPointer >> 8);
        dataLen     = (uint8_t) (pMapPointer);

        if (dataLen&0x07)   {           // wrong data length ? report & abort
          oCO_RPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
          bMappingPossible = false;
          break;  //for(j)
        }

        dataLen >>= 3;                // got data length in bytes
        nPDOlength += dataLen;
        if (nPDOlength > 8)  {       // total RPDO length exceeded ?
          oCO_RPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
          bMappingPossible = false;
          break;  //for(j)
        }

        if (index <=7 && subIndex == 0)  {       // is there a reference to dummy entries?
          uint8_t dummySize = 4;
          if(index<2) dummySize = 0;
          else if(index==2 || index==5) dummySize = 1;
          else if(index==3 || index==6) dummySize = 2;
          if(dummySize < dataLen)   {           // is size of variable big enough for map ? no ? report & abort
            oCO_RPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
            bMappingPossible = false;
            break;  //for(j)
          }
          nPDOMapCntr += dummySize;         // "insert" dummy mapping
        }//if (index <=7 && subIndex == 0)
        else  {

          entryNo = oCO_RPDO.m_pCO_OD_Interface -> nCO_OD_Find(index);        // trying to find object in Object Dictionary
          if ( (entryNo == 0xFFFF) ||
              (subIndex > oCO_RPDO.m_pCO_OD_Interface -> nCO_OD_GetMaxSubindex(entryNo)))  { // doesn't object exist in OD?
            oCO_RPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
            bMappingPossible = false;
            break;  //for(j)
          }
          attr = oCO_RPDO.m_pCO_OD_Interface -> nCO_OD_GetAttribute(entryNo,subIndex);                      // got attributes
          if (!((attr & CO_ODA_RPDO_MAPABLE) && (attr & CO_ODA_WRITEABLE)))  {                 // isn't object Mappable for RPDO?
            oCO_RPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
            bMappingPossible = false;
            break;  //for(j)
          }
          if (oCO_RPDO.m_pCO_OD_Interface -> nCO_OD_GetLength(entryNo,subIndex) < dataLen) {                // got length
            oCO_RPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, pMapPointer);
            bMappingPossible = false;
            break;  //for(j)
          }

          // filling map vector
          uint8_t*    pData =
              (uint8_t*) oCO_RPDO.m_pCO_OD_Interface -> pCO_OD_GetDataPointer(entryNo,subIndex);    // got pointer to data
          while (dataLen > 0)  {
            //vecPDOMap.push_back(pData);
            aPDOMap[nPDOMapCntr] = pData;
            nPDOMapCntr++;
            pData++;
            dataLen--;
          }

        }//if-else (index <=7 && subIndex == 0)
      }//for(j)

      if (bMappingPossible) {                   // at last mapping check complete, is it all right ? adding record to map
        j=0;
        while ((xRPDO_Map[j].m_nCOB_ID != 0) && (j<20))   j++;
        if (j >= 20)  j=19;
        switch (i)  {
        case 0:
        case 1:
        case 2:
        case 3:
          xRPDO_Map[j].m_nCOB_ID = OD_RPDOCommunicationParameter[i].COB_IDUsedByRPDO + OD_CANNodeID;
          break;
        default:
          xRPDO_Map[j].m_nCOB_ID = OD_RPDOCommunicationParameter[i].COB_IDUsedByRPDO;
          break;
        }//switch
        for(uint8_t k=0; k<8; k++)
          (xRPDO_Map[j]).m_aPDOMap[k] = aPDOMap[k];
        xRPDO_Map[j].m_nSize = nPDOMapCntr;
      }//if (bMappingPossible)

    }//if ( (OD_RPDOCommunicationParameter[i].COB_IDUsedByRPDO & PDO_VALID_MASK) == 0)
  }//for(i)

  // get number of entries in map
  nPDOMapCntr=0;
  while ((xRPDO_Map[nPDOMapCntr].m_nCOB_ID != 0) && (nPDOMapCntr<20))   nPDOMapCntr++;

  // main part
  for(;;)   {

    xSemaphoreTake(oCO_RPDO.m_xBinarySemaphore, portMAX_DELAY);         // try to take semaphore forever
    while (xQueueReceive(oCO_RPDO.m_xQueueHandle_NMTStateChange,
      &nNMTStateSelf, 0) == pdTRUE);                                       // retreving all state change messages

    if (nNMTStateSelf != CO_NMT_OPERATIONAL)                                // not in operational state ?
      xQueueReset(oCO_RPDO.m_xQueueHandle_CANReceive);          // delete all RPDO messages
    else  {

      bool  bODChanged = false;                                             // flag: have new info from RPDO

      while (xQueueReceive(oCO_RPDO.m_xQueueHandle_CANReceive,
          &CO_CanMsg, 0) == pdTRUE)  {                                      // try to get new RPDO messages

        for (i=0; i < nPDOMapCntr; i++) {
          if (xRPDO_Map[i].m_nCOB_ID == CO_CanMsg.m_nStdId) {                // try to find received CAN message ID in map

            if (CO_CanMsg.m_nDLC < xRPDO_Map[i].m_nSize)  {                  // is message size incorrect ?
              oCO_RPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_RPDO_WRONG_LENGTH, CO_EMC_PDO_LENGTH,
                  CO_CanMsg.m_nStdId);  // report
            }
            else  {
              if (CO_CanMsg.m_nDLC > xRPDO_Map[i].m_nSize)  {   // is message size exceeded ?
                oCO_RPDO.m_pCO_NMT_EMCY -> bSignalErrorOccured(CO_EM_RPDO_WRONG_LENGTH, CO_EMC_PDO_LENGTH_EXC,
                    CO_CanMsg.m_nStdId);  // report
              }
              for(j = 0; j < xRPDO_Map[i].m_nSize; j++)  {
                if ( (xRPDO_Map[i]).m_aPDOMap[j] != NULL ) {                    // not dummy byte map ?
                  if ( CO_CanMsg.m_aData[j] != *((xRPDO_Map[i]).m_aPDOMap[j]) ) {    // new data ?
                    *((xRPDO_Map[i]).m_aPDOMap[j]) = CO_CanMsg.m_aData[j];           // copy data
                    bODChanged = true;                                      // set new info flag
                  }
                }
              }//for
            }//if-else (CO_CanMsg.DLC < xRPDO_Map[i].m_nSize)
            break;

          }//if (xRPDO_Map[i].m_nCOB_ID == CO_CanMsg.StdId)
        }//for (i=0; i < nPDOMapCntr; i++)

      }//while

      if (bODChanged)
        oCO_RPDO.m_pUserInterface -> vSignalDOChanged();       // signal to user task

    }//if (nNMTStateSelf != CO_NMT_OPERATIONAL)
  }//for
}//vCO_RPDO_Task --------------------------------------------------------------
