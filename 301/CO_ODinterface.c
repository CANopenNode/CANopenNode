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
OD_size_t OD_readOriginal(OD_stream_t *stream, uint8_t subIndex,
                          void *buf, OD_size_t count, ODR_t *returnCode)
{
    (void) subIndex;

    if (stream == NULL || buf == NULL || returnCode == NULL) {
        if (returnCode != NULL) *returnCode = ODR_DEV_INCOMPAT;
        return 0;
    }

    OD_size_t dataLenToCopy = stream->dataLength; /* length of OD variable */
    const char *odData = (const char *)stream->dataObjectOriginal;

    if (odData == NULL) {
        *returnCode = ODR_SUB_NOT_EXIST;
        return 0;
    }

    *returnCode = ODR_OK;

    /* If previous read was partial or OD variable length is larger than
     * current buffer len, then data was (will be) read in several segments */
    if (stream->dataOffset > 0 || dataLenToCopy > count) {
        if (stream->dataOffset >= dataLenToCopy) {
            *returnCode = ODR_DEV_INCOMPAT;
            return 0;
        }
        /* reduce for already copied data */
        dataLenToCopy -= stream->dataOffset;
        odData += stream->dataOffset;

        if (dataLenToCopy > count) {
            /* not enough space in destionation buffer */
            dataLenToCopy = count;
            stream->dataOffset += dataLenToCopy;
            *returnCode = ODR_PARTIAL;
        }
        else {
            stream->dataOffset = 0; /* copy finished, reset offset */
        }
    }

    CO_LOCK_OD();
    memcpy(buf, odData, dataLenToCopy);
    CO_UNLOCK_OD();
    return dataLenToCopy;
}

/******************************************************************************/
OD_size_t OD_writeOriginal(OD_stream_t *stream, uint8_t subIndex,
                           const void *buf, OD_size_t count, ODR_t *returnCode)
{
    (void) subIndex;

    if (stream == NULL || buf == NULL || returnCode == NULL) {
        if (returnCode != NULL) *returnCode = ODR_DEV_INCOMPAT;
        return 0;
    }

    OD_size_t dataLenToCopy = stream->dataLength; /* length of OD variable */
    char *odData = (char *)stream->dataObjectOriginal;

    if (odData == NULL) {
        *returnCode = ODR_SUB_NOT_EXIST;
        return 0;
    }

    *returnCode = ODR_OK;

    /* If previous write was partial or OD variable length is larger than
     * current data len, then data was (will be) written in several segments */
    if (stream->dataOffset > 0 || dataLenToCopy > count) {
        if (stream->dataOffset >= dataLenToCopy) {
            *returnCode = ODR_DEV_INCOMPAT;
            return 0;
        }
        /* reduce for already copied data */
        dataLenToCopy -= stream->dataOffset;
        odData += stream->dataOffset;

        if (dataLenToCopy > count) {
            /* Remaining data space in OD variable is larger than current count
             * of data, so only current count of data will be copied */
            dataLenToCopy = count;
            stream->dataOffset += dataLenToCopy;
            *returnCode = ODR_PARTIAL;
        }
        else {
            stream->dataOffset = 0; /* copy finished, reset offset */
        }
    }

    if (dataLenToCopy < count) {
        /* OD variable is smaller than current amount of data */
        *returnCode = ODR_DATA_LONG;
        return 0;
    }
    else {
        CO_LOCK_OD();
        memcpy(odData, buf, dataLenToCopy);
        CO_UNLOCK_OD();
        return dataLenToCopy;
    }
}

/* Read value from variable from Object Dictionary disabled, see OD_IO_t*/
static OD_size_t OD_readDisabled(OD_stream_t *stream, uint8_t subIndex,
                                 void *buf, OD_size_t count,
                                 ODR_t *returnCode)
{
    (void) stream; (void) subIndex; (void) buf; (void) count;

    if (returnCode != NULL) *returnCode = ODR_UNSUPP_ACCESS;
    return 0;
}

