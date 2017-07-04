/*
 * CANopen Process Data Object.
 *
 * @file        CO_PDO.c
 * @ingroup     CO_PDO
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
 */


#include "CO_driver.h"
#include "CO_SDO.h"
#include "CO_Emergency.h"
#include "CO_NMT_Heartbeat.h"
#include "CO_SYNC.h"
#include "CO_PDO.h"
#include <string.h>

/*
 * Read received message from CAN module.
 *
 * Function will be called (by CAN receive interrupt) every time, when CAN
 * message with correct identifier will be received. For more information and
 * description of parameters see file CO_driver.h.
 * If new message arrives and previous message wasn't processed yet, then
 * previous message will be lost and overwritten by new message. That's OK with PDOs.
 */
static void CO_PDO_receive(void *object, const CO_CANrxMsg_t *msg){
    CO_RPDO_t *RPDO;

    RPDO = (CO_RPDO_t*)object;   /* this is the correct pointer type of the first argument */

    if( (RPDO->valid) &&
        (*RPDO->operatingState == CO_NMT_OPERATIONAL) &&
        (msg->DLC >= RPDO->dataLength))
    {
        if(RPDO->synchronous && RPDO->SYNC->CANrxToggle) {
            /* copy data into second buffer and set 'new message' flag */
            RPDO->CANrxData[1][0] = msg->data[0];
            RPDO->CANrxData[1][1] = msg->data[1];
            RPDO->CANrxData[1][2] = msg->data[2];
            RPDO->CANrxData[1][3] = msg->data[3];
            RPDO->CANrxData[1][4] = msg->data[4];
            RPDO->CANrxData[1][5] = msg->data[5];
            RPDO->CANrxData[1][6] = msg->data[6];
            RPDO->CANrxData[1][7] = msg->data[7];

            RPDO->CANrxNew[1] = true;
        }
        else {
            /* copy data into default buffer and set 'new message' flag */
            RPDO->CANrxData[0][0] = msg->data[0];
            RPDO->CANrxData[0][1] = msg->data[1];
            RPDO->CANrxData[0][2] = msg->data[2];
            RPDO->CANrxData[0][3] = msg->data[3];
            RPDO->CANrxData[0][4] = msg->data[4];
            RPDO->CANrxData[0][5] = msg->data[5];
            RPDO->CANrxData[0][6] = msg->data[6];
            RPDO->CANrxData[0][7] = msg->data[7];

            RPDO->CANrxNew[0] = true;
        }
    }
}


/*
 * Configure RPDO Communication parameter.
 *
 * Function is called from commuincation reset or when parameter changes.
 *
 * Function configures following variable from CO_RPDO_t: _valid_. It also
 * configures CAN rx buffer. If configuration fails, emergency message is send
 * and device is not able to enter NMT operational.
 *
 * @param RPDO RPDO object.
 * @param COB_IDUsedByRPDO _RPDO communication parameter_, _COB-ID for PDO_ variable
 * from Object dictionary (index 0x1400+, subindex 1).
 */
static void CO_RPDOconfigCom(CO_RPDO_t* RPDO, uint32_t COB_IDUsedByRPDO){
    uint16_t ID;
    CO_ReturnError_t r;

    ID = (uint16_t)COB_IDUsedByRPDO;

    /* is RPDO used? */
    if((COB_IDUsedByRPDO & 0xBFFFF800L) == 0 && RPDO->dataLength && ID){
        /* is used default COB-ID? */
        if(ID == RPDO->defaultCOB_ID) ID += RPDO->nodeId;
        RPDO->valid = true;
        RPDO->synchronous = (RPDO->RPDOCommPar->transmissionType <= 240) ? true : false;
    }
    else{
        ID = 0;
        RPDO->valid = false;
        RPDO->CANrxNew[0] = RPDO->CANrxNew[1] = false;
    }
    r = CO_CANrxBufferInit(
            RPDO->CANdevRx,         /* CAN device */
            RPDO->CANdevRxIdx,      /* rx buffer index */
            ID,                     /* CAN identifier */
            0x7FF,                  /* mask */
            0,                      /* rtr */
            (void*)RPDO,            /* object passed to receive function */
            CO_PDO_receive);        /* this function will process received message */
    if(r != CO_ERROR_NO){
        RPDO->valid = false;
        RPDO->CANrxNew[0] = RPDO->CANrxNew[1] = false;
    }
}


/*
 * Configure TPDO Communication parameter.
 *
 * Function is called from commuincation reset or when parameter changes.
 *
 * Function configures following variable from CO_TPDO_t: _valid_. It also
 * configures CAN tx buffer. If configuration fails, emergency message is send
 * and device is not able to enter NMT operational.
 *
 * @param TPDO TPDO object.
 * @param COB_IDUsedByTPDO _TPDO communication parameter_, _COB-ID for PDO_ variable
 * from Object dictionary (index 0x1400+, subindex 1).
 * @param syncFlag Indicate, if TPDO is synchronous.
 */
static void CO_TPDOconfigCom(CO_TPDO_t* TPDO, uint32_t COB_IDUsedByTPDO, uint8_t syncFlag){
    uint16_t ID;

    ID = (uint16_t)COB_IDUsedByTPDO;

    /* is TPDO used? */
    if((COB_IDUsedByTPDO & 0xBFFFF800L) == 0 && TPDO->dataLength && ID){
        /* is used default COB-ID? */
        if(ID == TPDO->defaultCOB_ID) ID += TPDO->nodeId;
        TPDO->valid = true;
    }
    else{
        ID = 0;
        TPDO->valid = false;
    }

    TPDO->CANtxBuff = CO_CANtxBufferInit(
            TPDO->CANdevTx,            /* CAN device */
            TPDO->CANdevTxIdx,         /* index of specific buffer inside CAN module */
            ID,                        /* CAN identifier */
            0,                         /* rtr */
            TPDO->dataLength,          /* number of data bytes */
            syncFlag);                 /* synchronous message flag bit */

    if(TPDO->CANtxBuff == 0){
        TPDO->valid = false;
    }
}


/*
 * Find mapped variable in Object Dictionary.
 *
 * Function is called from CO_R(T)PDOconfigMap or when mapping parameter changes.
 *
 * @param SDO SDO object.
 * @param map PDO mapping parameter.
 * @param R_T 0 for RPDO map, 1 for TPDO map.
 * @param ppData Pointer to returning parameter: pointer to data of mapped variable.
 * @param pLength Pointer to returning parameter: *add* length of mapped variable.
 * @param pSendIfCOSFlags Pointer to returning parameter: sendIfCOSFlags variable.
 * @param pIsMultibyteVar Pointer to returning parameter: true for multibyte variable.
 *
 * @return 0 on success, otherwise SDO abort code.
 */
static uint32_t CO_PDOfindMap(
        CO_SDO_t               *SDO,
        uint32_t                map,
        uint8_t                 R_T,
        uint8_t               **ppData,
        uint8_t                *pLength,
        uint8_t                *pSendIfCOSFlags,
        uint8_t                *pIsMultibyteVar)
{
    uint16_t entryNo;
    uint16_t index;
    uint8_t subIndex;
    uint8_t dataLen;
    uint8_t objectLen;
    uint8_t attr;

    index = (uint16_t)(map>>16);
    subIndex = (uint8_t)(map>>8);
    dataLen = (uint8_t) map;   /* data length in bits */

    /* data length must be byte aligned */
    if(dataLen&0x07) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */

    dataLen >>= 3;    /* new data length is in bytes */
    *pLength += dataLen;

    /* total PDO length can not be more than 8 bytes */
    if(*pLength > 8) return CO_SDO_AB_MAP_LEN;  /* The number and length of the objects to be mapped would exceed PDO length. */

    /* is there a reference to dummy entries */
    if(index <=7 && subIndex == 0){
        static uint32_t dummyTX = 0;
        static uint32_t dummyRX;
        uint8_t dummySize = 4;

        if(index<2) dummySize = 0;
        else if(index==2 || index==5) dummySize = 1;
        else if(index==3 || index==6) dummySize = 2;

        /* is size of variable big enough for map */
        if(dummySize < dataLen) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */

        /* Data and ODE pointer */
        if(R_T == 0) *ppData = (uint8_t*) &dummyRX;
        else         *ppData = (uint8_t*) &dummyTX;

        return 0;
    }

    /* find object in Object Dictionary */
    entryNo = CO_OD_find(SDO, index);

    /* Does object exist in OD? */
    if(entryNo == 0xFFFF || subIndex > SDO->OD[entryNo].maxSubIndex)
        return CO_SDO_AB_NOT_EXIST;   /* Object does not exist in the object dictionary. */

    attr = CO_OD_getAttribute(SDO, entryNo, subIndex);
    /* Is object Mappable for RPDO? */
    if(R_T==0 && !((attr&CO_ODA_RPDO_MAPABLE) && (attr&CO_ODA_WRITEABLE))) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */
    /* Is object Mappable for TPDO? */
    if(R_T!=0 && !((attr&CO_ODA_TPDO_MAPABLE) && (attr&CO_ODA_READABLE))) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */

    /* is size of variable big enough for map */
    objectLen = CO_OD_getLength(SDO, entryNo, subIndex);
    if(objectLen < dataLen) return CO_SDO_AB_NO_MAP;   /* Object cannot be mapped to the PDO. */

    /* mark multibyte variable */
    *pIsMultibyteVar = (attr&CO_ODA_MB_VALUE) ? 1 : 0;

    /* pointer to data */
    *ppData = (uint8_t*) CO_OD_getDataPointer(SDO, entryNo, subIndex);
#ifdef CO_BIG_ENDIAN
    /* skip unused MSB bytes */
    if(*pIsMultibyteVar){
        *ppData += objectLen - dataLen;
    }
#endif

    /* setup change of state flags */
    if(attr&CO_ODA_TPDO_DETECT_COS){
        int16_t i;
        for(i=*pLength-dataLen; i<*pLength; i++){
            *pSendIfCOSFlags |= 1<<i;
        }
    }

    return 0;
}


/*
 * Configure RPDO Mapping parameter.
 *
 * Function is called from communication reset or when parameter changes.
 *
 * Function configures following variables from CO_RPDO_t: _dataLength_ and
 * _mapPointer_.
 *
 * @param RPDO RPDO object.
 * @param noOfMappedObjects Number of mapped object (from OD).
 *
 * @return 0 on success, otherwise SDO abort code.
 */
static uint32_t CO_RPDOconfigMap(CO_RPDO_t* RPDO, uint8_t noOfMappedObjects){
    int16_t i;
    uint8_t length = 0;
    uint32_t ret = 0;
    const uint32_t* pMap = &RPDO->RPDOMapPar->mappedObject1;

    for(i=noOfMappedObjects; i>0; i--){
        int16_t j;
        uint8_t* pData;
        uint8_t dummy = 0;
        uint8_t prevLength = length;
        uint8_t MBvar;
        uint32_t map = *(pMap++);

        /* function do much checking of errors in map */
        ret = CO_PDOfindMap(
                RPDO->SDO,
                map,
                0,
                &pData,
                &length,
                &dummy,
                &MBvar);
        if(ret){
            length = 0;
            CO_errorReport(RPDO->em, CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, map);
            break;
        }

        /* write PDO data pointers */
#ifdef CO_BIG_ENDIAN
        if(MBvar){
            for(j=length-1; j>=prevLength; j--)
                RPDO->mapPointer[j] = pData++;
        }
        else{
            for(j=prevLength; j<length; j++)
                RPDO->mapPointer[j] = pData++;
        }
#else
        for(j=prevLength; j<length; j++){
            RPDO->mapPointer[j] = pData++;
        }
#endif

    }

    RPDO->dataLength = length;

    return ret;
}


/*
 * Configure TPDO Mapping parameter.
 *
 * Function is called from communication reset or when parameter changes.
 *
 * Function configures following variables from CO_TPDO_t: _dataLength_,
 * _mapPointer_ and _sendIfCOSFlags_.
 *
 * @param TPDO TPDO object.
 * @param noOfMappedObjects Number of mapped object (from OD).
 *
 * @return 0 on success, otherwise SDO abort code.
 */
static uint32_t CO_TPDOconfigMap(CO_TPDO_t* TPDO, uint8_t noOfMappedObjects){
    int16_t i;
    uint8_t length = 0;
    uint32_t ret = 0;
    const uint32_t* pMap = &TPDO->TPDOMapPar->mappedObject1;

    TPDO->sendIfCOSFlags = 0;

    for(i=noOfMappedObjects; i>0; i--){
        int16_t j;
        uint8_t* pData;
        uint8_t prevLength = length;
        uint8_t MBvar;
        uint32_t map = *(pMap++);

        /* function do much checking of errors in map */
        ret = CO_PDOfindMap(
                TPDO->SDO,
                map,
                1,
                &pData,
                &length,
                &TPDO->sendIfCOSFlags,
                &MBvar);
        if(ret){
            length = 0;
            CO_errorReport(TPDO->em, CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, map);
            break;
        }

        /* write PDO data pointers */
#ifdef CO_BIG_ENDIAN
        if(MBvar){
            for(j=length-1; j>=prevLength; j--)
                TPDO->mapPointer[j] = pData++;
        }
        else{
            for(j=prevLength; j<length; j++)
                TPDO->mapPointer[j] = pData++;
        }
#else
        for(j=prevLength; j<length; j++){
            TPDO->mapPointer[j] = pData++;
        }
#endif

    }

    TPDO->dataLength = length;

    return ret;
}


/*
 * Function for accessing _RPDO communication parameter_ (index 0x1400+) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static CO_SDO_abortCode_t CO_ODF_RPDOcom(CO_ODF_arg_t *ODF_arg){
    CO_RPDO_t *RPDO;

    RPDO = (CO_RPDO_t*) ODF_arg->object;

    /* Reading Object Dictionary variable */
    if(ODF_arg->reading){
        if(ODF_arg->subIndex == 1){
            uint32_t *value = (uint32_t*) ODF_arg->data;

            /* if default COB ID is used, write default value here */
            if(((*value)&0xFFFF) == RPDO->defaultCOB_ID && RPDO->defaultCOB_ID)
                *value += RPDO->nodeId;

            /* If PDO is not valid, set bit 31 */
            if(!RPDO->valid) *value |= 0x80000000L;
        }
        return CO_SDO_AB_NONE;
    }

    /* Writing Object Dictionary variable */
    if(RPDO->restrictionFlags & 0x04)
        return CO_SDO_AB_READONLY;  /* Attempt to write a read only object. */
    if(*RPDO->operatingState == CO_NMT_OPERATIONAL && (RPDO->restrictionFlags & 0x01))
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */

    if(ODF_arg->subIndex == 1){   /* COB_ID */
        uint32_t *value = (uint32_t*) ODF_arg->data;

        /* bits 11...29 must be zero */
        if(*value & 0x3FFF8000L)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* if default COB-ID is being written, write defaultCOB_ID without nodeId */
        if(((*value)&0xFFFF) == (RPDO->defaultCOB_ID + RPDO->nodeId)){
            *value &= 0xC0000000L;
            *value += RPDO->defaultCOB_ID;
        }

        /* if PDO is valid, bits 0..29 can not be changed */
        if(RPDO->valid && ((*value ^ RPDO->RPDOCommPar->COB_IDUsedByRPDO) & 0x3FFFFFFFL))
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* configure RPDO */
        CO_RPDOconfigCom(RPDO, *value);
    }
    else if(ODF_arg->subIndex == 2){   /* Transmission_type */
        uint8_t *value = (uint8_t*) ODF_arg->data;
        bool_t synchronousPrev = RPDO->synchronous;

        /* values from 241...253 are not valid */
        if(*value >= 241 && *value <= 253)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        RPDO->synchronous = (*value <= 240) ? true : false;

        /* Remove old message from second buffer. */
        if(RPDO->synchronous != synchronousPrev) {
            RPDO->CANrxNew[1] = false;
        }
    }

    return CO_SDO_AB_NONE;
}


/*
 * Function for accessing _TPDO communication parameter_ (index 0x1800+) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static CO_SDO_abortCode_t CO_ODF_TPDOcom(CO_ODF_arg_t *ODF_arg){
    CO_TPDO_t *TPDO;

    TPDO = (CO_TPDO_t*) ODF_arg->object;

    if(ODF_arg->subIndex == 4) return CO_SDO_AB_SUB_UNKNOWN;  /* Sub-index does not exist. */

    /* Reading Object Dictionary variable */
    if(ODF_arg->reading){
        if(ODF_arg->subIndex == 1){   /* COB_ID */
            uint32_t *value = (uint32_t*) ODF_arg->data;

            /* if default COB ID is used, write default value here */
            if(((*value)&0xFFFF) == TPDO->defaultCOB_ID && TPDO->defaultCOB_ID)
                *value += TPDO->nodeId;

            /* If PDO is not valid, set bit 31 */
            if(!TPDO->valid) *value |= 0x80000000L;
        }
        return CO_SDO_AB_NONE;
    }

    /* Writing Object Dictionary variable */
    if(TPDO->restrictionFlags & 0x04)
        return CO_SDO_AB_READONLY;  /* Attempt to write a read only object. */
    if(*TPDO->operatingState == CO_NMT_OPERATIONAL && (TPDO->restrictionFlags & 0x01))
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */

    if(ODF_arg->subIndex == 1){   /* COB_ID */
        uint32_t *value = (uint32_t*) ODF_arg->data;

        /* bits 11...29 must be zero */
        if(*value & 0x3FFF8000L)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* if default COB-ID is being written, write defaultCOB_ID without nodeId */
        if(((*value)&0xFFFF) == (TPDO->defaultCOB_ID + TPDO->nodeId)){
            *value &= 0xC0000000L;
            *value += TPDO->defaultCOB_ID;
        }

        /* if PDO is valid, bits 0..29 can not be changed */
        if(TPDO->valid && ((*value ^ TPDO->TPDOCommPar->COB_IDUsedByTPDO) & 0x3FFFFFFFL))
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* configure TPDO */
        CO_TPDOconfigCom(TPDO, *value, TPDO->CANtxBuff->syncFlag);
        TPDO->syncCounter = 255;
    }
    else if(ODF_arg->subIndex == 2){   /* Transmission_type */
        uint8_t *value = (uint8_t*) ODF_arg->data;

        /* values from 241...253 are not valid */
        if(*value >= 241 && *value <= 253)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */
        TPDO->CANtxBuff->syncFlag = (*value <= 240) ? 1 : 0;
        TPDO->syncCounter = 255;
    }
    else if(ODF_arg->subIndex == 3){   /* Inhibit_Time */
        /* if PDO is valid, value can not be changed */
        if(TPDO->valid)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        TPDO->inhibitTimer = 0;
    }
    else if(ODF_arg->subIndex == 5){   /* Event_Timer */
        uint16_t *value = (uint16_t*) ODF_arg->data;

        TPDO->eventTimer = ((uint32_t) *value) * 1000;
    }
    else if(ODF_arg->subIndex == 6){   /* SYNC start value */
        uint8_t *value = (uint8_t*) ODF_arg->data;

        /* if PDO is valid, value can not be changed */
        if(TPDO->valid)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* values from 240...255 are not valid */
        if(*value > 240)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */
    }

    return CO_SDO_AB_NONE;
}


/*
 * Function for accessing _RPDO mapping parameter_ (index 0x1600+) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static CO_SDO_abortCode_t CO_ODF_RPDOmap(CO_ODF_arg_t *ODF_arg){
    CO_RPDO_t *RPDO;

    RPDO = (CO_RPDO_t*) ODF_arg->object;

    /* Reading Object Dictionary variable */
    if(ODF_arg->reading){
        uint8_t *value = (uint8_t*) ODF_arg->data;

        if(ODF_arg->subIndex == 0){
            /* If there is error in mapping, dataLength is 0, so numberOfMappedObjects is 0. */
            if(!RPDO->dataLength) *value = 0;
        }
        return CO_SDO_AB_NONE;
    }

    /* Writing Object Dictionary variable */
    if(RPDO->restrictionFlags & 0x08)
        return CO_SDO_AB_READONLY;  /* Attempt to write a read only object. */
    if(*RPDO->operatingState == CO_NMT_OPERATIONAL && (RPDO->restrictionFlags & 0x02))
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
    if(RPDO->valid)
        return CO_SDO_AB_UNSUPPORTED_ACCESS;  /* Unsupported access to an object. */

    /* numberOfMappedObjects */
    if(ODF_arg->subIndex == 0){
        uint8_t *value = (uint8_t*) ODF_arg->data;

        if(*value > 8)
            return CO_SDO_AB_MAP_LEN;  /* Number and length of object to be mapped exceeds PDO length. */

        /* configure mapping */
        return CO_RPDOconfigMap(RPDO, *value);
    }

    /* mappedObject */
    else{
        uint32_t *value = (uint32_t*) ODF_arg->data;
        uint8_t* pData;
        uint8_t length = 0;
        uint8_t dummy = 0;
        uint8_t MBvar;

        if(RPDO->dataLength)
            return CO_SDO_AB_UNSUPPORTED_ACCESS;  /* Unsupported access to an object. */

        /* verify if mapping is correct */
        return CO_PDOfindMap(
                RPDO->SDO,
               *value,
                0,
               &pData,
               &length,
               &dummy,
               &MBvar);
    }

    return CO_SDO_AB_NONE;
}


/*
 * Function for accessing _TPDO mapping parameter_ (index 0x1A00+) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static CO_SDO_abortCode_t CO_ODF_TPDOmap(CO_ODF_arg_t *ODF_arg){
    CO_TPDO_t *TPDO;

    TPDO = (CO_TPDO_t*) ODF_arg->object;

    /* Reading Object Dictionary variable */
    if(ODF_arg->reading){
        uint8_t *value = (uint8_t*) ODF_arg->data;

        if(ODF_arg->subIndex == 0){
            /* If there is error in mapping, dataLength is 0, so numberOfMappedObjects is 0. */
            if(!TPDO->dataLength) *value = 0;
        }
        return CO_SDO_AB_NONE;
    }

    /* Writing Object Dictionary variable */
    if(TPDO->restrictionFlags & 0x08)
        return CO_SDO_AB_READONLY;  /* Attempt to write a read only object. */
    if(*TPDO->operatingState == CO_NMT_OPERATIONAL && (TPDO->restrictionFlags & 0x02))
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
    if(TPDO->valid)
        return CO_SDO_AB_UNSUPPORTED_ACCESS;  /* Unsupported access to an object. */

    /* numberOfMappedObjects */
    if(ODF_arg->subIndex == 0){
        uint8_t *value = (uint8_t*) ODF_arg->data;

        if(*value > 8)
            return CO_SDO_AB_MAP_LEN;  /* Number and length of object to be mapped exceeds PDO length. */

        /* configure mapping */
        return CO_TPDOconfigMap(TPDO, *value);
    }

    /* mappedObject */
    else{
        uint32_t *value = (uint32_t*) ODF_arg->data;
        uint8_t* pData;
        uint8_t length = 0;
        uint8_t dummy = 0;
        uint8_t MBvar;

        if(TPDO->dataLength)
            return CO_SDO_AB_UNSUPPORTED_ACCESS;  /* Unsupported access to an object. */

        /* verify if mapping is correct */
        return CO_PDOfindMap(
                TPDO->SDO,
               *value,
                1,
               &pData,
               &length,
               &dummy,
               &MBvar);
    }

    return CO_SDO_AB_NONE;
}


/******************************************************************************/
CO_ReturnError_t CO_RPDO_init(
        CO_RPDO_t              *RPDO,
        CO_EM_t                *em,
        CO_SDO_t               *SDO,
        CO_SYNC_t              *SYNC,
        uint8_t                *operatingState,
        uint8_t                 nodeId,
        uint16_t                defaultCOB_ID,
        uint8_t                 restrictionFlags,
        const CO_RPDOCommPar_t *RPDOCommPar,
        const CO_RPDOMapPar_t  *RPDOMapPar,
        uint16_t                idx_RPDOCommPar,
        uint16_t                idx_RPDOMapPar,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx)
{
    /* verify arguments */
    if(RPDO==NULL || em==NULL || SDO==NULL || SYNC==NULL || operatingState==NULL ||
        RPDOCommPar==NULL || RPDOMapPar==NULL || CANdevRx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    RPDO->em = em;
    RPDO->SDO = SDO;
    RPDO->SYNC = SYNC;
    RPDO->RPDOCommPar = RPDOCommPar;
    RPDO->RPDOMapPar = RPDOMapPar;
    RPDO->operatingState = operatingState;
    RPDO->nodeId = nodeId;
    RPDO->defaultCOB_ID = defaultCOB_ID;
    RPDO->restrictionFlags = restrictionFlags;

    /* Configure Object dictionary entry at index 0x1400+ and 0x1600+ */
    CO_OD_configure(SDO, idx_RPDOCommPar, CO_ODF_RPDOcom, (void*)RPDO, 0, 0);
    CO_OD_configure(SDO, idx_RPDOMapPar, CO_ODF_RPDOmap, (void*)RPDO, 0, 0);

    /* configure communication and mapping */
    RPDO->CANrxNew[0] = RPDO->CANrxNew[1] = false;
    RPDO->CANdevRx = CANdevRx;
    RPDO->CANdevRxIdx = CANdevRxIdx;

    CO_RPDOconfigMap(RPDO, RPDOMapPar->numberOfMappedObjects);
    CO_RPDOconfigCom(RPDO, RPDOCommPar->COB_IDUsedByRPDO);

    return CO_ERROR_NO;
}


/******************************************************************************/
CO_ReturnError_t CO_TPDO_init(
        CO_TPDO_t              *TPDO,
        CO_EM_t                *em,
        CO_SDO_t               *SDO,
        uint8_t                *operatingState,
        uint8_t                 nodeId,
        uint16_t                defaultCOB_ID,
        uint8_t                 restrictionFlags,
        const CO_TPDOCommPar_t *TPDOCommPar,
        const CO_TPDOMapPar_t  *TPDOMapPar,
        uint16_t                idx_TPDOCommPar,
        uint16_t                idx_TPDOMapPar,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx)
{
    /* verify arguments */
    if(TPDO==NULL || em==NULL || SDO==NULL || operatingState==NULL ||
        TPDOCommPar==NULL || TPDOMapPar==NULL || CANdevTx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    TPDO->em = em;
    TPDO->SDO = SDO;
    TPDO->TPDOCommPar = TPDOCommPar;
    TPDO->TPDOMapPar = TPDOMapPar;
    TPDO->operatingState = operatingState;
    TPDO->nodeId = nodeId;
    TPDO->defaultCOB_ID = defaultCOB_ID;
    TPDO->restrictionFlags = restrictionFlags;

    /* Configure Object dictionary entry at index 0x1800+ and 0x1A00+ */
    CO_OD_configure(SDO, idx_TPDOCommPar, CO_ODF_TPDOcom, (void*)TPDO, 0, 0);
    CO_OD_configure(SDO, idx_TPDOMapPar, CO_ODF_TPDOmap, (void*)TPDO, 0, 0);

    /* configure communication and mapping */
    TPDO->CANdevTx = CANdevTx;
    TPDO->CANdevTxIdx = CANdevTxIdx;
    TPDO->syncCounter = 255;
    TPDO->inhibitTimer = 0;
    TPDO->eventTimer = ((uint32_t) TPDOCommPar->eventTimer) * 1000;
    if(TPDOCommPar->transmissionType>=254) TPDO->sendRequest = 1;

    CO_TPDOconfigMap(TPDO, TPDOMapPar->numberOfMappedObjects);
    CO_TPDOconfigCom(TPDO, TPDOCommPar->COB_IDUsedByTPDO, ((TPDOCommPar->transmissionType<=240) ? 1 : 0));

    if((TPDOCommPar->transmissionType>240 &&
         TPDOCommPar->transmissionType<254) ||
         TPDOCommPar->SYNCStartValue>240){
            TPDO->valid = false;
    }

    return CO_ERROR_NO;
}


/******************************************************************************/
uint8_t CO_TPDOisCOS(CO_TPDO_t *TPDO){

    /* Prepare TPDO data automatically from Object Dictionary variables */
    uint8_t* pPDOdataByte;
    uint8_t** ppODdataByte;

    pPDOdataByte = &TPDO->CANtxBuff->data[TPDO->dataLength];
    ppODdataByte = &TPDO->mapPointer[TPDO->dataLength];

    switch(TPDO->dataLength){
        case 8: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x80)) return 1;
        case 7: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x40)) return 1;
        case 6: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x20)) return 1;
        case 5: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x10)) return 1;
        case 4: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x08)) return 1;
        case 3: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x04)) return 1;
        case 2: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x02)) return 1;
        case 1: if(*(--pPDOdataByte) != **(--ppODdataByte) && (TPDO->sendIfCOSFlags&0x01)) return 1;
    }

    return 0;
}

//#define TPDO_CALLS_EXTENSION
/******************************************************************************/
int16_t CO_TPDOsend(CO_TPDO_t *TPDO){
    int16_t i;
    uint8_t* pPDOdataByte;
    uint8_t** ppODdataByte;

#ifdef TPDO_CALLS_EXTENSION
    if(TPDO->SDO->ODExtensions){
        /* for each mapped OD, check mapping to see if an OD extension is available, and call it if it is */
        const uint32_t* pMap = &TPDO->TPDOMapPar->mappedObject1;
        CO_SDO_t *pSDO = TPDO->SDO;

        for(i=TPDO->TPDOMapPar->numberOfMappedObjects; i>0; i--){
            uint32_t map = *(pMap++);
            uint16_t index = (uint16_t)(map>>16);
            uint8_t subIndex = (uint8_t)(map>>8);
            uint16_t entryNo = CO_OD_find(pSDO, index);
            if ( entryNo == 0xFFFF ) continue;
            CO_OD_extension_t *ext = &pSDO->ODExtensions[entryNo];
            if( ext->pODFunc == NULL) continue;
            CO_ODF_arg_t ODF_arg;
            memset((void*)&ODF_arg, 0, sizeof(CO_ODF_arg_t));
            ODF_arg.reading = true;
            ODF_arg.index = index;
            ODF_arg.subIndex = subIndex;
            ODF_arg.object = ext->object;
            ODF_arg.attribute = CO_OD_getAttribute(pSDO, entryNo, subIndex);
            ODF_arg.pFlags = CO_OD_getFlagsPointer(pSDO, entryNo, subIndex);
            ODF_arg.data = pSDO->OD[entryNo].pData;
            ODF_arg.dataLength = CO_OD_getLength(pSDO, entryNo, subIndex);
            ext->pODFunc(&ODF_arg);
        }
    }
#endif
    i = TPDO->dataLength;
    pPDOdataByte = &TPDO->CANtxBuff->data[0];
    ppODdataByte = &TPDO->mapPointer[0];

    /* Copy data from Object dictionary. */
    for(; i>0; i--) {
        *(pPDOdataByte++) = **(ppODdataByte++);
    }

    TPDO->sendRequest = 0;

    return CO_CANsend(TPDO->CANdevTx, TPDO->CANtxBuff);
}

