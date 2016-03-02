/**
 * eCos flash support for CANopen stack
 *
 * @file        CO_Flash.c
 * @author      Uwe Kindler
 * @copyright   2013 Uwe Kindler
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
//                                   INCLUDES
//============================================================================
#include <cyg/io/flash.h>
#include <cyg/infra/diag.h>

#include "CANopen.h"


//============================================================================
//                                 DEFINES
//============================================================================
#define CHECK_FLASH_RESULT(_result_) \
	if ((_result_) != CYG_FLASH_ERR_OK) { \
		CO_DBG_PRINT("Flash operation error: %s", cyg_flash_errmsg(_result_)); }
#define PARAM_STORE_PASSWORD 0x65766173
#define PARAM_RESTORE_PASSWORD 0x64616F6C


//============================================================================
//                             LOCAL DATA
//============================================================================
static cyg_flash_info_t flash_info;
static const int CYGNUM_CANOPEN_FLASH_DATA_BLOCK = -3;
static cyg_flashaddr_t CO_OD_Flash_Adress;
static cyg_flashaddr_t CO_OD_Flash_Default_Param;
extern struct sCO_OD_ROM CO_OD_ROM;


enum CO_OD_H1010_StoreParam_Sub
{
	OD_H1010_STORE_PARAM_COUNT,
	OD_H1010_STORE_PARAM_ALL,
	OD_H1010_STORE_PARAM_COMM,
	OD_H1010_STORE_PARAM_APP,
	OD_H1010_STORE_PARAM_MANUFACTURER,
	OD_H1010_STORE_PARAM_RESERVED     = 0x80
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
static CO_SDO_abortCode_t storeParameters(cyg_flashaddr_t FlashAddress,
	uint8_t ParametersSub)
{
	CO_DBG_PRINT("Store parameters\n");

	cyg_flashaddr_t ErrorAddress;

	//unlock flash
#ifdef CYGHWR_IO_FLASH_BLOCK_LOCKING
	int Result = cyg_flash_unlock(FlashAddress, sizeof(struct sCO_OD_ROM),
		&ErrorAddress);
	CHECK_FLASH_RESULT(Result);
	if (CYG_FLASH_ERR_OK != Result)
	{
		return CO_SDO_AB_HW;
	}
#endif

	// erase block
	Result = cyg_flash_erase(FlashAddress, sizeof(struct sCO_OD_ROM),
		&ErrorAddress);
	CHECK_FLASH_RESULT(Result);
	if (CYG_FLASH_ERR_OK != Result)
	{
		return CO_SDO_AB_HW;
	}

	// program data into flash
	Result = cyg_flash_program(FlashAddress, &CO_OD_ROM,
		sizeof(CO_OD_ROM), &ErrorAddress);
	CHECK_FLASH_RESULT(Result);
	if (CYG_FLASH_ERR_OK != Result)
	{
		return CO_SDO_AB_HW;
	}

	return CO_SDO_AB_NONE;
}


//============================================================================
/**
 * Restore parameters of object dictionary from flash memory.
 * \param[in] FlashAddress Use CO_OD_Flash_Adress for the normal parameter
 *                         block and CO_OD_Flash_Default_Param for the
 *                         default parameters
 */
static CO_SDO_abortCode_t restoreParameters(cyg_flashaddr_t FlashAddress,
	uint8_t ParametersSub)
{
	CO_DBG_PRINT("Restore parameters\n");
	cyg_flashaddr_t ErrorAddress;
	int Result = cyg_flash_read(FlashAddress, &CO_OD_ROM,
		sizeof(CO_OD_ROM), &ErrorAddress);
	CHECK_FLASH_RESULT(Result);
	if (CYG_FLASH_ERR_OK != Result)
	{
		return CO_SDO_AB_HW;
	}
	return CO_SDO_AB_NONE;
}


//============================================================================
/**
 * Access to object dictionary OD_H1010_STORE_PARAM_FUNC
 */
static CO_SDO_abortCode_t CO_ODF_1010_StoreParam(CO_ODF_arg_t *ODF_arg)
{
	CO_DBG_PRINT("CO_ODF_1010 Sub: %d\n", ODF_arg->subIndex);
	CO_DBG_PRINT("sizeof(sCO_OD_ROM): %d", sizeof(CO_OD_ROM));
	uint32_t* value = (uint32_t*)ODF_arg->data;
	if (ODF_arg->reading)
	{
		if(OD_H1010_STORE_PARAM_ALL == ODF_arg->subIndex)
		{
			*value = SAVES_PARAM_ON_COMMAND;
		}
		return CO_SDO_AB_NONE;
	}

	if(OD_H1010_STORE_PARAM_ALL != ODF_arg->subIndex)
	{
		return CO_SDO_AB_NONE;
	}

	if (*value != PARAM_STORE_PASSWORD)
	{
		return CO_SDO_AB_DATA_TRANSF;
	}

    return storeParameters(CO_OD_Flash_Adress, ODF_arg->subIndex);
}

