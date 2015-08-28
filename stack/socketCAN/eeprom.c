/*
 * Eeprom object for Linux SocketCAN.
 *
 * @file        eeprom.c
 * @author      Janez Paternoster
 * @copyright   2015 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <http://canopennode.sourceforge.net>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "CO_driver.h"
#include "CO_SDO.h"
#include "CO_Emergency.h"
#include "eeprom.h"
#include "crc16-ccitt.h"

#include <string.h>     /* for memcpy */
#include <stdlib.h>     /* for malloc, free */
#if 0
/* Store parameters ***********************************************************/
static CO_SDO_abortCode_t CO_ODF_1010(CO_ODF_arg_t *ODF_arg){
    CO_EE_t *ee;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    ee = (CO_EE_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading){
        /* don't change the old value */
        CO_memcpy(ODF_arg->data, (const uint8_t*)ODF_arg->ODdataStorage, 4U);

        if(ODF_arg->subIndex == 1){
            if(value == 0x65766173UL){
                /* store parameters */
                /* rename current file to .old */
                remove(EE_ROM_FILENAME_OLD);
                rename(EE_ROM_FILENAME, EE_ROM_FILENAME_OLD);
                /* open a file */
                FILE *fp = fopen(EE_ROM_FILENAME, "wb");
                if(!fp){
                    rename(EE_ROM_FILENAME_OLD, EE_ROM_FILENAME);
                    return CO_SDO_AB_HW;
                }

                /* write data to the file */
                fwrite((const void *)ee->OD_ROMAddress, 1, ee->OD_ROMSize, fp);
                /* write CRC to the end of the file */
                uint16_t CRC = crc16_ccitt((unsigned char*)ee->OD_ROMAddress, ee->OD_ROMSize, 0);
                fwrite((const void *)&CRC, 1, 2, fp);
                fclose(fp);

                /* verify data */
                void *buf = malloc(ee->OD_ROMSize + 4);
                if(buf){
                    fp = fopen(EE_ROM_FILENAME, "rb");
                    uint32_t cnt = 0;
                    uint16_t CRC2 = 0;
                    if(fp){
                        cnt = fread(buf, 1, ee->OD_ROMSize, fp);
                        CRC2 = crc16_ccitt((unsigned char*)buf, ee->OD_ROMSize, 0);
                        /* read also two bytes of CRC */
                        cnt += fread(buf, 1, 4, fp);
                        fclose(fp);
                    }
                    free(buf);
                    if(cnt == (ee->OD_ROMSize + 2) && CRC == CRC2){
                        /* write successful */
                        return CO_SDO_AB_NONE;
                    }
                }
                /* error, set back the old file */
                remove(EE_ROM_FILENAME);
                rename(EE_ROM_FILENAME_OLD, EE_ROM_FILENAME);

                return CO_SDO_AB_HW;
            }
            else
                return CO_SDO_AB_DATA_TRANSF;
        }
    }

    return ret;
}


