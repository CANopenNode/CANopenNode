/*
 * Eeprom object for Microchip PIC32MX microcontroller.
 *
 * @file        eeprom.h
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


/* For documentation see file drvTemplate/eeprom.h */


/*
 * PIC32MX specific
 *
 * 25LC128 eeprom from Microchip is used connected on SPI2A.
 *
 * Two blocks of CANopen Object Dictionary data are stored in eeprom:
 * OD_EEPROM     - Stored is from eeprom address 0. Data are stored automatically on
 *                 change. No data corruption control is made.
 * OD_ROM        - Stored from upper half eeprom address. Data are protected from
 *                 accidental write, can also be hardware protected. Data integrity
 *                 is verified with CRC.
 *                 Data are stored on special CANopen command - Writing 0x65766173
 *                 into Object dictionary (index 1010, subindex 1). Default values
 *                 are restored after reset, if writing 0x64616F6C into (1011, 1).
 */


/* Constants */
#define EE_SIZE         0x4000
#define EE_PAGE_SIZE    64


/* Master boot record is stored on the last page in eeprom */
typedef struct{
    uint32_t            CRC;            /* CRC code of the OD_ROM block */
    uint32_t            OD_EEPROMSize;  /* Size of OD_EEPROM block */
    uint32_t            OD_ROMSize;     /* Size of OD_ROM block */
}EE_MBR_t;


/* Eeprom object */
typedef struct{
    uint8_t            *OD_EEPROMAddress;
    uint32_t            OD_EEPROMSize;
    uint8_t            *OD_ROMAddress;
    uint32_t            OD_ROMSize;
    uint32_t            OD_EEPROMCurrentIndex;
    bool_t              OD_EEPROMWriteEnable;

}CO_EE_t;


/* First part of eeprom initialization.
 *
 * Allocate memory for object, configure SPI port for use with 25LCxxx, read
 * eeprom and store to OD_EEPROM and OD_ROM.
 */
CO_ReturnError_t CO_EE_init_1(
        CO_EE_t                *ee,
        uint8_t                *OD_EEPROMAddress,
        uint32_t                OD_EEPROMSize,
        uint8_t                *OD_ROMAddress,
        uint32_t                OD_ROMSize);


/* Second part of eeprom initialization. */
void CO_EE_init_2(
        CO_EE_t                *ee,
        CO_ReturnError_t        eeStatus,
        CO_SDO_t               *SDO,
        CO_EM_t                *em);


/* Process eeprom object. */
void CO_EE_process(CO_EE_t *ee);


#endif
