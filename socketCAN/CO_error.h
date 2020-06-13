/**
 * CANopenNode Linux socketCAN Error handling.
 *
 * @file        CO_error.h
 * @ingroup     CO_socketCAN_ERROR
 * @author      Martin Wagner
 * @copyright   2018 - 2020 Neuberger Gebaeudeautomation GmbH
 *
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


#ifndef CO_ERROR_H
#define CO_ERROR_H

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <linux/can.h>
#include <net/if.h>

#if __has_include("CO_error_custom.h")
    #include "CO_error_custom.h"
#else
    #include "CO_error_msgs.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_socketCAN_ERROR CAN errors & Log
 * @ingroup CO_socketCAN
 * @{
 *
 * CANopen Errors and System message log
 */

/**
 * Message logging function.
 *
 * Function must be defined by application. It should record log message to some
 * place, for example syslog() call in Linux or logging functionality in
 * CANopen gateway @ref CO_CANopen_309_3.
 *
 * By default system stores messages in /var/log/syslog file.
 * Log can optionally be configured before, for example to filter out less
 * critical errors than LOG_NOTICE, specify program name, print also process PID
 * and print also to standard error, set 'user' type of program, use:
 * @code
 * setlogmask (LOG_UPTO (LOG_NOTICE));
 * openlog ("exampleprog", LOG_PID | LOG_PERROR, LOG_USER);
 * @endcode
 *
 * @param priority one of LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR, LOG_WARNING,
 *                 LOG_NOTICE, LOG_INFO, LOG_DEBUG
 * @param format format string as in printf
 */
void log_printf(int priority, const char *format, ...);


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
    char                ifName[IFNAMSIZ]; /**< interface name as string */
    uint32_t            noackCounter;   /**< counts no ACK on CAN transmission */
    volatile unsigned char listenOnly;  /**< set to listen only mode */
    struct timespec     timestamp;      /**< listen only mode started at this time */
    uint16_t CANerrorStatus;            /**< CAN error status bitfield, see @ref CO_CAN_ERR_status_t */
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
 * When a message is received at least one other CAN module is connected.
 * Function clears listenOnly and noackCounter error flags.
 *
 * @param CANerrorhandler CAN error object.
 */
void CO_CANerror_rxMsg(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler);


/**
 * Check if interface is ready for message transmission
 *
 * Message mustn't be transmitted if not ready.
 *
 * @param CANerrorhandler CAN error object.
 * @return CO_INTERFACE_ACTIVE message transmission ready
 */
CO_CANinterfaceState_t CO_CANerror_txMsg(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler);


/**
 * Error message received event
 *
 * This handles all received error messages.
 *
 * @param CANerrorhandler CAN error object.
 * @param msg received error message
 * @return #CO_CANinterfaceState_t
 */
CO_CANinterfaceState_t CO_CANerror_rxMsgError(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler,
        const struct can_frame            *msg);


/** @} */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_ERROR_H */
