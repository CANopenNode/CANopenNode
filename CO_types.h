/**
 * CANopenNode types.
 *
 * Defines common types for CANopenNode.
 *
 * @file        CO_types.h
 * @ingroup     CO_CANopen
 * @author      Olivier Desenfans
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

#ifndef CO_TYPES_H
#define CO_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Return values of some CANopen functions.
 *
 * A function should return 0 on success and a negative value on error.
 */
typedef enum {
    /** Operation completed successfully */
    CO_ERROR_NO = 0,
    /** Error in function arguments */
    CO_ERROR_ILLEGAL_ARGUMENT = -1,
    /** Memory allocation failed */
    CO_ERROR_OUT_OF_MEMORY = -2,
    /** Function timeout */
    CO_ERROR_TIMEOUT = -3,
    /** Illegal baudrate passed to function CO_CANmodule_init() */
    CO_ERROR_ILLEGAL_BAUDRATE = -4,
    /** Previous message was not processed yet */
    CO_ERROR_RX_OVERFLOW = -5,
    /** previous PDO was not processed yet */
    CO_ERROR_RX_PDO_OVERFLOW = -6,
    /** Wrong receive message length */
    CO_ERROR_RX_MSG_LENGTH = -7,
    /** Wrong receive PDO length */
    CO_ERROR_RX_PDO_LENGTH = -8,
    /** Previous message is still waiting, buffer full */
    CO_ERROR_TX_OVERFLOW = -9,
    /** Synchronous TPDO is outside window */
    CO_ERROR_TX_PDO_WINDOW = -10,
    /** Transmit buffer was not confugured properly */
    CO_ERROR_TX_UNCONFIGURED = -11,
    /** Error in function function parameters */
    CO_ERROR_PARAMETERS = -12,
    /** Stored data are corrupt */
    CO_ERROR_DATA_CORRUPT = -13,
    /** CRC does not match */
    CO_ERROR_CRC = -14,
    /** Sending rejected because driver is busy. Try again */
    CO_ERROR_TX_BUSY = -15,
    /** Command can't be processed in current state */
    CO_ERROR_WRONG_NMT_STATE = -16,
    /** Syscall failed */
    CO_ERROR_SYSCALL = -17,
    /** Driver not ready */
    CO_ERROR_INVALID_STATE = -18,
} CO_ReturnError_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */
#endif /* CO_TYPES_H */
