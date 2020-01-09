/**
 * Microcontroller specific code for CANopenNode nonvolatile variables.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        eeprom.h
 * @ingroup     CO_eeprom
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
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


#include "CO_driver.h"
#include "CO_SDO.h"
#include "CO_Emergency.h"
#include "eeprom.h"
#include "crc16-ccitt.h"


/**
 * OD function for accessing _Store parameters_ (index 0x1010) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static CO_SDO_abortCode_t CO_ODF_1010(CO_ODF_arg_t *ODF_arg);
static CO_SDO_abortCode_t CO_ODF_1010(CO_ODF_arg_t *ODF_arg){
    //CO_EE_t *ee;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    //ee = (CO_EE_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading){
        /* don't change the old value */
        CO_memcpy(ODF_arg->data, (const uint8_t*)ODF_arg->ODdataStorage, 4U);

        if(ODF_arg->subIndex == 1U){
            if(value == 0x65766173UL){
                /* write ee->OD_ROMAddress, ee->OD_ROMSize to eeprom (blocking function) */

                /* verify data */
                if(0/*error*/){
                    ret = CO_SDO_AB_HW;
                }
            }
            else{
                ret = CO_SDO_AB_DATA_TRANSF;
            }
        }
    }

    return ret;
}


/**
 * OD function for accessing _Restore default parameters_ (index 0x1011) from SDO server.
 *
 * For more information see file CO_SDO.h.
 */
static CO_SDO_abortCode_t CO_ODF_1011(CO_ODF_arg_t *ODF_arg);
static CO_SDO_abortCode_t CO_ODF_1011(CO_ODF_arg_t *ODF_arg){
    //CO_EE_t *ee;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    //ee = (CO_EE_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading){
        /* don't change the old value */
        CO_memcpy(ODF_arg->data, (const uint8_t*)ODF_arg->ODdataStorage, 4U);

        if(ODF_arg->subIndex >= 1U){
            if(value == 0x64616F6CUL){
                /* Clear the eeprom */

            }
            else{
                ret = CO_SDO_AB_DATA_TRANSF;
            }
        }
    }

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_EE_init_1(
        CO_EE_t                *ee,
        uint8_t                *OD_EEPROMAddress,
        uint32_t                OD_EEPROMSize,
        uint8_t                *OD_ROMAddress,
        uint32_t                OD_ROMSize)
{
    /* verify arguments */
    if(ee==NULL || OD_EEPROMAddress==NULL || OD_ROMAddress==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure eeprom */


    /* configure object variables */
    ee->OD_EEPROMAddress = OD_EEPROMAddress;
    ee->OD_EEPROMSize = OD_EEPROMSize;
    ee->OD_ROMAddress = OD_ROMAddress;
    ee->OD_ROMSize = OD_ROMSize;
    ee->OD_EEPROMCurrentIndex = 0U;
    ee->OD_EEPROMWriteEnable = false;

    /* read the CO_OD_EEPROM from EEPROM, first verify, if data are OK */

    /* read the CO_OD_ROM from EEPROM and verify CRC */

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
    if((ee != 0) && (ee->OD_EEPROMWriteEnable)/* && !isWriteInProcess()*/){
        uint32_t i;
        uint8_t RAMdata, eeData;

        /* verify next word */
        if(++ee->OD_EEPROMCurrentIndex == ee->OD_EEPROMSize){
            ee->OD_EEPROMCurrentIndex = 0U;
        }
        i = ee->OD_EEPROMCurrentIndex;

        /* read eeprom */
        RAMdata = ee->OD_EEPROMAddress[i];
        eeData = 0/*EE_readByte(i)*/;

        /* if bytes in EEPROM and in RAM are different, then write to EEPROM */
        if(eeData != RAMdata){
            /* EE_writeByteNoWait(RAMdata, i);*/
        }
    }
}
