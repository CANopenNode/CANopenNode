/**
 * CAN driver functions.
 *
 * Defines functions that must be implemented for each target.
 *
 * @file        CO_driver.h
 * @ingroup     CO_driver
 * @author      Janez Paternoster
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

#ifndef CO_DRIVER_H
#define CO_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "CO_driver_target.h"
#include "CO_types.h"

/* Include processor header file */
#include <stdint.h>         /* for 'int8_t' to 'uint64_t' */

/**
 * @defgroup CO_driver Driver
 * @ingroup CO_CANopen
 * @{
 *
 * Microcontroller specific code for CANopenNode.
 *
 * This file contains type definitions, functions and macros for:
 *  - Basic data types.
 *  - Receive and transmit buffers for CANopen messages.
 *  - Interaction with CAN module on the microcontroller.
 *  - CAN receive and transmit interrupts.
 *
 * This file is not only a CAN driver. There are no classic CAN queues for CAN
 * messages. This file provides direct connection with other CANopen
 * objects. It tries to provide fast responses and tries to avoid unnecessary
 * calculations and memory consumptions.
 *
 * CO_CANmodule_t contains an array of _Received message objects_ (of type
 * CO_CANrx_t) and an array of _Transmit message objects_ (of type CO_CANtx_t).
 * Each CANopen communication object owns one member in one of the arrays.
 * For example Heartbeat producer generates one CANopen transmitting object,
 * so it has reserved one member in CO_CANtx_t array.
 * SYNC module may produce sync or consume sync, so it has reserved one member
 * in CO_CANtx_t and one member in CO_CANrx_t array.
 *
 * ###Reception of CAN messages.
 * Before CAN messages can be received, each member in CO_CANrx_t must be
 * initialized. CO_CANrxBufferInit() is called by CANopen module, which
 * uses specific member. For example @ref CO_HBconsumer uses multiple members
 * in CO_CANrx_t array. (It monitors multiple heartbeat messages from remote
 * nodes.) It must call CO_CANrxBufferInit() multiple times.
 *
 * Main arguments to the CO_CANrxBufferInit() function are CAN identifier
 * and a pointer to callback function. Those two arguments (and some others)
 * are copied to the member of the CO_CANrx_t array.
 *
 * Callback function is a function, specified by specific CANopen module
 * (for example by @ref CO_HBconsumer). Each CANopen module defines own
 * callback function. Callback function will process the received CAN message.
 * It will copy the necessary data from CAN message to proper place. It may
 * also trigger additional task, which will further process the received message.
 * Callback function must be fast and must only make the necessary calculations
 * and copying.
 *
 * Received CAN messages are processed by CAN receive interrupt function.
 * After CAN message is received, function first tries to find matching CAN
 * identifier from CO_CANrx_t array. If found, then a corresponding callback
 * function is called.
 *
 * Callback function accepts two parameters:
 *  - object is pointer to object registered by CO_CANrxBufferInit().
 *  - msg  is pointer to CAN message of type CO_CANrxMsg_t.
 *
 * Callback function must return #CO_ReturnError_t: CO_ERROR_NO,
 * CO_ERROR_RX_OVERFLOW, CO_ERROR_RX_PDO_OVERFLOW, CO_ERROR_RX_MSG_LENGTH or
 * CO_ERROR_RX_PDO_LENGTH.
 *
 *
 * ###Transmission of CAN messages.
 * Before CAN messages can be transmitted, each member in CO_CANtx_t must be
 * initialized. CO_CANtxBufferInit() is called by CANopen module, which
 * uses specific member. For example Heartbeat producer must initialize it's
 * member in CO_CANtx_t array.
 *
 * CO_CANtxBufferInit() returns a pointer of type CO_CANtx_t, which contains buffer
 * where CAN message data can be written. CAN message is send with calling
 * CO_CANsend() function. If at that moment CAN transmit buffer inside
 * microcontroller's CAN module is free, message is copied directly to CAN module.
 * Otherwise CO_CANsend() function sets _bufferFull_ flag to true. Message will be
 * then sent by CAN TX interrupt as soon as CAN module is freed. Until message is
 * not copied to CAN module, its contents must not change. There may be multiple
 * _bufferFull_ flags in CO_CANtx_t array set to true. In that case messages with
 * lower index inside array will be sent first.
 */

/**
 * Request CAN configuration (stopped) mode and *wait* untill it is set.
 *
 * @param CANdriverState User-provided CAN module structure.
 */
void CO_CANsetConfigurationMode(void *CANdriverState);


/**
 * Request CAN normal (opearational) mode and *wait* untill it is set.
 *
 * @param CANmodule This object.
 */
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule);


/**
 * Initialize CAN module object.
 *
 * Function must be called in the communication reset section. CAN module must
 * be in Configuration Mode before.
 *
 * @param CANmodule This object will be initialized.
 * @param CANdriverState User-provided CAN module structure..
 * @param rxArray Array for handling received CAN messages
 * @param rxSize Size of the above array. Must be equal to number of receiving CAN objects.
 * @param txArray Array for handling transmitting CAN messages
 * @param txSize Size of the above array. Must be equal to number of transmitting CAN objects.
 * @param CANbitRate Valid values are (in kbps): 10, 20, 50, 125, 250, 500, 800, 1000.
 * If value is illegal, bitrate defaults to 125.
 *
 * Return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        void                   *CANdriverState,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate);


/**
 * Switch off CANmodule. Call at program exit.
 *
 * @param CANmodule CAN module object.
 */
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule);


/**
 * Read CAN identifier from received message
 *
 * @param rxMsg Pointer to received message
 * @return 11-bit CAN standard identifier.
 */
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg);


/**
 * Configure CAN message receive buffer.
 *
 * Function configures specific CAN receive buffer. It sets CAN identifier
 * and connects buffer with specific object. Function must be called for each
 * member in _rxArray_ from CO_CANmodule_t.
 *
 * @param CANmodule This object.
 * @param index Index of the specific buffer in _rxArray_.
 * @param ident 11-bit standard CAN Identifier.
 * @param mask 11-bit mask for identifier. Most usually set to 0x7FF.
 * Received message (rcvMsg) will be accepted if the following
 * condition is true: (((rcvMsgId ^ ident) & mask) == 0).
 * @param rtr If true, 'Remote Transmit Request' messages will be accepted.
 * @param object CANopen object, to which buffer is connected. It will be used as
 * an argument to pFunct. Its type is (void), pFunct will change its
 * type back to the correct object type.
 * @param pFunct Pointer to function, which will be called, if received CAN
 * message matches the identifier. It must be fast function.
 *
 * Return #CO_ReturnError_t: CO_ERROR_NO CO_ERROR_ILLEGAL_ARGUMENT or
 * CO_ERROR_OUT_OF_MEMORY (not enough masks for configuration).
 */
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        bool_t                  rtr,
        void                   *object,
        void                  (*pFunct)(void *object, const CO_CANrxMsg_t *message));


/**
 * Configure CAN message transmit buffer.
 *
 * Function configures specific CAN transmit buffer. Function must be called for
 * each member in _txArray_ from CO_CANmodule_t.
 *
 * @param CANmodule This object.
 * @param index Index of the specific buffer in _txArray_.
 * @param ident 11-bit standard CAN Identifier.
 * @param rtr If true, 'Remote Transmit Request' messages will be transmitted.
 * @param noOfBytes Length of CAN message in bytes (0 to 8 bytes).
 * @param syncFlag This flag bit is used for synchronous TPDO messages. If it is set,
 * message will not be sent, if curent time is outside synchronous window.
 *
 * @return Pointer to CAN transmit message buffer. 8 bytes data array inside
 * buffer should be written, before CO_CANsend() function is called.
 * Zero is returned in case of wrong arguments.
 */
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        bool_t                  rtr,
        uint8_t                 noOfBytes,
        bool_t                  syncFlag);


/**
 * Send CAN message.
 *
 * @param CANmodule This object.
 * @param buffer Pointer to transmit buffer, returned by CO_CANtxBufferInit().
 * Data bytes must be written in buffer before function call.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_TX_OVERFLOW or
 * CO_ERROR_TX_PDO_WINDOW (Synchronous TPDO is outside window).
 */
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer);


/**
 * Clear all synchronous TPDOs from CAN module transmit buffers.
 *
 * CANopen allows synchronous PDO communication only inside time between SYNC
 * message and SYNC Window. If time is outside this window, new synchronous PDOs
 * must not be sent and all pending sync TPDOs, which may be on CAN TX buffers,
 * must be cleared.
 *
 * This function checks (and aborts transmission if necessary) CAN TX buffers
 * when it is called. Function should be called by the stack in the moment,
 * when SYNC time was just passed out of synchronous window.
 *
 * @param CANmodule This object.
 */
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule);


/**
 * Verify all errors of CAN module.
 *
 * Function is called directly from CO_EM_process() function.
 *
 * @param CANmodule This object.
 */
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */
#endif /* CO_DRIVER_H */
