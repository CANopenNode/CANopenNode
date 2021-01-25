/*
 * CANopen Object Dictionary storage object for Linux SocketCAN.
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
static ODR_t storeLinux(void *object, void *addr, OD_size_t len) {
    ODR_t ret = ODR_OK;
    char *filename = object;
    uint16_t crc_store;

    /* Create names for temporary and old file */
    size_t fn_len = strlen(filename) + 5;
    char *filename_tmp = malloc(fn_len);
    char *filename_old = malloc(fn_len);
    if (filename_tmp == NULL || filename_old == NULL) {
        ret = ODR_OUT_OF_MEM;
    }
    else {
        strcpy(filename_tmp, filename);
        strcpy(filename_old, filename);
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
            CO_LOCK_OD();
            size_t cnt = fwrite(addr, 1, len, fp);
            crc_store = crc16_ccitt(addr, len, 0);
            CO_UNLOCK_OD();
            cnt += fwrite(&crc_store, 1, sizeof(crc_store), fp);
            fclose(fp);
            if (cnt != (len + sizeof(crc_store))) {
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

        buf = malloc(len + 4);
        if (buf != NULL) {
            fp = fopen(filename_tmp, "r");
            if (fp != NULL) {
                cnt = fread(buf, 1, len + 4, fp);
                crc_verify = crc16_ccitt(buf, len, 0);
                fclose(fp);
                memcpy(&crc_read, &buf[len], sizeof(crc_read));
            }
            free(buf);
        }
        /* If size or CRC differs, report error */
        if (buf == NULL || fp == NULL || cnt != (len + sizeof(crc_verify))
            || crc_store != crc_verify || crc_store != crc_read
        ) {
            ret = ODR_HW;
        }
    }

    /* rename existing file to *.old and *.tmp to existing */
    if (ret == ODR_OK) {
        if (rename(filename, filename_old) != 0
            || rename(filename_tmp, filename) != 0
        ) {
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
static ODR_t restoreLinux(void *object, void *addr, OD_size_t len) {
    (void) addr; (void) len;

    char *filename = object;
    ODR_t ret = ODR_OK;

    /* Rename existing filename to *.old. */
    char *filename_old = malloc(strlen(filename) + 5);
    if (filename_old == NULL) {
        ret = ODR_OUT_OF_MEM;
    }
    else {
        strcpy(filename_old, filename);
        strcat(filename_old, ".old");
        rename(filename, filename_old);
        free(filename_old);
    }

    /* create an empty file and write "-\n" to it. */
    if (ret == ODR_OK) {
        FILE *fp = fopen(filename, "w");
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


CO_ReturnError_t CO_storageLinux_entry_init(CO_storage_t *storage,
                                            CO_storage_entry_t *newEntry,
                                            void *addr,
                                            OD_size_t len,
                                            char *filename,
                                            uint8_t subIndexOD)
{
    /* verify arguments */
    if (storage == NULL || newEntry == NULL || addr == NULL || len == 0
        || filename == NULL || subIndexOD < 1
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* configure newEntry and add it to the storage */
    newEntry->addr = addr;
    newEntry->len = len;
    newEntry->object = filename;
    newEntry->subIndexOD = subIndexOD;
    newEntry->store = storeLinux;
    newEntry->restore = restoreLinux;

    CO_ReturnError_t ret;
    FILE *fp = NULL;
    uint8_t *buf = NULL;

    /* Add newEntry to storage object */
    ret = CO_storage_entry_init(storage, newEntry);

    /* Open file, check existence and create temporary buffer */
    if (ret == CO_ERROR_NO) {
        fp = fopen(filename, "r");
        if (fp == NULL) {
            ret = CO_ERROR_DATA_CORRUPT;
        }
        else {
            buf = malloc(len + 4);
            if (buf == NULL) {
                ret = CO_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    /* Read data into temporary buffer first. Then verify and copy to addr */
    if (ret == CO_ERROR_NO) {
        size_t cnt = fread(buf, 1, len + 4, fp);

        uint16_t crc1, crc2;
        crc1 = crc16_ccitt(buf, len, 0);
        memcpy(&crc2, &buf[len], sizeof(crc2));

        if (cnt == 2 && buf[0] == '-') {
            /* File is empty, default values will be used, no error */
        }
        else if (cnt != (len + sizeof(crc2)) || crc1 != crc2) {
            /* Data length does not match */
            ret = CO_ERROR_DATA_CORRUPT;
        }
        else {
            /* no errors, copy data into Object dictionary */
            memcpy(addr, buf, len);
        }
    }

    if (buf != NULL) free(buf);
    if (fp != NULL) fclose(fp);

    return ret;
}


CO_ReturnError_t CO_storageLinux_auto_init(CO_storageLinux_auto_t *storageAuto,
                                           void *addr,
                                           OD_size_t len,
                                           char *filename)
{
    /* verify arguments */
    if (storageAuto == NULL || addr == NULL || len == 0 || filename == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* configure variables */
    storageAuto->addr = addr;
    storageAuto->len = len;
    storageAuto->crc = 0;
    bool_t readOK = false;
    char *writeFileAccess = "w";

    /* Open the file, read data into temporary buffer, verify them and copy them
     * into addr. If file does not exist or data is corrupt, just skip reading
     * and keep the default values. */
    FILE *fp = fopen(filename, "r");
    if (fp != NULL) {
        uint8_t *buf = malloc(len + 4);
        if (buf != NULL) {
            size_t cnt = fread(buf, 1, len + 4, fp);

            if (cnt == 2 && buf[0] == '-') {
                /* File is empty, default values will be used, no error */
                readOK = true;
            }
            else {
                uint16_t crc1, crc2;
                crc1 = crc16_ccitt(buf, len, 0);
                memcpy(&crc2, &buf[len], sizeof(crc2));

                if (crc1 == crc2 && cnt == (len + sizeof(crc2))) {
                    memcpy(addr, buf, len);
                    storageAuto->crc = crc1;
                    readOK = true;
                    writeFileAccess = "r+";
                }
            }
            free(buf);
        }
        fclose(fp);
    }

    storageAuto->fp = fopen(filename, writeFileAccess);
    if (storageAuto->fp == NULL) return CO_ERROR_ILLEGAL_ARGUMENT;

    return readOK ? CO_ERROR_NO : CO_ERROR_DATA_CORRUPT;
}


bool_t CO_storageLinux_auto_process(CO_storageLinux_auto_t *storageAuto,
                                    bool_t closeFile)
{
    bool_t ret = false;

    /* verify arguments */
    if (storageAuto == NULL || storageAuto->fp == NULL) {
        return ret;
    }

    /* If CRC of the current data differs, save the file */
    uint16_t crc = crc16_ccitt(storageAuto->addr, storageAuto->len, 0);
    if (crc == storageAuto->crc) {
        ret = true;
    }
    else {
        size_t cnt;
        rewind(storageAuto->fp);
        CO_LOCK_OD();
        cnt = fwrite(storageAuto->addr, 1, storageAuto->len, storageAuto->fp);
        CO_UNLOCK_OD();
        cnt += fwrite(&crc, 1, sizeof(crc), storageAuto->fp);
        fflush(storageAuto->fp);
        if (cnt == (storageAuto->len + sizeof(crc))) {
            storageAuto->crc = crc;
            ret = true;
        }
    }

    if (closeFile) {
        fclose(storageAuto->fp);
        storageAuto->fp = NULL;
    }

    return ret;
}

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */
