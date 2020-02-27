/**
 * CAN module object for Linux socketCAN.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_msgs.h
 * @ingroup     CO_driver
 * @author      Martin Wagner
 * @copyright   2020 Neuberger Gebaeudeautomation GmbH
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


#ifndef CO_MSGS_H
#define CO_MSGS_H

#include <stdio.h>
#include <syslog.h>
#include "CO_msgs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * message printing function
 */
#define log_printf(macropar_prio, macropar_message, ...) \
  if (macropar_prio < LOG_DEBUG) { \
    printf(macropar_message, ##__VA_ARGS__); \
  }

/*
 * message logging function. You need to open the log previous to this.
 */
//#define log_printf(macropar_prio, macropar_message, ...) syslog(macropar_prio, macropar_message, ##__VA_ARGS__);



/*
 * Message definitions for Linux CANopen socket driver (notice and errors)
 */
#define CAN_NOT_FOUND             "CAN Interface \"%s\" not found"
#define CAN_INIT_FAILED           "CAN Interface  \"%s\" Init failed"
#define CAN_NAMETOINDEX           "Interface \"%s\" -> Index %d"
#define CAN_SOCKET_BUF_SIZE       "CAN Interface \"%s\" Buffer set to %d messages (%d Bytes)"
#define CAN_BINDING_FAILED        "Binding CAN Interface \"%s\" failed"
#define CAN_ERROR_FILTER_FAILED   "Setting CAN Interface \"%s\" error filter failed"
#define CAN_FILTER_FAILED         "Setting CAN Interface \"%s\" message filter failed"
#define CAN_RX_SOCKET_QUEUE_OVERFLOW "CAN Interface \"%s\" has lost %d messages"
#define CAN_BUSOFF                "CAN Interface \"%s\" changed to \"Bus Off\". Switching to Listen Only mode..."
#define CAN_NOACK                 "CAN Interface \"%s\" no \"ACK\" received.  Switching to Listen Only mode..."
#define CAN_RX_PASSIVE            "CAN Interface \"%s\" changed state to \"Rx Passive\""
#define CAN_TX_PASSIVE            "CAN Interface \"%s\" changed state to \"Tx Passive\""
#define CAN_TX_LEVEL_ACTIVE       "CAN Interface \"%s\" changed state to \"Active\""
#define CAN_RX_BUF_OVERFLOW       "CAN Interface \"%s\" Rx buffer overflow. Message dropped"
#define CAN_TX_BUF_OVERFLOW       "CAN Interface \"%s\" Tx buffer overflow. Message dropped"
#define CAN_RX_LEVEL_WARNING      "CAN Interface \"%s\" reached Rx Warning Level"
#define CAN_TX_LEVEL_WARNING      "CAN Interface \"%s\" reached Tx Warning Level"

/*
 * Debug
 */
#define DBG_ERRNO                 "(%s) OS error \"%s\" in %s", __func__, strerror(errno)
#define DBG_CAN_TX_FAILED         "(%s) Transmitting CAN msg OID 0x%08x failed(%s)", __func__
#define DBG_CAN_RX_PARAM_FAILED   "(%s) Setting CAN rx buffer failed (%s)", __func__
#define DBG_CAN_RX_FAILED         "(%s) Receiving CAN msg failed (%s)", __func__
#define DBG_CAN_ERROR_GENERAL     "(%s) Socket error msg ID: 0x%08x, Data[0..7]: 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x (%s)", __func__
#define DBG_CAN_RX_EPOLL          "(%s) CAN Epoll error (0x%02x - %s)", __func__
#define DBG_CAN_SET_LISTEN_ONLY   "(%s) %s Set Listen Only", __func__
#define DBG_CAN_CLR_LISTEN_ONLY   "(%s) %s Leave Listen Only", __func__


#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */
#endif
