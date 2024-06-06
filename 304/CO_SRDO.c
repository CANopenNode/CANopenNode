/**
 * CANopen Safety Related Data Object protocol.
 *
 * @file        CO_SRDO.c
 * @ingroup     CO_SRDO
 * @author      Robert Grüning
 * @copyright   2020 Robert Grüning
 * @copyright   2024 temi54c1l8@github
 * @copyright   2024 Janez Paternoster
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

#ifdef CO_CONFORMANCE_TEST_TOOL_ADAPTATION
#warning CO_CONFORMANCE_TEST_TOOL_ADAPTATION may be used only for conformance testing (because of CTT limitations)
#endif

/* values for informationDirection and configurationValid */
#define CO_SRDO_INVALID            (0U)
#define CO_SRDO_TX                 (1U)
#define CO_SRDO_RX                 (2U)
#define CO_SRDO_VALID_MAGIC        (0xA5)

/* macro for information about SRDO configuration error */
#define ERR_INFO(index, subindex, info) (((uint32_t)(index) << 16) | ((uint32_t)(subindex) << 8) | ((uint32_t)(info)))

static void
CO_SRDO_receive_normal(void* object, void* msg) {
    CO_SRDO_t* SRDO = (CO_SRDO_t*)object;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t* data = CO_CANrxMsg_readData(msg);

    if ((SRDO->informationDirection == CO_SRDO_RX) && (DLC >= SRDO->dataLength) && !CO_FLAG_READ(SRDO->CANrxNew[1])) {
        /* copy data into appropriate buffer and set 'new message' flag */
        memcpy(SRDO->CANrxData[0], data, sizeof(SRDO->CANrxData[0]));
        CO_FLAG_SET(SRDO->CANrxNew[0]);

#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
        /* Optional signal to RTOS, which can resume task, which handles SRDO. */
        if (SRDO->pFunctSignalPre != NULL) {
            SRDO->pFunctSignalPre(SRDO->functSignalObjectPre);
        }
#endif
    }
    else if (DLC < SRDO->dataLength) {
        SRDO->rxSrdoShort = true;
    }
    else { /* MISRA C 2004 14.10 */ }
}

static void
CO_SRDO_receive_inverted(void* object, void* msg) {
    CO_SRDO_t* SRDO = (CO_SRDO_t*)object;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t* data = CO_CANrxMsg_readData(msg);

    if ((SRDO->informationDirection == CO_SRDO_RX) && (DLC >= SRDO->dataLength) && CO_FLAG_READ(SRDO->CANrxNew[0])) {
        /* copy data into appropriate buffer and set 'new message' flag */
        memcpy(SRDO->CANrxData[1], data, sizeof(SRDO->CANrxData[1]));
        CO_FLAG_SET(SRDO->CANrxNew[1]);

#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
        /* Optional signal to RTOS, which can resume task, which handles SRDO. */
        if (SRDO->pFunctSignalPre != NULL) {
            SRDO->pFunctSignalPre(SRDO->functSignalObjectPre);
        }
#endif
    }
    else if (DLC < SRDO->dataLength) {
        SRDO->rxSrdoShort = true;
    }
    else { /* MISRA C 2004 14.10 */ }
}

/* Set OD object 13FE:00 to CO_SRDO_INVALID and clear configurationValid flag. */
static void
configurationValidUnset(CO_SRDOGuard_t* SRDOGuard) {
    if (SRDOGuard != NULL) {
        OD_IO_t* OD_IO = &SRDOGuard->OD_IO_configurationValid;
        uint8_t val = CO_SRDO_INVALID;
        OD_size_t dummy;

        SRDOGuard->configurationValid = SRDOGuard->_configurationValid = false;

        OD_IO->write(&OD_IO->stream, &val, sizeof(val), &dummy);
    }
}

/*
 * Custom functions for reading or writing OD object.
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t
OD_write_dummy(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten)
{
    (void) stream; (void) buf;
    if (countWritten != NULL) { *countWritten = count; }
    return ODR_OK;
}

static ODR_t
OD_read_dummy(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *countRead)
{
    if (buf == NULL || stream == NULL || countRead == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    if (count > stream->dataLength) {
        count = stream->dataLength;
    }

    memset(buf, 0, count);

    *countRead = count;
    return ODR_OK;
}

#ifdef CO_CONFORMANCE_TEST_TOOL_ADAPTATION
bool_t
OD_not_write_same_value(OD_stream_t *stream, const void *buf, OD_size_t count) {
    // The conformance test tool does not recognize CANopen Safety and on all object dictionaty tries to read and write the same value
    OD_size_t countRead = 0;
    uint8_t bufRead[6] = { 0 };
    if( count > 6 ) {
        return false;
    }
    ODR_t returnCode = OD_readOriginal(stream, bufRead, count, &countRead);
    if ( returnCode != ODR_OK ){
        return false;
    }
    if ( memcmp(buf,bufRead,count) == 0 ){
        return true;
    }
    return false;
}
#endif

static ODR_t
OD_read_SRDO_communicationParam(OD_stream_t* stream, void* buf, OD_size_t count, OD_size_t* countRead) {
    ODR_t returnCode = OD_readOriginal(stream, buf, count, countRead);

    /* When reading COB_ID, add Node-Id to the read value, if necessary */
    if (returnCode == ODR_OK && (stream->subIndex == 5U || stream->subIndex == 6U) && *countRead == 4) {
        CO_SRDO_t* SRDO = stream->object;

        uint32_t value = CO_getUint32(buf);
        uint16_t defaultCOB_ID = SRDO->defaultCOB_ID + (stream->subIndex - 5U);

        /* If default COB ID is used, then OD entry does not contain $NodeId. Add it here. */
        if ((value == defaultCOB_ID) && (SRDO->nodeId <= 64U)) {
            value += (uint32_t)SRDO->nodeId * 2;
        }

        CO_setUint32(buf, value);
    }

    return returnCode;
}

