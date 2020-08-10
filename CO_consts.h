/*
 * Main CANopen stack file. It combines Object dictionary (CO_OD) and all other
 * CANopen source files. Configuration information are read from CO_OD.h file.
 *
 * @file        CANopen.c
 * @ingroup     CO_CANopen
 * @author      Janez Paternoster
 * @copyright   2010 - 2020 Janez Paternoster
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

#ifndef CANOPENNODE_CO_CONSTS_H_
#define CANOPENNODE_CO_CONSTS_H_
#include <stdint.h>
#include <stddef.h>
//#include <stdint.h>
typedef struct CO_consts_t_ {
	uint16_t NO_SYNC;                   //Associated objects: 1005-1007
	uint16_t NO_EMERGENCY;              //Associated objects: 1014, 1015
	uint16_t NO_TIME;                   //Associated objects: 1012, 1013
	uint16_t NO_SDO_SERVER;             //Associated objects: 1200-127F
	uint16_t NO_SDO_CLIENT;             //Associated objects: 1280-12FF
	uint16_t NO_LSS_SERVER;             //LSS Slave
	uint16_t NO_LSS_CLIENT;             //LSS Master
	uint16_t NO_RPDO;                   //Associated objects: 14xx, 16xx
	uint16_t NO_TPDO;                   //Associated objects: 18xx, 1Axx
	uint16_t NO_NMT_MASTER;
	uint16_t NO_TRACE;
	uint16_t consumerHeartbeatTime_arrayLength;
	uint16_t NO_SRDO;
	uint16_t NO_GFC;
	uint16_t OD_NoOfElements;
	uint16_t sizeof_OD_TPDOCommunicationParameter;
	uint16_t sizeof_OD_TPDOMappingParameter;
	uint16_t sizeof_OD_RPDOCommunicationParameter;
	uint16_t sizeof_OD_RPDOMappingParameter;
	uint8_t* errorRegister;
	uint32_t* preDefinedErrorField;
	uint8_t preDefinedErrorFieldSize;
} CO_consts_t;

#endif /* CANOPENNODE_CO_CONSTS_H_ */
