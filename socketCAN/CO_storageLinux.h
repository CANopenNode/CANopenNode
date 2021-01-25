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
 * Initialize / add one entry into data storage object (Linux specific)
 *
 * This function configures newEntry and calls @ref CO_storage_entry_init().
 * Then reads data from file, verifies and writes them to addr.
 *
 * @param storage Data storage object.
 * @param newEntry Data storage object for one entry. It will be configured and
 * must exist permanently.
 * @param addr Address of data to store
 * @param len Length of data to store
 * @param filename Name of the file, where data are stored.
 * @param subIndexOD Sub index in OD 1010 and 1011 ,see @ref CO_storage_entry_t
 *
 * @return CO_ERROR_NO, CO_ERROR_DATA_CORRUPT if memory can not be initialized,
 * CO_ERROR_ILLEGAL_ARGUMENT or CO_ERROR_OUT_OF_MEMORY.
 */
CO_ReturnError_t CO_storageLinux_entry_init(CO_storage_t *storage,
                                            CO_storage_entry_t *newEntry,
                                            void *addr,
                                            OD_size_t len,
                                            char *filename,
                                            uint8_t subIndexOD);


/**
 * Object for automatic data storage
 */
typedef struct {
    /** Address of data to store */
    void *addr;
    /** Length of data to store */
    OD_size_t len;
    /** CRC checksum of the data stored previously. */
    uint16_t crc;
    /** Pointer to file opened by @ref CO_storageLinux_auto_init() */
    FILE *fp;
} CO_storageLinux_auto_t;


/**
 * Initialize automatic storage with Linux
 *
 * Function in combination with @ref CO_storageLinux_auto_process() are
 * standalone functions. They are used for automatic data storage.
 *
 * This function configures storageAuto, then reads data from file. If data are
 * valid, then it writes them to addr. Function also open file pointer for
 * writing and keeps it open.
 *
 * @param storageAuto This object will be initialized.
 * @param addr Address of data to store
 * @param len Length of data to store
 * @param filename Name of the file, where data are stored.
 *
 * @return CO_ERROR_NO, CO_ERROR_DATA_CORRUPT if memory can not be initialized,
 * CO_ERROR_ILLEGAL_ARGUMENT, if file can not be opened for writing.
 */
CO_ReturnError_t CO_storageLinux_auto_init(CO_storageLinux_auto_t *storageAuto,
                                           void *addr,
                                           OD_size_t len,
                                           char *filename);


/**
 * Automatically save data if differs from previous call.
 *
 * Should be called cyclically by program. Each interval it verifies, if crc
 * checksum of data differs previous checksum. If it does, data are saved into
 * file, opened by CO_storageLinux_auto_init().
 *
 * @param storageAuto This object
 * @param closeFile If true, then data will be stored regardless interval and
 * file will be closed. Use on end of program.
 *
 * @return true, if data are unchanged or saved successfully.
 */
bool_t CO_storageLinux_auto_process(CO_storageLinux_auto_t *storageAuto,
                                    bool_t closeFile);

/** @} */ /* CO_storageLinux */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */

#endif /* CO_STORAGE_LINUX_H */
