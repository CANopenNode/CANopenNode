/*
 * Linux socketCAN specific definitions for CANopenNode.
 *
 * @file        CO_driver_target.h
 * @author      Janez Paternoster
 * @author      Martin Wagner
 * @copyright   2004 - 2020 Janez Paternoster
 * @copyright   2018 - 2020 Neuberger Gebaeudeautomation GmbH
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


#ifndef CO_DRIVER_TARGET
#define CO_DRIVER_TARGET

/* This file contains device and application specific definitions.
 * It is included from CO_driver.h, which contains documentation
 * for definitions below. */

/*
 * @name multi interface support
 *
 * Enable this to use interface combining at driver level. This
 * adds functions to broadcast/selective transmit messages on the
 * given interfaces as well as combining all received message into
 * one queue.
 *
 * If CO_DRIVER_MULTI_INTERFACE is disabled, then CO_CANmodule_init()
 * adds single socketCAN interface specified by CANptr argument. In case of
 * failure, CO_CANmodule_init() returns CO_ERROR_SYSCALL.
 *
 * If CO_DRIVER_MULTI_INTERFACE is enabled, then CO_CANmodule_init()
 * ignores CANptr argument. Interfaces must be added by
 * CO_CANmodule_addInterface() function after CO_CANmodule_init().
 *
 * This is not intended to realize interface redundancy!!!
 */
/* #define CO_DRIVER_MULTI_INTERFACE */

/*
 * @name CAN bus error reporting
 *
 * CO_DRIVER_ERROR_REPORTING enabled adds support for socketCAN error detection
 * and handling functions inside the driver. This is needed when you have
 * CANopen with "0" connected nodes as a use case, as this is normally
 * forbidden in CAN.
 *
 * you need to enable error reporting in your kernel driver using
 * "ip link set canX type can berr-reporting on". Of course, the kernel
 * driver for your hardware needs this functionality to be implemented...
 */
#ifndef CO_DRIVER_ERROR_REPORTING_DISABLE
#define CO_DRIVER_ERROR_REPORTING
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <endian.h>
#include <pthread.h>
#include <linux/can.h>
#include <net/if.h>

#include "CO_notify_pipe.h"
#ifdef CO_DRIVER_ERROR_REPORTING
    #include "CO_error.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Basic definitions */
#ifdef __BYTE_ORDER
#if __BYTE_ORDER == __LITTLE_ENDIAN
    #define CO_LITTLE_ENDIAN
#else
    #define CO_BIG_ENDIAN
#endif
#endif
/* #define CO_USE_LEDS */
/* NULL is defined in stddef.h */
/* true and false are defined in stdbool.h */
/* int8_t to uint64_t are defined in stdint.h */
typedef unsigned char           bool_t;
typedef float                   float32_t;
typedef long double             float64_t;
typedef char                    char_t;
typedef unsigned char           oChar_t;
typedef unsigned char           domain_t;


/* CAN receive message structure as aligned in socketCAN. */
typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t padding[3];
    uint8_t data[8];
} CO_CANrxMsg_t;

/* Access to received CAN message */
static inline uint16_t CO_CANrxMsg_readIdent(void *rxMsg);
static inline uint8_t CO_CANrxMsg_readDLC(void *rxMsg);
static inline uint8_t *CO_CANrxMsg_readData(void *rxMsg);

/* Received message object */
typedef struct {
    uint32_t ident;
    uint32_t mask;
    void *object;
    void (*CANrx_callback)(void *object, void *message);
#ifdef CO_DRIVER_MULTI_INTERFACE
    /* info about last received message */
    int32_t             CANbaseAddress; /* CAN Interface identifier */
    struct timespec     timestamp;      /* time of reception */
#endif
} CO_CANrx_t;

/* Transmit message object as aligned in socketCAN. */
typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t padding[3];     /* ensure alignment */
    uint8_t data[8];
    volatile bool_t syncFlag;
    /* info about transmit message */
    int32_t CANbaseAddress; /* CAN Interface identifier to use */
} CO_CANtx_t;


/* Max COB ID for standard frame format */
#define CO_CAN_MSG_SFF_MAX_COB_ID (1 << CAN_SFF_ID_BITS)

/* socketCAN interface object */
typedef struct {
    int32_t CANbaseAddress;     /* CAN Interface identifier */
    char ifName[IFNAMSIZ];      /* CAN Interface name */
    int fd;                     /* socketCAN file descriptor */
#ifdef CO_DRIVER_ERROR_REPORTING
    CO_CANinterfaceErrorhandler_t errorhandler;
#endif
} CO_CANinterface_t;

/* CAN module object */
typedef struct {
    /* List of can interfaces. From CO_CANmodule_init() or one per
     *  CO_CANmodule_addInterface() call */
    CO_CANinterface_t *CANinterfaces;
    uint32_t CANinterfaceCount; /* interface count */
    CO_CANrx_t *rxArray;
    uint16_t rxSize;
    struct can_filter *rxFilter;/* socketCAN filter list, one per rx buffer */
    uint32_t rxDropCount;       /* messages dropped on rx socket queue */
    CO_CANtx_t *txArray;
    uint16_t txSize;
    volatile bool_t CANnormal;
    void *em;
    CO_NotifyPipe_t *pipe;      /* Notification Pipe */
    int fdEpoll;                /* epoll FD */
    int fdTimerRead;            /* timer handle from CANrxWait() */
#ifdef CO_DRIVER_MULTI_INTERFACE
    /* Lookup tables Cob ID to rx/tx array index.
     *  Only feasible for SFF Messages. */
    uint32_t rxIdentToIndex[CO_CAN_MSG_SFF_MAX_COB_ID];
    uint32_t txIdentToIndex[CO_CAN_MSG_SFF_MAX_COB_ID];
#endif
} CO_CANmodule_t;


/* (un)lock critical section in CO_CANsend() - unused */
#define CO_LOCK_CAN_SEND()
#define CO_UNLOCK_CAN_SEND()

/* (un)lock critical section in CO_errorReport() or CO_errorReset() */
extern pthread_mutex_t CO_EMCY_mutex;
static inline int CO_LOCK_EMCY()    { return pthread_mutex_lock(&CO_EMCY_mutex); }
static inline void CO_UNLOCK_EMCY() { (void)pthread_mutex_unlock(&CO_EMCY_mutex); }

/* (un)lock critical section when accessing Object Dictionary */
extern pthread_mutex_t CO_OD_mutex;
static inline int CO_LOCK_OD()      { return pthread_mutex_lock(&CO_OD_mutex); }
static inline void CO_UNLOCK_OD()   { (void)pthread_mutex_unlock(&CO_OD_mutex); }

/* Synchronization between CAN receive and message processing threads. */
#define CO_MemoryBarrier() {__sync_synchronize();}
#define CO_CANrxNew_READ(rxNew) ((bool_t)rxNew)
#define CO_CANrxNew_SET(rxNew) {CO_MemoryBarrier(); rxNew = (void*)1L;}
#define CO_CANrxNew_CLEAR(rxNew) {CO_MemoryBarrier(); rxNew = (void*)0L;}


#ifdef CO_DRIVER_MULTI_INTERFACE
/*
 * Add socketCAN interface to can driver
 *
 * Function must be called after CO_CANmodule_init.
 *
 * @param CANmodule This object will be initialized.
 * @param CANbaseAddress CAN module base address.
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT,
 * CO_ERROR_SYSCALL or CO_ERROR_INVALID_STATE.
 */
CO_ReturnError_t CO_CANmodule_addInterface(CO_CANmodule_t *CANmodule,
                                           int32_t CANbaseAddress);

/*
 * Check on which interface the last message for one message buffer was received
 *
 * It is in the responsibility of the user to check that this information is
 * useful as some messages can be received at any time on any bus.
 *
 * @param CANmodule This object.
 * @param ident 11-bit standard CAN Identifier.
 * @param [out] CANbaseAddressRx message was received on this interface
 * @param [out] timestamp message was received at this time (system clock)
 *
 * @retval false message has never been received, therefore no base address
 * and timestamp are available
 * @retval true base address and timestamp are valid
 */
bool_t CO_CANrxBuffer_getInterface(CO_CANmodule_t *CANmodule,
                                   uint32_t ident,
                                   int32_t *CANbaseAddressRx,
                                   struct timespec *timestamp);

/*
 * Set which interface should be used for message buffer transmission
 *
 * It is in the responsibility of the user to ensure that the correct interface
 * is used. Some messages need to be transmitted on all interfaces.
 *
 * If given interface is unknown or "-1" is used, a message is transmitted on
 * all available interfaces.
 *
 * @param CANmodule This object.
 * @param ident 11-bit standard CAN Identifier.
 * @param CANbaseAddressTx use this interface. -1 = not specified
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_CANtxBuffer_setInterface(CO_CANmodule_t *CANmodule,
                                             uint32_t ident,
                                             int32_t CANbaseAddressTx);
#endif


/*
 * Functions receives CAN messages. It is blocking.
 *
 * This function can be used in two ways
 * - automatic mode (call callback that is set by CO_CANrxBufferInit() function)
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
int32_t CO_CANrxWait(CO_CANmodule_t *CANmodule,
                     int fdTimer,
                     CO_CANrxMsg_t *buffer);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_DRIVER_TARGET */
