/**
 * Atmel SAM3 flash support for CANopen stack
 *
 * @file        CO_Flash.h
 * @author      Olof Larsson
 * @copyright   2014 Olof Larsson
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
 *
 * This piece of software is using The Atmel Software Framework (ASF).
 *
 * http://www.atmel.com/asf
 *
 */

#ifndef CO_FLASH_H
#define CO_FLASH_H

//============================================================================
//                                INCLUDES
//============================================================================
#include "CANopen.h"

/**
 * Initialize flash library and data storage in flash
 * We use two blocks in flash for data storage. One block is used for the
 * default data that will be restored. The default parameters are stored
 * at address CO_OD_Flash_Default_Param. The data that will be loaded at
 * startup or saved if user modifies data.
 */
void CO_FlashInit(void);

/**
 * Register object dictionary functions for parameter storage and restoring
 * parameters (Object dictionary index 0x1010 Store Param and 0x1011 Restore
 * default param.
 */
void CO_FlashRegisterODFunctions(CO_t* CO);

#endif
