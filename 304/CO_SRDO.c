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
        (DLC >= SRDO->dataLength) && !CO_FLAG_READ(SRDO->CANrxNew[1])){
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
        (DLC >= SRDO->dataLength) && CO_FLAG_READ(SRDO->CANrxNew[0])){
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
    if((*SRDO->SRDOGuard->configurationValid == CO_SRDO_VALID_MAGIC) && ((SRDO->CommPar_informationDirection == CO_SRDO_TX) || (SRDO->CommPar_informationDirection == CO_SRDO_RX)) &&
        SRDO->dataLength){
        ID = &IDs[SRDO->CommPar_informationDirection - 1][0];
        /* is used default COB-ID? */
        for(i = 0; i < 2; i++){
            if(!(COB_ID[i] & 0xBFFFF800L)){
                ID[i] = (uint16_t)COB_ID[i] & 0x7FF;

                if((ID[i] == SRDO->defaultCOB_ID[i]) && (SRDO->nodeId <= 64U)){
                    ID[i] += (uint16_t)SRDO->nodeId*2;
                }
                if((0x101U <= ID[i]) && (ID[i] <= 0x180U) && ((ID[i] & 1) != i )){
                    successCount++;
                }
            }
        }
    }
    /* all ids are ok*/
    if(successCount == 2){
        SRDO->valid = SRDO->CommPar_informationDirection;

        if (SRDO->valid == CO_SRDO_TX){
            SRDO->timer = (uint32_t)SRDO->nodeId * 500U;  /* 0.5ms * node-ID delay*/
        }
        else if (SRDO->valid == CO_SRDO_RX){
            SRDO->timer = (uint32_t)SRDO->CommPar_safetyCycleTime * 1000U;
        }
        else { /* MISRA C 2004 14.10 */ }
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

static CO_ReturnError_t SRDO_configMap(CO_SRDO_t* SRDO,
                                        OD_t *OD,
                                        OD_entry_t *OD_SRDOMapPar){
    ODR_t odRet;
    size_t pdoDataLength[2] = { 0, 0 };
    uint8_t mappedObjectsCount = 0;
    uint8_t *errInfo = NULL;         
    //uint32_t *erroneousMap = NULL;

    /* number of mapped application objects in SRDO */
    odRet = OD_get_u8(OD_SRDOMapPar, 0, &mappedObjectsCount, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = (((uint32_t)OD_getIndex(OD_SRDOMapPar)) << 8) | 0U;
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    if (mappedObjectsCount > CO_SRDO_MAX_SIZE) {
        //*erroneousMap = 1;
        return CO_ERROR_NO;
    }
    
    /* iterate mapped OD variables */
    for(uint8_t i=0; i<mappedObjectsCount; i++){
        bool_t isRSRDO = SRDO->CommPar_informationDirection == CO_SRDO_RX;
        uint32_t map = 0;
        uint8_t plain_inverted = i%2;

        odRet = OD_get_u32(OD_SRDOMapPar, i + 1, &map, true);
        if (odRet != ODR_OK) {
            if (errInfo != NULL) {
                *errInfo = (((uint32_t)OD_getIndex(OD_SRDOMapPar)) << 8) | i;
            }
            return CO_ERROR_OD_PARAMETERS;
        }

        uint16_t index = (uint16_t) (map >> 16);
        uint8_t subIndex = (uint8_t) (map >> 8);
        uint8_t mappedLengthBits = (uint8_t) map;
        uint8_t mappedLength = mappedLengthBits >> 3;
        uint8_t pdoDataStart = pdoDataLength[plain_inverted];
        pdoDataLength[plain_inverted] += mappedLength;

        if (((mappedLengthBits & 0x07) != 0) || (pdoDataLength[plain_inverted] > CO_SRDO_MAX_SIZE)) {
            //*erroneousMap = map;
            return CO_ERROR_NO;
        }

        /* is there a reference to the dummy entry */
        if ((index < 0x20) && (subIndex == 0U)) {
            for (uint8_t j = pdoDataStart; j < pdoDataLength[plain_inverted]; j++) {
                static uint8_t dummyTX = 0;
                static uint8_t dummyRX;
                SRDO->mapPointer[plain_inverted][j] = isRSRDO ? &dummyRX : &dummyTX;
            }
            continue;
        }

        /* find entry in the Object Dictionary, original location */
        OD_IO_t OD_IO;
        OD_entry_t *entry = OD_find(OD, index);
        OD_attr_t testAttribute = isRSRDO ? ODA_RSRDO : ODA_TSRDO;

        odRet = OD_getSub(entry, subIndex, &OD_IO, true);
        if (odRet != ODR_OK
            || (OD_IO.stream.attribute & testAttribute) == 0
            || OD_IO.stream.dataLength < mappedLength
            || OD_IO.stream.dataOrig == NULL
        ) {
            //*erroneousMap = map;
            return CO_ERROR_NO;
        }

        /* write locations to OD variable data bytes into PDO map pointers */
#ifdef CO_BIG_ENDIAN
        if((OD_IO.stream.attribute & ODA_MB) != 0) {
            uint8_t *odDataPointer = OD_IO.stream.dataOrig
                                   + OD_IO.stream.dataLength - 1;
            for (uint8_t j = pdoDataStart; j < pdoDataLength; j++) {
                SRDO->mapPointer[j] = odDataPointer--;
            }
        }
        else
#endif
        {
            uint8_t *odDataPointer = OD_IO.stream.dataOrig;
            for (uint8_t j = pdoDataStart; j < pdoDataLength[plain_inverted]; j++) {
                SRDO->mapPointer[plain_inverted][j] = odDataPointer++;
            }
        }
    }

    if(pdoDataLength[0] == pdoDataLength[1]){
        SRDO->dataLength = pdoDataLength[0];
        //SRDO->mappedObjectsCount = pdoDataLength[0];
    }else{
        SRDO->dataLength = 0;
        CO_errorReport(SRDO->em, CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, 0);
        return CO_ERROR_OD_PARAMETERS;
    }

    return CO_ERROR_NO;
}

static uint16_t CO_SRDOcalcCrc(const CO_SRDO_t *SRDO){
    uint16_t i,tmp_u16;
    uint16_t result = 0x0000;
    uint8_t buffer[4];
    uint32_t cob, tmp_u32;
    uint32_t map;
    ODR_t odRet;
    
    result = crc16_ccitt(&SRDO->CommPar_informationDirection, 1, result);
    tmp_u16 = CO_SWAP_16(SRDO->CommPar_safetyCycleTime);
    memcpy(&buffer[0], &tmp_u16, sizeof(tmp_u16));
    result = crc16_ccitt(&buffer[0], 2, result);
    result = crc16_ccitt(&SRDO->CommPar_safetyRelatedValidationTime, 1, result);

    /* adjust COB-ID if the default is used
    Caution: if the node id changes and you are using the default COB-ID you have to recalculate the checksum
    This behaviour is controversial and could be changed or made optional.
    */
    cob = SRDO->CommPar_COB_ID1_normal;
    if(((cob&0x7FFU) == SRDO->defaultCOB_ID[0]) && (SRDO->nodeId <= 64U)){
        cob += (uint32_t)SRDO->nodeId*2;
    }
    tmp_u32 = CO_SWAP_32(cob);
    memcpy(&buffer[0], &tmp_u32, sizeof(tmp_u32));
    result = crc16_ccitt(&buffer[0], 4, result);

    cob = SRDO->CommPar_COB_ID2_inverted;
    if(((cob&0x7FFU) == SRDO->defaultCOB_ID[1]) && (SRDO->nodeId <= 64U)){
        cob += (uint32_t)SRDO->nodeId*2;
    }
    tmp_u32 = CO_SWAP_32(cob);
    memcpy(&buffer[0], &tmp_u32, sizeof(tmp_u32));
    result = crc16_ccitt(&buffer[0], 4, result);

    result = crc16_ccitt(&SRDO->mappedObjectsCount, 1, result);
    for(i = 0; i < SRDO->mappedObjectsCount;){
        uint8_t subindex = i + 1U;
        result = crc16_ccitt(&subindex, 1, result);
        odRet = OD_get_u32(SRDO->SRDOMapPar, subindex, &map, true);
        if (odRet != ODR_OK) {
            map = 0;
        }
        tmp_u32 = CO_SWAP_32(map);
        memcpy(&buffer[0], &tmp_u32, sizeof(tmp_u32));
        result = crc16_ccitt(&buffer[0], 4, result);
        i = subindex;
    }
    return result;
}



static ODR_t OD_read_SRDO_communicationParam(OD_stream_t *stream, void *buf,
                                   OD_size_t count, OD_size_t *countRead)
{
    ODR_t returnCode = OD_readOriginal(stream, buf, count, countRead);
    if ( returnCode != ODR_OK ){
        return returnCode;
    }
    
    if ((stream == NULL) || (stream->subIndex == 0U) || (buf == NULL)
        || (count > sizeof(uint32_t)) || (countRead == NULL)
    ) {
        return ODR_DEV_INCOMPAT;
    }
    
    CO_SRDO_t *SRDO = stream->object;
    
    /* Reading Object Dictionary variable */
    if ((stream->subIndex == 5U) || (stream->subIndex == 6U)){
        uint32_t value = CO_getUint32(buf);
        uint16_t index = stream->subIndex - 5U;

        /* if default COB ID is used, write default value here */
        if(((value&0x7FFU) == SRDO->defaultCOB_ID[index]) && (SRDO->nodeId <= 64U)){
            value += (uint32_t)SRDO->nodeId*2;
        }

        /* If SRDO is not valid, set bit 31. but I do not think this applies to SRDO ?! */
        /* if(!SRDO->valid){ value |= 0x80000000L; }*/

        CO_setUint32(buf, value);
    }
    return ODR_OK;
}


static ODR_t OD_write_SRDO_communicationParam(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if ((stream == NULL) || (stream->subIndex == 0U) || (buf == NULL)
        || (count > sizeof(uint32_t)) || (countWritten == NULL)
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_SRDO_t *SRDO = stream->object;
    CO_SRDOGuard_t *SRDOGuard = SRDO->SRDOGuard;
    uint8_t bufCopy[4];
    memcpy(bufCopy, buf, count);

    /* Writing Object Dictionary variable */
    if(*SRDOGuard->operatingState == CO_NMT_OPERATIONAL){
        return ODR_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
    }

    if(stream->subIndex == 1U){
        uint8_t value = CO_getUint8(buf);
        if (value > 2U){
            return ODR_INVALID_VALUE;
        }
        SRDO->CommPar_informationDirection = value;
    }
    else if(stream->subIndex == 2U){
        uint16_t value = CO_getUint16(buf);
        if (value == 0U){
            return ODR_INVALID_VALUE;
        }
        SRDO->CommPar_safetyCycleTime = value;
    }
    else if(stream->subIndex == 3U){
        uint8_t value = CO_getUint8(buf);
        if (value == 0U){
            return ODR_INVALID_VALUE;
        }
        SRDO->CommPar_safetyRelatedValidationTime = value;
    }
    else if(stream->subIndex == 4){   /* Transmission_type */
        uint8_t value = CO_getUint8(buf);
        if(value != 254){
            return ODR_INVALID_VALUE;  /* Invalid value for parameter (download only). */
        }
        SRDO->CommPar_transmissionType = value;
    }
    else if(stream->subIndex == 5U || stream->subIndex == 6U){   /* COB_ID */
        uint32_t value = CO_getUint32(buf);
        uint16_t index = stream->subIndex - 5U;

        /* check value range, the spec does not specify if COB-ID flags are allowed */
        if((value < 0x101) || (value > 0x180U) || ((value & 1) == index)){
            return ODR_INVALID_VALUE;  /* Invalid value for parameter (download only). */
        }

        /* if default COB-ID is being written, write defaultCOB_ID without nodeId */
        if((SRDO->nodeId <= 64U) && (value == (SRDO->defaultCOB_ID[index] + ((uint32_t)SRDO->nodeId*2)))){
            value = SRDO->defaultCOB_ID[index];
            CO_setUint32(bufCopy, value);
        }
        if(index == 0U){
            SRDO->CommPar_COB_ID1_normal = value;
        }
        else{
            SRDO->CommPar_COB_ID2_inverted = value;
        }
    }
    else { /* MISRA C 2004 14.10 */ }

    *SRDOGuard->configurationValid = CO_SRDO_INVALID;

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, bufCopy, count, countWritten);
}


static ODR_t OD_write_SRDO_mappingParam(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if ((stream == NULL) || (stream->subIndex != 0U) || (buf == NULL)
        || (count > sizeof(uint32_t)) || (countWritten == NULL)
    ) {
        return ODR_DEV_INCOMPAT;
    }
    
    CO_SRDO_t *SRDO = stream->object;
    CO_SRDOGuard_t *SRDOGuard = SRDO->SRDOGuard;

    /* Writing Object Dictionary variable */
    if(*SRDOGuard->operatingState == CO_NMT_OPERATIONAL){
        return ODR_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
    }
    if(SRDO->CommPar_informationDirection){ /* SRDO must be deleted */
        return ODR_UNSUPP_ACCESS;  /* Unsupported access to an object. */
    }

    /* numberOfMappedObjects */
    if(stream->subIndex == 0U){
        uint8_t value = CO_getUint8(buf);
        if((value > 16U) || (value & 1)){ /*only odd numbers are allowed*/
            return ODR_MAP_LEN;  /* Number and length of object to be mapped exceeds SRDO length. */
        }
        SRDO->mappedObjectsCount = value;
    }
    else{
        if (SRDO->mappedObjectsCount != 0U){
            return ODR_UNSUPP_ACCESS;
        }
    }
    *SRDOGuard->configurationValid = CO_SRDO_INVALID;
    
    
    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

static ODR_t OD_write_13FE(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if ((stream == NULL) || (stream->subIndex != 0U) || (buf == NULL)
        || (count != sizeof(uint8_t)) || (countWritten == NULL)
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_SRDOGuard_t *SRDOGuard = stream->object;
    uint8_t value = CO_getUint8(buf);

    if(*SRDOGuard->operatingState == CO_NMT_OPERATIONAL){
        return ODR_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
    }
    SRDOGuard->checkCRC = ( value == CO_SRDO_VALID_MAGIC );

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

static ODR_t OD_write_13FF(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    if ((stream == NULL) || (stream->subIndex == 0U) || (buf == NULL)
        || (count != sizeof(uint16_t)) || (countWritten == NULL)
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_SRDOGuard_t *SRDOGuard = stream->object;
    uint16_t value = CO_getUint16(buf);

    if(*SRDOGuard->operatingState == CO_NMT_OPERATIONAL){
        return ODR_DATA_DEV_STATE;   /* Data cannot be transferred or stored to the application because of the present device state. */
    }
    *SRDOGuard->configurationValid = CO_SRDO_INVALID;

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

CO_ReturnError_t CO_SRDOGuard_init(
        CO_SRDOGuard_t         *SRDOGuard,
        CO_SDOserver_t         *SDO,
        CO_NMT_internalState_t *operatingState,
        uint8_t                *configurationValid,
        OD_entry_t             *OD_13FE_SRDOValid,
        OD_entry_t             *OD_13FF_SRDOCRC,
        uint32_t *errInfo)
{
    /* verify arguments */
    if((SRDOGuard==NULL) || (SDO==NULL) || (operatingState==NULL) || (configurationValid==NULL) || (OD_13FE_SRDOValid==NULL) || (OD_13FF_SRDOCRC==NULL)){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
    
    /* clear object */
    memset(SRDOGuard, 0, sizeof(CO_SRDOGuard_t));

    SRDOGuard->operatingState = operatingState;
    SRDOGuard->operatingStatePrev = CO_NMT_INITIALIZING;
    SRDOGuard->configurationValid = configurationValid;
    SRDOGuard->checkCRC = *configurationValid == CO_SRDO_VALID_MAGIC;


    /* Configure Object dictionary entry at index 0x13FE and 0x13FF */
    SRDOGuard->OD_13FE_extension.object = SRDOGuard;
    SRDOGuard->OD_13FE_extension.read = OD_readOriginal;
    SRDOGuard->OD_13FE_extension.write = OD_write_13FE;
    OD_extension_init(OD_13FE_SRDOValid, &SRDOGuard->OD_13FE_extension);
    
    SRDOGuard->OD_13FF_extension.object = SRDOGuard;
    SRDOGuard->OD_13FF_extension.read = OD_readOriginal;
    SRDOGuard->OD_13FF_extension.write = OD_write_13FF;
    OD_extension_init(OD_13FF_SRDOCRC, &SRDOGuard->OD_13FF_extension);

    return CO_ERROR_NO;
}

uint8_t CO_SRDOGuard_process(
        CO_SRDOGuard_t         *SRDOGuard)
{
    uint8_t result = 0;
    CO_NMT_internalState_t operatingState = *SRDOGuard->operatingState;
    if(operatingState != SRDOGuard->operatingStatePrev){
        SRDOGuard->operatingStatePrev = operatingState;
        if (operatingState == CO_NMT_OPERATIONAL){
            result |= 1 << 0;
        }
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
        CO_SDOserver_t         *SDO,
        uint8_t                 nodeId,
        uint16_t                defaultCOB_ID,
        OD_entry_t *OD_130x_SRDOCommPar,
        OD_entry_t  *OD_138x_SRDOMapPar,
        const uint16_t         *checksum,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdxNormal,
        uint16_t                CANdevRxIdxInverted,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdxNormal,
        uint16_t                CANdevTxIdxInverted,
        uint32_t *errInfo)
{
    ODR_t odRet;

        /* verify arguments */
    if((SRDO==NULL) || (SRDOGuard==NULL) || (em==NULL) || (SDO==NULL) || (checksum==NULL) ||
        (OD_130x_SRDOCommPar==NULL) || (OD_138x_SRDOMapPar==NULL) || (CANdevRx==NULL) || (CANdevTx==NULL)){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
        
    /* clear object */
    memset(SRDO, 0, sizeof(CO_SRDO_t));

    /* Configure object variables */
    SRDO->SRDOGuard = SRDOGuard;
    SRDO->em = em;
    SRDO->SDO = SDO;
    SRDO->SRDOCommPar = OD_130x_SRDOCommPar;
    SRDO->SRDOMapPar = OD_138x_SRDOMapPar;
    SRDO->checksum = checksum;
    SRDO->CANdevRx = CANdevRx;
    SRDO->CANdevRxIdx[0] = CANdevRxIdxNormal;
    SRDO->CANdevRxIdx[1] = CANdevRxIdxInverted;
    SRDO->CANdevTx = CANdevTx;
    SRDO->CANdevTxIdx[0] = CANdevTxIdxNormal;
    SRDO->CANdevTxIdx[1] = CANdevTxIdxInverted;
    SRDO->nodeId = nodeId;
    SRDO->defaultCOB_ID[0] = defaultCOB_ID;
    SRDO->defaultCOB_ID[1] = defaultCOB_ID + 1U;
    SRDO->valid = CO_SRDO_INVALID;
    SRDO->toogle = 0;
    SRDO->timer = 0;
    SRDO->pFunctSignalSafe = NULL;
    SRDO->functSignalObjectSafe = NULL;
#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
    SRDO->pFunctSignalPre = NULL;
    SRDO->functSignalObjectPre = NULL;
#endif

    uint8_t informationDirection = 0;
    odRet = OD_get_u8(OD_130x_SRDOCommPar, 1, &informationDirection, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = (((uint32_t)OD_getIndex(OD_130x_SRDOCommPar)) << 8) | 1U;
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    SRDO->CommPar_informationDirection = informationDirection;

    uint16_t safetyCycleTime = 0;
    odRet = OD_get_u16(OD_130x_SRDOCommPar, 2, &safetyCycleTime, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = (((uint32_t)OD_getIndex(OD_130x_SRDOCommPar)) << 8) | 2U;
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    SRDO->CommPar_safetyCycleTime= safetyCycleTime;
    
    uint8_t safetyRelatedValidationTime = 0;
    odRet = OD_get_u8(OD_130x_SRDOCommPar, 3, &safetyRelatedValidationTime, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = (((uint32_t)OD_getIndex(OD_130x_SRDOCommPar)) << 8) | 3U;
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    SRDO->CommPar_safetyRelatedValidationTime = safetyRelatedValidationTime;
    
    uint8_t transmissionType = 0;
    odRet = OD_get_u8(OD_130x_SRDOCommPar, 4, &transmissionType, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = (((uint32_t)OD_getIndex(OD_130x_SRDOCommPar)) << 8) | 4U;
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    SRDO->CommPar_transmissionType = transmissionType;
    
    uint32_t COB_ID1_normal = 0;
    odRet = OD_get_u32(OD_130x_SRDOCommPar, 5, &COB_ID1_normal, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = (((uint32_t)OD_getIndex(OD_130x_SRDOCommPar)) << 8) | 5U;
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    SRDO->CommPar_COB_ID1_normal = COB_ID1_normal;
    
    uint32_t COB_ID2_inverted = 0;
    odRet = OD_get_u32(OD_130x_SRDOCommPar, 6, &COB_ID2_inverted, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = (((uint32_t)OD_getIndex(OD_130x_SRDOCommPar)) << 8) | 6U;
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    SRDO->CommPar_COB_ID2_inverted = COB_ID2_inverted;

    /* Configure Object dictionary entry at index 0x1301+ and 0x1381+ */
    SRDO->OD_communicationParam_ext.object = SRDO;
    SRDO->OD_communicationParam_ext.read = OD_read_SRDO_communicationParam;
    SRDO->OD_communicationParam_ext.write = OD_write_SRDO_communicationParam;
    OD_extension_init(OD_130x_SRDOCommPar, &SRDO->OD_communicationParam_ext);
    
    uint8_t numberOfMappedObjects = 0;
    odRet = OD_get_u8(OD_138x_SRDOMapPar, 0, &numberOfMappedObjects, true);
    if (odRet != ODR_OK) {
        if (errInfo != NULL) {
            *errInfo = (((uint32_t)OD_getIndex(OD_138x_SRDOMapPar)) << 8) | 0U;
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    SRDO->mappedObjectsCount = numberOfMappedObjects;

    SRDO->OD_mappingParam_extension.object = SRDO;
    SRDO->OD_mappingParam_extension.read = OD_readOriginal;
    SRDO->OD_mappingParam_extension.write = OD_write_SRDO_mappingParam;
    OD_extension_init(OD_138x_SRDOMapPar, &SRDO->OD_mappingParam_extension);

    return CO_ERROR_NO;
}

CO_ReturnError_t CO_SRDO_requestSend(
        CO_SRDO_t              *SRDO)
{
    if (*SRDO->SRDOGuard->operatingState != CO_NMT_OPERATIONAL){
        return CO_ERROR_WRONG_NMT_STATE;
    }

    if (SRDO->valid != CO_SRDO_TX){
        return CO_ERROR_TX_UNCONFIGURED;
    }

    if(SRDO->toogle != 0U){
        return CO_ERROR_TX_BUSY;
    }

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
        if (*SRDO->checksum != crcValue){
            *SRDO->SRDOGuard->configurationValid = 0;
        }
    }

    if((commands & (1<<0)) && (*SRDO->SRDOGuard->configurationValid == CO_SRDO_VALID_MAGIC)){
        if(SRDO_configMap(SRDO,SRDO->SDO->OD,SRDO->SRDOMapPar) == CO_ERROR_NO){
            CO_SRDOconfigCom(SRDO, SRDO->CommPar_COB_ID1_normal, SRDO->CommPar_COB_ID2_inverted);
        }
        else{
            SRDO->valid = CO_SRDO_INVALID;
        }
    }

    if(SRDO->valid && (*SRDO->SRDOGuard->operatingState == CO_NMT_OPERATIONAL)){
        SRDO->timer = (SRDO->timer > timeDifference_us) ? (SRDO->timer - timeDifference_us) : 0U;
        if(SRDO->valid == CO_SRDO_TX){
            if(SRDO->timer == 0U){
                if(SRDO->toogle){
                    CO_CANsend(SRDO->CANdevTx, SRDO->CANtxBuff[1]);
                    SRDO->timer = ((uint32_t)SRDO->CommPar_safetyCycleTime * 1000U) - CO_CONFIG_SRDO_MINIMUM_DELAY;
                }
                else{
                    uint16_t i;
                    uint8_t* pSRDOdataByte_normal;
                    uint8_t* pSRDOdataByte_inverted;

                    uint8_t** ppODdataByte_normal;
                    uint8_t** ppODdataByte_inverted;

                    bool_t data_ok = true;
#if (CO_CONFIG_SRDO) & CO_CONFIG_TSRDO_CALLS_EXTENSION
                    if(SRDO->SDO->ODExtensions){
                        /* for each mapped OD, check mapping to see if an OD extension is available, and call it if it is */
                        const uint32_t* pMap = &SRDO->SRDOMapPar->mappedObjects[0];
                        CO_SDOserver_t *pSDO = SRDO->SDO;

                        for(i=SRDO->mappedObjectsCount; i>0; i--){
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
                        uint8_t invert = (uint8_t)(~pSRDOdataByte_inverted[i]);
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
                            CO_SDOserver_t *pSDO = SRDO->SDO;

                            for(i=SRDO->mappedObjectsCount; i>0; i--){
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

                    SRDO->timer = (uint32_t)SRDO->CommPar_safetyCycleTime * 1000U;
                }
                else{
                    SRDO->timer = (uint32_t)SRDO->CommPar_safetyRelatedValidationTime * 1000U;
                }
                SRDO->toogle = (uint8_t)(!SRDO->toogle);
            }

            if(SRDO->timer == 0U){
                SRDO->toogle = 0;
                SRDO->timer = (uint32_t)SRDO->CommPar_safetyRelatedValidationTime * 1000U;
                CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
                CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
                /* save state */
                if(SRDO->pFunctSignalSafe != NULL){
                    SRDO->pFunctSignalSafe(SRDO->functSignalObjectSafe);
                }
            }
        }
        else { /* MISRA C 2004 14.10 */ }
    }
    else{
        SRDO->valid = CO_SRDO_INVALID;
        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
    }
}

#endif /* (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE */
