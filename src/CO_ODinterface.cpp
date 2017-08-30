/*
 * CO_ODinterface.cpp
 *
 *  Created on: 2 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen Object Dictionary interface
 */

/** Includes *****************************************************************/
#include "CO_OD.h"
#include "CO_ODinterface.h"
#include "CO_SDO.h"

// generate error, if features are not correctly configured for this project
#ifndef CO_OD_NoOfElements
#error Features from CO_OD.h file are not correctly configured for this project!
#endif

/** Defines and constants ****************************************************/
/** Private function prototypes **********************************************/
/** Private typedef, enumeration, class **************************************/
/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_OD_Interface oCO_OD_Interface;             // OD interface object

/** Class methods ************************************************************/

void cCO_OD_Interface::vCO_OD_Init(CO_OD_entry_t const * const  pCO_OD)  {
  m_pCO_OD = (CO_OD_entry_t*) pCO_OD;
}// cCO_OD_Interface::vCO_OD_Init --------------------------------------------

uint16_t cCO_OD_Interface::nCO_OD_Find(uint16_t index) {

  uint16_t cur, min, max;

  min = 0;
  max = CO_OD_NoOfElements - 1;
  while (min < max) {
    cur = (min + max) / 2;
    if (index == (m_pCO_OD[cur]).index) {     // is object matched ?
      return cur;
    }
    if (index < (m_pCO_OD[cur]).index) {
      max = cur;
      if (max)
        max--;
    } else
      min = cur + 1;
  }

  if (min == max) {
    if (index == (m_pCO_OD[min]).index) {     // is object matched ?
      return min;
    }
  }

  return 0xFFFF; // object does not exist in OD
}//cCO_OD_Interface::nCO_OD_find ----------------------------------------------

uint16_t cCO_OD_Interface::nCO_OD_GetLength(uint16_t entryNo, uint8_t subIndex) {

  const CO_OD_entry_t* object = &m_pCO_OD[entryNo];

  if (entryNo == 0xFFFF) {
    return 0;
  }

  if (object->maxSubIndex == 0) {     // Object type is Var
    if (object->pData == 0) {       // data type is domain
      return CO_SDO_BUFFER_SIZE;
    } else {
      return object->length;
    }
  } else if (object->attribute != 0) {  // Object type is Array
    if (subIndex == 0) {
      return 1;
    } else if (object->pData == 0) {     // data type is domain
      return CO_SDO_BUFFER_SIZE;
    } else {
      return object->length;
    }
  } else {                            // Object type is Record
    if (((const CO_OD_entryRecord_t*) (object->pData))[subIndex].pData == 0) {
      /* data type is domain */
      return CO_SDO_BUFFER_SIZE;
    } else {
      return ((const CO_OD_entryRecord_t*) (object->pData))[subIndex].length;
    }
  }
}//cCO_OD_Interface::nCO_OD_getLength -----------------------------------------

uint16_t cCO_OD_Interface::nCO_OD_GetAttribute(uint16_t entryNo, uint8_t subIndex) {

  const CO_OD_entry_t* object = &m_pCO_OD[entryNo];

  if (entryNo == 0xFFFF) {
    return 0;
  }

  if (object->maxSubIndex == 0) {   // Object type is Var
    return object->attribute;
  } else if (object->attribute != 0) {   // Object type is Array
    uint16_t attr = object->attribute;
    if (subIndex == 0) {
      // First subIndex is readonly
      attr &= ~(CO_ODA_WRITEABLE | CO_ODA_RPDO_MAPABLE);
      attr |= CO_ODA_READABLE;
    }
    return attr;
  } else {                            // Object type is Record
    return ((const CO_OD_entryRecord_t*) (object->pData))[subIndex].attribute;
  }
}//cCO_OD_Interface::nCO_OD_getAttribute --------------------------------------

void* cCO_OD_Interface::pCO_OD_GetDataPointer(uint16_t entryNo, uint8_t subIndex) {

  const CO_OD_entry_t* object = &m_pCO_OD[entryNo];

  if (entryNo == 0xFFFF) {
    return 0;
  }

  if (object->maxSubIndex == 0) {   // Object type is Var
    return object->pData;
  } else if (object->attribute != 0) {   // Object type is Array
    if (subIndex == 0) {
      // this is the data, for the subIndex 0 in the array
      return (void*) &object->maxSubIndex;
    } else if (object->pData == 0) {
      // data type is domain
      return 0;
    } else {
      return (void*) (((int8_t*) object->pData) + ((subIndex - 1) * object->length));
    }
  } else {                            // Object Type is Record
    return ((const CO_OD_entryRecord_t*) (object->pData))[subIndex].pData;
  }
}//cCO_OD_Interface::pCO_OD_getDataPointer ------------------------------------

uint8_t cCO_OD_Interface::nCO_OD_GetMaxSubindex(uint16_t entryNo)  {

  const CO_OD_entry_t* object = &m_pCO_OD[entryNo];

  if (entryNo == 0xFFFF) {
    return 0;
  }
  else  {
    return object->maxSubIndex;
  }
}//cCO_OD_Interface::nCO_OD_GetMaxSubindex ------------------------------------