/* Restore default parameters *************************************************/
static CO_SDO_abortCode_t CO_ODF_1011(CO_ODF_arg_t *ODF_arg){
    CO_EE_t *ee;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    ee = (CO_EE_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading){
        /* don't change the old value */
        CO_memcpy(ODF_arg->data, (const uint8_t*)ODF_arg->ODdataStorage, 4U);

        if(ODF_arg->subIndex >= 1){
            if(value == 0x64616F6CUL){
                /* restore default parameters */
                /* rename current file to .old, so it no longer exist */
                remove(EE_ROM_FILENAME_OLD);
                rename(EE_ROM_FILENAME, EE_ROM_FILENAME_OLD);

                /* create an empty file */
                FILE *fp = fopen(EE_ROM_FILENAME, "wt");
                if(!fp){
                    rename(EE_ROM_FILENAME_OLD, EE_ROM_FILENAME);
                    return CO_SDO_AB_HW;
                }
                /* write one byte '-' to the file */
                fputc('-', fp);
                fclose(fp);

                return CO_SDO_AB_NONE;
            }
            else
                return CO_SDO_AB_DATA_TRANSF;
        }
    }

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_EE_init_1(
        CO_EE_t                *ee,
        uint8_t                *SRAMAddress,
        uint8_t                *OD_EEPROMAddress,
        uint32_t                OD_EEPROMSize,
        uint8_t                *OD_ROMAddress,
        uint32_t                OD_ROMSize)
{
    /* verify arguments */
    if(ee==NULL || OD_EEPROMAddress==NULL || OD_ROMAddress==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* configure object variables */
    ee->pSRAM = (uint32_t*)SRAMAddress;
    ee->OD_EEPROMAddress = (uint32_t*) OD_EEPROMAddress;
    ee->OD_EEPROMSize = OD_EEPROMSize / 4;
    ee->OD_ROMAddress = OD_ROMAddress;
    ee->OD_ROMSize = OD_ROMSize;
    ee->OD_EEPROMCurrentIndex = 0;
    ee->OD_EEPROMWriteEnable = false;

    /* read the CO_OD_EEPROM from SRAM, first verify, if data are OK */
    if(ee->pSRAM == 0) return CO_ERROR_OUT_OF_MEMORY;
    uint32_t firstWordRAM = *(ee->OD_EEPROMAddress);
    uint32_t firstWordEE = *(ee->pSRAM);
    uint32_t lastWordEE = *(ee->pSRAM + ee->OD_EEPROMSize - 1);
    if(firstWordRAM == firstWordEE && firstWordRAM == lastWordEE){
        unsigned int i;
        for(i=0; i<ee->OD_EEPROMSize; i++)
            (ee->OD_EEPROMAddress)[i] = (ee->pSRAM)[i];
    }
    ee->OD_EEPROMWriteEnable = true;

    /* read the CO_OD_ROM from file and verify CRC */
    void *buf = malloc(ee->OD_ROMSize);
    if(buf){
        CO_ReturnError_t ret = CO_ERROR_NO;
        FILE *fp = fopen(EE_ROM_FILENAME, "rb");
        uint32_t cnt = 0;
        uint16_t CRC[2];
        if(fp){
            cnt = fread(buf, 1, ee->OD_ROMSize, fp);
            /* read also two bytes of CRC from file */
            cnt += fread(&CRC[0], 1, 4, fp);
            CRC[1] = crc16_ccitt((unsigned char*)buf, ee->OD_ROMSize, 0);
            fclose(fp);
        }

        if(cnt == 1 && *((char*)buf) == '-'){
            ret = CO_ERROR_NO; /* file is empty, default values will be used, no error */
        }
        else if(cnt != (ee->OD_ROMSize + 2)){
            ret = CO_ERROR_DATA_CORRUPT; /* file length does not match */
        }
        else if(CRC[0] != CRC[1]){
            ret = CO_ERROR_CRC;       /* CRC does not match */
        }
        else{
            /* no errors, copy data into object dictionary */
            memcpy(ee->OD_ROMAddress, buf, ee->OD_ROMSize);
        }

        free(buf);
        return ret;
    }
    else return CO_ERROR_OUT_OF_MEMORY;

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_EE_init_2(
        CO_EE_t                *ee,
        CO_ReturnError_t        eeStatus,
        CO_SDO_t               *SDO,
        CO_EM_t                *em)
{
    CO_OD_configure(SDO, OD_H1010_STORE_PARAM_FUNC, CO_ODF_1010, (void*)ee, 0, 0U);
    CO_OD_configure(SDO, OD_H1011_REST_PARAM_FUNC, CO_ODF_1011, (void*)ee, 0, 0U);
    if(eeStatus != CO_ERROR_NO){
        CO_errorReport(em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, (uint32_t)eeStatus);
    }
}


/******************************************************************************/
void CO_EE_process(CO_EE_t *ee){
    if(ee && ee->OD_EEPROMWriteEnable){
        /* verify next word */
        unsigned int i = ee->OD_EEPROMCurrentIndex;
        if(++i == ee->OD_EEPROMSize) i = 0;
        ee->OD_EEPROMCurrentIndex = i;

        /* update SRAM */
        (ee->pSRAM)[i] = (ee->OD_EEPROMAddress)[i];
  }
}
#endif
