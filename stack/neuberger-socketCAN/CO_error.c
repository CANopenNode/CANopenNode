/*
 * CAN module object for Linux socketCAN Error handling.
 *
 * @file        CO_error.c
 * @ingroup     CO_driver
 * @author      Martin Wagner
 * @copyright   2018 Neuberger Gebaeudeautomation GmbH
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/can/error.h>

#include "CO_error.h"


#if defined CO_DRIVER_ERROR_REPORTING && __has_include("syslog/log.h")
  #include "syslog/log.h"
  #include "msgs.h"
#else
  #define log_printf(macropar_prio, macropar_message, ...)
#endif


/**
 * Reset CAN interface and set to listen only mode
 */
static CO_CANinterfaceState_t CO_CANerrorSetListenOnly(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler,
        bool_t                             resetIf)
{
    char command[100];

    log_printf(LOG_DEBUG, DBG_CAN_SET_LISTEN_ONLY, CANerrorhandler->ifName);

    clock_gettime(CLOCK_MONOTONIC, &CANerrorhandler->timestamp);
    CANerrorhandler->listenOnly = true;

    if (resetIf) {
        snprintf(command, sizeof(command), "ip link set %s down && "
                                           "ip link set %s up "
                                           "&",
                                           CANerrorhandler->ifName,
                                           CANerrorhandler->ifName);
        system(command);
    }

    return CO_INTERFACE_LISTEN_ONLY;
}


/**
 * Clear listen only
 */
static void CO_CANerrorClearListenOnly(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler)
{
    log_printf(LOG_DEBUG, DBG_CAN_CLR_LISTEN_ONLY, CANerrorhandler->ifName);

    CANerrorhandler->listenOnly = false;
    CANerrorhandler->timestamp.tv_sec = 0;
    CANerrorhandler->timestamp.tv_nsec = 0;
}


/**
 * Check and handle "bus off" state
 */
static CO_CANinterfaceState_t CO_CANerrorBusoff(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler,
        const struct can_frame            *msg)
{
    CO_CANinterfaceState_t result = CO_INTERFACE_ACTIVE;

    if ((msg->can_id & CAN_ERR_BUSOFF) != 0) {
        log_printf(LOG_NOTICE, CAN_BUSOFF, CANerrorhandler->ifName);

        /* The can interface changed it's state to "bus off" (e.g. because of
         * a short on the can wires). We re-start the interface and mark it
         * "listen only".
         * Restarting the interface is the only way to clear kernel and hardware
         * tx queues */
        result = CO_CANerrorSetListenOnly(CANerrorhandler, true);
    }
    return result;
}


/**
 * Check and handle controller problems
 */
static CO_CANinterfaceState_t CO_CANerrorCrtl(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler,
        const struct can_frame            *msg)
{
    CO_CANinterfaceState_t result = CO_INTERFACE_ACTIVE;

    /* Control
     * - error counters (rec/tec) are handled inside CAN hardware, nothing
     *   to do in here
     * - we can't really do anything about buffer overflows here. Confirmed
     *   CANopen protocols will detect the error, non-confirmed protocols
     *   need to be error tolerant */
    if ((msg->can_id & CAN_ERR_CRTL) != 0) {
        if ((msg->data[1] & CAN_ERR_CRTL_RX_PASSIVE) != 0) {
            log_printf(LOG_NOTICE, CAN_RX_PASSIVE, CANerrorhandler->ifName);
        }
        else if ((msg->data[1] & CAN_ERR_CRTL_TX_PASSIVE) != 0) {
            log_printf(LOG_NOTICE, CAN_TX_PASSIVE, CANerrorhandler->ifName);
        }
        else if ((msg->data[1] & CAN_ERR_CRTL_RX_OVERFLOW) != 0) {
            log_printf(LOG_NOTICE, CAN_RX_BUF_OVERFLOW, CANerrorhandler->ifName);
        }
        else if ((msg->data[1] & CAN_ERR_CRTL_TX_OVERFLOW) != 0) {
            log_printf(LOG_NOTICE, CAN_TX_BUF_OVERFLOW, CANerrorhandler->ifName);
        }
        else if ((msg->data[1] & CAN_ERR_CRTL_RX_WARNING) != 0) {
            log_printf(LOG_INFO, CAN_RX_LEVEL_WARNING, CANerrorhandler->ifName);
        }
        else if ((msg->data[1] & CAN_ERR_CRTL_TX_WARNING) != 0) {
            log_printf(LOG_INFO, CAN_TX_LEVEL_WARNING, CANerrorhandler->ifName);
        }
        else if ((msg->data[1] & CAN_ERR_CRTL_ACTIVE) != 0) {
            log_printf(LOG_NOTICE, CAN_TX_LEVEL_ACTIVE, CANerrorhandler->ifName);
        }
    }
    return result;
}


/**
 * Check and handle controller problems
 */
static CO_CANinterfaceState_t CO_CANerrorNoack(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler,
        const struct can_frame            *msg)
{
    CO_CANinterfaceState_t result = CO_INTERFACE_ACTIVE;

    if (CANerrorhandler->listenOnly) {
        return CO_INTERFACE_LISTEN_ONLY;
    }

    /* received no ACK on transmission */
    if ((msg->can_id & CAN_ERR_ACK) != 0) {
        CANerrorhandler->noackCounter ++;
        if (CANerrorhandler->noackCounter > CO_CANerror_NOACK_MAX) {
            log_printf(LOG_INFO, CAN_NOACK, CANerrorhandler->ifName);

            /* We get the NO-ACK error continuously when no other CAN node
             * is active on the bus (Error Counting exception 1 in CAN spec).
             * todo - you need to pull the message causing no-ack from the CAN
             * hardware buffer. This can be done by either resetting interface
             * in here or deleting it within Linux Kernel can driver  (set "false"). */
            result = CO_CANerrorSetListenOnly(CANerrorhandler, true);
        }
    }
    else {
        CANerrorhandler->noackCounter = 0;
    }
    return result;
}


/******************************************************************************/
void CO_CANerror_init(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler,
        int                                fd,
        const char                        *ifName)
{
    if (CANerrorhandler == NULL) {
        return;
    }

    CANerrorhandler->fd = fd;
    CANerrorhandler->ifName = ifName;
    CANerrorhandler->noackCounter = 0;
    CANerrorhandler->listenOnly = false;
    CANerrorhandler->timestamp.tv_sec = 0;
    CANerrorhandler->timestamp.tv_nsec = 0;
}


/******************************************************************************/
void CO_CANerror_disable(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler)
{
    if (CANerrorhandler == NULL) {
        return;
    }

    memset(CANerrorhandler, 0, sizeof(*CANerrorhandler));
    CANerrorhandler->fd = -1;
}


/******************************************************************************/
void CO_CANerror_rxMsg(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler)
{
    if (CANerrorhandler == NULL) {
        return;
    }

    /* someone is active, we can leave listen only immediately */
    if (CANerrorhandler->listenOnly) {
        CO_CANerrorClearListenOnly(CANerrorhandler);
    }
    CANerrorhandler->noackCounter = 0;
}


/******************************************************************************/
CO_CANinterfaceState_t CO_CANerror_txMsg(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler)
{
    struct timespec now;

    if (CANerrorhandler == NULL) {
        return CO_INTERFACE_BUS_OFF;
    }

    if (CANerrorhandler->listenOnly) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (CANerrorhandler->timestamp.tv_sec + CO_CANerror_LISTEN_ONLY < now.tv_sec) {
            /* let's try that again. Maybe someone is waiting for LSS now. It
             * doesn't matter which message is sent, as all messages are ACKed. */
            CO_CANerrorClearListenOnly(CANerrorhandler);
            return CO_INTERFACE_ACTIVE;
        }
        return CO_INTERFACE_LISTEN_ONLY;
    }
    return CO_INTERFACE_ACTIVE;
}


/******************************************************************************/
CO_CANinterfaceState_t CO_CANerror_rxMsgError(
        CO_CANinterfaceErrorhandler_t     *CANerrorhandler,
        const struct can_frame            *msg)
{
    if (CANerrorhandler == NULL) {
        return CO_INTERFACE_BUS_OFF;
    }

    CO_CANinterfaceState_t result;

    /* Log all error messages in full to debug log, even if analysis is done
     * further on. */
    log_printf(LOG_DEBUG, DBG_CAN_ERROR_GENERAL, (int)msg->can_id,
               msg->data[0], msg->data[1], msg->data[2], msg->data[3],
               msg->data[4], msg->data[5], msg->data[6], msg->data[7],
               CANerrorhandler->ifName);

    /* Process errors - start with the most unambiguous one */

    result = CO_CANerrorBusoff(CANerrorhandler, msg);
    if (result != CO_INTERFACE_ACTIVE) {
      return result;
    }

    result = CO_CANerrorCrtl(CANerrorhandler, msg);
    if (result != CO_INTERFACE_ACTIVE) {
      return result;
    }

    result = CO_CANerrorNoack(CANerrorhandler, msg);
    if (result != CO_INTERFACE_ACTIVE) {
      return result;
    }

    return CO_INTERFACE_ACTIVE;
}
