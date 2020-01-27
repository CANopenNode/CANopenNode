/**
 * Eeprom object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        eeprom.h
 * @ingroup     CO_eeprom
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
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


#ifndef EEPROM_H
#define EEPROM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_eeprom Nonvolatile storage
 * @ingroup CO_CANopen
 * @{
 *
 * Storage of nonvolatile CANopen variables into the eeprom.
 */


/**
 * Eeprom object.
 */
typedef struct{
    uint8_t     *OD_EEPROMAddress;      /**< From CO_EE_init_1() */
    uint32_t     OD_EEPROMSize;         /**< From CO_EE_init_1() */
    uint8_t     *OD_ROMAddress;         /**< From CO_EE_init_1() */
    uint32_t     OD_ROMSize;            /**< From CO_EE_init_1() */
    uint32_t     OD_EEPROMCurrentIndex; /**< Internal variable controls the OD_EEPROM vrite */
    bool_t       OD_EEPROMWriteEnable;  /**< Writing to EEPROM is enabled */
}CO_EE_t;


/**
 * First part of eeprom initialization. Called after microcontroller reset.
 *
 * @param ee This object will be initialized.
 * @param OD_EEPROMAddress Address of OD_EEPROM structure from object dictionary.
 * @param OD_EEPROMSize Size of OD_EEPROM structure from object dictionary.
 * @param OD_ROMAddress Address of OD_ROM structure from object dictionary.
 * @param OD_ROMSize Size of OD_ROM structure from object dictionary.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_DATA_CORRUPT (Data in eeprom corrupt) or
 * CO_ERROR_CRC (CRC from MBR does not match the CRC of OD_ROM block in eeprom).
 */
CO_ReturnError_t CO_EE_init_1(
        CO_EE_t                *ee,
        uint8_t                *OD_EEPROMAddress,
        uint32_t                OD_EEPROMSize,
        uint8_t                *OD_ROMAddress,
        uint32_t                OD_ROMSize);


/**
 * Second part of eeprom initialization. Called after CANopen communication reset.
 *
 * @param ee          - This object.
 * @param eeStatus    - Return value from CO_EE_init_1().
 * @param SDO         - SDO object.
 * @param em          - Emergency object.
 */
void CO_EE_init_2(
        CO_EE_t                *ee,
        CO_ReturnError_t        eeStatus,
        CO_SDO_t               *SDO,
        CO_EM_t                *em);


/**
 * Process eeprom object.
 *
 * Function must be called cyclically. It strores variables from OD_EEPROM data
 * block into eeprom byte by byte (only if values are different).
 *
 * @param ee This object.
 */
void CO_EE_process(CO_EE_t *ee);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
