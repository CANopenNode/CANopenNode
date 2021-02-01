/**
 * CANopen data storage object for Linux
 *
 * @file        CO_storageLinux.h
 * @ingroup     CO_storageLinux
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

#ifndef CO_STORAGE_LINUX_H
#define CO_STORAGE_LINUX_H

#include "301/CO_storage.h"
#include <stdio.h>

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_storageLinux Data storage with Linux
 * @ingroup CO_socketCAN
 * @{
 *
 * Data initialize, store and restore functions with Linux, see @ref CO_storage
 */


/**
 * Initialize data storage object (Linux specific)
 *
 * This function should be called by application after the program startup,
 * before @ref CO_CANopenInit(). This function initializes storage object,
 * OD extensions on objects 1010 and 1011, reads data from file, verifies them
 * and writes data to addresses specified inside entries. This function
 * internally calls @ref CO_storage_init().
 *
 * @param storage This object will be initialized. It must be defined by
 * application and must exist permanently.
 * @param OD_1010_StoreParameters OD entry for 0x1010 -"Store parameters".
 * Entry is optional, may be NULL.
 * @param OD_1011_RestoreDefaultParam OD entry for 0x1011 -"Restore default
 * parameters". Entry is optional, may be NULL.
 * @param entries Pointer to array of storage entries, see @ref CO_storage_init.
 * @param entriesCount Count of storage entries
 * @param [out] storageInitError If function returns CO_ERROR_DATA_CORRUPT,
 * then this variable contains a bit mask from subIndexOD values, where data
 * was not properly initialized. If other error, then this variable contains
 * index or erroneous entry.
 *
 * @return CO_ERROR_NO, CO_ERROR_DATA_CORRUPT if data can not be initialized,
 * CO_ERROR_ILLEGAL_ARGUMENT or CO_ERROR_OUT_OF_MEMORY.
 */
CO_ReturnError_t CO_storageLinux_init(CO_storage_t *storage,
                                      OD_entry_t *OD_1010_StoreParameters,
                                      OD_entry_t *OD_1011_RestoreDefaultParam,
                                      CO_storage_entry_t *entries,
                                      uint8_t entriesCount,
                                      uint32_t *storageInitError);


/**
 * Automatically save data if differs from previous call.
 *
 * Should be called cyclically by program. Each interval it verifies, if crc
 * checksum of data differs from previous checksum. If it does, data are saved
 * into pre-opened file.
 *
 * @param storage This object
 * @param closeFiles If true, then all files will be closed. Use on end of the
 * program.
 *
 * @return 0 on success or bit mask from subIndexOD values, where data was not
 * able to be saved.
 */
uint32_t CO_storageLinux_auto_process(CO_storage_t *storage,
                                      bool_t closeFiles);

/** @} */ /* CO_storageLinux */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */

#endif /* CO_STORAGE_LINUX_H */
