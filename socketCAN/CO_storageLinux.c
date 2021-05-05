/*
 * CANopen data storage object for Linux
 *
 * @file        CO_storageLinux.c
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

#include "CO_storageLinux.h"
#include "301/crc16-ccitt.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE


/*
 * Function for writing data on "Store parameters" command - OD object 1010
 *
 * For more information see file CO_storage.h, CO_storage_entry_t.
 */
static ODR_t storeLinux(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule) {
    ODR_t ret = ODR_OK;
    uint16_t crc_store;

    /* Create names for temporary and old file */
    size_t fn_len = strlen(entry->filename) + 5;
    char *filename_tmp = malloc(fn_len);
    char *filename_old = malloc(fn_len);
    if (filename_tmp == NULL || filename_old == NULL) {
        if (filename_tmp != NULL) free(filename_tmp);
        if (filename_old != NULL) free(filename_old);
        ret = ODR_OUT_OF_MEM;
    }
    else {
        strcpy(filename_tmp, entry->filename);
        strcpy(filename_old, entry->filename);
        strcat(filename_tmp, ".tmp");
        strcat(filename_old, ".old");
    }

    /* Open a temporary file and write data to it */
    if (ret == ODR_OK) {
        FILE *fp = fopen(filename_tmp, "w");
        if (fp == NULL) {
            ret = ODR_HW;
        }
        else {
            CO_LOCK_OD(CANmodule);
            size_t cnt = fwrite(entry->addr, 1, entry->len, fp);
            crc_store = crc16_ccitt(entry->addr, entry->len, 0);
            CO_UNLOCK_OD(CANmodule);
            cnt += fwrite(&crc_store, 1, sizeof(crc_store), fp);
            fclose(fp);
            if (cnt != (entry->len + sizeof(crc_store))) {
                ret = ODR_HW;
            }
        }
    }

    /* Verify data */
    if (ret == ODR_OK) {
        uint8_t *buf = NULL;
        FILE *fp = NULL;
        size_t cnt = 0;
        uint16_t crc_verify, crc_read;

        buf = malloc(entry->len + 4);
        if (buf != NULL) {
            fp = fopen(filename_tmp, "r");
            if (fp != NULL) {
                cnt = fread(buf, 1, entry->len + 4, fp);
                crc_verify = crc16_ccitt(buf, entry->len, 0);
                fclose(fp);
                memcpy(&crc_read, &buf[entry->len], sizeof(crc_read));
            }
            free(buf);
        }
        /* If size or CRC differs, report error */
        if (buf == NULL || fp == NULL || cnt != (entry->len+sizeof(crc_verify))
            || crc_store != crc_verify || crc_store != crc_read
        ) {
            ret = ODR_HW;
        }
    }

    /* rename existing file to *.old and *.tmp to existing */
    if (ret == ODR_OK) {
        rename(entry->filename, filename_old);
        if (rename(filename_tmp, entry->filename) != 0) {
            ret = ODR_HW;
        }
    }

    free(filename_tmp);
    free(filename_old);

    return ret;
}


/*
 * Function for restoring data on "Restore default parameters" command - OD 1011
 *
 * For more information see file CO_storage.h, CO_storage_entry_t.
 */
static ODR_t restoreLinux(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule){
    (void) CANmodule;
    ODR_t ret = ODR_OK;

    /* close the file first, if auto storage */
    if ((entry->attr & CO_storage_auto) != 0 && entry->fp != NULL) {
        fclose(entry->fp);
        entry->fp = NULL;
    }

    /* Rename existing filename to *.old. */
    char *filename_old = malloc(strlen(entry->filename) + 5);
    if (filename_old == NULL) {
        ret = ODR_OUT_OF_MEM;
    }
    else {
        strcpy(filename_old, entry->filename);
        strcat(filename_old, ".old");
        rename(entry->filename, filename_old);
        free(filename_old);
    }

    /* create an empty file and write "-\n" to it. */
    if (ret == ODR_OK) {
        FILE *fp = fopen(entry->filename, "w");
        if (fp == NULL) {
            ret = ODR_HW;
        }
        else {
            fputs("-\n", fp);
            fclose(fp);
        }
    }

    return ret;
}


