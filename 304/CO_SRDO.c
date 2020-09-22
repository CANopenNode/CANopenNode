/**
 * CANopen Safety Related Data Object protocol.
 *
 * @file        CO_SRDO.c
 * @ingroup     CO_SRDO
 * @author      Robert Grüning
 * @copyright   2020 - 2020 Robert Grüning
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "304/CO_SRDO.h"

#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE

#include "301/crc16-ccitt.h"

/* verify configuration */
#if !((CO_CONFIG_CRC16) & CO_CONFIG_CRC16_ENABLE)
 #error CO_CONFIG_CRC16_ENABLE must be enabled.
#endif

#define CO_SRDO_INVALID          (0U)
#define CO_SRDO_TX               (1U)
#define CO_SRDO_RX               (2U)

#define CO_SRDO_VALID_MAGIC    (0xA5)


static void CO_SRDO_receive_normal(void *object, void *msg){
    CO_SRDO_t *SRDO;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    SRDO = (CO_SRDO_t*)object;   /* this is the correct pointer type of the first argument */

    if( (SRDO->valid == CO_SRDO_RX) &&
        (DLC >= SRDO->dataLength) && !CO_FLAG_READ(SRDO->CANrxNew[1]))
    {
        /* copy data into appropriate buffer and set 'new message' flag */
        memcpy(SRDO->CANrxData[0], data, sizeof(SRDO->CANrxData[0]));
        CO_FLAG_SET(SRDO->CANrxNew[0]);

#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
        /* Optional signal to RTOS, which can resume task, which handles SRDO. */
        if(SRDO->pFunctSignalPre != NULL) {
            SRDO->pFunctSignalPre(SRDO->functSignalObjectPre);
        }
#endif
    }
}

static void CO_SRDO_receive_inverted(void *object, void *msg){
    CO_SRDO_t *SRDO;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    SRDO = (CO_SRDO_t*)object;   /* this is the correct pointer type of the first argument */

    if( (SRDO->valid == CO_SRDO_RX) &&
        (DLC >= SRDO->dataLength) && CO_FLAG_READ(SRDO->CANrxNew[0]))
    {
        /* copy data into appropriate buffer and set 'new message' flag */
        memcpy(SRDO->CANrxData[1], data, sizeof(SRDO->CANrxData[1]));
        CO_FLAG_SET(SRDO->CANrxNew[1]);

#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
        /* Optional signal to RTOS, which can resume task, which handles SRDO. */
        if(SRDO->pFunctSignalPre != NULL) {
            SRDO->pFunctSignalPre(SRDO->functSignalObjectPre);
        }
#endif
    }
}

static void CO_SRDOconfigCom(CO_SRDO_t* SRDO, uint32_t COB_IDnormal, uint32_t COB_IDinverted){
    uint16_t IDs[2][2] = {0};
    uint16_t* ID;
    uint16_t successCount = 0;

    int16_t i;

    uint32_t COB_ID[2];
    COB_ID[0] = COB_IDnormal;
    COB_ID[1] = COB_IDinverted;

    SRDO->valid = CO_SRDO_INVALID;

    /* is SRDO used? */
    if(*SRDO->SRDOGuard->configurationValid == CO_SRDO_VALID_MAGIC && (SRDO->SRDOCommPar->informationDirection == CO_SRDO_TX || SRDO->SRDOCommPar->informationDirection == CO_SRDO_RX) &&
        SRDO->dataLength){
        ID = &IDs[SRDO->SRDOCommPar->informationDirection - 1][0];
        /* is used default COB-ID? */
        for(i = 0; i < 2; i++){
            if(!(COB_ID[i] & 0xBFFFF800L)){
                ID[i] = (uint16_t)COB_ID[i] & 0x7FF;

                if(ID[i] == SRDO->defaultCOB_ID[i] && SRDO->nodeId <= 64){
                    ID[i] += 2*SRDO->nodeId;
                }
                if(0x101 <= ID[i] && ID[i] <= 0x180 && ((ID[i] & 1) != i )){
                    successCount++;
                }
            }
        }
    }
    /* all ids are ok*/
    if(successCount == 2){
        SRDO->valid = SRDO->SRDOCommPar->informationDirection;

        if (SRDO->valid == CO_SRDO_TX){
            SRDO->timer = 500 * SRDO->nodeId; /* 0.5ms * node-ID delay*/
        }
        else if (SRDO->valid == CO_SRDO_RX){
            SRDO->timer = SRDO->SRDOCommPar->safetyCycleTime * 1000U;
        }
    }
    else{
        memset(IDs, 0, sizeof(IDs));
        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
        SRDO->valid = CO_SRDO_INVALID;
    }


    for(i = 0; i < 2; i++){
        CO_ReturnError_t r;
        SRDO->CANtxBuff[i] = CO_CANtxBufferInit(
            SRDO->CANdevTx,            /* CAN device */
            SRDO->CANdevTxIdx[i],      /* index of specific buffer inside CAN module */
            IDs[0][i],                   /* CAN identifier */
            0,                         /* rtr */
            SRDO->dataLength,          /* number of data bytes */
            0);                 /* synchronous message flag bit */

        if(SRDO->CANtxBuff[i] == 0){
            SRDO->valid = CO_SRDO_INVALID;
        }

        r = CO_CANrxBufferInit(
                SRDO->CANdevRx,         /* CAN device */
                SRDO->CANdevRxIdx[i],   /* rx buffer index */
                IDs[1][i],                /* CAN identifier */
                0x7FF,                  /* mask */
                0,                      /* rtr */
                (void*)SRDO,            /* object passed to receive function */
                i ? CO_SRDO_receive_inverted : CO_SRDO_receive_normal);        /* this function will process received message */
        if(r != CO_ERROR_NO){
            SRDO->valid = CO_SRDO_INVALID;
            CO_FLAG_CLEAR(SRDO->CANrxNew[i]);
        }
    }
}

