/*
 * CANopen data storage base object
 *
 * @file        CO_storage.c
 * @author      Janez Paternoster
 * @copyright   2021 Janez Paternoster
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

#include "storage/CO_storage.h"

#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE

/*
 * Custom function for writing OD object "Store parameters"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_1010(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    /* verify arguments */
    if (stream == NULL || stream->subIndex == 0 || buf == NULL || count != 4
        || countWritten == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_storage_t *storage = stream->object;

    if (stream->subIndex == 0 || storage->store == NULL || !storage->enabled) {
        return ODR_READONLY;
    }

    if (CO_getUint32(buf) != 0x65766173) {
        return ODR_DATA_TRANSF;
    }

    /* loop through entries and store relevant */
    uint8_t found = 0;
    ODR_t returnCode = ODR_OK;

    for (uint8_t i = 0; i < storage->entriesCount; i++) {
        CO_storage_entry_t *entry = &storage->entries[i];

        if (stream->subIndex == 1 || entry->subIndexOD == stream->subIndex) {
            if (found == 0) found = 1;
            if ((entry->attr & CO_storage_cmd) != 0) {
                ODR_t code = storage->store(entry, storage->CANmodule);
                if (code != ODR_OK) returnCode = code;
                found = 2;
            }
        }
    }

    if (found != 2)
        returnCode = found == 0 ? ODR_SUB_NOT_EXIST : ODR_READONLY;

    if (returnCode == ODR_OK) *countWritten = sizeof(uint32_t);
    return returnCode;
}


/*
 * Custom function for writing OD object "Restore default parameters"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_1011(OD_stream_t *stream, const void *buf,
                           OD_size_t count, OD_size_t *countWritten)
{
    /* verify arguments */
    if (stream == NULL || stream->subIndex == 0 || buf == NULL || count != 4
        || countWritten == NULL
    ) {
        return ODR_DEV_INCOMPAT;
    }

    CO_storage_t *storage = stream->object;

    if (stream->subIndex == 0 || storage->restore == NULL || !storage->enabled){
        return ODR_READONLY;
    }

    if (CO_getUint32(buf) != 0x64616F6C) {
        return ODR_DATA_TRANSF;
    }

    /* loop through entries and store relevant */
    uint8_t found = 0;
    ODR_t returnCode = ODR_OK;

    for (uint8_t i = 0; i < storage->entriesCount; i++) {
        CO_storage_entry_t *entry = &storage->entries[i];

        if (stream->subIndex == 1 || entry->subIndexOD == stream->subIndex) {
            if (found == 0) found = 1;
            if ((entry->attr & CO_storage_restore) != 0) {
                ODR_t code = storage->restore(entry, storage->CANmodule);
                if (code != ODR_OK) returnCode = code;
                found = 2;
            }
        }
    }

    if (found != 2)
        returnCode = found == 0 ? ODR_SUB_NOT_EXIST : ODR_READONLY;

    if (returnCode == ODR_OK) *countWritten = sizeof(uint32_t);
    return returnCode;
}


CO_ReturnError_t CO_storage_init(CO_storage_t *storage,
                                 CO_CANmodule_t *CANmodule,
                                 OD_entry_t *OD_1010_StoreParameters,
                                 OD_entry_t *OD_1011_RestoreDefaultParameters,
                                 ODR_t (*store)(CO_storage_entry_t *entry,
                                                CO_CANmodule_t *CANmodule),
                                 ODR_t (*restore)(CO_storage_entry_t *entry,
                                                  CO_CANmodule_t *CANmodule),
                                 CO_storage_entry_t *entries,
                                 uint8_t entriesCount)
{
    /* verify arguments */
    if (storage == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    storage->CANmodule = CANmodule;
    storage->store = store;
    storage->restore = restore;
    storage->entries = entries;
    storage->entriesCount = entriesCount;

    /* configure extensions */
    if (OD_1010_StoreParameters != NULL) {
        storage->OD_1010_extension.object = storage;
        storage->OD_1010_extension.read = OD_readOriginal;
        storage->OD_1010_extension.write = OD_write_1010;
        OD_extension_init(OD_1010_StoreParameters, &storage->OD_1010_extension);
    }

    if (OD_1011_RestoreDefaultParameters != NULL) {
        storage->OD_1011_extension.object = storage;
        storage->OD_1011_extension.read = OD_readOriginal;
        storage->OD_1011_extension.write = OD_write_1011;
        OD_extension_init(OD_1011_RestoreDefaultParameters,
                          &storage->OD_1011_extension);
    }

    return CO_ERROR_NO;
}

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */
