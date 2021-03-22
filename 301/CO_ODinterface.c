/*
 * CANopen Object Dictionary interface
 *
 * @file        CO_ODinterface.c
 * @author      Janez Paternoster
 * @copyright   2020 Janez Paternoster
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

#include <string.h>
#define OD_DEFINITION
#include "301/CO_ODinterface.h"


/******************************************************************************/
ODR_t OD_readOriginal(OD_stream_t *stream, void *buf,
                      OD_size_t count, OD_size_t *countRead)
{
    if (stream == NULL || buf == NULL || countRead == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    OD_size_t dataLenToCopy = stream->dataLength; /* length of OD variable */
    const uint8_t *dataOrig = stream->dataOrig;

    if (dataOrig == NULL) {
        return ODR_SUB_NOT_EXIST;
    }

    ODR_t returnCode = ODR_OK;

    /* If previous read was partial or OD variable length is larger than
     * current buffer size, then data was (will be) read in several segments */
    if (stream->dataOffset > 0 || dataLenToCopy > count) {
        if (stream->dataOffset >= dataLenToCopy) {
            return ODR_DEV_INCOMPAT;
        }
        /* Reduce for already copied data */
        dataLenToCopy -= stream->dataOffset;
        dataOrig += stream->dataOffset;

        if (dataLenToCopy > count) {
            /* Not enough space in destination buffer */
            dataLenToCopy = count;
            stream->dataOffset += dataLenToCopy;
            returnCode = ODR_PARTIAL;
        }
        else {
            stream->dataOffset = 0; /* copy finished, reset offset */
        }
    }

    memcpy(buf, dataOrig, dataLenToCopy);

    *countRead = dataLenToCopy;
    return returnCode;
}

/******************************************************************************/
ODR_t OD_writeOriginal(OD_stream_t *stream, const void *buf,
                       OD_size_t count, OD_size_t *countWritten)
{
    if (stream == NULL || buf == NULL || countWritten == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    OD_size_t dataLenToCopy = stream->dataLength; /* length of OD variable */
    uint8_t *dataOrig = stream->dataOrig;

    if (dataOrig == NULL) {
        return ODR_SUB_NOT_EXIST;
    }

    ODR_t returnCode = ODR_OK;

    /* If previous write was partial or OD variable length is larger than
     * current buffer size, then data was (will be) written in several
     * segments */
    if (stream->dataOffset > 0 || dataLenToCopy > count) {
        if (stream->dataOffset >= dataLenToCopy) {
            return ODR_DEV_INCOMPAT;
        }
        /* reduce for already copied data */
        dataLenToCopy -= stream->dataOffset;
        dataOrig += stream->dataOffset;

        if (dataLenToCopy > count) {
            /* Remaining data space in OD variable is larger than current count
             * of data, so only current count of data will be copied */
            dataLenToCopy = count;
            stream->dataOffset += dataLenToCopy;
            returnCode = ODR_PARTIAL;
        }
        else {
            stream->dataOffset = 0; /* copy finished, reset offset */
        }
    }

    if (dataLenToCopy < count) {
        /* OD variable is smaller than current amount of data */
        return ODR_DATA_LONG;
    }

    memcpy(dataOrig, buf, dataLenToCopy);

    *countWritten = dataLenToCopy;
    return returnCode;
}

/* Read value from variable from Object Dictionary disabled, see OD_IO_t*/
static ODR_t OD_readDisabled(OD_stream_t *stream, void *buf,
                             OD_size_t count, OD_size_t *countRead)
{
    (void) stream; (void) buf; (void) count; (void) countRead;
    return ODR_UNSUPP_ACCESS;
}

/* Write value to variable from Object Dictionary disabled, see OD_IO_t */
static ODR_t OD_writeDisabled(OD_stream_t *stream, const void *buf,
                              OD_size_t count, OD_size_t *countWritten)
{
    (void) stream; (void) buf; (void) count; (void) countWritten;
    return ODR_UNSUPP_ACCESS;
}


/******************************************************************************/
OD_entry_t *OD_find(OD_t *od, uint16_t index) {
    if (od == NULL || od->size == 0) {
        return NULL;
    }

    uint16_t min = 0;
    uint16_t max = od->size - 1;

    /* Fast search in ordered Object Dictionary. If indexes are mixed,
     * this won't work. If Object Dictionary has up to N entries, then the
     * max number of loop passes is log2(N) */
    while (min < max) {
        /* get entry between min and max */
        uint16_t cur = (min + max) >> 1;
        OD_entry_t* entry = &od->list[cur];

        if (index == entry->index) {
            return entry;
        }

        if (index < entry->index) {
            max = (cur > 0) ? (cur - 1) : cur;
        }
        else {
            min = cur + 1;
        }
    }

    if (min == max) {
        OD_entry_t* entry = &od->list[min];
        if (index == entry->index) {
            return entry;
        }
    }

    return NULL;  /* entry does not exist in OD */
}

/******************************************************************************/
ODR_t OD_getSub(const OD_entry_t *entry, uint8_t subIndex,
                OD_IO_t *io, bool_t odOrig)
{
    if (entry == NULL || entry->odObject == NULL) return ODR_IDX_NOT_EXIST;
    if (io == NULL) return ODR_DEV_INCOMPAT;

    OD_stream_t *stream = &io->stream;

    /* attribute, dataOrig and dataLength, depends on object type */
    switch (entry->odObjectType & ODT_TYPE_MASK) {
    case ODT_VAR: {
        if (subIndex > 0) return ODR_SUB_NOT_EXIST;
        CO_PROGMEM OD_obj_var_t *odo = entry->odObject;


        stream->attribute = odo->attribute;
        stream->dataOrig = odo->dataOrig;
        stream->dataLength = odo->dataLength;
        break;
    }
    case ODT_ARR: {
        if (subIndex >= entry->subEntriesCount) return ODR_SUB_NOT_EXIST;
        CO_PROGMEM OD_obj_array_t *odo = entry->odObject;

        if (subIndex == 0) {
            stream->attribute = odo->attribute0;
            stream->dataOrig = odo->dataOrig0;
            stream->dataLength = 1;
        }
        else {
            stream->attribute = odo->attribute;
            uint8_t *ptr = odo->dataOrig;
            stream->dataOrig = ptr == NULL ? ptr
                             : ptr + odo->dataElementSizeof * (subIndex - 1);
            stream->dataLength = odo->dataElementLength;
        }
        break;
    }
    case ODT_REC: {
        CO_PROGMEM OD_obj_record_t *odoArr = entry->odObject;
        CO_PROGMEM OD_obj_record_t *odo = NULL;
        for (uint8_t i = 0; i < entry->subEntriesCount; i++) {
            if (odoArr[i].subIndex == subIndex) {
                odo = &odoArr[i];
                break;
            }
        }
        if (odo == NULL) return ODR_SUB_NOT_EXIST;

        stream->attribute = odo->attribute;
        stream->dataOrig = odo->dataOrig;
        stream->dataLength = odo->dataLength;
        break;
    }
    default: {
        return ODR_DEV_INCOMPAT;
    }
    }

    /* Access data from the original OD location */
    if (entry->extension == NULL || odOrig) {
        io->read = OD_readOriginal;
        io->write = OD_writeOriginal;
        stream->object = NULL;
    }
    /* Access data from extension specified by application */
    else {
        io->read = entry->extension->read != NULL ?
                   entry->extension->read : OD_readDisabled;
        io->write = entry->extension->write != NULL ?
                    entry->extension->write : OD_writeDisabled;
        stream->object = entry->extension->object;
    }

    /* Reset stream data offset */
    stream->dataOffset = 0;
    stream->subIndex = subIndex;

    return ODR_OK;
}

/******************************************************************************/
uint32_t OD_getSDOabCode(ODR_t returnCode) {
    static const uint32_t abortCodes[ODR_COUNT] = {
        0x00000000UL, /* No abort */
        0x05040005UL, /* Out of memory */
        0x06010000UL, /* Unsupported access to an object */
        0x06010001UL, /* Attempt to read a write only object */
        0x06010002UL, /* Attempt to write a read only object */
        0x06020000UL, /* Object does not exist in the object dictionary */
        0x06040041UL, /* Object cannot be mapped to the PDO */
        0x06040042UL, /* Num and len of object to be mapped exceeds PDO len */
        0x06040043UL, /* General parameter incompatibility reasons */
        0x06040047UL, /* General internal incompatibility in device */
        0x06060000UL, /* Access failed due to hardware error */
        0x06070010UL, /* Data type does not match, length does not match */
        0x06070012UL, /* Data type does not match, length too high */
        0x06070013UL, /* Data type does not match, length too short */
        0x06090011UL, /* Sub index does not exist */
        0x06090030UL, /* Invalid value for parameter (download only). */
        0x06090031UL, /* Value range of parameter written too high */
        0x06090032UL, /* Value range of parameter written too low */
        0x06090036UL, /* Maximum value is less than minimum value. */
        0x060A0023UL, /* Resource not available: SDO connection */
        0x08000000UL, /* General error */
        0x08000020UL, /* Data cannot be transferred or stored to application */
        0x08000021UL, /* Data cannot be transferred because of local control */
        0x08000022UL, /* Data cannot be tran. because of present device state */
        0x08000023UL, /* Object dict. not present or dynamic generation fails */
        0x08000024UL  /* No data available */
    };

    return (returnCode < 0 || returnCode >= ODR_COUNT) ?
        abortCodes[ODR_DEV_INCOMPAT] : abortCodes[returnCode];
}


/******************************************************************************/
ODR_t OD_get_value(const OD_entry_t *entry, uint8_t subIndex,
                   void *val, OD_size_t len, bool_t odOrig)
{
    if (val == NULL) return ODR_DEV_INCOMPAT;

    OD_IO_t io;
    OD_stream_t *stream = (OD_stream_t *)&io;
    OD_size_t countRd = 0;

    ODR_t ret = OD_getSub(entry, subIndex, &io, odOrig);

    if (ret != ODR_OK) return ret;
    if (stream->dataLength != len) return ODR_TYPE_MISMATCH;

    return io.read(stream, val, len, &countRd);
}

ODR_t OD_set_value(const OD_entry_t *entry, uint8_t subIndex, void *val,
                   OD_size_t len, bool_t odOrig)
{
    OD_IO_t io;
    OD_stream_t *stream = &io.stream;
    OD_size_t countWritten = 0;

    ODR_t ret = OD_getSub(entry, subIndex, &io, odOrig);

    if (ret != ODR_OK) return ret;
    if (stream->dataLength != len) return ODR_TYPE_MISMATCH;

    return io.write(stream, val, len, &countWritten);
}

void *OD_getPtr(const OD_entry_t *entry, uint8_t subIndex, OD_size_t len,
                ODR_t *err)
{
    ODR_t errCopy;
    OD_IO_t io;
    OD_stream_t *stream = &io.stream;

    errCopy = OD_getSub(entry, subIndex, &io, true);

    if (errCopy == ODR_OK) {
        if (stream->dataOrig == NULL || stream->dataLength == 0) {
            errCopy = ODR_DEV_INCOMPAT;
        }
        else if (len != 0 && len != stream->dataLength) {
            errCopy = ODR_TYPE_MISMATCH;
        }
    }

    if (err != NULL) *err = errCopy;

    return errCopy == ODR_OK ? stream->dataOrig : NULL;
}