//============================================================================
/**
 * Access to object dictionary OD_H1010_STORE_PARAM_FUNC
 */
static CO_SDO_abortCode_t CO_ODF_1011_RestoreParam(CO_ODF_arg_t *ODF_arg)
{
	CO_DBG_PRINT("CO_ODF_1010 Sub: %d\n", ODF_arg->subIndex);
	CO_DBG_PRINT("sizeof(sCO_OD_ROM): %d", sizeof(CO_OD_ROM));
	uint32_t* value = (uint32_t*)ODF_arg->data;
	if (ODF_arg->reading)
	{
		if (OD_H1011_RESTORE_PARAM_ALL == ODF_arg->subIndex)
		{
			*value = RESTORES_PARAMETERS;
		}
		return CO_SDO_AB_NONE;
	}

	if (OD_H1011_RESTORE_PARAM_ALL != ODF_arg->subIndex)
	{
		return CO_SDO_AB_NONE;
	}

	if (*value != PARAM_RESTORE_PASSWORD)
	{
		return CO_SDO_AB_DATA_TRANSF;
	}

	CO_SDO_abortCode_t Result = restoreParameters(CO_OD_Flash_Default_Param,
		ODF_arg->subIndex);
	if (Result != CO_SDO_AB_NONE)
	{
		CO_DBG_PRINT("restoreParameters returned error");
		return Result;
	}

	return storeParameters(CO_OD_Flash_Adress, OD_H1011_RESTORE_PARAM_ALL);
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
	int i = 0;
	//
	// Initialize flash library
	//
	int Result = cyg_flash_init(0);
	CHECK_FLASH_RESULT(Result);
#ifdef CYGDBG_IO_CANOPEN_DEBUG
	cyg_flash_set_global_printf(diag_printf);
#endif

	//
	// Read info about flash device (number of blocks. block size)
	//
	Result = cyg_flash_get_info(0, &flash_info);
	CHECK_FLASH_RESULT(Result);
	CO_DBG_PRINT("Flash info dev %d: 0x%x - 0x%x, %d blocks\n", 0, flash_info.start,
		flash_info.end, flash_info.num_block_infos);
	const cyg_flash_block_info_t* block_info = flash_info.block_info;
	for (i = 0; i < flash_info.num_block_infos; ++i)
	{
		CO_DBG_PRINT("Block %d: block size: %d blocks: %d\n", i,
			block_info->block_size, block_info->blocks);
		block_info++;
	}

	//
	// Calculate addresses for flash data and default flash data
	//
	block_info = &flash_info.block_info[flash_info.num_block_infos - 1];
	CO_DBG_PRINT("Last block - block size: %d blocks: %d\n",
		block_info->block_size, block_info->blocks);
	CO_OD_Flash_Adress = flash_info.end + 1 +
		(CYGNUM_CANOPEN_FLASH_DATA_BLOCK * block_info->block_size);
	CO_DBG_PRINT("CO_OD_Flash_Adress 0x%8x\n", CO_OD_Flash_Adress);
	CO_OD_Flash_Default_Param = CO_OD_Flash_Adress - block_info->block_size;
	CO_DBG_PRINT("CO_OD_Flash_Default_Param 0x%8x\n", CO_OD_Flash_Default_Param);

	//
	// Before we can access the data, we need to make sure, that the flash
	// block are properly initialized. We do this by reading the block into
	// a local sCO_OD_ROM variable and verifying the FirstWord and LastWord
	// members
	//
	cyg_flashaddr_t ErrorAddress;
	struct sCO_OD_ROM DefaultObjDicParam;
	Result = cyg_flash_read(CO_OD_Flash_Default_Param, &DefaultObjDicParam,
		sizeof(DefaultObjDicParam), &ErrorAddress);
	CHECK_FLASH_RESULT(Result);

	//
	// If the default parameters are not present in flash, then we know that
	// we need to create them for later restore
	//
	if ((DefaultObjDicParam.FirstWord != CO_OD_FIRST_LAST_WORD)
	  ||(DefaultObjDicParam.LastWord != CO_OD_FIRST_LAST_WORD))
	{
		storeParameters(CO_OD_Flash_Adress, OD_H1010_STORE_PARAM_ALL);
		storeParameters(CO_OD_Flash_Default_Param, OD_H1010_STORE_PARAM_ALL);
	}
	else
	{
		restoreParameters(CO_OD_Flash_Adress, OD_H1010_STORE_PARAM_ALL);
	}
}


//===========================================================================
void CO_FlashRegisterODFunctions(CO_t* CO)
{
	CO_OD_configure(CO->SDO, OD_H1010_STORE_PARAM_FUNC,
		CO_ODF_1010_StoreParam, (void*)0, 0, 0);
	CO_OD_configure(CO->SDO, OD_H1011_REST_PARAM_FUNC,
		CO_ODF_1011_RestoreParam, (void*)0, 0, 0);
}

//---------------------------------------------------------------------------
// EOF CO_flash.c
