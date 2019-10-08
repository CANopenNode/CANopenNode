/**
 * CAN module object for Linux socketCAN Error handling.
 *
 * @file        CO_error.h
 * @ingroup     CO_driver
 * @author      Martin Wagner
 * @copyright   2018 Neuberger Gebaeudeautomation GmbH
 *
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


#ifndef CO_ERROR_H
#define CO_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "CO_driver_base.h"

/**
 * driver interface state
 *
 * CAN hardware can be in the followning states:
 * - error active   (OK)
 * - error passive  (Can't generate error flags)
 * - bus off        (no influence on bus)
 */
typedef enum {
    CO_INTERFACE_ACTIVE,              /**< CAN error passive/active */
    CO_INTERFACE_LISTEN_ONLY,         /**< CAN error passive/active, but currently no other device on bus */
    CO_INTERFACE_BUS_OFF              /**< CAN bus off */
} CO_CANinterfaceState_t;


/**
 * This is how many NO-ACKs need to be received in a row to assume
 * that no other nodes are connected to a bus and therefore are
 * assuming listen-only
 */
#define CO_CANerror_NOACK_MAX 16


/**
 * This is how long we are going to block transmission if listen-only
 * mode is active
 *
 * Time is in seconds.
 */
#define CO_CANerror_LISTEN_ONLY 10


/**
 * socketCAN interface error handling
 */
typedef struct {
  int                 fd;             /**< interface FD */
  const char         *ifName;         /**< interface name as string */

  uint32_t            noackCounter;

  volatile bool_t     listenOnly;     /**< set to listen only mode */
  struct timespec     timestamp;      /**< listen only mode started at this time */
} CO_CANinterfaceErrorhandler_t;

/**
 * Initialize CAN error handler
 *
 * We need one error handler per interface
 *
 * @param CANerrorhandler This object will be initialized.
 * @param fd interface file descriptor
 * @param ifname interface name as string
 */
void CO_CANerror_init(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler,
        int                                fd,
        const char                        *ifname);

/**
 * Reset CAN error handler
 *
 * @param CANerrorhandler CAN error object.
 */
void CO_CANerror_disable(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler);

/**
 * Message received event
 *
 * when a message is received at least one other CAN module is connected
 *
 * @param CANerrorhandler CAN error object.
 */
void CO_CANerror_rxMsg(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler);


/**
 * Check if interface is ready for message transmission
 *
 * message musn't be transmitted if not ready
 *
 * @param CANerrorhandler CAN error object.
 * @return CO_INTERFACE_ACTIVE message transmission ready
 */
CO_CANinterfaceState_t CO_CANerror_txMsg(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler);

/**
 * Error message received event
 *
 * this handles all received error messages
 *
 * @param CANerrorhandler CAN error object.
 * @param msg received error message
 * @return #CO_CANinterfaceState_t
 */
CO_CANinterfaceState_t CO_CANerror_rxMsgError(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler,
        const struct can_frame            *msg);


#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
