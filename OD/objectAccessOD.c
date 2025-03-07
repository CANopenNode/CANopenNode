/*
 * Example object oriented access to the Object Dictionary variable.
 *
 * @file        objectAccessOD.c
 * @author      --
 * @copyright   2021 --
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

#include "objectAccessOD.h"

#define SUBIDX_I64 0x01
#define SUBIDX_U64 0x02
#define SUBIDX_R32 0x03
#define SUBIDX_R64 0x04
#define SUBIDX_AVERAGE 0x05
#define SUBIDX_PARAMETER 0x06

/*
 * Custom function for reading OD object _demoRecord_
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_read_demoRecord(OD_stream_t* stream, void* buf, OD_size_t count, OD_size_t* countRead) {
    if (stream == NULL || buf == NULL || countRead == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    /* Object was passed by OD_extensionIO_init, use correct type. */
    objectAccessOD_t* thisObj = (objectAccessOD_t*)stream->object;

    switch (stream->subIndex) {
        case SUBIDX_AVERAGE: {
            OD_size_t varSize = sizeof(float64_t);

            if (count < varSize || stream->dataLength != varSize) {
                return ODR_DEV_INCOMPAT;
            }

            float64_t average = (float64_t)*thisObj->i64;
            average += (float64_t)*thisObj->u64;
            average += (float64_t)*thisObj->r32;
            average += *thisObj->r64;
            average /= 4;

            memcpy(buf, &average, varSize);

            *countRead = varSize;
            return ODR_OK;
        }

        case SUBIDX_PARAMETER: {
            uint16_t paramU16 = (uint16_t)(thisObj->internalParameter / 1000);
            OD_size_t varSize = sizeof(paramU16);

            if (count < varSize || stream->dataLength != varSize) {
                return ODR_DEV_INCOMPAT;
            }

            CO_setUint16(buf, paramU16);

            *countRead = varSize;
            return ODR_OK;
        }

        default: {
            return OD_readOriginal(stream, buf, count, countRead);
        }
    }
}

/*
 * Custom function for reading OD object _demoRecord_
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_demoRecord(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten) {
    if (stream == NULL || buf == NULL || countWritten == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    /* Object was passed by OD_extensionIO_init, use correct type. */
    objectAccessOD_t* thisObj = (objectAccessOD_t*)stream->object;

    switch (stream->subIndex) {
        case SUBIDX_PARAMETER: {
            uint16_t paramU16 = CO_getUint16(buf);
            thisObj->internalParameter = (uint32_t)paramU16 * 1000;

            /* write value to the original location in the Object Dictionary */
            return OD_writeOriginal(stream, buf, count, countWritten);
        }

        default: {
            return OD_writeOriginal(stream, buf, count, countWritten);
        }
    }
}

/******************************************************************************/
CO_ReturnError_t objectAccessOD_init(objectAccessOD_t* thisObj, OD_entry_t* OD_demoRecord, uint32_t* errInfo) {
    if (thisObj == NULL || errInfo == NULL || OD_demoRecord == NULL)
        return CO_ERROR_ILLEGAL_ARGUMENT;

    CO_ReturnError_t err = CO_ERROR_NO;
    ODR_t odRet;

    /* initialize object variables */
    memset(thisObj, 0, sizeof(objectAccessOD_t));

    /* Initialize custom OD object "demoRecord" */
    thisObj->OD_demoRecord_extension.object = thisObj;
    thisObj->OD_demoRecord_extension.read = OD_read_demoRecord;
    thisObj->OD_demoRecord_extension.write = OD_write_demoRecord;
    odRet = OD_extension_init(OD_demoRecord, &thisObj->OD_demoRecord_extension);

    /* This is strict behavior and will exit the program on error. Error
     * checking on all OD functions can also be omitted. In that case program
     * will run, but specific OD entry may not be accessible. */
    if (odRet != ODR_OK) {
        *errInfo = OD_getIndex(OD_demoRecord);
        return CO_ERROR_OD_PARAMETERS;
    }

    /* Get variables from Object dictionary, related to "Average" */
    thisObj->i64 = OD_getPtr(OD_demoRecord, SUBIDX_I64, sizeof(int64_t), NULL);
    thisObj->u64 = OD_getPtr(OD_demoRecord, SUBIDX_U64, sizeof(uint64_t), NULL);
    thisObj->r32 = OD_getPtr(OD_demoRecord, SUBIDX_R32, sizeof(float32_t), NULL);
    thisObj->r64 = OD_getPtr(OD_demoRecord, SUBIDX_R64, sizeof(float64_t), NULL);

    if (thisObj->i64 == NULL || thisObj->u64 == NULL || thisObj->r32 == NULL || thisObj->r64 == NULL) {
        *errInfo = OD_getIndex(OD_demoRecord);
        return CO_ERROR_OD_PARAMETERS;
    }

    /* Sub entry SUBIDX_AVERAGE will be read by application via
     * OD_read_demoRecord() function. Initialize structure "io_average" here. */
    odRet = OD_getSub(OD_demoRecord, SUBIDX_AVERAGE, &thisObj->io_average, false);
    if (odRet != ODR_OK) {
        *errInfo = OD_getIndex(OD_demoRecord);
        return CO_ERROR_OD_PARAMETERS;
    }

    /* Get variable 'Parameter with default value' from Object dictionary */
    uint16_t parameterU16;
    odRet = OD_get_u16(OD_demoRecord, SUBIDX_PARAMETER, &parameterU16, true);

    if (odRet != ODR_OK) {
        *errInfo = OD_getIndex(OD_demoRecord);
        return CO_ERROR_OD_PARAMETERS;
    }
    thisObj->internalParameter = (uint32_t)parameterU16 * 1000;

    return err;
}
