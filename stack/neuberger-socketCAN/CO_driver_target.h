/**
 * CAN module object for Linux socketCAN.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver_target.h
 * @ingroup     CO_driver
 * @author      Janez Paternoster, Martin Wagner
 * @copyright   2004 - 2015 Janez Paternoster, 2017 - 2020 Neuberger Gebaeudeautomation GmbH
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


#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * @name multi interface support
 *
 * Enable this to use interface combining at driver level. This
 * adds functions to broadcast/selective transmit messages on the
 * given interfaces as well as combining all received message into
 * one queue.
 *
 * This is not intended to realize interface redundancy!!!
 */
//#define CO_DRIVER_MULTI_INTERFACE

/**
 * @name CAN bus error reporting
 *
 * Enable this to add support for socketCAN error detection- and
 * handling functions inside the driver. This is needed when you have
 * CANopen with "0" connected nodes as a use case, as this is normally
 * forbidden in CAN.
 *
 * you need to enable error reporting in your kernel driver using
 * "ip link set canX type can berr-reporting on". Of course, the kernel
 * driver for your hardware needs this functionallity to be implemented...
 */
//#define CO_DRIVER_ERROR_REPORTING


#include "CO_driver_base.h"
#include "CO_notify_pipe.h"

#ifdef CO_DRIVER_ERROR_REPORTING
  #include "CO_error.h"
#endif /* CO_DRIVER_ERROR_REPORTING */

/**
 * socketCAN interface object
 */
typedef struct {
    const void         *CANdriverState;   /**< CAN Interface identifier */
    char                ifName[IFNAMSIZ]; /**< CAN Interface name */
    int                 fd;               /**< socketCAN file descriptor */
#ifdef CO_DRIVER_ERROR_REPORTING
    CO_CANinterfaceErrorhandler_t errorhandler;
#endif
} CO_CANinterface_t;

/**
 * CAN module object. It may be different in different microcontrollers.
 */
typedef struct{
    /** List of can interfaces. From CO_CANmodule_init()/ one per CO_CANmodule_addInterface() call */
    CO_CANinterface_t  *CANinterfaces;
    uint32_t            CANinterfaceCount; /** interface count */
    CO_CANrx_t         *rxArray;        /**< From CO_CANmodule_init() */
    uint16_t            rxSize;         /**< From CO_CANmodule_init() */
    struct can_filter  *rxFilter;       /**< socketCAN filter list, one per rx buffer */
    uint32_t            rxDropCount;    /**< messages dropped on rx socket queue */
    CO_CANtx_t         *txArray;        /**< From CO_CANmodule_init() */
    uint16_t            txSize;         /**< From CO_CANmodule_init() */
    volatile bool_t     CANnormal;      /**< CAN module is in normal mode */
    void               *em;             /**< Emergency object */
    CO_NotifyPipe_t    *pipe;           /**< Notification Pipe */
    int                 fdEpoll;        /**< epoll FD */
    int                 fdTimerRead;    /**< timer handle from CANrxWait() */
#ifdef CO_DRIVER_MULTI_INTERFACE
    /**
     * Lookup tables Cob ID to rx/tx array index. Only feasible for SFF Messages.
     */
    uint32_t            rxIdentToIndex[CO_CAN_MSG_SFF_MAX_COB_ID]; /**< COB ID to index assignment */
    uint32_t            txIdentToIndex[CO_CAN_MSG_SFF_MAX_COB_ID]; /**< COB ID to index assignment */
#endif /* CO_DRIVER_MULTI_INTERFACE */
}CO_CANmodule_t;


#ifdef CO_DRIVER_MULTI_INTERFACE
/**
 * Initialize CAN module object
 *
 * Function must be called in the communication reset section. CAN module must
 * be in Configuration Mode before.
 *
 * @param CANmodule This object will be initialized.
 * @param CANdriverState unused
 * @param rxArray Array for handling received CAN messages
 * @param rxSize Size of the above array. Must be equal to number of receiving CAN objects.
 * @param txArray Array for handling transmitting CAN messages
 * @param txSize Size of the above array. Must be equal to number of transmitting CAN objects.
 * @param CANbitRate not supported in this driver. Needs to be set by OS
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT.
 */
#else
/**
 * Initialize CAN module object and open socketCAN connection.
 *
 * Function must be called in the communication reset section. CAN module must
 * be in Configuration Mode before.
 *
 * @param CANmodule This object will be initialized.
 * @param CANdriverState CAN module interface index (return value if_nametoindex(), NO pointer!).
 * @param rxArray Array for handling received CAN messages
 * @param rxSize Size of the above array. Must be equal to number of receiving CAN objects.
 * @param txArray Array for handling transmitting CAN messages
 * @param txSize Size of the above array. Must be equal to number of transmitting CAN objects.
 * @param CANbitRate not supported in this driver. Needs to be set by OS
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT or
 * CO_ERROR_SYSCALL.
 */
#endif /* CO_DRIVER_MULTI_INTERFACE */
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        void                   *CANdriverState,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate);

#ifdef CO_DRIVER_MULTI_INTERFACE

/**
 * Add socketCAN interface to can driver
 *
 * Function must be called after CO_CANmodule_init.
 *
 * @param CANmodule This object will be initialized.
 * @param CANdriverState CAN module interface index (return value if_nametoindex(), NO pointer!).
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT,
 * CO_ERROR_SYSCALL or CO_ERROR_INVALID_STATE.
 */
CO_ReturnError_t CO_CANmodule_addInterface(
        CO_CANmodule_t         *CANmodule,
        const void             *CANdriverState);

#endif /* CO_DRIVER_MULTI_INTERFACE */


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
 * @return #CO_ReturnError_t: CO_ERROR_NO CO_ERROR_ILLEGAL_ARGUMENT or
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

#ifdef CO_DRIVER_MULTI_INTERFACE

/**
 * Check on which interface the last message for one message buffer was received
 *
 * It is in the responsibility of the user to check that this information is
 * useful as some messages can be received at any time on any bus.
 *
 * @param CANmodule This object.
 * @param ident 11-bit standard CAN Identifier.
 * @param [out] CANdriverStateRx message was received on this interface
 * @param [out] timestamp message was received at this time (system clock)
 *
 * @retval false message has never been received, therefore no base address
 * and timestamp are available
 * @retval true base address and timestamp are valid
 */
bool_t CO_CANrxBuffer_getInterface(
        CO_CANmodule_t         *CANmodule,
        uint16_t                ident,
        const void            **const CANdriverStateRx,
        struct timespec        *timestamp);

/**
 * Set which interface should be used for message buffer transmission
 *
 * It is in the responsibility of the user to ensure that the correct interface
 * is used. Some messages need to be transmitted on all interfaces.
 *
 * If given interface is unknown or NULL is used, a message is transmitted on
 * all available interfaces.
 *
 * @param CANmodule This object.
 * @param ident 11-bit standard CAN Identifier.
 * @param CANdriverStateTx use this interface. NULL = not specified
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_CANtxBuffer_setInterface(
        CO_CANmodule_t         *CANmodule,
        uint16_t                ident,
        const void             *CANdriverStateTx);

#endif /* CO_DRIVER_MULTI_INTERFACE */


/**
 * The same as #CO_CANsend(), but ensures that there is enough space remaining
 * in the driver for more important messages.
 *
 * The default threshold is 50%, or at least 1 message buffer. If sending
 * would violate those limits, #CO_ERROR_TX_OVERFLOW is returned and the
 * message will not be sent.
 *
 * @param CANmodule This object.
 * @param buffer Pointer to transmit buffer, returned by CO_CANtxBufferInit().
 * Data bytes must be written in buffer before function call.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_TX_OVERFLOW, CO_ERROR_TX_BUSY or
 * CO_ERROR_TX_PDO_WINDOW (Synchronous TPDO is outside window).
 */
CO_ReturnError_t CO_CANCheckSend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer);


/**
 * Functions receives CAN messages. It is blocking.
 *
 * This function can be used in two ways
 * - automatic mode (call callback that is set by #CO_CANrxBufferInit() function)
 * - manual mode (evaluate message filters, return received message)
 *
 * Both modes can be combined.
 *
 * @param CANmodule This object.
 * @param fdTimer file descriptor with activated timeout. fd is not read after
 *                expiring! -1 if not used.
 * @param buffer [out] storage for received message or _NULL_
 * @retval >= 0 index of received message in array set by #CO_CANmodule_init()
 *         _rxArray_, copy available in _buffer_
 * @retval -1 no message received
 */
int32_t CO_CANrxWait(CO_CANmodule_t *CANmodule, int fdTimer, CO_CANrxMsg_t *buffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */
#endif
