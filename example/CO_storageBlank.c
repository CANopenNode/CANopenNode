/*
 * CANopen Object Dictionary storage object (blank example).
 *
 * @file        CO_storageBlank.c
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

#include "CO_storageBlank.h"


#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE


/*
 * Function for writing data on "Store parameters" command - OD object 1010
 *
 * For more information see file CO_storage.h, CO_storage_entry_t.
 */
static ODR_t storeBlank(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule) {

    /* Open a file and write data to it */
    /* file = open(entry->pathToFileOrPointerToMemory); */
    CO_LOCK_OD(CANmodule);
    /* write(entry->addr, entry->len, file); */
    CO_UNLOCK_OD(CANmodule);

    return ODR_OK;
}


/*
 * Function for restoring data on "Restore default parameters" command - OD 1011
 *
 * For more information see file CO_storage.h, CO_storage_entry_t.
 */
static ODR_t restoreBlank(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule){

    /* disable (delete) the file, so default values will stay after startup */

    return ODR_OK;
}


CO_ReturnError_t CO_storageBlank_init(CO_storage_t *storage,
                                      CO_CANmodule_t *CANmodule,
                                      OD_entry_t *OD_1010_StoreParameters,
                                      OD_entry_t *OD_1011_RestoreDefaultParam,
                                      CO_storage_entry_t *entries,
                                      uint8_t entriesCount,
                                      uint32_t *storageInitError)
{
    CO_ReturnError_t ret;

    /* verify arguments */
    if (storage == NULL || entries == NULL || entriesCount == 0
        || storageInitError == NULL
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* initialize storage and OD extensions */
    ret = CO_storage_init(storage,
                          CANmodule,
                          OD_1010_StoreParameters,
                          OD_1011_RestoreDefaultParam,
                          storeBlank,
                          restoreBlank,
                          entries,
                          entriesCount);
    if (ret != CO_ERROR_NO) {
        return ret;
    }

    /* initialize entries */
    *storageInitError = 0;
    for (uint8_t i = 0; i < entriesCount; i++) {
        CO_storage_entry_t *entry = &entries[i];

        /* verify arguments */
        if (entry->addr == NULL || entry->len == 0 || entry->subIndexOD < 2) {
            *storageInitError = i;
            return CO_ERROR_ILLEGAL_ARGUMENT;
        }

        /* Open a file and read data from file to entry->addr */
        /* file = open(entry->pathToFileOrPointerToMemory); */
        /* read(entry->addr, entry->len, file); */

    }

    return ret;
}

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */
