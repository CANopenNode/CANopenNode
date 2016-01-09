/**
 * Microcontroller specific code for CANopenNode nonvolatile variables.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        eeprom.h
 * @ingroup     CO_eeprom
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
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
