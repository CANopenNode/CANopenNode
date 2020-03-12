/**
 * CANopen Object Dictionary storage object for Linux SocketCAN.
 *
 * @file        CO_OD_storage.h
 * @ingroup     CO_socketCAN_OD_storage
 * @author      Janez Paternoster
 * @copyright   2015 - 2020 Janez Paternoster
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


#ifndef CO_OD_STORAGE_H
#define CO_OD_STORAGE_H


#include "301/CO_driver.h"
#include "301/CO_SDOserver.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_socketCAN_OD_storage OD storage
 * @ingroup CO_socketCAN
 * @{
 *
 * Object Dictionary storage implementation for CANopenNode on Linux
 */


/**
 * Callback for use inside @ref CO_OD_configure() function for OD object 1010
 */
CO_SDO_abortCode_t CO_ODF_1010(CO_ODF_arg_t *ODF_arg);

/**
 * Callback for use inside @ref CO_OD_configure() function for OD object 1011
 */
CO_SDO_abortCode_t CO_ODF_1011(CO_ODF_arg_t *ODF_arg);


/**
 * Save memory block to a file.
 *
 * Function renames current file to filename.old, copies contents from odAddress
 * to filename, adds two bytes of CRC code. It then verifies the written file and
 * in case of errors sets back the old file and returns error.
 *
 * Function is used with CANopen OD object at index 1010.
 *
 * @param odAddress Address of the memory block, which will be stored.
 * @param odSize Size of the above memory block.
 * @param filename Name of the file, where data will be stored.
 *
 * @return 0 on success, -1 on error.
 */
int CO_OD_storage_saveSecure(
        uint8_t                *odAddress,
        uint32_t                odSize,
        char                   *filename);


/**
 * Remove OD storage file.
 *
 * Function renames current file to filename.old, then creates empty file and
 * writes two bytes "-\n" to it. When program will start next time, default values
 * are used for Object Dictionary. In case of error in renaming to .old it
 * keeps the original file and returns error.
 *
 * Writing data to file is secured with mutex CO_LOCK_OD.
 *
 * Function is used with CANopen OD object at index 1011.
 *
 * @param filename Name of the file.
 *
 * @return 0 on success, -1 on error.
 */
int CO_OD_storage_restoreSecure(char *filename);


/**
 * Object Dictionary storage object.
 *
 * Object is used with CANopen OD objects at index 1010 and 1011.
 */
typedef struct {
    uint8_t    *odAddress;      /**< From CO_OD_storage_init() */
    uint32_t    odSize;         /**< From CO_OD_storage_init() */
    char       *filename;       /**< From CO_OD_storage_init() */
    /** If CO_OD_storage_autoSave() is used, file stays opened and fp is stored here. */
    FILE       *fp;
    uint32_t    lastSavedUs;    /**< used with CO_OD_storage_autoSave. */
} CO_OD_storage_t;


/**
 * Initialize OD storage object and load data from file.
 *
 * Called after program startup. Load storage file and copy data to Object
 * Dictionary variables.
 *
 * @param odStor This object will be initialized.
 * @param odAddress Address of the memory block from Object dictionary, where data will be copied.
 * @param odSize Size of the above memory block.
 * @param filename Name of the file, where data are stored.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_DATA_CORRUPT (Data in file corrupt),
 * CO_ERROR_CRC (CRC from MBR does not match the CRC of OD_ROM block in file),
 * CO_ERROR_ILLEGAL_ARGUMENT or CO_ERROR_OUT_OF_MEMORY (malloc failed).
 */
CO_ReturnError_t CO_OD_storage_init(
        CO_OD_storage_t        *odStor,
        uint8_t                *odAddress,
        uint32_t                odSize,
        char                   *filename);


/**
 * Automatically save memory block if differs from file.
 *
 * Should be called cyclically by program. It first verifies, if memory block
 * differs from file and if it does, it saves it to file with two additional
 * CRC bytes. File remains opened.
 *
 * @param odStor OD storage object.
 * @param timer1usDiff Time difference in microseconds since last call.
 * @param delay_us Delay (inhibit) time between writes to disk in microseconds
 * (60000 for example).
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_DATA_CORRUPT (Data in file
 * corrupt), CO_ERROR_ILLEGAL_ARGUMENT or CO_ERROR_OUT_OF_MEMORY (malloc failed).
 */
CO_ReturnError_t CO_OD_storage_autoSave(
        CO_OD_storage_t        *odStor,
        uint32_t                timer1usDiff,
        uint32_t                delay_us);


/**
 * Closes file opened by CO_OD_storage_autoSave.
 *
 * @param odStor OD storage object.
 */
void CO_OD_storage_autoSaveClose(CO_OD_storage_t *odStor);

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_OD_STORAGE_H */