static ODR_t
OD_write_SRDO_communicationParam(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if ((stream == NULL) || (buf == NULL) || (countWritten == NULL) || (count > 4)) {
        return ODR_DEV_INCOMPAT;
    }

#ifdef CO_CONFORMANCE_TEST_TOOL_ADAPTATION
    if( OD_not_write_same_value(stream, buf, count) ) {
        return ODR_OK;
    }
#endif

    CO_SRDO_t* SRDO = stream->object;
    CO_SRDOGuard_t* SRDOGuard = SRDO->SRDOGuard;
    uint8_t bufCopy[4];
    memcpy(bufCopy, buf, count);

    /* Writing Object Dictionary variable */
    if (SRDOGuard->NMTisOperational) {
        /* Data cannot be transferred or stored to the application because of the present device state. */
        return ODR_DATA_DEV_STATE;
    }

    if (stream->subIndex == 1U) { /* Information direction */
        uint8_t value = CO_getUint8(buf);
        if (value > 2U) {
            return ODR_INVALID_VALUE;
        }
        SRDO->informationDirection = value;
    } else if (stream->subIndex == 2) { /* SCT */
        uint16_t value = CO_getUint16(buf);
        if (value < ((CO_CONFIG_SRDO_MINIMUM_DELAY / 1000) + 1)) {
            return ODR_INVALID_VALUE;
        }
    } else if (stream->subIndex == 3) { /* SRVT */
        uint8_t value = CO_getUint8(buf);
        if (value == 0) {
            return ODR_INVALID_VALUE;
        }
    } else if (stream->subIndex == 4) { /* Transmission_type */
        uint8_t value = CO_getUint8(buf);
        if (value != 254) {
            return ODR_INVALID_VALUE;
        }
    } else if (stream->subIndex == 5U || stream->subIndex == 6U) { /* COB_ID */
        uint32_t value = CO_getUint32(buf);
        uint16_t index = stream->subIndex - 5U;
        uint16_t defaultCOB_ID = SRDO->defaultCOB_ID + index;

        /* check value range, the spec does not specify if COB-ID flags are allowed */
        if ((value < 0x101) || (value > 0x180U) || ((value & 1) == index)) {
            return ODR_INVALID_VALUE; /* Invalid value for parameter (download only). */
        }

        /* if default COB-ID is being written, write defaultCOB_ID without nodeId */
        if ((SRDO->nodeId <= 64U) && (value == (defaultCOB_ID + ((uint32_t)SRDO->nodeId * 2)))) {
            value = defaultCOB_ID;
            CO_setUint32(bufCopy, value);
        }
    } else { /* MISRA C 2004 14.10 */
    }

    /* set OD object 13FE:00 to CO_SRDO_INVALID */
    configurationValidUnset(SRDOGuard);

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, bufCopy, count, countWritten);
}

