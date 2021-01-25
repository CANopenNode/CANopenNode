/**
 * CANopen data storage object
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
 * @defgroup CO_storage Data storage
 * @ingroup CO_CANopen_301
 * @{
 *
 * CANopen data storage - connection to Object Dictionary
 */


/**
 * Data storage object for one entry.
 *
 * Object is defined by application and registered with
 * @ref CO_storage_entry_init() function.
 */
typedef struct {
    /** Address of data to store */
    void *addr;
    /** Length of data to store */
    OD_size_t len;
    /** Object defined by application, passed to store and restore functions. */
    void *object;
    /** Sub index in OD objects 1010 and 1011, from 2 to 254. Writing
     * 0x65766173 to 1010,subIndexOD will store data to non-volatile memory.
     * Writing 0x64616F6C to 1011,subIndexOD will restore default data. */
    uint8_t subIndexOD;
    /**
     * Pointer to externally defined function, which will store data from addr.
     *
     * @param object object from above.
     * @param addr Address form above.
     * @param len Length from above.
     *
     * @return Value from @ref ODR_t, "ODR_OK" in case of success, "ODR_HW" in
     * case of error in hardware.
     */
    ODR_t (*store)(void *object, void *addr, OD_size_t len);
    /**
     * Pointer to externally defined function, which will restore data to addr.
     *
     * For description of arguments see above.
     */
    ODR_t (*restore)(void *object, void *addr, OD_size_t len);
    /** Pointer to next entry, initialized inside @ref CO_storage_entry_init()
     * function. */
    void *nextEntry;
} CO_storage_entry_t;


/**
 * Data storage object.
 *
 * Object is used with CANopen OD objects at index 1010 and 1011.
 */
typedef struct {
    /** Extension for OD object */
    OD_extension_t OD_1010_extension;
    /** Extension for OD object */
    OD_extension_t OD_1011_extension;
    /** Null on the beginning, @ref CO_storage_entry_init() adds objects here
     * and creates linked list. */
    CO_storage_entry_t *firstEntry;
    /** From @ref CO_storage_init(). */
    bool_t sub1_all;
} CO_storage_t;


/**
 * Pre-initialize data storage object.
 *
 * This function must be called before first @ref CO_storage_entry_init()
 * function call. It is used inside @ref CO_new().
 *
 * @param storage This object will be pre-initialized.
 */
static inline void CO_storage_pre_init(CO_storage_t *storage) {
    if (storage != NULL) storage->firstEntry = NULL;
}


/**
 * Initialize data storage object for usage with OD objects 1010 and 1011.
 *
 * Function is used inside @ref CO_CANopenInit(). It does not erase
 * pre-configured entries from CO_storage_entry_init() calls.
 *
 * @param storage This object will be initialized.
 * @param OD_1010_StoreParameters OD entry for 0x1010 -"Store parameters".
 * @param OD_1011_RestoreDefaultParameters OD entry for 0x1011 -"Restore default
 * parameters". Entry is optional, may be NULL.
 * @param sub1_all If true, then writing to sub-index 1 of 1010 and 1011 will
 * store/restore all data. This is default in CANopen.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT or CO_ERROR_OD_PARAMETERS.
 */
CO_ReturnError_t CO_storage_init(CO_storage_t *storage,
                                 OD_entry_t *OD_1010_StoreParameters,
                                 OD_entry_t *OD_1011_RestoreDefaultParameters,
                                 bool_t sub1_all,
                                 uint32_t *errInfo);


/**
 * Initialize / add one entry into data storage object
 *
 * This function may be called by application one or several times after
 * @ref CO_storage_pre_init() and before @ref CO_CANopenInit(). If entry with
 * the same CO_storage_entry_t->subIndexOD as in newEntry already exists in
 * storage object, then that entry in storage object will be replaced with
 * newEntry. So it is not a problem to call this function multiple times with
 * the same subIndexOD in CANopen communication reset section.
 *
 * @param storage Data storage object.
 * @param newEntry New entry for data storage object. It must be initialized
 * externally. This object must exist permanently. To disable entry, set store
 * and restore function pointers to NULL.
 *
 * @return CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_storage_entry_init(CO_storage_t *storage,
                                       CO_storage_entry_t *newEntry);


/** @} */ /* CO_storage */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */

#endif /* CO_STORAGE_H */
