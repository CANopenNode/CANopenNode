/**
 * Device identificators for CANopenNode.
 *
 * @file        CO_identificators.h
 * @author      --
 * @copyright   2021 --
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

#ifndef CO_IDENTIFICATORS_H
#define CO_IDENTIFICATORS_H

#include "301/CO_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Function configures default CAN bitRate and CANopen nodeId. Further it
 * configures OD objects 0x1008(manufacturerDeviceName),
 * 0x1009(manufacturerHardwareVersion), 0x100A(manufacturerSoftwareVersion) and
 * 0x1018(identity). It reads definitions from target device specified
 * CO_ident_defs.h file.
 *
 * @param [in,out] bitRate CAN bit rate, set if undefined.
 * @param [in,out] nodeId CANopen NodeId, set if undefined.
 */
void CO_identificators_init(uint16_t* bitRate, uint8_t* nodeId);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_IDENTIFICATORS_H */
