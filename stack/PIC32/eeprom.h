/*
 * Eeprom object for Microchip PIC32MX microcontroller.
 *
 * @file        eeprom.h
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
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
