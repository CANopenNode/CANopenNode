/**
 * CANopenNode types.
 *
 * Defines common types for CANopenNode.
 *
 * @file        CO_types.h
 * @ingroup     CO_CANopen
 * @author      Olivier Desenfans
 * @copyright   2004 - 2019 Janez Paternoster
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