//#define RPDO_CALLS_EXTENSION
/******************************************************************************/
void CO_RPDO_process(CO_RPDO_t *RPDO, bool_t syncWas){

    if(!RPDO->valid || !(*RPDO->operatingState == CO_NMT_OPERATIONAL))
    {
        RPDO->CANrxNew[0] = RPDO->CANrxNew[1] = false;
    }
    else if(!RPDO->synchronous || syncWas)
    {
        uint8_t bufNo = 0;

        /* Determine, which of the two rx buffers, contains relevant message. */
        if(RPDO->synchronous && !RPDO->SYNC->CANrxToggle) {
            bufNo = 1;
        }

        while(RPDO->CANrxNew[bufNo]){
            int16_t i;
            uint8_t* pPDOdataByte;
            uint8_t** ppODdataByte;

            i = RPDO->dataLength;
            pPDOdataByte = &RPDO->CANrxData[bufNo][0];
            ppODdataByte = &RPDO->mapPointer[0];

            /* Copy data to Object dictionary. If between the copy operation CANrxNew
             * is set to true by receive thread, then copy the latest data again. */
            RPDO->CANrxNew[bufNo] = false;
            for(; i>0; i--) {
                **(ppODdataByte++) = *(pPDOdataByte++);
            }

#ifdef RPDO_CALLS_EXTENSION
            if(RPDO->SDO->ODExtensions){
                /* for each mapped OD, check mapping to see if an OD extension is available, and call it if it is */
                const uint32_t* pMap = &RPDO->RPDOMapPar->mappedObject1;
                CO_SDO_t *pSDO = RPDO->SDO;

                for(i=RPDO->RPDOMapPar->numberOfMappedObjects; i>0; i--){
                    uint32_t map = *(pMap++);
                    uint16_t index = (uint16_t)(map>>16);
                    uint8_t subIndex = (uint8_t)(map>>8);
                    uint16_t entryNo = CO_OD_find(pSDO, index);
                    if ( entryNo == 0xFFFF ) continue;
                    CO_OD_extension_t *ext = &pSDO->ODExtensions[entryNo];
                    if( ext->pODFunc == NULL) continue;
                    CO_ODF_arg_t ODF_arg;
                    memset((void*)&ODF_arg, 0, sizeof(CO_ODF_arg_t));
                    ODF_arg.reading = false;
                    ODF_arg.index = index;
                    ODF_arg.subIndex = subIndex;
                    ODF_arg.object = ext->object;
                    ODF_arg.attribute = CO_OD_getAttribute(pSDO, entryNo, subIndex);
                    ODF_arg.pFlags = CO_OD_getFlagsPointer(pSDO, entryNo, subIndex);
                    ODF_arg.data = pSDO->OD[entryNo].pData;
                    ODF_arg.dataLength = CO_OD_getLength(pSDO, entryNo, subIndex);
                    ext->pODFunc(&ODF_arg);
                }
            }
#endif
        }
    }
}


/******************************************************************************/
void CO_TPDO_process(
        CO_TPDO_t              *TPDO,
        CO_SYNC_t              *SYNC,
        bool_t                  syncWas,
        uint32_t                timeDifference_us)
{
    if(TPDO->valid && *TPDO->operatingState == CO_NMT_OPERATIONAL){

        /* Send PDO by application request or by Event timer */
        if(TPDO->TPDOCommPar->transmissionType >= 253){
            if(TPDO->inhibitTimer == 0 && (TPDO->sendRequest || (TPDO->TPDOCommPar->eventTimer && TPDO->eventTimer == 0))){
                if(CO_TPDOsend(TPDO) == CO_ERROR_NO){
                    /* successfully sent */
                    TPDO->inhibitTimer = ((uint32_t) TPDO->TPDOCommPar->inhibitTime) * 100;
                    TPDO->eventTimer = ((uint32_t) TPDO->TPDOCommPar->eventTimer) * 1000;
                }
            }
        }

        /* Synchronous PDOs */
        else if(SYNC && syncWas){
            /* send synchronous acyclic PDO */
            if(TPDO->TPDOCommPar->transmissionType == 0){
                if(TPDO->sendRequest) CO_TPDOsend(TPDO);
            }
            /* send synchronous cyclic PDO */
            else{
                /* is the start of synchronous TPDO transmission */
                if(TPDO->syncCounter == 255){
                    if(SYNC->counterOverflowValue && TPDO->TPDOCommPar->SYNCStartValue)
                        TPDO->syncCounter = 254;   /* SYNCStartValue is in use */
                    else
                        TPDO->syncCounter = TPDO->TPDOCommPar->transmissionType;
                }
                /* if the SYNCStartValue is in use, start first TPDO after SYNC with matched SYNCStartValue. */
                if(TPDO->syncCounter == 254){
                    if(SYNC->counter == TPDO->TPDOCommPar->SYNCStartValue){
                        TPDO->syncCounter = TPDO->TPDOCommPar->transmissionType;
                        CO_TPDOsend(TPDO);
                    }
                }
                /* Send PDO after every N-th Sync */
                else if(--TPDO->syncCounter == 0){
                    TPDO->syncCounter = TPDO->TPDOCommPar->transmissionType;
                    CO_TPDOsend(TPDO);
                }
            }
        }

    }
    else{
        /* Not operational or valid. Force TPDO first send after operational or valid. */
        if(TPDO->TPDOCommPar->transmissionType>=254) TPDO->sendRequest = 1;
        else                                         TPDO->sendRequest = 0;
    }

    /* update timers */
    TPDO->inhibitTimer = (TPDO->inhibitTimer > timeDifference_us) ? (TPDO->inhibitTimer - timeDifference_us) : 0;
    TPDO->eventTimer = (TPDO->eventTimer > timeDifference_us) ? (TPDO->eventTimer - timeDifference_us) : 0;
}
