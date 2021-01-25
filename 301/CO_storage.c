/*
 * CANopen data storage object
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

#include "301/CO_storage.h"

#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE

/*
 * Custom function for writing OD object "Store parameters"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static OD_size_t OD_write_1010(OD_stream_t *stream, uint8_t subIndex,
                               const void *buf, OD_size_t count,
                               ODR_t *returnCode)
{
    /* verify arguments */
    if (stream == NULL || subIndex == 0 || buf == NULL || count != 4
        || returnCode == NULL
    ) {
        if (returnCode != NULL) *returnCode = ODR_DEV_INCOMPAT;
        return 0;
    }

    if (subIndex == 0) {
        *returnCode = ODR_READONLY;
        return 0;
    }

    if (CO_getUint32(buf) != 0x65766173) {
        *returnCode = ODR_DATA_TRANSF;
        return 0;
    }

    CO_storage_t *storage = stream->object;
    CO_storage_entry_t *entry = storage->firstEntry;
    bool_t found = false;
    *returnCode = ODR_OK;

    /* call pre-configured function matching subIndex or call all functions */
    while (entry != NULL) {
        if (entry->store != NULL
            && (entry->subIndexOD == subIndex
                || (storage->sub1_all && subIndex == 1))
        ) {
            ODR_t code = entry->store(entry->object, entry->addr, entry->len);
            found = true;

            if (code != ODR_OK) *returnCode = code;
            if (!(storage->sub1_all && subIndex == 1)) {
                break;
            }
        }
        entry = entry->nextEntry;
    }

    if (!found) *returnCode = ODR_SUB_NOT_EXIST;

    return *returnCode == ODR_OK ? 4 : 0;
}


/*
 * Custom function for writing OD object "Restore default parameters"
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static OD_size_t OD_write_1011(OD_stream_t *stream, uint8_t subIndex,
                               const void *buf, OD_size_t count,
                               ODR_t *returnCode)
{
    /* verify arguments */
    if (stream == NULL || subIndex == 0 || buf == NULL || count != 4
        || returnCode == NULL
    ) {
        if (returnCode != NULL) *returnCode = ODR_DEV_INCOMPAT;
        return 0;
    }

    if (subIndex == 0) {
        *returnCode = ODR_READONLY;
        return 0;
    }

    if (CO_getUint32(buf) != 0x64616F6C) {
        *returnCode = ODR_DATA_TRANSF;
        return 0;
    }

    CO_storage_t *storage = stream->object;
    CO_storage_entry_t *entry = storage->firstEntry;
    bool_t found = false;
    *returnCode = ODR_OK;

    /* call pre-configured function matching subIndex or call all functions */
    while (entry != NULL) {
        if (entry->restore != NULL
            && (entry->subIndexOD == subIndex
                || (storage->sub1_all && subIndex == 1))
        ) {
            ODR_t code = entry->restore(entry->object, entry->addr, entry->len);
            found = true;

            if (code != ODR_OK) *returnCode = code;
            if (!(storage->sub1_all && subIndex == 1)) {
                break;
            }
        }
        entry = entry->nextEntry;
    }

    if (!found) *returnCode = ODR_SUB_NOT_EXIST;

    return *returnCode == ODR_OK ? 4 : 0;
}


CO_ReturnError_t CO_storage_init(CO_storage_t *storage,
                                 OD_entry_t *OD_1010_StoreParameters,
                                 OD_entry_t *OD_1011_RestoreDefaultParameters,
                                 bool_t sub1_all,
                                 uint32_t *errInfo)
{
    ODR_t odRet;

    /* verify arguments */
    if (storage == NULL || OD_1010_StoreParameters == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* object was pre-initialised by CO_storage_pre_init() */

    storage->sub1_all = sub1_all;

    /* configure extensions */
    storage->OD_1010_extension.object = storage;
    storage->OD_1010_extension.read = OD_readOriginal;
    storage->OD_1010_extension.write = OD_write_1010;
    odRet = OD_extension_init(OD_1010_StoreParameters,
                              &storage->OD_1010_extension);
    if (odRet != ODR_OK) {
        if (errInfo != NULL)
            *errInfo = OD_getIndex(OD_1010_StoreParameters);
        return CO_ERROR_OD_PARAMETERS;
    }

    if (OD_1011_RestoreDefaultParameters != NULL) {
        storage->OD_1011_extension.object = storage;
        storage->OD_1011_extension.read = OD_readOriginal;
        storage->OD_1011_extension.write = OD_write_1011;
        odRet = OD_extension_init(OD_1011_RestoreDefaultParameters,
                                  &storage->OD_1011_extension);
        if (odRet != ODR_OK) {
            if (errInfo != NULL)
                *errInfo = OD_getIndex(OD_1011_RestoreDefaultParameters);
            return CO_ERROR_OD_PARAMETERS;
        }
    }

    return CO_ERROR_NO;
}


CO_ReturnError_t CO_storage_entry_init(CO_storage_t *storage,
                                       CO_storage_entry_t *newEntry) {
    /* verify arguments */
    if (storage == NULL || newEntry == NULL || newEntry->addr == NULL
        || newEntry->len == 0 || newEntry->subIndexOD == 0
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CO_storage_entry_t **entry = &storage->firstEntry;

    /* Add newEntry on the end of linked list or replace existing entry */
    for ( ; ; ) {
        if (*entry == NULL) {
            newEntry->nextEntry = NULL;
            *entry = newEntry;
            break;
        }
        if ((*entry)->subIndexOD == newEntry->subIndexOD) {
            newEntry->nextEntry = (*entry)->nextEntry;
            *entry = newEntry;
            break;
        }
        *entry = (*entry)->nextEntry;
    };

    return CO_ERROR_NO;
}

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */
