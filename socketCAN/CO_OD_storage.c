/*
 * CANopen Object Dictionary storage object for Linux SocketCAN.
 *
 * @file        CO_OD_storage.c
 * @author      Janez Paternoster
 * @copyright   2015 - 2020 Janez Paternoster
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


#include "301/CO_driver.h"
#include "301/CO_SDOserver.h"
#include "301/CO_Emergency.h"
#include "301/crc16-ccitt.h"
#include "CO_OD_storage.h"

#include <stdio.h>
#include <string.h>     /* for memcpy */
#include <stdlib.h>     /* for malloc, free */


#define RETURN_SUCCESS  0
#define RETURN_ERROR   -1


/******************************************************************************/
CO_SDO_abortCode_t CO_ODF_1010(CO_ODF_arg_t *ODF_arg) {
    CO_OD_storage_t *odStor;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    odStor = (CO_OD_storage_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading) {
        /* don't change the old value */
        memcpy(ODF_arg->data, ODF_arg->ODdataStorage, 4);

        if(ODF_arg->subIndex == 1) {
            /* store parameters */
            if(value == 0x65766173UL) {
                if(CO_OD_storage_saveSecure(odStor->odAddress, odStor->odSize, odStor->filename) != 0) {
                    ret = CO_SDO_AB_HW;
                }
            }
            else {
                ret = CO_SDO_AB_DATA_TRANSF;
            }
        }
    }

    return ret;
}


/******************************************************************************/
CO_SDO_abortCode_t CO_ODF_1011(CO_ODF_arg_t *ODF_arg) {
    CO_OD_storage_t *odStor;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    odStor = (CO_OD_storage_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading) {
        /* don't change the old value */
        memcpy(ODF_arg->data, ODF_arg->ODdataStorage, 4);

        if(ODF_arg->subIndex >= 1) {
            /* restore default parameters */
            if(value == 0x64616F6CUL) {
                if(CO_OD_storage_restoreSecure(odStor->filename) != 0) {
                    ret = CO_SDO_AB_HW;
                }
            }
            else {
                ret = CO_SDO_AB_DATA_TRANSF;
            }
        }
    }

    return ret;
}


/******************************************************************************/
int CO_OD_storage_saveSecure(
        uint8_t                *odAddress,
        uint32_t                odSize,
        char                   *filename)
{
    int ret = RETURN_SUCCESS;

    char *filename_old = NULL;
    uint16_t CRC = 0;

    /* Generate new string with extension '.old' and rename current file to it. */
    filename_old = malloc(strlen(filename)+10);
    if(filename_old != NULL) {
        strcpy(filename_old, filename);
        strcat(filename_old, ".old");

        remove(filename_old);
        if(rename(filename, filename_old) != 0) {
            ret = RETURN_ERROR;
        }
    } else {
        ret = RETURN_ERROR;
    }

    /* Open a new file and write data to it, including CRC. */
    if(ret == RETURN_SUCCESS) {
        FILE *fp = fopen(filename, "w");
        if(fp != NULL) {

            //CO_LOCK_OD();
            fwrite((const void *)odAddress, 1, odSize, fp);
            CRC = crc16_ccitt((unsigned char*)odAddress, odSize, 0);
            //CO_UNLOCK_OD();

            fwrite((const void *)&CRC, 1, 2, fp);
            fclose(fp);
        } else {
            ret = RETURN_ERROR;
        }
    }

    /* Verify data */
    if(ret == RETURN_SUCCESS) {
        void *buf = NULL;
        FILE *fp = NULL;
        uint32_t cnt = 0;
        uint16_t CRC2 = 0;

        buf = malloc(odSize + 4);
        if(buf != NULL) {
            fp = fopen(filename, "r");
            if(fp != NULL) {
                cnt = fread(buf, 1, odSize, fp);
                CRC2 = crc16_ccitt((unsigned char*)buf, odSize, 0);
                /* read also two bytes of CRC */
                cnt += fread(buf, 1, 4, fp);
                fclose(fp);
            }
            free(buf);
        }
        /* If size or CRC differs, report error */
        if(buf == NULL || fp == NULL || cnt != (odSize + 2) || CRC != CRC2) {
            ret = RETURN_ERROR;
        }
    }

    /* In case of error, set back the old file. */
    if(ret != RETURN_SUCCESS && filename_old != NULL) {
        remove(filename);
        rename(filename_old, filename);
    }

    free(filename_old);

    return ret;
}


/******************************************************************************/
int CO_OD_storage_restoreSecure(char *filename) {
    int ret = RETURN_SUCCESS;
    FILE *fp = NULL;

    /* If filename already exists, rename it to '.old'. */
    fp = fopen(filename, "r");
    if(fp != NULL) {
        char *filename_old = NULL;

        fclose(fp);

        filename_old = malloc(strlen(filename)+10);
        if(filename_old != NULL) {
            strcpy(filename_old, filename);
            strcat(filename_old, ".old");

            remove(filename_old);
            if(rename(filename, filename_old) != 0) {
                ret = RETURN_ERROR;
            }
            free(filename_old);
        }
        else {
            ret = RETURN_ERROR;
        }
    }

    /* create an empty file and write "-\n" to it. */
    if(ret == RETURN_SUCCESS) {
        fp = fopen(filename, "w");
        if(fp != NULL) {
            fputs("-\n", fp);
            fclose(fp);
        } else {
            ret = RETURN_ERROR;
        }
    }

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_OD_storage_init(
        CO_OD_storage_t        *odStor,
        uint8_t                *odAddress,
        uint32_t                odSize,
        char                   *filename)
{
    CO_ReturnError_t ret = CO_ERROR_NO;
    void *buf = NULL;

    /* verify arguments */
    if(odStor==NULL || odAddress==NULL) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* configure object variables and allocate buffer */
    if(ret == CO_ERROR_NO) {
        odStor->odAddress = odAddress;
        odStor->odSize = odSize;
        odStor->filename = filename;
        odStor->fp = NULL;
        odStor->lastSavedUs = 0;

        buf = malloc(odStor->odSize);
        if(buf == NULL) {
            ret = CO_ERROR_OUT_OF_MEMORY;
        }
    }

    /* read data from the file and verify CRC */
    if(ret == CO_ERROR_NO) {
        FILE *fp;
        uint32_t cnt = 0;
        uint16_t CRC[2];

        fp = fopen(odStor->filename, "r");
        if(fp) {
            cnt = fread(buf, 1, odStor->odSize, fp);
            /* read also two bytes of CRC from file */
            cnt += fread(&CRC[0], 1, 4, fp);
            CRC[1] = crc16_ccitt((unsigned char*)buf, odStor->odSize, 0);
            fclose(fp);
        }

        if(cnt == 2 && *((char*)buf) == '-') {
            /* file is empty, default values will be used, no error */
            ret = CO_ERROR_NO;
        }
        else if(cnt != (odStor->odSize + 2)) {
            /* file length does not match */
            ret = CO_ERROR_DATA_CORRUPT;
        }
        else if(CRC[0] != CRC[1]) {
            /* CRC does not match */
            ret = CO_ERROR_CRC;
        }
        else {
            /* no errors, copy data into Object dictionary */
            memcpy(odStor->odAddress, buf, odStor->odSize);
        }
    }

    free(buf);

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_OD_storage_autoSave(
        CO_OD_storage_t        *odStor,
        uint32_t                timer1usDiff,
        uint32_t                delay_us)
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if(odStor==NULL || odStor->odAddress==NULL) {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* don't save file more often than delay */
    odStor->lastSavedUs += timer1usDiff;
    if (odStor->lastSavedUs > delay_us) {
        void *buf = NULL;
        bool_t saveData = false;

        /* allocate buffer and open file if necessary */
        if(ret == CO_ERROR_NO) {
            buf = malloc(odStor->odSize);
            if(odStor->fp == NULL) {
                odStor->fp = fopen(odStor->filename, "r+");
            }
            if(buf == NULL || odStor->fp == NULL) {
                ret = CO_ERROR_OUT_OF_MEMORY;
            }
        }

        /* read data from the beginning of the file */
        if(ret == CO_ERROR_NO) {
            uint32_t cnt = 0;

            rewind(odStor->fp);
            cnt = fread(buf, 1, odStor->odSize, odStor->fp);

            if(cnt == 2 && *((char*)buf) == '-') {
                /* file is empty, data will be saved. */
                saveData = true;
            }
            else if(cnt == odStor->odSize) {
                /* verify, if data differs */
                if(memcmp((const void *)buf, (const void *)odStor->odAddress, odStor->odSize) != 0) {
                    saveData = true;
                }
            }
            else {
                /* file length does not match */
                ret = CO_ERROR_DATA_CORRUPT;
            }
        }

        /* Save the data to the file only if data differs. */
        if(ret == CO_ERROR_NO && saveData) {
            uint16_t CRC;

            /* copy data to temporary buffer */
            memcpy(buf, odStor->odAddress, odStor->odSize);

            rewind(odStor->fp);
            fwrite((const void *)buf, 1, odStor->odSize, odStor->fp);

            /* write also CRC */
            CRC = crc16_ccitt((unsigned char*)buf, odStor->odSize, 0);
            fwrite((const void *)&CRC, 1, 2, odStor->fp);

            fflush(odStor->fp);

            odStor->lastSavedUs = 0;
        }

        free(buf);
    }

    return ret;
}

void CO_OD_storage_autoSaveClose(CO_OD_storage_t *odStor) {
    if(odStor->fp != NULL) {
        fclose(odStor->fp);
    }
}
