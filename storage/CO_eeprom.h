/**
 * Eeprom interface for use with CO_storageEeprom
 *
 * @file        CO_eeprom.h
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

#ifndef CO_EEPROM_H
#define CO_EEPROM_H

#include "301/CO_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup CO_storage_eeprom
 * @{
 */

/**
 * Initialize eeprom device, target system specific function.
 *
 * @param storageModule Pointer to storage module.
 *
 * @return True on success
 */
bool_t CO_eeprom_init(void *storageModule);


/**
 * Get free address inside eeprom, target system specific function.
 *
 * Function is called several times for each storage block in the initialization
 * phase after CO_eeprom_init().
 *
 * @param storageModule Pointer to storage module.
 * @param isAuto True, if variable is auto stored or false if protected
 * @param len Length of data, which will be stored to that location
 * @param [out] overflow set to true, if not enough eeprom memory
 *
 * @return Asigned eeprom address
 */
size_t CO_eeprom_getAddr(void *storageModule, bool_t isAuto,
                         size_t len, bool_t *overflow);


/**
 * Read block of data from the eeprom, target system specific function.
 *
 * @param storageModule Pointer to storage module.
 * @param data Pointer to data buffer, where data will be stored.
 * @param eepromAddr Address in eeprom, from where data will be read.
 * @param len Length of the data block to be read.
 */
void CO_eeprom_readBlock(void *storageModule, uint8_t *data,
                         size_t eepromAddr, size_t len);


/**
 * Write block of data to the eeprom, target system specific function.
 *
 * It is blocking function, so it waits, until all data is written.
 *
 * @param storageModule Pointer to storage module.
 * @param data Pointer to data buffer which will be written.
 * @param eepromAddr Address in eeprom, where data will be written. If data is
 * stored accross multiple pages, address must be aligned with page.
 * @param len Length of the data block.
 *
 * @return true on success
 */
bool_t CO_eeprom_writeBlock(void *storageModule, uint8_t *data,
                            size_t eepromAddr, size_t len);


/**
 * Get CRC checksum of the block of data stored in the eeprom, target system
 * specific function.
 *
 * @param storageModule Pointer to storage module.
 * @param eepromAddr Address of data in eeprom.
 * @param len Length of the data.
 *
 * @return CRC checksum
 */
uint16_t CO_eeprom_getCrcBlock(void *storageModule,
                               size_t eepromAddr, size_t len);


/**
 * Update one byte of data in the eeprom, target system specific function.
 *
 * Function is used by automatic storage. It updates byte in eeprom only if
 * differs from data.
 *
 * @param storageModule Pointer to storage module.
 * @param data Data byte to be written
 * @param eepromAddr Address in eeprom, from where data will be updated.
 *
 * @return true if write was successful or false, if still waiting previous
 * data to finish writing.
 */
bool_t CO_eeprom_updateByte(void *storageModule, uint8_t data,
                            size_t eepromAddr);


/** @} */ /* CO_storage_eeprom */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_EEPROM_H */