/* Write value to variable from Object Dictionary disabled, see OD_IO_t */
static OD_size_t OD_writeDisabled(OD_stream_t *stream, uint8_t subIndex,
                                  const void *buf, OD_size_t count,
                                  ODR_t *returnCode)
{
    (void) stream; (void) subIndex; (void) buf; (void) count;

    if (returnCode != NULL) *returnCode = ODR_UNSUPP_ACCESS;
    return 0;
}


/******************************************************************************/
const OD_entry_t *OD_find(const OD_t *od, uint16_t index) {
    unsigned int cur;
    unsigned int min = 0;
    unsigned int max = od->size - 1;

    if (od == NULL || od->size == 0) {
        return NULL;
    }

    /* Fast search in ordered Object Dictionary. If indexes are mixed,
     * this won't work. If Object Dictionary has up to 2^N entries, then N is
     * max number of loop passes. */
    while (min < max) {
        /* get entry between min and max */
        cur = (min + max) >> 1;
        const OD_entry_t* entry = &od->list[cur];

        if (index == entry->index) {
            return entry;
        }
        else if (index < entry->index) {
            max = cur;
            if(max > 0) max--;
        }
        else {
            min = cur + 1;
        }
    }

    if (min == max) {
        const OD_entry_t* entry = &od->list[min];
        if (index == entry->index) {
            return entry;
        }
    }

    return NULL;  /* entry does not exist in OD */
}

/******************************************************************************/
ODR_t OD_getSub(const OD_entry_t *entry, uint8_t subIndex,
                OD_subEntry_t *subEntry, OD_IO_t *io, bool_t odOrig)
{
    if (entry == NULL || entry->odObject == NULL) return ODR_IDX_NOT_EXIST;
    else if (io == NULL) return ODR_DEV_INCOMPAT;

    const void *odObjectOrig = entry->odObject;
    const OD_obj_extended_t *odObjectExt = NULL;
    uint8_t odBasicType = entry->odObjectType & ODT_TYPE_MASK;
    OD_attr_t attr = 0;

    /* Is object type extended? */
    if ((entry->odObjectType & ODT_EXTENSION_MASK) != 0) {
        odObjectExt = (const OD_obj_extended_t *)odObjectOrig;
        odObjectOrig = odObjectExt->odObjectOriginal;
        if (odObjectOrig == NULL) return ODR_DEV_INCOMPAT;
    }

    /* attribute, dataObjectOriginal and dataLength, depends on object type */
    if (odBasicType == ODT_VAR) {
        if (subIndex > 0) return ODR_SUB_NOT_EXIST;
        const OD_obj_var_t *odo = (const OD_obj_var_t *)odObjectOrig;

        attr = odo->attribute;
        io->stream.dataObjectOriginal = odo->data;
        io->stream.dataLength = odo->dataLength;
    }
    else if (odBasicType == ODT_ARR) {
        if (subIndex >= entry->subEntriesCount) return ODR_SUB_NOT_EXIST;
        const OD_obj_array_t *odo = (const OD_obj_array_t *)odObjectOrig;

        if (subIndex == 0) {
            attr = odo->attribute0;
            io->stream.dataObjectOriginal = odo->data0;
            io->stream.dataLength = 1;
        }
        else {
            attr = odo->attribute;
            if (odo->data == NULL) {
                io->stream.dataObjectOriginal = NULL;
            }
            else {
                char *data = (char *)odo->data;
                int i = subIndex - 1;
                io->stream.dataObjectOriginal = data + odo->dataElementSizeof * i;
            }
            io->stream.dataLength = odo->dataElementLength;
        }
    }
    else if (odBasicType == ODT_REC) {
        const OD_obj_record_t *odoArr = (const OD_obj_record_t *)odObjectOrig;
        const OD_obj_record_t *odo = NULL;
        for (int i = 0; i< entry->subEntriesCount; i++) {
            if (odoArr[i].subIndex == subIndex) {
                odo = &odoArr[i];
                break;
            }
        }
        if (odo == NULL) return ODR_SUB_NOT_EXIST;

        attr = odo->attribute;
        io->stream.dataObjectOriginal = odo->data;
        io->stream.dataLength = odo->dataLength;
    }
    else {
        return ODR_DEV_INCOMPAT;
    }

    /* read, write and dataObject, direct or with IO extension */
    if (odObjectExt == NULL || odObjectExt->extIO == NULL || odOrig) {
        io->read = OD_readOriginal;
        io->write = OD_writeOriginal;
        io->stream.object = NULL;
    }
    else {
        io->read = odObjectExt->extIO->read != NULL ?
                   odObjectExt->extIO->read : OD_readDisabled;
        io->write = odObjectExt->extIO->write != NULL ?
                    odObjectExt->extIO->write : OD_writeDisabled;
        io->stream.object = odObjectExt->extIO->object;
    }

    /* common properties */
    if (subEntry != NULL) {
        subEntry->index = entry->index;
        subEntry->subIndex = subIndex;
        subEntry->subEntriesCount = entry->subEntriesCount;
        subEntry->attribute = attr;
        subEntry->flagsPDO = odObjectExt != NULL ?
                             odObjectExt->flagsPDO : NULL;
    }
    io->stream.dataOffset = 0;

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
        0x08000021UL, /* Data cannot be transf. because of local control */
        0x08000022UL, /* Data cannot be tran. because of present device state */
        0x08000023UL, /* Object dict. not present or dynamic generation fails */
        0x08000024UL  /* No data available */
    };

    return (returnCode < 0 || returnCode >= ODR_COUNT) ?
        abortCodes[ODR_DEV_INCOMPAT] : abortCodes[returnCode];
}

