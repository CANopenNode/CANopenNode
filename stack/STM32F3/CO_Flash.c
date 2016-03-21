/*
 * STM32F3 flash support for CANopen stack
 *
 * @file        CO_Flash.c
 * @author      Janez Paternoster
 * @author      Olof Larsson
 * @author      Petteri Mustonen
 * @copyright   2014 Janez Paternoster
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

//============================================================================
//                                INCLUDES
//============================================================================
#include "CANopen.h"
#include "stm32f30x.h"

//============================================================================
//                                DEFINES
//============================================================================
#define PARAM_STORE_PASSWORD   0x65766173
#define PARAM_RESTORE_PASSWORD 0x64616F6C

#define DEBUG 0

#define LAST_PAGE_ADDRESS           0x0800F800
#define PAGES_PER_FLASH_AREA        1
#define FLASH_PAGE_SIZE             0x800
#define CO_OD_FLASH_PARAM_DEFAULT   LAST_PAGE_ADDRESS - (1*PAGES_PER_FLASH_AREA*FLASH_PAGE_SIZE)
#define CO_OD_FLASH_PARAM_RUNTIME   LAST_PAGE_ADDRESS - (2*PAGES_PER_FLASH_AREA*FLASH_PAGE_SIZE)

#define CO_UNUSED(v)  (void)(v)

//============================================================================
//                                LOCAL DATA
//============================================================================
extern struct sCO_OD_ROM CO_OD_ROM;

enum CO_OD_H1010_StoreParam_Sub
{
    OD_H1010_STORE_PARAM_COUNT,
    OD_H1010_STORE_PARAM_ALL,
    OD_H1010_STORE_PARAM_COMM,
    OD_H1010_STORE_PARAM_APP,
    OD_H1010_STORE_PARAM_MANUFACTURER,
    OD_H1010_STORE_PARAM_RESERVED       = 0x80
};

enum CO_OD_H1011_RestoreDefaultParam_Sub
{
    OD_H1011_RESTORE_PARAM_COUNT,
    OD_H1011_RESTORE_PARAM_ALL,
    OD_H1011_RESTORE_PARAM_COMM,
    OD_H1011_RESTORE_PARAM_APP,
    OD_H1011_RESTORE_PARAM_MANUFACTURER,
    OD_H1011_RESTORE_PARAM_RESERVED     = 0x80
};

enum CO_StorageFunctionality_Flags
{
    SAVES_PARAM_ON_COMMAND   = 0x01,
    SAVES_PARAM_AUTONOMOUSLY = 0x02
};

enum CO_RestoreFunctionality_Flags
{
    RESTORES_PARAMETERS   = 0x01
};

//============================================================================
/**
* Store parameters of object dictionary into flash memory.
* \param[in] FlashAddress Use CO_OD_Flash_Adress for the normal parameter
*                         block and CO_OD_Flash_Default_Param for the
*                         default parameters
*/
static CO_SDO_abortCode_t storeParameters(uint32_t FlashAddress, uint8_t ParametersSub)
{
    uint32_t addressPtr = 0x00;
    uint32_t addressFinal = 0x00;
    uint32_t* chunk;

    CO_UNUSED(ParametersSub);

    /* Unlock flash*/
    FLASH_Unlock();

    /* Check flash allocation */
    int32_t bytes_to_write = sizeof(CO_OD_ROM);
    if ((uint32_t)bytes_to_write > (FLASH_PAGE_SIZE*PAGES_PER_FLASH_AREA)) {
        return CO_SDO_AB_HW;
    }

    addressFinal = FlashAddress + bytes_to_write;
    /* Clear pending flakgs (if any) */  
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR); 

    /* Erase page */
    FLASH_ErasePage(FlashAddress);

    /* Program data into flash word by word */
    chunk = (uint32_t*) &CO_OD_ROM;
    for (addressPtr = FlashAddress; addressPtr < addressFinal; addressPtr += 4) {
        if (FLASH_ProgramWord(addressPtr, *chunk) != FLASH_COMPLETE) {
            return CO_SDO_AB_HW;
        }
        chunk++;
    }
    FLASH_Lock();

    return CO_SDO_AB_NONE;
}

//============================================================================
/**
* Restore parameters of object dictionary from flash memory.
* \param[in] FlashAddress Use CO_OD_Flash_Adress for the normal parameter
*                         block and CO_OD_Flash_Default_Param for the
*                         default parameters
*/
void flash_read(uint32_t FlashAddress, void* RamAddress, size_t len)
{
    uint8_t* p_flash = (uint8_t *) FlashAddress;
    uint8_t* p_ram = (uint8_t *) RamAddress;
    uint32_t idx;

    for (idx = 0; idx < len; idx++) {
        *p_ram = *(__IO uint8_t *)p_flash;
        p_ram++;
        p_flash++;
    }
}

