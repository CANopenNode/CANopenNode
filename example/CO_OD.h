/*
 * CANopen Object Dictionary.
 *
 * This file was automatically generated with CANopenNode Object
 * Dictionary Editor. DON'T EDIT THIS FILE MANUALLY !!!!
 * Object Dictionary Editor is currently an older, but functional web
 * application. For more info see See 'Object_Dictionary_Editor/about.html' in
 * <http://sourceforge.net/p/canopennode/code_complete/ci/master/tree/>
 * For more information on CANopen Object Dictionary see <CO_SDO.h>.
 *
 * @file        CO_OD.h
 * @author      Janez Paternoster
 * @copyright   2010 - 2016 Janez Paternoster
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


#ifndef CO_OD_H
#define CO_OD_H


/*******************************************************************************
   CANopen DATA DYPES
*******************************************************************************/
   typedef uint8_t      UNSIGNED8;
   typedef uint16_t     UNSIGNED16;
   typedef uint32_t     UNSIGNED32;
   typedef uint64_t     UNSIGNED64;
   typedef int8_t       INTEGER8;
   typedef int16_t      INTEGER16;
   typedef int32_t      INTEGER32;
   typedef int64_t      INTEGER64;
   typedef float32_t    REAL32;
   typedef float64_t    REAL64;
   typedef char_t       VISIBLE_STRING;
   typedef oChar_t      OCTET_STRING;
   typedef domain_t     DOMAIN;


/*******************************************************************************
   FILE INFO:
      FileName:     IO Example
      FileVersion:  -
      CreationTime: 18:04:29
      CreationDate: 2016-03-25
      CreatedBy:    JP
*******************************************************************************/


/*******************************************************************************
   DEVICE INFO:
      VendorName:     CANopenNode
      VendorNumber:   0
      ProductName:    CANopenNode
      ProductNumber:  0
*******************************************************************************/


/*******************************************************************************
   FEATURES
*******************************************************************************/
   #define CO_NO_SYNC                     1   //Associated objects: 1005, 1006, 1007, 2103, 2104
   #define CO_NO_EMERGENCY                1   //Associated objects: 1014, 1015
   #define CO_NO_SDO_SERVER               1   //Associated objects: 1200
   #define CO_NO_SDO_CLIENT               0   
   #define CO_NO_RPDO                     4   //Associated objects: 1400, 1401, 1402, 1403, 1600, 1601, 1602, 1603
   #define CO_NO_TPDO                     4   //Associated objects: 1800, 1801, 1802, 1803, 1A00, 1A01, 1A02, 1A03
   #define CO_NO_NMT_MASTER               0   
   #define CO_NO_TRACE                    0   


/*******************************************************************************
   OBJECT DICTIONARY
*******************************************************************************/
   #define CO_OD_NoOfElements             55


/*******************************************************************************
   TYPE DEFINITIONS FOR RECORDS
*******************************************************************************/
/*1018      */ typedef struct{
               UNSIGNED8      maxSubIndex;
               UNSIGNED32     vendorID;
               UNSIGNED32     productCode;
               UNSIGNED32     revisionNumber;
               UNSIGNED32     serialNumber;
               }              OD_identity_t;

/*1200[1]   */ typedef struct{
               UNSIGNED8      maxSubIndex;
               UNSIGNED32     COB_IDClientToServer;
               UNSIGNED32     COB_IDServerToClient;
               }              OD_SDOServerParameter_t;

/*1400[4]   */ typedef struct{
               UNSIGNED8      maxSubIndex;
               UNSIGNED32     COB_IDUsedByRPDO;
               UNSIGNED8      transmissionType;
               }              OD_RPDOCommunicationParameter_t;

/*1600[4]   */ typedef struct{
               UNSIGNED8      numberOfMappedObjects;
               UNSIGNED32     mappedObject1;
               UNSIGNED32     mappedObject2;
               UNSIGNED32     mappedObject3;
               UNSIGNED32     mappedObject4;
               UNSIGNED32     mappedObject5;
               UNSIGNED32     mappedObject6;
               UNSIGNED32     mappedObject7;
               UNSIGNED32     mappedObject8;
               }              OD_RPDOMappingParameter_t;

/*1800[4]   */ typedef struct{
               UNSIGNED8      maxSubIndex;
               UNSIGNED32     COB_IDUsedByTPDO;
               UNSIGNED8      transmissionType;
               UNSIGNED16     inhibitTime;
               UNSIGNED8      compatibilityEntry;
               UNSIGNED16     eventTimer;
               UNSIGNED8      SYNCStartValue;
               }              OD_TPDOCommunicationParameter_t;

/*1A00[4]   */ typedef struct{
               UNSIGNED8      numberOfMappedObjects;
               UNSIGNED32     mappedObject1;
               UNSIGNED32     mappedObject2;
               UNSIGNED32     mappedObject3;
               UNSIGNED32     mappedObject4;
               UNSIGNED32     mappedObject5;
               UNSIGNED32     mappedObject6;
               UNSIGNED32     mappedObject7;
               UNSIGNED32     mappedObject8;
               }              OD_TPDOMappingParameter_t;

/*2120      */ typedef struct{
               UNSIGNED8      maxSubIndex;
               INTEGER64      I64;
               UNSIGNED64     U64;
               REAL32         R32;
               REAL64         R64;
               DOMAIN         domain;
               }              OD_testVar_t;

/*2130      */ typedef struct{
               UNSIGNED8      maxSubIndex;
               VISIBLE_STRING string[30];
               UNSIGNED64     epochTimeBaseMs;
               UNSIGNED32     epochTimeOffsetMs;
               }              OD_time_t;


/*******************************************************************************
   STRUCTURES FOR VARIABLES IN DIFFERENT MEMORY LOCATIONS
*******************************************************************************/
#define  CO_OD_FIRST_LAST_WORD     0x55 //Any value from 0x01 to 0xFE. If changed, EEPROM will be reinitialized.

/***** Structure for RAM variables ********************************************/
struct sCO_OD_RAM{
               UNSIGNED32     FirstWord;

/*1001      */ UNSIGNED8      errorRegister;
/*1002      */ UNSIGNED32     manufacturerStatusRegister;
/*1003      */ UNSIGNED32     preDefinedErrorField[8];
/*1010      */ UNSIGNED32     storeParameters[1];
/*1011      */ UNSIGNED32     restoreDefaultParameters[1];
/*2100      */ OCTET_STRING   errorStatusBits[10];
/*2103      */ UNSIGNED16     SYNCCounter;
/*2104      */ UNSIGNED16     SYNCTime;
/*2107      */ UNSIGNED16     performance[5];
/*2108      */ INTEGER16      temperature[1];
/*2109      */ INTEGER16      voltage[1];
/*2110      */ INTEGER32      variableInt32[16];
/*2120      */ OD_testVar_t   testVar;
/*2130      */ OD_time_t      time;
/*6000      */ UNSIGNED8      readInput8Bit[8];
/*6200      */ UNSIGNED8      writeOutput8Bit[8];
/*6401      */ INTEGER16      readAnalogueInput16Bit[12];
/*6411      */ INTEGER16      writeAnalogueOutput16Bit[8];

               UNSIGNED32     LastWord;
};

/***** Structure for EEPROM variables *****************************************/
struct sCO_OD_EEPROM{
               UNSIGNED32     FirstWord;

/*2106      */ UNSIGNED32     powerOnCounter;
/*2112      */ INTEGER32      variableNVInt32[16];

               UNSIGNED32     LastWord;
};


/***** Structure for ROM variables ********************************************/
struct sCO_OD_ROM{
               UNSIGNED32     FirstWord;

/*1000      */ UNSIGNED32     deviceType;
/*1005      */ UNSIGNED32     COB_ID_SYNCMessage;
/*1006      */ UNSIGNED32     communicationCyclePeriod;
/*1007      */ UNSIGNED32     synchronousWindowLength;
/*1008      */ VISIBLE_STRING manufacturerDeviceName[11];
/*1009      */ VISIBLE_STRING manufacturerHardwareVersion[4];
/*100A      */ VISIBLE_STRING manufacturerSoftwareVersion[4];
/*1014      */ UNSIGNED32     COB_ID_EMCY;
/*1015      */ UNSIGNED16     inhibitTimeEMCY;
/*1016      */ UNSIGNED32     consumerHeartbeatTime[4];
/*1017      */ UNSIGNED16     producerHeartbeatTime;
/*1018      */ OD_identity_t  identity;
/*1019      */ UNSIGNED8      synchronousCounterOverflowValue;
/*1029      */ UNSIGNED8      errorBehavior[6];
/*1200[1]   */ OD_SDOServerParameter_t SDOServerParameter[1];
/*1400[4]   */ OD_RPDOCommunicationParameter_t RPDOCommunicationParameter[4];
/*1600[4]   */ OD_RPDOMappingParameter_t RPDOMappingParameter[4];
/*1800[4]   */ OD_TPDOCommunicationParameter_t TPDOCommunicationParameter[4];
/*1A00[4]   */ OD_TPDOMappingParameter_t TPDOMappingParameter[4];
/*1F80      */ UNSIGNED32     NMTStartup;
/*2101      */ UNSIGNED8      CANNodeID;
/*2102      */ UNSIGNED16     CANBitRate;
/*2111      */ INTEGER32      variableROMInt32[16];

               UNSIGNED32     LastWord;
};


/***** Declaration of Object Dictionary variables *****************************/
extern struct sCO_OD_RAM CO_OD_RAM;

extern struct sCO_OD_EEPROM CO_OD_EEPROM;

extern struct sCO_OD_ROM CO_OD_ROM;


/*******************************************************************************
   ALIASES FOR OBJECT DICTIONARY VARIABLES
*******************************************************************************/
/*1000, Data Type: UNSIGNED32 */
      #define OD_deviceType                              CO_OD_ROM.deviceType

/*1001, Data Type: UNSIGNED8 */
      #define OD_errorRegister                           CO_OD_RAM.errorRegister

/*1002, Data Type: UNSIGNED32 */
      #define OD_manufacturerStatusRegister              CO_OD_RAM.manufacturerStatusRegister

/*1003, Data Type: UNSIGNED32, Array[8] */
      #define OD_preDefinedErrorField                    CO_OD_RAM.preDefinedErrorField
      #define ODL_preDefinedErrorField_arrayLength       8

/*1005, Data Type: UNSIGNED32 */
      #define OD_COB_ID_SYNCMessage                      CO_OD_ROM.COB_ID_SYNCMessage

/*1006, Data Type: UNSIGNED32 */
      #define OD_communicationCyclePeriod                CO_OD_ROM.communicationCyclePeriod

/*1007, Data Type: UNSIGNED32 */
      #define OD_synchronousWindowLength                 CO_OD_ROM.synchronousWindowLength

/*1008, Data Type: VISIBLE_STRING, Array[11] */
      #define OD_manufacturerDeviceName                  CO_OD_ROM.manufacturerDeviceName
      #define ODL_manufacturerDeviceName_stringLength    11

/*1009, Data Type: VISIBLE_STRING, Array[4] */
      #define OD_manufacturerHardwareVersion             CO_OD_ROM.manufacturerHardwareVersion
      #define ODL_manufacturerHardwareVersion_stringLength 4

/*100A, Data Type: VISIBLE_STRING, Array[4] */
      #define OD_manufacturerSoftwareVersion             CO_OD_ROM.manufacturerSoftwareVersion
      #define ODL_manufacturerSoftwareVersion_stringLength 4

/*1010, Data Type: UNSIGNED32, Array[1] */
      #define OD_storeParameters                         CO_OD_RAM.storeParameters
      #define ODL_storeParameters_arrayLength            1
      #define ODA_storeParameters_saveAllParameters      0

/*1011, Data Type: UNSIGNED32, Array[1] */
      #define OD_restoreDefaultParameters                CO_OD_RAM.restoreDefaultParameters
      #define ODL_restoreDefaultParameters_arrayLength   1
      #define ODA_restoreDefaultParameters_restoreAllDefaultParameters 0

/*1014, Data Type: UNSIGNED32 */
      #define OD_COB_ID_EMCY                             CO_OD_ROM.COB_ID_EMCY

/*1015, Data Type: UNSIGNED16 */
      #define OD_inhibitTimeEMCY                         CO_OD_ROM.inhibitTimeEMCY

/*1016, Data Type: UNSIGNED32, Array[4] */
      #define OD_consumerHeartbeatTime                   CO_OD_ROM.consumerHeartbeatTime
      #define ODL_consumerHeartbeatTime_arrayLength      4

/*1017, Data Type: UNSIGNED16 */
      #define OD_producerHeartbeatTime                   CO_OD_ROM.producerHeartbeatTime

/*1018, Data Type: OD_identity_t */
      #define OD_identity                                CO_OD_ROM.identity

/*1019, Data Type: UNSIGNED8 */
      #define OD_synchronousCounterOverflowValue         CO_OD_ROM.synchronousCounterOverflowValue

/*1029, Data Type: UNSIGNED8, Array[6] */
      #define OD_errorBehavior                           CO_OD_ROM.errorBehavior
      #define ODL_errorBehavior_arrayLength              6
      #define ODA_errorBehavior_communication            0
      #define ODA_errorBehavior_communicationOther       1
      #define ODA_errorBehavior_communicationPassive     2
      #define ODA_errorBehavior_generic                  3
      #define ODA_errorBehavior_deviceProfile            4
      #define ODA_errorBehavior_manufacturerSpecific     5

/*1200[1], Data Type: OD_SDOServerParameter_t, Array[1] */
      #define OD_SDOServerParameter                      CO_OD_ROM.SDOServerParameter

/*1400[4], Data Type: OD_RPDOCommunicationParameter_t, Array[4] */
      #define OD_RPDOCommunicationParameter              CO_OD_ROM.RPDOCommunicationParameter

/*1600[4], Data Type: OD_RPDOMappingParameter_t, Array[4] */
      #define OD_RPDOMappingParameter                    CO_OD_ROM.RPDOMappingParameter

/*1800[4], Data Type: OD_TPDOCommunicationParameter_t, Array[4] */
      #define OD_TPDOCommunicationParameter              CO_OD_ROM.TPDOCommunicationParameter

/*1A00[4], Data Type: OD_TPDOMappingParameter_t, Array[4] */
      #define OD_TPDOMappingParameter                    CO_OD_ROM.TPDOMappingParameter

/*1F80, Data Type: UNSIGNED32 */
      #define OD_NMTStartup                              CO_OD_ROM.NMTStartup

/*2100, Data Type: OCTET_STRING, Array[10] */
      #define OD_errorStatusBits                         CO_OD_RAM.errorStatusBits
      #define ODL_errorStatusBits_stringLength           10

/*2101, Data Type: UNSIGNED8 */
      #define OD_CANNodeID                               CO_OD_ROM.CANNodeID

/*2102, Data Type: UNSIGNED16 */
      #define OD_CANBitRate                              CO_OD_ROM.CANBitRate

/*2103, Data Type: UNSIGNED16 */
      #define OD_SYNCCounter                             CO_OD_RAM.SYNCCounter

/*2104, Data Type: UNSIGNED16 */
      #define OD_SYNCTime                                CO_OD_RAM.SYNCTime

/*2106, Data Type: UNSIGNED32 */
      #define OD_powerOnCounter                          CO_OD_EEPROM.powerOnCounter

/*2107, Data Type: UNSIGNED16, Array[5] */
      #define OD_performance                             CO_OD_RAM.performance
      #define ODL_performance_arrayLength                5
      #define ODA_performance_cyclesPerSecond            0
      #define ODA_performance_timerCycleTime             1
      #define ODA_performance_timerCycleMaxTime          2
      #define ODA_performance_mainCycleTime              3
      #define ODA_performance_mainCycleMaxTime           4

/*2108, Data Type: INTEGER16, Array[1] */
      #define OD_temperature                             CO_OD_RAM.temperature
      #define ODL_temperature_arrayLength                1
      #define ODA_temperature_mainPCB                    0

/*2109, Data Type: INTEGER16, Array[1] */
      #define OD_voltage                                 CO_OD_RAM.voltage
      #define ODL_voltage_arrayLength                    1
      #define ODA_voltage_mainPCBSupply                  0

/*2110, Data Type: INTEGER32, Array[16] */
      #define OD_variableInt32                           CO_OD_RAM.variableInt32
      #define ODL_variableInt32_arrayLength              16

/*2111, Data Type: INTEGER32, Array[16] */
      #define OD_variableROMInt32                        CO_OD_ROM.variableROMInt32
      #define ODL_variableROMInt32_arrayLength           16

/*2112, Data Type: INTEGER32, Array[16] */
      #define OD_variableNVInt32                         CO_OD_EEPROM.variableNVInt32
      #define ODL_variableNVInt32_arrayLength            16

/*2120, Data Type: OD_testVar_t */
      #define OD_testVar                                 CO_OD_RAM.testVar

/*2130, Data Type: OD_time_t */
      #define OD_time                                    CO_OD_RAM.time

/*6000, Data Type: UNSIGNED8, Array[8] */
      #define OD_readInput8Bit                           CO_OD_RAM.readInput8Bit
      #define ODL_readInput8Bit_arrayLength              8

/*6200, Data Type: UNSIGNED8, Array[8] */
      #define OD_writeOutput8Bit                         CO_OD_RAM.writeOutput8Bit
      #define ODL_writeOutput8Bit_arrayLength            8

/*6401, Data Type: INTEGER16, Array[12] */
      #define OD_readAnalogueInput16Bit                  CO_OD_RAM.readAnalogueInput16Bit
      #define ODL_readAnalogueInput16Bit_arrayLength     12

/*6411, Data Type: INTEGER16, Array[8] */
      #define OD_writeAnalogueOutput16Bit                CO_OD_RAM.writeAnalogueOutput16Bit
      #define ODL_writeAnalogueOutput16Bit_arrayLength   8


#endif