/******************************************************************************/
ODR_t OD_extensionIO_init(const OD_entry_t *entry,
                          void *object,
                          OD_size_t (*read)(OD_stream_t *stream,
                                            uint8_t subIndex,
                                            void *buf,
                                            OD_size_t count,
                                            ODR_t *returnCode),
                          OD_size_t (*write)(OD_stream_t *stream,
                                             uint8_t subIndex,
                                             const void *buf,
                                             OD_size_t count,
                                             ODR_t *returnCode))
{
    if (entry == NULL) {
        return ODR_IDX_NOT_EXIST;
    }

    const OD_obj_extended_t *odo = (const OD_obj_extended_t *) entry->odObject;

    if ((entry->odObjectType & ODT_EXTENSION_MASK) == 0 || odo->extIO == NULL) {
        return ODR_PAR_INCOMPAT;
    }

    odo->extIO->object = object;
    odo->extIO->read = read;
    odo->extIO->write = write;

    return ODR_OK;
}


/******************************************************************************/
ODR_t OD_get_i8(const OD_entry_t *entry, uint8_t subIndex,
                int8_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

ODR_t OD_get_i16(const OD_entry_t *entry, uint8_t subIndex,
                 int16_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

ODR_t OD_get_i32(const OD_entry_t *entry, uint8_t subIndex,
                 int32_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

ODR_t OD_get_i64(const OD_entry_t *entry, uint8_t subIndex,
                 int64_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

ODR_t OD_get_u8(const OD_entry_t *entry, uint8_t subIndex,
                uint8_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

ODR_t OD_get_u16(const OD_entry_t *entry, uint8_t subIndex,
                 uint16_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

ODR_t OD_get_u32(const OD_entry_t *entry, uint8_t subIndex,
                 uint32_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

ODR_t OD_get_u64(const OD_entry_t *entry, uint8_t subIndex,
                 uint64_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

ODR_t OD_get_r32(const OD_entry_t *entry, uint8_t subIndex,
                 float32_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

ODR_t OD_get_r64(const OD_entry_t *entry, uint8_t subIndex,
                 float64_t *val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(*val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.read(&io.stream, subIndex, val, sizeof(*val), &ret);
    return ret;
}

/******************************************************************************/
ODR_t OD_set_i8(const OD_entry_t *entry, uint8_t subIndex,
                int8_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

ODR_t OD_set_i16(const OD_entry_t *entry, uint8_t subIndex,
                 int16_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

ODR_t OD_set_i32(const OD_entry_t *entry, uint8_t subIndex,
                 int32_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

ODR_t OD_set_i64(const OD_entry_t *entry, uint8_t subIndex,
                 int64_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

ODR_t OD_set_u8(const OD_entry_t *entry, uint8_t subIndex,
                uint8_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

ODR_t OD_set_u16(const OD_entry_t *entry, uint8_t subIndex,
                 uint16_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

ODR_t OD_set_u32(const OD_entry_t *entry, uint8_t subIndex,
                 uint32_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

ODR_t OD_set_u64(const OD_entry_t *entry, uint8_t subIndex,
                 uint64_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

ODR_t OD_set_r32(const OD_entry_t *entry, uint8_t subIndex,
                 float32_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

ODR_t OD_set_r64(const OD_entry_t *entry, uint8_t subIndex,
                 float64_t val, bool_t odOrig)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, odOrig);

    if (ret == ODR_OK && io.stream.dataLength != sizeof(val))
        ret = ODR_TYPE_MISMATCH;
    if (ret == ODR_OK) io.write(&io.stream, subIndex, &val, sizeof(val), &ret);
    return ret;
}

/******************************************************************************/
ODR_t OD_getPtr_i8(const OD_entry_t *entry, uint8_t subIndex, int8_t **val) {
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (int8_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_i16(const OD_entry_t *entry, uint8_t subIndex, int16_t **val) {
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (int16_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_i32(const OD_entry_t *entry, uint8_t subIndex, int32_t **val) {
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (int32_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_i64(const OD_entry_t *entry, uint8_t subIndex, int64_t **val) {
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (int64_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_u8(const OD_entry_t *entry, uint8_t subIndex, uint8_t **val) {
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (uint8_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_u16(const OD_entry_t *entry, uint8_t subIndex, uint16_t **val) {
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (uint16_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_u32(const OD_entry_t *entry, uint8_t subIndex, uint32_t **val) {
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (uint32_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_u64(const OD_entry_t *entry, uint8_t subIndex, uint64_t **val) {
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (uint64_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_r32(const OD_entry_t *entry, uint8_t subIndex, float32_t **val){
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (float32_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_r64(const OD_entry_t *entry, uint8_t subIndex, float64_t **val){
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL)
        ret = ODR_DEV_INCOMPAT;
    else if (ret == ODR_OK && io.stream.dataLength != sizeof(**val))
        ret = ODR_TYPE_MISMATCH;
    else if (ret == ODR_OK)
        *val = (float64_t *)io.stream.dataObjectOriginal;
    return ret;
}

ODR_t OD_getPtr_vs(const OD_entry_t *entry, uint8_t subIndex,
                   char **val, OD_size_t *dataLength)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL
        || io.stream.dataLength == 0)
    {
        ret = ODR_DEV_INCOMPAT;
    }
    else if (ret == ODR_OK) {
        *val = (char *)io.stream.dataObjectOriginal;
        if (dataLength != NULL) *dataLength = io.stream.dataLength;
    }
    return ret;
}

ODR_t OD_getPtr_os(const OD_entry_t *entry, uint8_t subIndex,
                   uint8_t **val, OD_size_t *dataLength)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL
        || io.stream.dataLength == 0)
    {
        ret = ODR_DEV_INCOMPAT;
    }
    else if (ret == ODR_OK) {
        *val = (uint8_t *)io.stream.dataObjectOriginal;
        if (dataLength != NULL) *dataLength = io.stream.dataLength;
    }
    return ret;
}

ODR_t OD_getPtr_us(const OD_entry_t *entry, uint8_t subIndex,
                   uint16_t **val, OD_size_t *dataLength)
{
    OD_IO_t io;
    ODR_t ret = OD_getSub(entry, subIndex, NULL, &io, true);

    if (val == NULL || io.stream.dataObjectOriginal == NULL
        || io.stream.dataLength == 0)
    {
        ret = ODR_DEV_INCOMPAT;
    }
    else if (ret == ODR_OK) {
        *val = (uint16_t *)io.stream.dataObjectOriginal;
        if (dataLength != NULL) *dataLength = io.stream.dataLength;
    }
    return ret;
}
