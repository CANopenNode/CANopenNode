/*
 * CANopen data storage object (blank example)
 *
 * @file        CO_storageBlank.h
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

#ifndef CO_STORAGE_BLANK_H
#define CO_STORAGE_BLANK_H

#include "storage/CO_storage.h"

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/* This is very basic example of implementing (object dictionary) data storage.
 * Data storage is target specific. CO_storageBlank.h and .c files only shows
 * the basic principle, but does nothing. For complete example of storage see:
 * - CANopenPIC/PIC32 uses eeprom with CANopenNode/storage/CO_storage.h/.c,
 *   CANopenNode/storage/CO_storageEeprom.h/.c, CANopenNode/storage/CO_eeprom.h
 *   and CANopenPIC/PIC32/CO_eepromPIC32.c files.
 * - CANopenLinux uses file system with CANopenNode/storage/CO_storage.h/.c and
 *   CANopenLinux/CO_storageLinux.h files.
 */

CO_ReturnError_t CO_storageBlank_init(CO_storage_t *storage,
                                      CO_CANmodule_t *CANmodule,
                                      OD_entry_t *OD_1010_StoreParameters,
                                      OD_entry_t *OD_1011_RestoreDefaultParam,
                                      CO_storage_entry_t *entries,
                                      uint8_t entriesCount,
                                      uint32_t *storageInitError);

uint32_t CO_storageBlank_auto_process(CO_storage_t *storage,
                                      bool_t closeFiles);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */

#endif /* CO_STORAGE_BLANK_H */
