/*
 * Eeprom object for BECK SC243 computer.
 *
 * @file        eeprom.h
 * @version     SVN: \$Id$
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <http://canopennode.sourceforge.net>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef EEPROM_H
#define EEPROM_H


/* For documentation see file genericDriver/eeprom.h */


/*
 * SC243 specific
 *
 * Usage of device file system or SRAM for storing non-volatile variables.
 *
 * Two blocks of CANopen Object Dictionary data are stored as non-volatile:
 * OD_EEPROM - Stored is in internal battery powered SRAM from address 0. Data
 *             are stored automatically on change. No data corruption control
 *             is made. Data are load on startup.
 * OD_ROM    - Stored in file named "OD_ROM01.dat". Data integrity is
 *             verified with CRC.
 *             Data are stored on special CANopen command - Writing 0x65766173
 *             into Object dictionary (index 1010, subindex 1). Default values
 *             are restored after reset, if writing 0x64616F6C into (1011, 1).
 *             Backup is stored to "OD_ROM01.old".
 */


/* Filename */
#ifndef EE_ROM_FILENAME
    #define EE_ROM_FILENAME       "OD_ROM01.dat"
#endif
#ifndef EE_ROM_FILENAME_OLD
    #define EE_ROM_FILENAME_OLD   "OD_ROM01.old"
#endif


/* Eeprom object */
typedef struct{
    uint32_t           *OD_EEPROMAddress;
    uint32_t            OD_EEPROMSize;
    uint8_t            *OD_ROMAddress;
    uint32_t            OD_ROMSize;
    uint32_t           *pSRAM;          /* SC243 specific: Pointer to start
                                           address of the battery powered SRAM */
    uint32_t            OD_EEPROMCurrentIndex;
    CO_bool_t           OD_EEPROMWriteEnable;
}CO_EE_t;


/* First part of eeprom initialization.
 *  @param SRAMAddress Address of battery powered SRAM memory.
 */
CO_ReturnError_t CO_EE_init_1(
        CO_EE_t                *ee,
        uint8_t                *SRAMAddress,
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