static CO_SDO_abortCode_t restoreParameters(uint32_t FlashAddress, uint8_t ParametersSub)
{
    CO_UNUSED(ParametersSub);

    flash_read(FlashAddress, &CO_OD_ROM, sizeof(CO_OD_ROM));

    return CO_SDO_AB_NONE;
}

//============================================================================
/**
* Access to object dictionary OD_H1010_STORE_PARAM_FUNC
*/
static CO_SDO_abortCode_t CO_ODF_1010_StoreParam(CO_ODF_arg_t *ODF_arg)
{

    uint32_t* value = (uint32_t*)ODF_arg->data;

    if (ODF_arg->reading) {
        if(OD_H1010_STORE_PARAM_ALL == ODF_arg->subIndex) {
            *value = SAVES_PARAM_ON_COMMAND;
        }
        return CO_SDO_AB_NONE;
    }

    if(OD_H1010_STORE_PARAM_ALL != ODF_arg->subIndex) {
        return CO_SDO_AB_NONE;
    }

    if (*value != PARAM_STORE_PASSWORD) {
        return CO_SDO_AB_DATA_TRANSF;
    }

    return storeParameters(CO_OD_FLASH_PARAM_RUNTIME, ODF_arg->subIndex);
}

//============================================================================
/**
* Access to object dictionary OD_H1010_STORE_PARAM_FUNC
*/
static CO_SDO_abortCode_t CO_ODF_1011_RestoreParam(CO_ODF_arg_t *ODF_arg)
{
    uint32_t* value = (uint32_t*)ODF_arg->data;

    if (ODF_arg->reading) {
        if (OD_H1011_RESTORE_PARAM_ALL == ODF_arg->subIndex) {
            *value = RESTORES_PARAMETERS;
        }
        return CO_SDO_AB_NONE;
    }

    if (OD_H1011_RESTORE_PARAM_ALL != ODF_arg->subIndex) {
        return CO_SDO_AB_NONE;
    }

    if (*value != PARAM_RESTORE_PASSWORD) {
        return CO_SDO_AB_DATA_TRANSF;
    }

    CO_SDO_abortCode_t Result = restoreParameters(CO_OD_FLASH_PARAM_DEFAULT, ODF_arg->subIndex);
    
    if (Result != CO_SDO_AB_NONE) {
        return Result;
    }

    return storeParameters(CO_OD_FLASH_PARAM_RUNTIME, OD_H1011_RESTORE_PARAM_ALL);
}

//===========================================================================
/**
* Initialize flash library and data storage in flash
* We use two blocks in flash for data storage. One block is used for the
* default data that will be restored. The default parameters are stored
* at address CO_OD_Flash_Default_Param. The data that will be loaded at
* startup or saved if user modifies data is store at CO_OD_Flash_Adress.
*/
void CO_FlashInit(void)
{
    /* Before we can access the data, we need to make sure, that the flash
    block are properly initialized. We do this by reading the block into
    a local sCO_OD_ROM variable and verifying the FirstWord and LastWord
    members. */

    struct sCO_OD_ROM DefaultObjDicParam;
    flash_read(CO_OD_FLASH_PARAM_DEFAULT, &DefaultObjDicParam, sizeof(DefaultObjDicParam));

    /* If the default parameters are not present in flash, then we know that
    we need to create them for later restore. */

    if ((DefaultObjDicParam.FirstWord != CO_OD_FIRST_LAST_WORD) ||
    (DefaultObjDicParam.LastWord  != CO_OD_FIRST_LAST_WORD)) {
        storeParameters(CO_OD_FLASH_PARAM_RUNTIME, OD_H1010_STORE_PARAM_ALL);
        storeParameters(CO_OD_FLASH_PARAM_DEFAULT, OD_H1010_STORE_PARAM_ALL);
    }
    else {
        restoreParameters(CO_OD_FLASH_PARAM_RUNTIME, OD_H1010_STORE_PARAM_ALL);
    }
}

//===========================================================================
void CO_FlashRegisterODFunctions(CO_t* CO)
{
    CO_OD_configure(*(CO->SDO), OD_H1010_STORE_PARAM_FUNC,
    CO_ODF_1010_StoreParam, (void*)0, 0, 0);

    CO_OD_configure(*(CO->SDO), OD_H1011_REST_PARAM_FUNC,
    CO_ODF_1011_RestoreParam, (void*)0, 0, 0);
}
