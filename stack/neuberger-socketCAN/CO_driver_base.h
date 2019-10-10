/**
 * CAN module object for Linux socketCAN.
 *
 * @file        CO_driver_base.h
 * @ingroup     CO_driver
 * @author      Janez Paternoster, Martin Wagner
 * @copyright   2004 - 2015 Janez Paternoster, 2018 Neuberger Gebaeudeautomation GmbH
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

#ifndef CO_DRIVER_BASE_H
#define CO_DRIVER_BASE_H

/* Include processor header file */
#include <stddef.h>         /* for 'NULL' */
#include <stdint.h>         /* for 'int8_t' to 'uint64_t' */
#include <stdbool.h>        /* for 'true', 'false' */
#include <sys/time.h>       /* for 'struct timespec' */
#include <endian.h>
#include <pthread.h>
#include <linux/can.h>
#include <net/if.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_driver Driver
 * @ingroup CO_CANopen
 * @{
 *
 * socketCAN specific code for CANopenNode.
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
 * @name Critical sections
 * CANopenNode is designed to run in different threads, as described in README.
 * Threads are implemented differently in different systems. In microcontrollers
 * threads are interrupts with different priorities, for example.
 * It is necessary to protect sections, where different threads access to the
 * same resource. In simple systems interrupts or scheduler may be temporary
 * disabled between access to the shared resource. Otherwise mutexes or
 * semaphores can be used.
 *
 * ####Reentrant functions.
 * Functions CO_CANsend() from C_driver.h, CO_errorReport() from CO_Emergency.h
 * and CO_errorReset() from CO_Emergency.h may be called from different threads.
 * Critical sections must be protected. Eather by disabling scheduler or
 * interrupts or by mutexes or semaphores.
 *
 * ####Object Dictionary variables.
 * In general, there are two threads, which accesses OD variables: mainline and
 * timer. CANopenNode initialization and SDO server runs in mainline. PDOs runs
 * in faster timer thread. Processing of PDOs must not be interrupted by
 * mainline. Mainline thread must protect sections, which accesses the same OD
 * variables as timer thread. This care must also take the application. Note
 * that not all variables are allowed to be mapped to PDOs, so they may not need
 * to be protected. SDO server protects sections with access to OD variables.
 *
 * ####CAN receive thread.
 * It partially processes received CAN data and puts them into appropriate
 * objects. Objects are later processed. It does not need protection of
 * critical sections. There is one circumstance, where CANrx should be disabled:
 * After presence of SYNC message on CANopen bus, CANrx should be temporary
 * disabled until all receive PDOs are processed. See also CO_SYNC.h file and
 * CO_SYNC_initCallback() function.
 * @{
 */

/* unused */
#define CO_LOCK_CAN_SEND()  /**< Lock critical section in CO_CANsend() */
#define CO_UNLOCK_CAN_SEND()/**< Unlock critical section in CO_CANsend() */

extern pthread_mutex_t CO_EMCY_mutex;
static inline int CO_LOCK_EMCY()    { return pthread_mutex_lock(&CO_EMCY_mutex); }  /**< Lock critical section in CO_errorReport() or CO_errorReset() */
static inline void CO_UNLOCK_EMCY() { (void)pthread_mutex_unlock(&CO_EMCY_mutex); } /**< Unlock critical section in CO_errorReport() or CO_errorReset() */

extern pthread_mutex_t CO_OD_mutex;
static inline int CO_LOCK_OD()      { return pthread_mutex_lock(&CO_OD_mutex); }    /**< Lock critical section when accessing Object Dictionary */
static inline void CO_UNLOCK_OD()   { (void)pthread_mutex_unlock(&CO_OD_mutex); }   /**< Unock critical section when accessing Object Dictionary */

/** @} */

/**
 * @name Syncronisation functions
 * syncronisation for message buffer for communication between CAN receive and
 * message processing threads.
 *
 * If receive function runs inside IRQ, no further synchronsiation is needed.
 * Otherwise, some kind of synchronsiation has to be included. The following
 * example uses GCC builtin memory barrier __sync_synchronize(). A comprehensive
 * list can be found here: https://gist.github.com/leo-yuriev/ba186a6bf5cf3a27bae7
 * \code{.c}
    #define CANrxMemoryBarrier() {__sync_synchronize();}
 * \endcode
 * @{
 */
/** Memory barrier */
#define CANrxMemoryBarrier() {__sync_synchronize();}
/** Check if new message has arrived */
#define IS_CANrxNew(rxNew) ((int)rxNew)
/** Set new message flag */
#define SET_CANrxNew(rxNew) {CANrxMemoryBarrier(); rxNew = (void*)1L;}
/** Clear new message flag */
#define CLEAR_CANrxNew(rxNew) {CANrxMemoryBarrier(); rxNew = (void*)0L;}
/** @} */

/**
 * @defgroup CO_dataTypes Data types
 * @{
 *
 * According to Misra C
 */
    /* int8_t to uint64_t are defined in stdint.h */
    typedef unsigned char           bool_t;     /**< bool_t */
    typedef float                   float32_t;  /**< float32_t */
    typedef long double             float64_t;  /**< float64_t */
    typedef char                    char_t;     /**< char_t */
    typedef unsigned char           oChar_t;    /**< oChar_t */
    typedef unsigned char           domain_t;   /**< domain_t */
/** @} */


/**
 * Return values of some CANopen functions. If function was executed
 * successfully it returns 0 otherwise it returns <0.
 */
typedef enum{
    CO_ERROR_NO                 = 0,    /**< Operation completed successfully */
    CO_ERROR_ILLEGAL_ARGUMENT   = -1,   /**< Error in function arguments */
    CO_ERROR_OUT_OF_MEMORY      = -2,   /**< Memory allocation failed */
    CO_ERROR_TIMEOUT            = -3,   /**< Function timeout */
    CO_ERROR_ILLEGAL_BAUDRATE   = -4,   /**< Illegal baudrate passed to function CO_CANmodule_init() */
    CO_ERROR_RX_OVERFLOW        = -5,   /**< Previous message was not processed yet */
    CO_ERROR_RX_PDO_OVERFLOW    = -6,   /**< previous PDO was not processed yet */
    CO_ERROR_RX_MSG_LENGTH      = -7,   /**< Wrong receive message length */
    CO_ERROR_RX_PDO_LENGTH      = -8,   /**< Wrong receive PDO length */
    CO_ERROR_TX_OVERFLOW        = -9,   /**< Previous message is still waiting, buffer full */
    CO_ERROR_TX_BUSY            = -10,  /**< Sending rejected because driver is busy. Try again */
    CO_ERROR_TX_PDO_WINDOW      = -11,  /**< Synchronous TPDO is outside window */
    CO_ERROR_TX_UNCONFIGURED    = -12,  /**< Transmit buffer was not confugured properly */
    CO_ERROR_PARAMETERS         = -13,  /**< Error in function function parameters */
    CO_ERROR_DATA_CORRUPT       = -14,  /**< Stored data are corrupt */
    CO_ERROR_CRC                = -15,  /**< CRC does not match */
    CO_ERROR_WRONG_NMT_STATE    = -16,  /**< Command can't be processed in current state */
    CO_ERROR_SYSCALL            = -17,  /**< Syscall failed */
    CO_ERROR_INVALID_STATE      = -18   /**< Driver not ready */
}CO_ReturnError_t;


/**
 * Max COB ID for standard frame format
 */
#define CO_CAN_MSG_SFF_MAX_COB_ID (1 << CAN_SFF_ID_BITS)

/**
 * CAN receive message structure as aligned in socketCAN.
 */
typedef struct{
    /** CAN identifier. It must be read through CO_CANrxMsg_readIdent() function. */
    uint32_t            ident;
    uint8_t             DLC ;           /**< Length of CAN message */
    uint8_t             padding[3];     /**< ensure alignment */
    uint8_t             data[8];        /**< 8 data bytes */
}CO_CANrxMsg_t;

/**
 * Received message object
 */
typedef struct{
    uint32_t            ident;          /**< Standard CAN Identifier (bits 0..10) + RTR (bit 11) */
    uint32_t            mask;           /**< Standard Identifier mask with same alignment as ident */
    void               *object;         /**< From CO_CANrxBufferInit() */
    void              (*pFunct)(void *object, const CO_CANrxMsg_t *message);  /**< From CO_CANrxBufferInit() */

#ifdef CO_DRIVER_MULTI_INTERFACE
    /** info about last received message */
    int32_t             CANbaseAddress; /**< CAN Interface identifier */
    struct timespec     timestamp;      /**< time of reception */
#endif
}CO_CANrx_t;


/**
 * Transmit message object as aligned in socketCAN.
 */
typedef struct{
    /** CAN identifier. It must be read through CO_CANrxMsg_readIdent() function. */
    uint32_t            ident;
    uint8_t             DLC ;           /**< Length of CAN message */
    uint8_t             padding[3];     /**< ensure alignment */
    uint8_t             data[8];        /**< 8 data bytes */
    volatile bool_t     bufferFull;     /**< True if previous message is still in buffer (not used in this driver) */
    /** Synchronous PDO messages has this flag set. It prevents them to be sent outside the synchronous window */
    volatile bool_t     syncFlag;

    /** info about transmit message */
    int32_t             CANbaseAddress; /**< CAN Interface identifier to use */
} CO_CANtx_t;

/**
 * Endianess.
 *
 * Depending on processor or compiler architecture, one of the two macros must
 * be defined: CO_LITTLE_ENDIAN or CO_BIG_ENDIAN. CANopen itself is little endian.
 */
#ifdef __BYTE_ORDER
#if __BYTE_ORDER == __LITTLE_ENDIAN
    #define CO_LITTLE_ENDIAN
#else
    #define CO_BIG_ENDIAN
#endif
#endif

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