static ODR_t
OD_write_SRDO_mappingParam(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if ((stream == NULL) || (buf == NULL) || (countWritten == NULL)
        || (stream->subIndex > CO_SRDO_MAX_MAPPED_ENTRIES)) {
        return ODR_DEV_INCOMPAT;
    }

#ifdef CO_CONFORMANCE_TEST_TOOL_ADAPTATION
    if( OD_not_write_same_value(stream, buf, count) ) {
        return ODR_OK;
    }
#endif

    CO_SRDO_t* SRDO = stream->object;
    CO_SRDOGuard_t* SRDOGuard = SRDO->SRDOGuard;

    /* Writing Object Dictionary variable */
    if (SRDOGuard->NMTisOperational) {
        /* Data cannot be transferred or stored to the application because of the present device state. */
        return ODR_DATA_DEV_STATE;
    }

    /* SRDO must be disabled */
    if (SRDO->informationDirection != 0) {
        return ODR_UNSUPP_ACCESS; /* Unsupported access to an object. */
    }

    /* numberOfMappedObjects */
    if (stream->subIndex == 0U) {
        uint8_t value = CO_getUint8(buf);
        /* only odd numbers are allowed */
        if ((value > CO_SRDO_MAX_MAPPED_ENTRIES) || (value & 1)) {
            return ODR_MAP_LEN; /* Number and length of object to be mapped exceeds SRDO length. */
        }
        SRDO->mappedObjectsCount = value;
    }
    /* mapping objects */
    else {
        if (SRDO->mappedObjectsCount != 0U) {
            return ODR_UNSUPP_ACCESS;
        }
        /* No other checking is implemented here. Values are validated in the configuration function. */
    }

    /* set OD object 13FE:00 to CO_SRDO_INVALID */
    configurationValidUnset(SRDOGuard);

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

static ODR_t
OD_write_13FE(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if ((stream == NULL) || (stream->subIndex != 0U) || (buf == NULL) || (countWritten == NULL)) {
        return ODR_DEV_INCOMPAT;
    }

    CO_SRDOGuard_t* SRDOGuard = stream->object;

    if (SRDOGuard->NMTisOperational) {
        /* Data cannot be transferred or stored to the application because of the present device state. */
        return ODR_DATA_DEV_STATE;
    }

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

static ODR_t
OD_write_13FF(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if ((stream == NULL) || (stream->subIndex == 0U) || (buf == NULL) || (countWritten == NULL)) {
        return ODR_DEV_INCOMPAT;
    }

    CO_SRDOGuard_t* SRDOGuard = stream->object;

    if (SRDOGuard->NMTisOperational) {
        /* Data cannot be transferred or stored to the application because of the present device state. */
        return ODR_DATA_DEV_STATE;
    }

    /* set OD object 13FE:00 to CO_SRDO_INVALID */
    configurationValidUnset(SRDOGuard);

    /* write value to the original location in the Object Dictionary */
    return OD_writeOriginal(stream, buf, count, countWritten);
}

#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
void
CO_SRDO_initCallbackPre(CO_SRDO_t* SRDO, void* object, void (*pFunctSignalPre)(void* object)) {
    if (SRDO != NULL) {
        SRDO->functSignalObjectPre = object;
        SRDO->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif

CO_ReturnError_t
CO_SRDO_init_start(CO_SRDOGuard_t* SRDOGuard, OD_entry_t* OD_13FE_configurationValid,
                   OD_entry_t* OD_13FF_safetyConfigurationSignature, uint32_t* errInfo) {
    ODR_t odRet;
    uint8_t configurationValid;

    /* verify arguments */
    if ((SRDOGuard == NULL) || (OD_13FE_configurationValid == NULL) || (OD_13FF_safetyConfigurationSignature == NULL)) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear object */
    memset(SRDOGuard, 0, sizeof(CO_SRDOGuard_t));

    /* Configure Object dictionary extensions */
    SRDOGuard->OD_13FE_extension.object = SRDOGuard;
    SRDOGuard->OD_13FE_extension.read = OD_readOriginal;
    SRDOGuard->OD_13FE_extension.write = OD_write_13FE;
    OD_extension_init(OD_13FE_configurationValid, &SRDOGuard->OD_13FE_extension);

    SRDOGuard->OD_13FF_extension.object = SRDOGuard;
    SRDOGuard->OD_13FF_extension.read = OD_readOriginal;
    SRDOGuard->OD_13FF_extension.write = OD_write_13FF;
    OD_extension_init(OD_13FF_safetyConfigurationSignature, &SRDOGuard->OD_13FF_extension);

    /* Configure SRDOGuard->OD_IO_configurationValid variable.
     * It will be used for writing 0 to OD variable 13FE,00 */
    odRet = OD_getSub(OD_13FE_configurationValid, 0, &SRDOGuard->OD_IO_configurationValid, false);
    if (odRet != ODR_OK || SRDOGuard->OD_IO_configurationValid.stream.dataLength != 1) {
        if (errInfo != NULL) {
            *errInfo = (((uint32_t)OD_getIndex(OD_13FE_configurationValid)) << 8) | 1U;
        }
        return CO_ERROR_OD_PARAMETERS;
    }
    
    if (OD_get_u8(OD_13FE_configurationValid, 0, &configurationValid, true) != ODR_OK) {
        *errInfo = (((uint32_t)OD_getIndex(OD_13FE_configurationValid)) << 8) | 1U;
        return CO_ERROR_OD_PARAMETERS;
    }
    
    if (configurationValid == CO_SRDO_VALID_MAGIC) {
        /* Private variable, erroneous SRDO initialization will clear this. */
        SRDOGuard->_configurationValid = true;
    }

    return CO_ERROR_NO;
}

CO_ReturnError_t
CO_SRDO_init(CO_SRDO_t* SRDO, uint8_t SRDO_Index, CO_SRDOGuard_t* SRDOGuard, OD_t* OD, CO_EM_t* em, uint8_t nodeId,
             uint16_t defaultCOB_ID, OD_entry_t* OD_130x_SRDOCommPar, OD_entry_t* OD_138x_SRDOMapPar,
             OD_entry_t* OD_13FE_configurationValid, OD_entry_t* OD_13FF_safetyConfigurationSignature,
             CO_CANmodule_t* CANdevRxNormal, CO_CANmodule_t* CANdevRxInverted, uint16_t CANdevRxIdxNormal,
             uint16_t CANdevRxIdxInverted, CO_CANmodule_t* CANdevTxNormal, CO_CANmodule_t* CANdevTxInverted,
             uint16_t CANdevTxIdxNormal, uint16_t CANdevTxIdxInverted, uint32_t* errInfo) {

    CO_ReturnError_t ret = CO_ERROR_NO;
    uint32_t err = 0;
    bool_t configurationInProgress = false;

    /* variables will be retrieved from Object Dictionary */
    uint8_t cp_highestSubindexSupported;
    uint8_t informationDirection;
    uint16_t safetyCycleTime;
    uint8_t safetyRelatedValidationTime;
    uint8_t transmissionType;
    uint32_t COB_ID1_normal;
    uint32_t COB_ID2_inverted;
    uint8_t configurationValid;
    uint16_t crcSignatureFromOD;
    uint8_t mappedObjectsCount;
    uint32_t mapping[CO_SRDO_MAX_MAPPED_ENTRIES];

    /* verify arguments */
    if ((SRDO == NULL) || (SRDOGuard == NULL) || (OD == NULL) || (em == NULL) || (OD_130x_SRDOCommPar == NULL)
        || (OD_138x_SRDOMapPar == NULL) || (OD_13FE_configurationValid == NULL) || (OD_13FF_safetyConfigurationSignature == NULL)
        || (CANdevRxNormal == NULL) || (CANdevRxInverted == NULL) || (CANdevTxNormal == NULL) || (CANdevTxInverted == NULL)) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
        err = 1;
    }

    /* clear object and configure some object variables */
    if (err == 0) {
        memset(SRDO, 0, sizeof(CO_SRDO_t));

        SRDO->SRDOGuard = SRDOGuard;
        SRDO->em = em;
        SRDO->defaultCOB_ID = defaultCOB_ID;
        SRDO->nodeId = nodeId;
        SRDO->CANdevTx[0] = CANdevTxNormal;
        SRDO->CANdevTx[1] = CANdevTxInverted;

        /* Configure Object dictionary entry at index 0x1301+ */
        SRDO->OD_communicationParam_ext.object = SRDO;
        SRDO->OD_communicationParam_ext.read = OD_read_SRDO_communicationParam;
        SRDO->OD_communicationParam_ext.write = OD_write_SRDO_communicationParam;
        OD_extension_init(OD_130x_SRDOCommPar, &SRDO->OD_communicationParam_ext);

        /* Configure Object dictionary entry at index 0x1381+ */
        SRDO->OD_mappingParam_extension.object = SRDO;
        SRDO->OD_mappingParam_extension.read = OD_readOriginal;
        SRDO->OD_mappingParam_extension.write = OD_write_SRDO_mappingParam;
        OD_extension_init(OD_138x_SRDOMapPar, &SRDO->OD_mappingParam_extension);
    }

    /* Get variables from object Dictionary and verify it's structure. */
    if (err == 0) {
        if (OD_get_u8(OD_130x_SRDOCommPar, 0, &cp_highestSubindexSupported, true) != ODR_OK) {
            err = ERR_INFO(0x1301 + SRDO_Index, 0, 1);
        }
        else if (OD_get_u8(OD_130x_SRDOCommPar, 1, &informationDirection, true) != ODR_OK) {
            err = ERR_INFO(0x1301 + SRDO_Index, 1, 1);
        }
        else if (OD_get_u16(OD_130x_SRDOCommPar, 2, &safetyCycleTime, true) != ODR_OK) {
            err = ERR_INFO(0x1301 + SRDO_Index, 2, 1);
        }
        else if (OD_get_u8(OD_130x_SRDOCommPar, 3, &safetyRelatedValidationTime, true) != ODR_OK) {
            err = ERR_INFO(0x1301 + SRDO_Index, 3, 1);
        }
        else if (OD_get_u8(OD_130x_SRDOCommPar, 4, &transmissionType, true) != ODR_OK) {
            err = ERR_INFO(0x1301 + SRDO_Index, 4, 1);
        }
        else if (OD_get_u32(OD_130x_SRDOCommPar, 5, &COB_ID1_normal, true) != ODR_OK) {
            err = ERR_INFO(0x1301 + SRDO_Index, 5, 1);
        }
        else if (OD_get_u32(OD_130x_SRDOCommPar, 6, &COB_ID2_inverted, true) != ODR_OK) {
            err = ERR_INFO(0x1301 + SRDO_Index, 6, 1);
        }
        else if (OD_get_u8(OD_13FE_configurationValid, 0, &configurationValid, true) != ODR_OK) {
            err = ERR_INFO(0x13FE, 0, 1);
        }
        else if (OD_get_u16(OD_13FF_safetyConfigurationSignature, SRDO_Index + 1, &crcSignatureFromOD, true) != ODR_OK) {
            err = ERR_INFO(0x13FF, SRDO_Index + 1, 1);
        }
        else if (OD_get_u8(OD_138x_SRDOMapPar, 0, &mappedObjectsCount, true) != ODR_OK) {
            err = ERR_INFO(0x1381 + SRDO_Index, 0, 1);
        }
        else {
            for (uint8_t i = 0; i < mappedObjectsCount; i++) {
                if (OD_get_u32(OD_138x_SRDOMapPar, i+1, &mapping[i], true) != ODR_OK) {
                    err = ERR_INFO(0x1381 + SRDO_Index, i+1, 1);
                    break;
                }
            }
        }

        /* if OD contains default COB_IDs, add node-id */
        if (COB_ID1_normal == defaultCOB_ID && COB_ID2_inverted == (defaultCOB_ID + 1) && nodeId <= 64U) {
            uint32_t add = (uint32_t)SRDO->nodeId * 2;
            COB_ID1_normal += add;
            COB_ID2_inverted += add;
        }

        /* If this fails, something is wrong with the Object Dictionary. Device have to be reprogrammed. */
        if (err != 0) {
            ret = CO_ERROR_OD_PARAMETERS;
        }
    }

    /* If configurationValid is set and SRDO is valid, continue with further configuration */
    if (err == 0 && configurationValid == CO_SRDO_VALID_MAGIC && informationDirection != CO_SRDO_INVALID) {
        configurationInProgress = true;
    }

    /* Verify parameters from OD */
    if (err == 0 && configurationInProgress) {
        if (cp_highestSubindexSupported != 6) {
            err = ERR_INFO(0x1301 + SRDO_Index, 0, 2);
        }
        else if (informationDirection > 3) {
            err = ERR_INFO(0x1301 + SRDO_Index, 1, 2);
        }
        else if (safetyCycleTime < ((CO_CONFIG_SRDO_MINIMUM_DELAY / 1000) + 1)) {
            err = ERR_INFO(0x1301 + SRDO_Index, 2, 2);
        }
        else if (safetyRelatedValidationTime < 1) {
            err = ERR_INFO(0x1301 + SRDO_Index, 3, 2);
        }
        else if (transmissionType != 254) {
            err = ERR_INFO(0x1301 + SRDO_Index, 4, 2);
        }
        else if (COB_ID1_normal < 0x101 || (COB_ID1_normal & 1) == 0) {
            err = ERR_INFO(0x1301 + SRDO_Index, 5, 2);
        }
        else if ((COB_ID1_normal + 1) != COB_ID2_inverted || COB_ID2_inverted > 0x180) {
            err = ERR_INFO(0x1301 + SRDO_Index, 6, 2);
        }
        else if (mappedObjectsCount > CO_SRDO_MAX_MAPPED_ENTRIES || (mappedObjectsCount & 1) != 0) {
            err = ERR_INFO(0x1381 + SRDO_Index, 0, 2);
        }
        else {
            /* MISRA C 2004 14.10 */
        }
    }

    /* Verify CRC */
    if (err == 0 && configurationInProgress) {
        uint16_t crcResult = 0x0000;
        uint16_t tmp_u16;
        uint32_t tmp_u32;

        crcResult = crc16_ccitt(&informationDirection, 1, crcResult);
        tmp_u16 = CO_SWAP_16(safetyCycleTime);
        crcResult = crc16_ccitt((uint8_t *)&tmp_u16, 2, crcResult);
        crcResult = crc16_ccitt(&safetyRelatedValidationTime, 1, crcResult);
        tmp_u32 = CO_SWAP_32(COB_ID1_normal);
        crcResult = crc16_ccitt((uint8_t *)&tmp_u32, 4, crcResult);
        tmp_u32 = CO_SWAP_32(COB_ID2_inverted);
        crcResult = crc16_ccitt((uint8_t *)&tmp_u32, 4, crcResult);
        crcResult = crc16_ccitt(&mappedObjectsCount, 1, crcResult);
        for (uint8_t i = 0; i < mappedObjectsCount; i++) {
            uint8_t subindex = i + 1U;
            crcResult = crc16_ccitt(&subindex, 1, crcResult);
            tmp_u32 = CO_SWAP_32(mapping[i]);
            crcResult = crc16_ccitt((uint8_t *)&tmp_u32, 4, crcResult);
        }

        if (crcResult != crcSignatureFromOD) {
            err = ERR_INFO(0x13FF, SRDO_Index + 1, 3);
        }
    }

    /* Configure mappings */
    if (err == 0 && configurationInProgress) {
        size_t srdoDataLength[2] = {0, 0};

        for (uint8_t i = 0; i < mappedObjectsCount; i++) {
            uint8_t plain_inverted = i % 2;
            uint32_t map = mapping[i];
            uint16_t index = (uint16_t) (map >> 16);
            uint8_t subIndex = (uint8_t) (map >> 8);
            uint8_t mappedLengthBits = (uint8_t) map;
            uint8_t mappedLength = mappedLengthBits >> 3;
            OD_IO_t *OD_IO = &SRDO->OD_IO[i];

            /* total SRDO length can not be more than CO_SRDO_MAX_SIZE bytes */
            if (mappedLength > CO_SRDO_MAX_SIZE) {
                err = ERR_INFO(0x1381 + SRDO_Index, i + 1, 4);
            }
            /* is there a reference to the dummy entry */
            else if (index < 0x20 && subIndex == 0) {
                OD_stream_t *stream = &OD_IO->stream;
                memset(stream, 0, sizeof(OD_stream_t));
                stream->dataLength = stream->dataOffset = mappedLength;
                OD_IO->read = OD_read_dummy;
                OD_IO->write = OD_write_dummy;
            }
            /* find entry in the Object Dictionary */
            else {
                OD_IO_t OD_IOcopy;
                OD_entry_t *entry = OD_find(OD, index);
                ODR_t odRet = OD_getSub(entry, subIndex, &OD_IOcopy, false);
                if (odRet != ODR_OK) {
                    err = ERR_INFO(0x1381 + SRDO_Index, i + 1, 5);
                }
                else {
                    /* verify access attributes, byte alignment and length */
                    OD_attr_t testAttribute = (informationDirection == CO_SRDO_RX) ? ODA_RSRDO : ODA_TSRDO;
                    if ((OD_IOcopy.stream.attribute & testAttribute) == 0
                        || (mappedLengthBits & 0x07) != 0
                        || OD_IOcopy.stream.dataLength < mappedLength
                    ) {
                        err = ERR_INFO(0x1381 + SRDO_Index, i + 1, 6);
                    }

                    /* Copy values and store mappedLength temporary. */
                    *OD_IO = OD_IOcopy;
                    OD_IO->stream.dataOffset = mappedLength;
                    srdoDataLength[plain_inverted] += mappedLength;
                }
            }
            if (err != 0) {
                break;
            }
        } /* for (uint8_t i = 0; i < mappedObjectsCount; i++) */

        if (err == 0) {
            if (srdoDataLength[0] != srdoDataLength[1]) {
                err = ERR_INFO(0x1381 + SRDO_Index, 0, 7);
            }
            else if (srdoDataLength[0] == 0 || srdoDataLength[0] > CO_SRDO_MAX_SIZE) {
                err = ERR_INFO(0x1381 + SRDO_Index, 0, 8);
            }
            else {
                SRDO->dataLength = srdoDataLength[0];
                SRDO->mappedObjectsCount = mappedObjectsCount;
            }
        }
    }

    /* Configure CAN tx buffers */
    if (err == 0 && configurationInProgress && informationDirection == CO_SRDO_TX) {

        SRDO->CANtxBuff[0] = CO_CANtxBufferInit(CANdevTxNormal, /* CAN device */
                                                CANdevTxIdxNormal, /* index of specific buffer inside CAN module */
                                                COB_ID1_normal, /* CAN identifier */
                                                0, /* rtr */
                                                SRDO->dataLength, /* number of data bytes */
                                                0); /* synchronous message flag bit */

        if (SRDO->CANtxBuff[0] == NULL) {
            err = ERR_INFO(0x1301 + SRDO_Index, 5, 10);
        }

        SRDO->CANtxBuff[1] = CO_CANtxBufferInit(CANdevTxInverted, /* CAN device */
                                                CANdevTxIdxInverted, /* index of specific buffer inside CAN module */
                                                COB_ID2_inverted, /* CAN identifier */
                                                0, /* rtr */
                                                SRDO->dataLength, /* number of data bytes */
                                                0); /* synchronous message flag bit */

        if (SRDO->CANtxBuff[1] == NULL) {
            err = ERR_INFO(0x1301 + SRDO_Index, 6, 10);
        }
    }

    /* Configure CAN rx buffers */
    if (err == 0 && configurationInProgress && informationDirection == CO_SRDO_RX) {
        CO_ReturnError_t ret;

        ret = CO_CANrxBufferInit(CANdevRxNormal, /* CAN device */
                                 CANdevRxIdxNormal, /* rx buffer index */
                                 COB_ID1_normal, /* CAN identifier */
                                 0x7FF, /* mask */
                                 0, /* rtr */
                                 (void*)SRDO, /* object passed to the receive function */
                                 CO_SRDO_receive_normal); /* this function will process received message */

        if (ret != CO_ERROR_NO) {
            err = ERR_INFO(0x1301 + SRDO_Index, 5, 11);
        }

        ret = CO_CANrxBufferInit(CANdevRxInverted, /* CAN device */
                                 CANdevRxIdxInverted, /* rx buffer index */
                                 COB_ID2_inverted, /* CAN identifier */
                                 0x7FF, /* mask */
                                 0, /* rtr */
                                 (void*)SRDO, /* object passed to the receive function */
                                 CO_SRDO_receive_inverted); /* this function will process received message */

        if (ret != CO_ERROR_NO) {
            err = ERR_INFO(0x1301 + SRDO_Index, 6, 11);
        }
    }

    /* Configure remaining variables */
    if (err == 0) {
        SRDO->informationDirection = informationDirection;
        SRDO->cycleTime_us = (uint32_t)safetyCycleTime * 1000U;
        SRDO->validationTime_us = (uint32_t)safetyRelatedValidationTime * 1000U;
    }
    else {
        if (ret == CO_ERROR_NO) {
            CO_errorReport(em, CO_EM_SRDO_CONFIGURATION, CO_EMC_DATA_SET, err);
            configurationValidUnset(SRDO->SRDOGuard);
        }
    }

    if (errInfo != NULL) {
        *errInfo = err;
    }

    return ret;
}

CO_ReturnError_t
CO_SRDO_requestSend(CO_SRDO_t* SRDO) {
    CO_ReturnError_t ret;

    if (SRDO->SRDOGuard->NMTisOperational == false) {
        ret = CO_ERROR_WRONG_NMT_STATE;
    } else if (SRDO->SRDOGuard->configurationValid == false) {
        ret = CO_ERROR_OD_PARAMETERS;
    } else if (SRDO->informationDirection != CO_SRDO_TX) {
        ret = CO_ERROR_TX_UNCONFIGURED;
    } else if (SRDO->nextIsNormal == false) {
        ret = CO_ERROR_TX_BUSY;
    } else {
        SRDO->cycleTimer = 0;
        ret = CO_ERROR_NO;
    }

    return ret;
}

CO_SRDO_state_t
CO_SRDO_process(CO_SRDO_t* SRDO, uint32_t timeDifference_us, uint32_t* timerNext_us, bool_t NMTisOperational) {
    (void)timerNext_us; /* may be unused */

    if (NMTisOperational && SRDO->informationDirection != CO_SRDO_INVALID && SRDO->SRDOGuard->configurationValid
        && SRDO->internalState >= CO_SRDO_state_unknown) {
        SRDO->cycleTimer = (SRDO->cycleTimer > timeDifference_us) ? (SRDO->cycleTimer - timeDifference_us) : 0U;
        SRDO->validationTimer = (SRDO->validationTimer > timeDifference_us) ? (SRDO->validationTimer - timeDifference_us) : 0U;

        /* Detect transition to NMT operational */
        if (!SRDO->NMTisOperationalPrevious) {
            SRDO->cycleTimer = (SRDO->informationDirection == CO_SRDO_TX)
                             ? (uint32_t)SRDO->nodeId * 500U /* 0.5ms * node-ID delay*/
                             : SRDO->cycleTime_us;
            SRDO->validationTimer = SRDO->cycleTime_us;
            SRDO->internalState = CO_SRDO_state_initializing;
            SRDO->nextIsNormal = true;
        }

        if (SRDO->internalState <= CO_SRDO_state_unknown) {
            SRDO->internalState = CO_SRDO_state_error_internal; /* should not happen */
        }
        else if (SRDO->informationDirection == CO_SRDO_TX) {
            if (SRDO->cycleTimer == 0U) {
                if (SRDO->nextIsNormal) {
                    uint8_t *dataSRDO[2] = {&SRDO->CANtxBuff[0]->data[0], &SRDO->CANtxBuff[1]->data[0]};
                    size_t verifyLength[2] = { 0, 0 };

                    /* copy mapped data from Object Dictionary into CAN buffers */
                    for (uint8_t i = 0; i < SRDO->mappedObjectsCount; i++) {
                        uint8_t plain_inverted = i % 2;
                        OD_IO_t *OD_IO = &SRDO->OD_IO[i];
                        OD_stream_t *stream = &OD_IO->stream;

                        /* get mappedLength from temporary storage */
                        uint8_t mappedLength = (uint8_t) stream->dataOffset;

                        /* additional safety check */
                        verifyLength[plain_inverted] += mappedLength;
                        if (verifyLength[plain_inverted] > CO_SRDO_MAX_SIZE) {
                            break;
                        }

                        /* length of OD variable may be larger than mappedLength */
                        OD_size_t ODdataLength = stream->dataLength;
                        if (ODdataLength > CO_SRDO_MAX_SIZE) {
                            ODdataLength = CO_SRDO_MAX_SIZE;
                        }
                        /* If mappedLength is smaller than ODdataLength, use auxiliary buffer */
                        uint8_t buf[CO_SRDO_MAX_SIZE];
                        uint8_t *dataSRDOCopy;
                        if (ODdataLength > mappedLength) {
                            memset(buf, 0, sizeof(buf));
                            dataSRDOCopy = buf;
                        }
                        else {
                            dataSRDOCopy = dataSRDO[plain_inverted];
                        }

                        /* Set stream.dataOffset to zero, perform OD_IO.read()
                        * and store mappedLength back to stream.dataOffset */
                        stream->dataOffset= 0;
                        OD_size_t countRd;
                        OD_IO->read(stream, dataSRDOCopy, ODdataLength, &countRd);
                        stream->dataOffset = mappedLength;

                        /* swap multibyte data if big-endian */
#ifdef CO_BIG_ENDIAN
                        if ((stream->attribute & ODA_MB) != 0) {
                            uint8_t *lo = dataSRDOCopy;
                            uint8_t *hi = dataSRDOCopy + ODdataLength - 1;
                            while (lo < hi) {
                                uint8_t swap = *lo;
                                *lo++ = *hi;
                                *hi-- = swap;
                            }
                        }
#endif

                        /* If auxiliary buffer, copy it to the SRDO */
                        if (ODdataLength > mappedLength) {
                            memcpy(dataSRDO[plain_inverted], buf, mappedLength);
                        }

                        dataSRDO[plain_inverted] += mappedLength;
                    }

                    if (verifyLength[0] != verifyLength[1] || verifyLength[0] > CO_SRDO_MAX_SIZE || verifyLength[0] != SRDO->dataLength) {
                        SRDO->internalState = CO_SRDO_state_error_internal; /* should not happen */
                    }
                    else {
                        bool_t data_ok = true;
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_CHECK_TX
                        /* check data before sending (optional) */
                        for (uint8_t i = 0; i < SRDO->dataLength; i++) {
                            if ((uint8_t)(~SRDO->CANtxBuff[0]->data[i]) != SRDO->CANtxBuff[1]->data[i]) {
                                SRDO->internalState = CO_SRDO_state_error_txNotInverted;
                                data_ok = false;
                                break;
                            }
                        }
#endif
                        if (data_ok) {
                            if (CO_CANsend(SRDO->CANdevTx[0], SRDO->CANtxBuff[0]) == CO_ERROR_NO) {
                                SRDO->cycleTimer = CO_CONFIG_SRDO_MINIMUM_DELAY;
                                SRDO->internalState = CO_SRDO_state_communicationEstablished;
                            }
                            else {
                                SRDO->internalState = CO_SRDO_state_error_txFail;
                            }
                        }
                    }
                } else {
                    if (CO_CANsend(SRDO->CANdevTx[1], SRDO->CANtxBuff[1]) == CO_ERROR_NO) {
                        SRDO->cycleTimer = SRDO->cycleTime_us - CO_CONFIG_SRDO_MINIMUM_DELAY; /* cycleTime_us is verified, result can't be <0 */
                    }
                    else {
                        SRDO->internalState = CO_SRDO_state_error_txFail;
                    }
                }
                SRDO->nextIsNormal = !SRDO->nextIsNormal;
            }
#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_TIMERNEXT
            if (timerNext_us != NULL) {
                if (*timerNext_us > SRDO->cycleTimer) {
                    *timerNext_us = SRDO->cycleTimer; /* Schedule for the next message timer */
                }
            }
#endif
        }
        else { /* CO_SRDO_RX */
            /* verify error from receive function */
            if (SRDO->rxSrdoShort) {
                CO_errorReport(SRDO->em, CO_EM_RPDO_WRONG_LENGTH, CO_EMC_PDO_LENGTH, 0);
                SRDO->internalState = CO_SRDO_state_error_rxShort;
            }
            else if (CO_FLAG_READ(SRDO->CANrxNew[SRDO->nextIsNormal ? 0 : 1])) {
                /* normal message received ? */
                if (SRDO->nextIsNormal) {
                    SRDO->validationTimer = SRDO->validationTime_us;
                    SRDO->nextIsNormal = false;
                }
                /* inverted message received */
                else {
                    SRDO->cycleTimer = SRDO->validationTimer = SRDO->cycleTime_us;
                    SRDO->nextIsNormal = true;

                    uint8_t *dataSRDO[2] = {&SRDO->CANrxData[0][0], &SRDO->CANrxData[1][0]};
                    bool_t data_ok = true;

                    /* Verify, if normal and inverted data matches properly */
                    for (uint8_t i = 0; i < SRDO->dataLength; i++) {
                        if ((uint8_t)(~dataSRDO[0][i]) != dataSRDO[1][i]) {
                            data_ok = false;
                            SRDO->internalState = CO_SRDO_state_error_rxNotInverted;
                            break;
                        }
                    }

                    /* copy data from CAN messages into mapped data from Object Dictionary */
                    if (data_ok) {
                        size_t verifyLength[2] = { 0, 0 };
                        for (uint8_t i = 0; i < SRDO->mappedObjectsCount; i++) {
                            uint8_t plain_inverted = i % 2;
                            OD_IO_t *OD_IO = &SRDO->OD_IO[i];
                            OD_stream_t *stream = &OD_IO->stream;

                            /* get mappedLength from temporary storage */
                            OD_size_t *dataOffset = &stream->dataOffset;
                            uint8_t mappedLength = (uint8_t) (*dataOffset);

                            /* additional safety check */
                            verifyLength[plain_inverted] += mappedLength;
                            if (verifyLength[plain_inverted] > CO_SRDO_MAX_SIZE) {
                                break;
                            }

                            /* length of OD variable may be larger than mappedLength */
                            OD_size_t ODdataLength = stream->dataLength;
                            if (ODdataLength > CO_SRDO_MAX_SIZE) {
                                ODdataLength = CO_SRDO_MAX_SIZE;
                            }
                            /* Prepare data for writing into OD variable. If mappedLength
                            * is smaller than ODdataLength, then use auxiliary buffer */
                            uint8_t buf[CO_SRDO_MAX_SIZE];
                            uint8_t *dataOD;
                            if (ODdataLength > mappedLength) {
                                memset(buf, 0, sizeof(buf));
                                memcpy(buf, dataSRDO[plain_inverted], mappedLength);
                                dataOD = buf;
                            }
                            else {
                                dataOD = dataSRDO[plain_inverted];
                            }

                            /* swap multibyte data if big-endian */
#ifdef CO_BIG_ENDIAN
                            if ((stream->attribute & ODA_MB) != 0) {
                                uint8_t *lo = dataOD;
                                uint8_t *hi = dataOD + ODdataLength - 1;
                                while (lo < hi) {
                                    uint8_t swap = *lo;
                                    *lo++ = *hi;
                                    *hi-- = swap;
                                }
                            }
#endif

                            /* Set stream.dataOffset to zero, perform OD_IO.write()
                            * and store mappedLength back to stream.dataOffset */
                            *dataOffset = 0;
                            OD_size_t countWritten;
                            OD_IO->write(&OD_IO->stream, dataOD,
                                        ODdataLength, &countWritten);
                            *dataOffset = mappedLength;

                            dataSRDO[plain_inverted] += mappedLength;
                        } /* for (uint8_t i = 0; i < SRDO->mappedObjectsCount; i++) */

                        /* safety check, this should not happen */
                        if (verifyLength[0] != verifyLength[1] || verifyLength[0] > CO_SRDO_MAX_SIZE || verifyLength[0] != SRDO->dataLength) {
                            SRDO->internalState = CO_SRDO_state_error_internal;
                        }
                        else {
                            SRDO->internalState = CO_SRDO_state_communicationEstablished;
                        }
                    } /* if (data_ok) { */

                    CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
                    CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
                } /* inverted message received */
            }

            /* verify timeouts */
            if (SRDO->cycleTimer == 0U) {
                SRDO->internalState = CO_SRDO_state_error_rxTimeoutSCT;
            }
            else if (SRDO->validationTimer == 0U) {
                SRDO->internalState = CO_SRDO_state_error_rxTimeoutSRVT;
            }
            else {
                /* MISRA C 2004 14.10 */
            }
#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_TIMERNEXT
            if (timerNext_us != NULL) {
                if (*timerNext_us > SRDO->cycleTimer) {
                    *timerNext_us = SRDO->cycleTimer; /* Schedule for the next message timer */
                }
                if (*timerNext_us > SRDO->validationTimer) {
                    *timerNext_us = SRDO->validationTimer; /* Schedule for the next message timer */
                }
            }
#endif
        } /* CO_SRDO_RX */
    } /* if (NMTisOperational && ... */
    else {
        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
        if (!SRDO->SRDOGuard->configurationValid) {
            SRDO->internalState = CO_SRDO_state_error_configuration;
        }
        else if (!NMTisOperational) {
            SRDO->internalState = CO_SRDO_state_nmtNotOperational;
        }
        else if (SRDO->informationDirection == CO_SRDO_INVALID) {
            SRDO->internalState = CO_SRDO_state_deleted;
        }
        else {
            /* keep internalState unchanged */
            /* MISRA C 2004 14.10 */
        }
    }

    SRDO->NMTisOperationalPrevious = SRDO->SRDOGuard->NMTisOperational = NMTisOperational;

    return SRDO->internalState;
}

#endif /* (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE */
