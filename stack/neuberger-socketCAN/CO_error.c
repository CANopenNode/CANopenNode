/*
 * CAN module object for Linux socketCAN Error handling.
 *
 * @file        CO_error.c
 * @ingroup     CO_driver
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/can/error.h>

#include "CO_error.h"

#if defined CO_DRIVER_ERROR_REPORTING
  #if __has_include("syslog/log.h")
    #include "syslog/log.h"
    #include "msgs.h"
  #else
    #include "CO_msgs.h"
  #endif
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
    memcpy(CANerrorhandler->ifName, ifName, sizeof(CANerrorhandler->ifName));
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