static uint32_t CO_SRDOfindMap(
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

static uint32_t CO_SRDOconfigMap(CO_SRDO_t* SRDO, uint8_t noOfMappedObjects){
    int16_t i;
    uint8_t lengths[2] = {0};
    uint32_t ret = 0;
    const uint32_t* pMap = &SRDO->SRDOMapPar->mappedObjects[0];


    for(i=noOfMappedObjects; i>0; i--){
        int16_t j;
        uint8_t* pData;
        uint8_t dummy = 0;
        uint8_t* length = &lengths[i%2];
        uint8_t** mapPointer = SRDO->mapPointer[i%2];
        uint8_t prevLength = *length;
        uint8_t MBvar;
        uint32_t map = *(pMap++);

        /* function do much checking of errors in map */
        ret = CO_SRDOfindMap(
                SRDO->SDO,
                map,
                SRDO->SRDOCommPar->informationDirection == CO_SRDO_TX,
                &pData,
                length,
                &dummy,
                &MBvar);
        if(ret){
            *length = 0;
            CO_errorReport(SRDO->em, CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, map);
            break;
        }

        /* write SRDO data pointers */
#ifdef CO_BIG_ENDIAN
        if(MBvar){
            for(j=*length-1; j>=prevLength; j--)
                mapPointer[j] = pData++;
        }
        else{
            for(j=prevLength; j<*length; j++)
                mapPointer[j] = pData++;
        }
#else
        for(j=prevLength; j<*length; j++){
            mapPointer[j] = pData++;
        }
#endif

    }
    if(lengths[0] == lengths[1])
        SRDO->dataLength = lengths[0];
    else{
        SRDO->dataLength = 0;
        CO_errorReport(SRDO->em, CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, 0);
        ret = CO_SDO_AB_MAP_LEN;
    }

    return ret;
}

static uint16_t CO_SRDOcalcCrc(const CO_SRDO_t *SRDO){
    uint16_t i;
    uint16_t result = 0x0000;
    uint8_t buffer[4];
    uint32_t cob;

    const CO_SRDOCommPar_t *com = SRDO->SRDOCommPar;
    const CO_SRDOMapPar_t *map = SRDO->SRDOMapPar;

    result = crc16_ccitt(&com->informationDirection, 1, result);
    CO_memcpySwap2(&buffer[0], &com->safetyCycleTime);
    result = crc16_ccitt(&buffer[0], 2, result);
    result = crc16_ccitt(&com->safetyRelatedValidationTime, 1, result);

    /* adjust COB-ID if the default is used
    Caution: if the node id changes and you are using the default COB-ID you have to recalculate the checksum
    This behaviour is controversial and could be changed or made optional.
    */
    cob = com->COB_ID1_normal;
    if(((cob)&0x7FF) == SRDO->defaultCOB_ID[0] && SRDO->nodeId <= 64)
        cob += SRDO->nodeId;
    CO_memcpySwap4(&buffer[0], &cob);
    result = crc16_ccitt(&buffer[0], 4, result);

    cob = com->COB_ID2_inverted;
    if(((cob)&0x7FF) == SRDO->defaultCOB_ID[1] && SRDO->nodeId <= 64)
        cob += SRDO->nodeId;
    CO_memcpySwap4(&buffer[0], &cob);
    result = crc16_ccitt(&buffer[0], 4, result);

    result = crc16_ccitt(&map->numberOfMappedObjects, 1, result);
    for(i = 0; i < map->numberOfMappedObjects;){
        uint8_t subindex = i + 1;
        result = crc16_ccitt(&subindex, 1, result);
        CO_memcpySwap4(&buffer[0], &map->mappedObjects[i]);
        result = crc16_ccitt(&buffer[0], 4, result);
        i = subindex;
    }
    return result;
}

static CO_SDO_abortCode_t CO_ODF_SRDOcom(CO_ODF_arg_t *ODF_arg){
    CO_SRDO_t *SRDO;

    SRDO = (CO_SRDO_t*) ODF_arg->object;

    /* Reading Object Dictionary variable */
    if(ODF_arg->reading){
        if(ODF_arg->subIndex == 5 || ODF_arg->subIndex == 6){
            uint32_t value = CO_getUint32(ODF_arg->data);
            uint16_t index = ODF_arg->subIndex - 5;

            /* if default COB ID is used, write default value here */
            if(((value)&0x7FF) == SRDO->defaultCOB_ID[index] && SRDO->nodeId <= 64)
                value += SRDO->nodeId;

            /* If SRDO is not valid, set bit 31. but I do not think this applies to SRDO ?! */
            /* if(!SRDO->valid) value |= 0x80000000L; */

            CO_setUint32(ODF_arg->data, value);
        }
        return CO_SDO_AB_NONE;
    }

    /* Writing Object Dictionary variable */
    if(*SRDO->SRDOGuard->operatingState == CO_NMT_OPERATIONAL)
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */

    if(ODF_arg->subIndex == 1){
        uint8_t value = ODF_arg->data[0];
        if (value > 2)
            return CO_SDO_AB_INVALID_VALUE;
    }
    else if(ODF_arg->subIndex == 2){
        uint16_t value = CO_getUint16(ODF_arg->data);
        if (value == 0)
            return CO_SDO_AB_INVALID_VALUE;
    }
    else if(ODF_arg->subIndex == 3){
        uint8_t value = ODF_arg->data[0];
        if (value == 0)
            return CO_SDO_AB_INVALID_VALUE;
    }
    else if(ODF_arg->subIndex == 4){   /* Transmission_type */
        uint8_t *value = (uint8_t*) ODF_arg->data;
        if(*value != 254)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */
    }
   else if(ODF_arg->subIndex == 5 || ODF_arg->subIndex == 6){   /* COB_ID */
        uint32_t value = CO_getUint32(ODF_arg->data);
        uint16_t index = ODF_arg->subIndex - 5;

        /* check value range, the spec does not specify if COB-ID flags are allowed */
        if(value < 0x101 || value > 0x180 || (value & 1) == index)
            return CO_SDO_AB_INVALID_VALUE;  /* Invalid value for parameter (download only). */

        /* if default COB-ID is being written, write defaultCOB_ID without nodeId */
        if(SRDO->nodeId <= 64 && value == (SRDO->defaultCOB_ID[index] + SRDO->nodeId)){
            value = SRDO->defaultCOB_ID[index];
            CO_setUint32(ODF_arg->data, value);
        }
    }

    *SRDO->SRDOGuard->configurationValid = CO_SRDO_INVALID;

    return CO_SDO_AB_NONE;
}

static CO_SDO_abortCode_t CO_ODF_SRDOmap(CO_ODF_arg_t *ODF_arg){
    CO_SRDO_t *SRDO;

    SRDO = (CO_SRDO_t*) ODF_arg->object;
    if(ODF_arg->reading)
        return CO_SDO_AB_NONE;

    /* Writing Object Dictionary variable */

    if(*SRDO->SRDOGuard->operatingState == CO_NMT_OPERATIONAL)
        return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
    if(SRDO->SRDOCommPar->informationDirection) /* SRDO must be deleted */
        return CO_SDO_AB_UNSUPPORTED_ACCESS;  /* Unsupported access to an object. */

    /* numberOfMappedObjects */
    if(ODF_arg->subIndex == 0){
        uint8_t *value = (uint8_t*) ODF_arg->data;

        if(*value > 16 || *value & 1) /*only odd numbers are allowed*/
            return CO_SDO_AB_MAP_LEN;  /* Number and length of object to be mapped exceeds SRDO length. */
    }
    else{
        if (SRDO->SRDOMapPar->numberOfMappedObjects != 0)
            return CO_SDO_AB_UNSUPPORTED_ACCESS;
    }
    *SRDO->SRDOGuard->configurationValid = CO_SRDO_INVALID;
    return CO_SDO_AB_NONE;
}

static CO_SDO_abortCode_t CO_ODF_SRDOcrc(CO_ODF_arg_t *ODF_arg){
    CO_SRDOGuard_t *SRDOGuard;

    SRDOGuard = (CO_SRDOGuard_t*) ODF_arg->object;

    if (!ODF_arg->reading){
        if(*SRDOGuard->operatingState == CO_NMT_OPERATIONAL)
            return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
        *SRDOGuard->configurationValid = CO_SRDO_INVALID;
    }
    return CO_SDO_AB_NONE;
}

static CO_SDO_abortCode_t CO_ODF_SRDOvalid(CO_ODF_arg_t *ODF_arg){
    CO_SRDOGuard_t *SRDOGuard;

    SRDOGuard = (CO_SRDOGuard_t*) ODF_arg->object;

    if(!ODF_arg->reading){
        if(*SRDOGuard->operatingState == CO_NMT_OPERATIONAL)
            return CO_SDO_AB_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
        SRDOGuard->checkCRC = ODF_arg->data[0] == CO_SRDO_VALID_MAGIC;
    }
    return CO_SDO_AB_NONE;
}

CO_ReturnError_t CO_SRDOGuard_init(
        CO_SRDOGuard_t         *SRDOGuard,
        CO_SDO_t               *SDO,
        CO_NMT_internalState_t *operatingState,
        uint8_t                *configurationValid,
        uint16_t                idx_SRDOvalid,
        uint16_t                idx_SRDOcrc)
{
    /* verify arguments */
    if(SRDOGuard==NULL || SDO==NULL || operatingState==NULL || configurationValid==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    SRDOGuard->operatingState = operatingState;
    SRDOGuard->operatingStatePrev = CO_NMT_INITIALIZING;
    SRDOGuard->configurationValid = configurationValid;
    SRDOGuard->checkCRC = *configurationValid == CO_SRDO_VALID_MAGIC;


    /* Configure Object dictionary entry at index 0x13FE and 0x13FF */
    CO_OD_configure(SDO, idx_SRDOvalid, CO_ODF_SRDOvalid, (void*)SRDOGuard, 0, 0);
    CO_OD_configure(SDO, idx_SRDOcrc, CO_ODF_SRDOcrc, (void*)SRDOGuard, 0, 0);

    return CO_ERROR_NO;
}

uint8_t CO_SRDOGuard_process(
        CO_SRDOGuard_t         *SRDOGuard)
{
    uint8_t result = 0;
    CO_NMT_internalState_t operatingState = *SRDOGuard->operatingState;
    if(operatingState != SRDOGuard->operatingStatePrev){
        SRDOGuard->operatingStatePrev = operatingState;
        if (operatingState == CO_NMT_OPERATIONAL)
            result |= 1 << 0;
    }

    if(SRDOGuard->checkCRC){
        result |= 1 << 1;
        SRDOGuard->checkCRC = 0;
    }
    return result;
}

#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
/******************************************************************************/
void CO_SRDO_initCallbackPre(
        CO_SRDO_t              *SRDO,
        void                   *object,
        void                  (*pFunctSignalPre)(void *object))
{
    if(SRDO != NULL){
        SRDO->functSignalObjectPre = object;
        SRDO->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif

/******************************************************************************/
void CO_SRDO_initCallbackEnterSafeState(
        CO_SRDO_t              *SRDO,
        void                   *object,
        void                  (*pFunctSignalSafe)(void *object))
{
    if(SRDO != NULL){
        SRDO->functSignalObjectSafe = object;
        SRDO->pFunctSignalSafe = pFunctSignalSafe;
    }
}

CO_ReturnError_t CO_SRDO_init(
        CO_SRDO_t              *SRDO,
        CO_SRDOGuard_t         *SRDOGuard,
        CO_EM_t                *em,
        CO_SDO_t               *SDO,
        uint8_t                 nodeId,
        uint16_t                defaultCOB_ID,
        const CO_SRDOCommPar_t *SRDOCommPar,
        const CO_SRDOMapPar_t  *SRDOMapPar,
        const uint16_t         *checksum,
        uint16_t                idx_SRDOCommPar,
        uint16_t                idx_SRDOMapPar,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdxNormal,
        uint16_t                CANdevRxIdxInverted,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdxNormal,
        uint16_t                CANdevTxIdxInverted)
{
        /* verify arguments */
    if(SRDO==NULL || SRDOGuard==NULL || em==NULL || SDO==NULL || checksum==NULL ||
        SRDOCommPar==NULL || SRDOMapPar==NULL || CANdevRx==NULL || CANdevTx==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    SRDO->SRDOGuard = SRDOGuard;
    SRDO->em = em;
    SRDO->SDO = SDO;
    SRDO->SRDOCommPar = SRDOCommPar;
    SRDO->SRDOMapPar = SRDOMapPar;
    SRDO->checksum = checksum;
    SRDO->CANdevRx = CANdevRx;
    SRDO->CANdevRxIdx[0] = CANdevRxIdxNormal;
    SRDO->CANdevRxIdx[1] = CANdevRxIdxInverted;
    SRDO->CANdevTx = CANdevTx;
    SRDO->CANdevTxIdx[0] = CANdevTxIdxNormal;
    SRDO->CANdevTxIdx[1] = CANdevTxIdxInverted;
    SRDO->nodeId = nodeId;
    SRDO->defaultCOB_ID[0] = defaultCOB_ID;
    SRDO->defaultCOB_ID[1] = defaultCOB_ID + 1;
    SRDO->valid = CO_SRDO_INVALID;
    SRDO->toogle = 0;
    SRDO->timer = 0;
    SRDO->pFunctSignalSafe = NULL;
    SRDO->functSignalObjectSafe = NULL;
#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
    SRDO->pFunctSignalPre = NULL;
    SRDO->functSignalObjectPre = NULL;
#endif

    /* Configure Object dictionary entry at index 0x1301+ and 0x1381+ */
    CO_OD_configure(SDO, idx_SRDOCommPar, CO_ODF_SRDOcom, (void*)SRDO, 0, 0);
    CO_OD_configure(SDO, idx_SRDOMapPar, CO_ODF_SRDOmap, (void*)SRDO, 0, 0);

    return CO_ERROR_NO;
}

CO_ReturnError_t CO_SRDO_requestSend(
        CO_SRDO_t              *SRDO)
{
    if (*SRDO->SRDOGuard->operatingState != CO_NMT_OPERATIONAL)
        return CO_ERROR_WRONG_NMT_STATE;

    if (SRDO->valid != CO_SRDO_TX)
        return CO_ERROR_TX_UNCONFIGURED;

    if(SRDO->toogle != 0)
        return CO_ERROR_TX_BUSY;

    SRDO->timer = 0;
    return CO_ERROR_NO;
}

void CO_SRDO_process(
        CO_SRDO_t              *SRDO,
        uint8_t                 commands,
        uint32_t                timeDifference_us,
        uint32_t               *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    if(commands & (1<<1)){
        uint16_t crcValue = CO_SRDOcalcCrc(SRDO);
        if (*SRDO->checksum != crcValue)
            *SRDO->SRDOGuard->configurationValid = 0;
    }

    if((commands & (1<<0)) && *SRDO->SRDOGuard->configurationValid == CO_SRDO_VALID_MAGIC){
        if(CO_SRDOconfigMap(SRDO, SRDO->SRDOMapPar->numberOfMappedObjects) == 0){
            CO_SRDOconfigCom(SRDO, SRDO->SRDOCommPar->COB_ID1_normal, SRDO->SRDOCommPar->COB_ID2_inverted);
        }
        else{
            SRDO->valid = CO_SRDO_INVALID;
        }
    }

    if(SRDO->valid && *SRDO->SRDOGuard->operatingState == CO_NMT_OPERATIONAL){
        SRDO->timer = (SRDO->timer > timeDifference_us) ? (SRDO->timer - timeDifference_us) : 0;
        if(SRDO->valid == CO_SRDO_TX){
            if(SRDO->timer == 0){
                if(SRDO->toogle){
                    CO_CANsend(SRDO->CANdevTx, SRDO->CANtxBuff[1]);
                    SRDO->timer = SRDO->SRDOCommPar->safetyCycleTime * 1000U - CO_CONFIG_SRDO_MINIMUM_DELAY;
                }
                else{

                    int16_t i;
                    uint8_t* pSRDOdataByte_normal;
                    uint8_t* pSRDOdataByte_inverted;

                    uint8_t** ppODdataByte_normal;
                    uint8_t** ppODdataByte_inverted;

                    bool_t data_ok = true;
#if (CO_CONFIG_SRDO) & CO_CONFIG_TSRDO_CALLS_EXTENSION
                    if(SRDO->SDO->ODExtensions){
                        /* for each mapped OD, check mapping to see if an OD extension is available, and call it if it is */
                        const uint32_t* pMap = &SRDO->SRDOMapPar->mappedObjects[0];
                        CO_SDO_t *pSDO = SRDO->SDO;

                        for(i=SRDO->SRDOMapPar->numberOfMappedObjects; i>0; i--){
                            uint32_t map = *(pMap++);
                            uint16_t index = (uint16_t)(map>>16);
                            uint8_t subIndex = (uint8_t)(map>>8);
                            uint16_t entryNo = CO_OD_find(pSDO, index);
                            if ( entryNo != 0xFFFF )
                            {
                                CO_OD_extension_t *ext = &pSDO->ODExtensions[entryNo];
                                if( ext->pODFunc != NULL)
                                {
                                    CO_ODF_arg_t ODF_arg;
                                    memset((void*)&ODF_arg, 0, sizeof(CO_ODF_arg_t));
                                    ODF_arg.reading = true;
                                    ODF_arg.index = index;
                                    ODF_arg.subIndex = subIndex;
                                    ODF_arg.object = ext->object;
                                    ODF_arg.attribute = CO_OD_getAttribute(pSDO, entryNo, subIndex);
                                    ODF_arg.pFlags = CO_OD_getFlagsPointer(pSDO, entryNo, subIndex);
                                    ODF_arg.data = CO_OD_getDataPointer(pSDO, entryNo, subIndex); /* https://github.com/CANopenNode/CANopenNode/issues/100 */
                                    ODF_arg.dataLength = CO_OD_getLength(pSDO, entryNo, subIndex);
                                    ext->pODFunc(&ODF_arg);
                                }
                            }
                        }
                    }
#endif
                    pSRDOdataByte_normal = &SRDO->CANtxBuff[0]->data[0];
                    ppODdataByte_normal = &SRDO->mapPointer[0][0];

                    pSRDOdataByte_inverted = &SRDO->CANtxBuff[1]->data[0];
                    ppODdataByte_inverted = &SRDO->mapPointer[1][0];

#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_CHECK_TX
                    /* check data before sending (optional) */
                    for(i = 0; i<SRDO->dataLength; i++){
                        uint8_t invert = ~*(ppODdataByte_inverted[i]);
                        if (*(ppODdataByte_normal[i]) != invert)
                        {
                            data_ok = false;
                            break;
                        }
                    }
#endif
                    if(data_ok){
                        /* Copy data from Object dictionary. */
                        for(i = 0; i<SRDO->dataLength; i++){
                            pSRDOdataByte_normal[i] = *(ppODdataByte_normal[i]);
                            pSRDOdataByte_inverted[i] = *(ppODdataByte_inverted[i]);
                        }

                        CO_CANsend(SRDO->CANdevTx, SRDO->CANtxBuff[0]);

                        SRDO->timer = CO_CONFIG_SRDO_MINIMUM_DELAY;
                    }
                    else{
                        SRDO->toogle = 1;
                        /* save state */
                        if(SRDO->pFunctSignalSafe != NULL){
                            SRDO->pFunctSignalSafe(SRDO->functSignalObjectSafe);
                        }
                    }
                }
                SRDO->toogle = !SRDO->toogle;
            }
#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_TIMERNEXT
            if(timerNext_us != NULL){
                if(*timerNext_us > SRDO->timer){
                    *timerNext_us = SRDO->timer; /* Schedule for next message timer */
                }
            }
#endif
        }
        else if(SRDO->valid == CO_SRDO_RX){
            if(CO_FLAG_READ(SRDO->CANrxNew[SRDO->toogle])){

                if(SRDO->toogle){
                    int16_t i;
                    uint8_t* pSRDOdataByte_normal;
                    uint8_t* pSRDOdataByte_inverted;

                    uint8_t** ppODdataByte_normal;
                    uint8_t** ppODdataByte_inverted;
                    bool_t data_ok = true;

                    pSRDOdataByte_normal = &SRDO->CANrxData[0][0];
                    pSRDOdataByte_inverted = &SRDO->CANrxData[1][0];
                    for(i = 0; i<SRDO->dataLength; i++){
                        uint8_t invert = ~pSRDOdataByte_inverted[i];
                        if(pSRDOdataByte_normal[i] != invert){
                            data_ok = false;
                            break;
                        }
                    }
                    if(data_ok){
                        ppODdataByte_normal = &SRDO->mapPointer[0][0];
                        ppODdataByte_inverted = &SRDO->mapPointer[1][0];

                        /* Copy data to Object dictionary. If between the copy operation CANrxNew
                        * is set to true by receive thread, then copy the latest data again. */

                        for(i = 0; i<SRDO->dataLength; i++){
                            *(ppODdataByte_normal[i]) = pSRDOdataByte_normal[i];
                            *(ppODdataByte_inverted[i]) = pSRDOdataByte_inverted[i];
                        }
                        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
                        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
#if (CO_CONFIG_SRDO) & CO_CONFIG_RSRDO_CALLS_EXTENSION
                        if(SRDO->SDO->ODExtensions){
                            int16_t i;
                            /* for each mapped OD, check mapping to see if an OD extension is available, and call it if it is */
                            const uint32_t* pMap = &SRDO->SRDOMapPar->mappedObjects[0];
                            CO_SDO_t *pSDO = SRDO->SDO;

                            for(i=SRDO->SRDOMapPar->numberOfMappedObjects; i>0; i--){
                                uint32_t map = *(pMap++);
                                uint16_t index = (uint16_t)(map>>16);
                                uint8_t subIndex = (uint8_t)(map>>8);
                                uint16_t entryNo = CO_OD_find(pSDO, index);
                                if ( entryNo != 0xFFFF )
                                {
                                    CO_OD_extension_t *ext = &pSDO->ODExtensions[entryNo];
                                    if( ext->pODFunc != NULL)
                                    {
                                        CO_ODF_arg_t ODF_arg;
                                        memset((void*)&ODF_arg, 0, sizeof(CO_ODF_arg_t));
                                        ODF_arg.reading = false;
                                        ODF_arg.index = index;
                                        ODF_arg.subIndex = subIndex;
                                        ODF_arg.object = ext->object;
                                        ODF_arg.attribute = CO_OD_getAttribute(pSDO, entryNo, subIndex);
                                        ODF_arg.pFlags = CO_OD_getFlagsPointer(pSDO, entryNo, subIndex);
                                        ODF_arg.data = CO_OD_getDataPointer(pSDO, entryNo, subIndex); /* https://github.com/CANopenNode/CANopenNode/issues/100 */
                                        ODF_arg.dataLength = CO_OD_getLength(pSDO, entryNo, subIndex);
                                        ext->pODFunc(&ODF_arg);
                                    }
                                }
                            }
                        }
#endif
                    }
                    else{
                        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
                        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
                        /* save state */
                        if(SRDO->pFunctSignalSafe != NULL){
                            SRDO->pFunctSignalSafe(SRDO->functSignalObjectSafe);
                        }
                    }

                    SRDO->timer = SRDO->SRDOCommPar->safetyCycleTime * 1000U;
                }
                else{
                    SRDO->timer = SRDO->SRDOCommPar->safetyRelatedValidationTime * 1000U;
                }
                SRDO->toogle = !SRDO->toogle;
            }

            if(SRDO->timer == 0){
                SRDO->toogle = 0;
                SRDO->timer = SRDO->SRDOCommPar->safetyRelatedValidationTime * 1000U;
                CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
                CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
                /* save state */
                if(SRDO->pFunctSignalSafe != NULL){
                    SRDO->pFunctSignalSafe(SRDO->functSignalObjectSafe);
                }
            }
        }
    }
    else{
        SRDO->valid = CO_SRDO_INVALID;
        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
    }
}

#endif /* (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE */
