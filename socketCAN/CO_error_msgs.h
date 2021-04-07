/*
 * Definitions for CANopenNode Linux socketCAN Error handling.
 *
 * @file        CO_Error_msgs.h
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


#ifndef CO_ERROR_MSGS_H
#define CO_ERROR_MSGS_H

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Message definitions for Linux CANopen socket driver (notice and errors)
 */
#define CAN_NOT_FOUND             "(%s) CAN Interface \"%s\" not found", __func__
#define CAN_INIT_FAILED           "(%s) CAN Interface  \"%s\" Init failed", __func__
#define CAN_BINDING_FAILED        "(%s) Binding CAN Interface \"%s\" failed", __func__
#define CAN_ERROR_FILTER_FAILED   "(%s) Setting CAN Interface \"%s\" error filter failed", __func__
#define CAN_FILTER_FAILED         "(%s) Setting CAN Interface \"%s\" message filter failed", __func__
#define CAN_NAMETOINDEX           "CAN Interface \"%s\" -> Index %d"
#define CAN_SOCKET_BUF_SIZE       "CAN Interface \"%s\" RX buffer set to %d messages (%d Bytes)"
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
 * Message definitions for debugging
 */
#define DBG_GENERAL               "(%s) Error: %s%d", __func__
#define DBG_ERRNO                 "(%s) OS error \"%s\" in %s", __func__, strerror(errno)
#define DBG_CO_DEBUG              "(%s) CO_DEBUG: %s", __func__
#define DBG_CAN_TX_FAILED         "(%s) Transmitting CAN msg OID 0x%03x failed(%s)", __func__
#define DBG_CAN_RX_PARAM_FAILED   "(%s) Setting CAN rx buffer failed (%s)", __func__
#define DBG_CAN_RX_FAILED         "(%s) Receiving CAN msg failed (%s)", __func__
#define DBG_CAN_ERROR_GENERAL     "(%s) Socket error msg ID: 0x%08x, Data[0..7]: 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x (%s)", __func__
#define DBG_CAN_RX_EPOLL          "(%s) CAN Epoll error (0x%02x - %s)", __func__
#define DBG_CAN_SET_LISTEN_ONLY   "(%s) %s Set Listen Only", __func__
#define DBG_CAN_CLR_LISTEN_ONLY   "(%s) %s Leave Listen Only", __func__

/* mainline */
#define DBG_EMERGENCY_RX          "CANopen Emergency message from node 0x%02X: errorCode=0x%04X, errorRegister=0x%02X, errorBit=0x%02X, infoCode=0x%08X"
#define DBG_NMT_CHANGE            "CANopen NMT state changed to: \"%s\" (%d)"
#define DBG_HB_CONS_NMT_CHANGE    "CANopen Remote node ID = 0x%02X (index = %d): NMT state changed to: \"%s\" (%d)"
#define DBG_ARGUMENT_UNKNOWN      "(%s) Unknown %s argument: \"%s\"", __func__
#define DBG_NOT_TCP_PORT          "(%s) -c argument \"%s\" is not a valid tcp port", __func__
#define DBG_WRONG_NODE_ID         "(%s) Wrong node ID \"%d\"", __func__
#define DBG_WRONG_PRIORITY        "(%s) Wrong RT priority \"%d\"", __func__
#define DBG_NO_CAN_DEVICE         "(%s) Can't find CAN device \"%s\"", __func__
#define DBG_STORAGE               "(%s) Error with storage \"%s\"", __func__
#define DBG_OD_ENTRY              "(%s) Error in Object Dictionary entry: 0x%X", __func__
#define DBG_CAN_OPEN              "(%s) CANopen error in %s, err=%d", __func__
#define DBG_CAN_OPEN_INFO         "CANopen device, Node ID = 0x%02X, %s"

/* CO_epoll_interface */
#define DBG_EPOLL_UNKNOWN         "(%s) CAN Epoll error, events=0x%02x, fd=%d", __func__
#define DBG_COMMAND_LOCAL_BIND    "(%s) Can't bind local socket to path \"%s\"", __func__
#define DBG_COMMAND_TCP_BIND      "(%s) Can't bind tcp socket to port \"%d\"", __func__
#define DBG_COMMAND_STDIO_INFO    "CANopen command interface on \"standard IO\" started"
#define DBG_COMMAND_LOCAL_INFO    "CANopen command interface on local socket \"%s\" started"
#define DBG_COMMAND_TCP_INFO      "CANopen command interface on tcp port \"%d\" started"


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_ERROR_MSGS_H */
