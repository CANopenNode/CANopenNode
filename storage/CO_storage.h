/**
 * CANopen data storage base object
 *
 * @file        CO_storage.h
 * @ingroup     CO_storage
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

#ifndef CO_STORAGE_H
#define CO_STORAGE_H

#include "301/CO_driver.h"
#include "301/CO_ODinterface.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_STORAGE
#define CO_CONFIG_STORAGE (CO_CONFIG_STORAGE_ENABLE)
#endif

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_storage Data storage base
 * Base module for Data storage.
 *
 * @ingroup CO_CANopen_storage
 * @{
 *
 * CANopen provides OD objects 0x1010 and 0x1011 for control of storing and
 * restoring data. Data source is usually a group of variables inside object
 * dictionary, but it is not limited to OD.
 *
 * When object dictionary is generated (OD.h and OD.c files), OD variables are
 * grouped into structures according to 'Storage group' parameter.
 *
 * Autonomous data storing must be implemented target specific, if in use.
 *
 * ### OD object 0x1010 - Store parameters:
 * - Sub index 0: Highest sub-index supported
 * - Sub index 1: Save all parameters, UNSIGNED32
 * - Sub index 2: Save communication parameters, UNSIGNED32
 * - Sub index 3: Save application parameters, UNSIGNED32
 * - Sub index 4 - 127: Manufacturer specific, UNSIGNED32
 *
 * Sub-indexes 1 and above:
 * - Reading provides information about its storage functionality:
 *   - bit 0: If set, CANopen device saves parameters on command
 *   - bit 1: If set, CANopen device saves parameters autonomously
 * - Writing value 0x65766173 ('s','a','v','e' from LSB to MSB) stores
 *   corresponding data.
 *
 * ### OD object 0x1011 - Restore default parameters
 * - Sub index 0: Highest sub-index supported
 * - Sub index 1: Restore all default parameters, UNSIGNED32
 * - Sub index 2: Restore communication default parameters, UNSIGNED32
 * - Sub index 3: Restore application default parameters, UNSIGNED32
 * - Sub index 4 - 127: Manufacturer specific, UNSIGNED32
 *
 * Sub-indexes 1 and above:
 * - Reading provides information about its restoring capability:
 *   - bit 0: If set, CANopen device restores parameters
 * - Writing value 0x64616F6C ('l','o','a','d' from LSB to MSB) restores
 *   corresponding data.
 */


/**
 * Attributes (bit masks) for Data storage object.
 */
typedef enum {
    /** CANopen device saves parameters on OD 1010 command */
    CO_storage_cmd = 0x01,
    /** CANopen device saves parameters autonomously */
    CO_storage_auto = 0x02,
    /** CANopen device restores parameters on OD 1011 command  */
    CO_storage_restore = 0x04
} CO_storage_attributes_t;


/**
 * Data storage object.
 *
 * Object is used with CANopen OD objects at index 1010 and 1011.
 */
typedef struct {
    OD_extension_t OD_1010_extension; /**< Extension for OD object */
    OD_extension_t OD_1011_extension; /**< Extension for OD object */
    CO_CANmodule_t *CANmodule; /**< From CO_storage_init() */
    ODR_t (*store)(CO_storage_entry_t *entry,
                   CO_CANmodule_t *CANmodule); /**< From CO_storage_init() */
    ODR_t (*restore)(CO_storage_entry_t *entry,
                     CO_CANmodule_t *CANmodule); /**< From CO_storage_init() */
    CO_storage_entry_t *entries; /**< From CO_storage_init() */
    uint8_t entriesCount; /**< From CO_storage_init() */
    bool_t enabled; /**< true, if storage is enabled. Setting of this variable
    is implementation specific. */
} CO_storage_t;


/**
 * Initialize data storage object
 *
 * This function should be called by application after the program startup,
 * before @ref CO_CANopenInit(). This function initializes storage object and
 * OD extensions on objects 1010 and 1011. Function does not load stored data
 * on startup, because loading data is target specific.
 *
 * @param storage This object will be initialized. It must be defined by
 * application and must exist permanently.
 * @param CANmodule CAN device, used for @ref CO_LOCK_OD() macro.
 * @param OD_1010_StoreParameters OD entry for 0x1010 -"Store parameters".
 * Entry is optional, may be NULL.
 * @param OD_1011_RestoreDefaultParameters OD entry for 0x1011 -"Restore default
 * parameters". Entry is optional, may be NULL.
 * @param store Pointer to externally defined function, which will store data
 * specified by @ref CO_storage_entry_t. Function will be called when
 * OD variable 0x1010 will be written. Argument to function is entry, where
 * 'entry->subIndexOD' equals accessed subIndex. Function returns value from
 * @ref ODR_t : "ODR_OK" in case of success, "ODR_HW" in case of hardware error.
 * @param restore Same as 'store', but for restoring default data.
 * @param entries Pointer to array of storage entries. Array must be defined and
 * initialized by application and must exist permanently.
 * Structure @ref CO_storage_entry_t is target specific and must be defined by
 * CO_driver_target.h. See CO_driver.h for required parameters.
 * @param entriesCount Count of storage entries
 *
 * @return CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_storage_init(CO_storage_t *storage,
                                 CO_CANmodule_t *CANmodule,
                                 OD_entry_t *OD_1010_StoreParameters,
                                 OD_entry_t *OD_1011_RestoreDefaultParameters,
                                 ODR_t (*store)(CO_storage_entry_t *entry,
                                                CO_CANmodule_t *CANmodule),
                                 ODR_t (*restore)(CO_storage_entry_t *entry,
                                                  CO_CANmodule_t *CANmodule),
                                 CO_storage_entry_t *entries,
                                 uint8_t entriesCount);

/** @} */ /* CO_storage */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */

#endif /* CO_STORAGE_H */
