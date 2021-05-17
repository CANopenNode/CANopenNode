/**
 * CANopen data storage object for storing data into block device (eeprom)
 *
 * @file        CO_storageEeprom.h
 * @ingroup     CO_storage_eeprom
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

#ifndef CO_STORAGE_EEPROM_H
#define CO_STORAGE_EEPROM_H

#include "storage/CO_storage.h"

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_storage_eeprom Data storage in eeprom
 * Eeprom specific data storage functions.
 *
 * @ingroup CO_CANopen_storage
 * @{
 *
 * This is an interface into generic CANopenNode @ref CO_storage for usage with
 * eeprom chip like 25LC256. Functions @ref CO_storageEeprom_init() and
 * @ref CO_storageEeprom_auto_process are target system independent. Functions
 * specified by @ref CO_eeprom.h file, must be defined by target system.
 * For example implementation see CANopenPIC/PIC32.
 *
 * Storage principle:
 * This function first reads 'signatures' for all entries from the known address
 * from the eeprom. If signature for each entry is correct, then data is read
 * from correct address from the eeprom into storage location. If signature is
 * wrong, then data for that entry is indicated as corrupt and CANopen
 * emergency message is sent.
 *
 * Signature also includes 16-bit CRC checksum of the data stored in eeprom. If
 * it differs from CRC checksum calculated from the data actually loaded (on
 * program startup), then entry is indicated as corrupt and CANopen emergency
 * message is sent.
 *
 * Signature is written to eeprom, when data block is stored via CANopen SDO
 * write command to object 0x1010. Signature is erased, with CANopen SDO write
 * command to object 0x1011. If signature is not valid or is erased for any
 * entry, emergency message is sent. If eeprom is new, then all signatures are
 * wrong, so it is best to store all parameters by writing to 0x1010, sub 1.
 *
 * If entry attribute has CO_storage_auto set, then data block is stored
 * autonomously, byte by byte, on change, during program run. Those data blocks
 * are stored into write unprotected location. For auto storage to work,
 * its signature in eeprom must be correct. CRC checksum for the data is not
 * used.
 */


/**
 * Initialize data storage object (block device (eeprom) specific)
 *
 * This function should be called by application after the program startup,
 * before @ref CO_CANopenInit(). This function initializes storage object,
 * OD extensions on objects 1010 and 1011, reads data from file, verifies them
 * and writes data to addresses specified inside entries. This function
 * internally calls @ref CO_storage_init().
 *
 * @param storage This object will be initialized. It must be defined by
 * application and must exist permanently.
 * @param CANmodule CAN device, used for @ref CO_LOCK_OD() macro.
 * @param storageModule Pointer to storage module passed to CO_eeprom functions.
 * @param OD_1010_StoreParameters OD entry for 0x1010 -"Store parameters".
 * Entry is optional, may be NULL.
 * @param OD_1011_RestoreDefaultParam OD entry for 0x1011 -"Restore default
 * parameters". Entry is optional, may be NULL.
 * @param entries Pointer to array of storage entries, see @ref CO_storage_init.
 * @param entriesCount Count of storage entries
 * @param [out] storageInitError If function returns CO_ERROR_DATA_CORRUPT,
 * then this variable contains a bit mask from subIndexOD values, where data
 * was not properly initialized. If other error, then this variable contains
 * index or erroneous entry. If there is hardware error like missing eeprom,
 * then storageInitError is 0xFFFFFFFF and function returns
 * CO_ERROR_DATA_CORRUPT.
 *
 * @return CO_ERROR_NO, CO_ERROR_DATA_CORRUPT if data can not be initialized,
 * CO_ERROR_ILLEGAL_ARGUMENT or CO_ERROR_OUT_OF_MEMORY.
 */
CO_ReturnError_t CO_storageEeprom_init(CO_storage_t *storage,
                                       CO_CANmodule_t *CANmodule,
                                       void *storageModule,
                                       OD_entry_t *OD_1010_StoreParameters,
                                       OD_entry_t *OD_1011_RestoreDefaultParam,
                                       CO_storage_entry_t *entries,
                                       uint8_t entriesCount,
                                       uint32_t *storageInitError);


/**
 * Automatically update data if differs inside eeprom.
 *
 * Should be called cyclically by program. Each interval it updates one byte.
 *
 * @param storage This object
 * @param saveAll If true, all bytes are updated, useful on program end.
 */
void CO_storageEeprom_auto_process(CO_storage_t *storage, bool_t saveAll);

/** @} */ /* CO_storage_eeprom */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */

#endif /* CO_STORAGE_EEPROM_H */