CO_ReturnError_t CO_storageLinux_init(CO_storage_t *storage,
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

    storage->enabled = false;

    /* initialize storage and OD extensions */
    ret = CO_storage_init(storage,
                          CANmodule,
                          OD_1010_StoreParameters,
                          OD_1011_RestoreDefaultParam,
                          storeLinux,
                          restoreLinux,
                          entries,
                          entriesCount);
    if (ret != CO_ERROR_NO) {
        return ret;
    }

    /* initialize entries */
    *storageInitError = 0;
    for (uint8_t i = 0; i < entriesCount; i++) {
        CO_storage_entry_t *entry = &entries[i];
        bool_t dataCorrupt = false;
        char *writeFileAccess = "w";

        /* verify arguments */
        if (entry->addr == NULL || entry->len == 0 || entry->subIndexOD < 2
            || strlen(entry->filename) == 0
        ) {
            *storageInitError = i;
            return CO_ERROR_ILLEGAL_ARGUMENT;
        }

        /* Open file, check existence and create temporary buffer */
        uint8_t *buf = NULL;
        FILE * fp = fopen(entry->filename, "r");
        if (fp == NULL) {
            dataCorrupt = true;
            ret = CO_ERROR_DATA_CORRUPT;
        }
        else {
            buf = malloc(entry->len + 4);
            if (buf == NULL) {
                fclose(fp);
                *storageInitError = i;
                return CO_ERROR_OUT_OF_MEMORY;
            }
        }

        /* Read data into temporary buffer first. Then verify and copy to addr*/
        if (!dataCorrupt) {
            size_t cnt = fread(buf, 1, entry->len + 4, fp);

            /* If file is empty, just skip loading, default values will be used,
             * no error. Otherwise verify length and crc and copy data. */
            if (!(cnt == 2 && buf[0] == '-')) {
                uint16_t crc1, crc2;
                crc1 = crc16_ccitt(buf, entry->len, 0);
                memcpy(&crc2, &buf[entry->len], sizeof(crc2));

                if (crc1 == crc2 && cnt == (entry->len + sizeof(crc2))) {
                    memcpy(entry->addr, buf, entry->len);
                    entry->crc = crc1;
                    writeFileAccess = "r+";
                }
                else {
                    dataCorrupt = true;
                    ret = CO_ERROR_DATA_CORRUPT;
                }
            }

            free(buf);
            fclose(fp);
        }

        /* additional info in case of error */
        if (dataCorrupt) {
            uint32_t errorBit = entry->subIndexOD;
            if (errorBit > 31) errorBit = 31;
            *storageInitError |= ((uint32_t) 1) << errorBit;
        }

        /* open file for auto storage, if set so */
        if ((entry->attr & CO_storage_auto) != 0) {
            entry->fp = fopen(entry->filename, writeFileAccess);
            if (entry->fp == NULL) {
                *storageInitError = i;
                return CO_ERROR_ILLEGAL_ARGUMENT;
            }
        }
    } /* for (entries) */

    storage->enabled = true;
    return ret;
}


uint32_t CO_storageLinux_auto_process(CO_storage_t *storage,
                                      bool_t closeFiles)
{
    uint32_t storageError = 0;

    /* verify arguments */
    if (storage == NULL) {
        return false;
    }

    /* loop through entries */
    for (uint8_t i = 0; i < storage->entriesCount; i++) {
        CO_storage_entry_t *entry = &storage->entries[i];

        if ((entry->attr & CO_storage_auto) == 0 || entry->fp == NULL)
            continue;

        /* If CRC of the current data differs, save the file */
        uint16_t crc = crc16_ccitt(entry->addr, entry->len, 0);
        if (crc != entry->crc) {
            size_t cnt;
            rewind(entry->fp);
            CO_LOCK_OD(storage->CANmodule);
            cnt = fwrite(entry->addr, 1, entry->len, entry->fp);
            CO_UNLOCK_OD(storage->CANmodule);
            cnt += fwrite(&crc, 1, sizeof(crc), entry->fp);
            fflush(entry->fp);
            if (cnt == (entry->len + sizeof(crc))) {
                entry->crc = crc;
            }
            else {
                /* error with save */
                uint32_t errorBit = entry->subIndexOD;
                if (errorBit > 31) errorBit = 31;
                storageError |= ((uint32_t) 1) << errorBit;
            }
        }

        if (closeFiles) {
            fclose(entry->fp);
            entry->fp = NULL;
        }
    }

    return storageError;
}

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */
