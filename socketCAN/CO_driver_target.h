/**
 * Linux socketCAN specific definitions for CANopenNode.
 *
 * @file        CO_driver_target.h
 * @ingroup     CO_socketCAN_driver_target
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
 * for common definitions below. */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <endian.h>
#include <pthread.h>
#include <linux/can.h>
#include <net/if.h>

#ifdef CO_DRIVER_CUSTOM
#include "CO_driver_custom.h"
#endif
#include "CO_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stack configuration override from CO_driver.h.
 * For more information see file CO_config.h. */
#ifndef CO_CONFIG_NMT
#define CO_CONFIG_NMT (CO_CONFIG_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_FLAG_TIMERNEXT | \
                       CO_CONFIG_NMT_CALLBACK_CHANGE | \
                       CO_CONFIG_NMT_MASTER)
#endif

#ifndef CO_CONFIG_SDO
#define CO_CONFIG_SDO (CO_CONFIG_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_FLAG_TIMERNEXT | \
                       CO_CONFIG_SDO_SEGMENTED | \
                       CO_CONFIG_SDO_BLOCK)
#endif

#ifndef CO_CONFIG_SDO_BUFFER_SIZE
#define CO_CONFIG_SDO_BUFFER_SIZE 1800
#endif

#ifndef CO_CONFIG_EM
#define CO_CONFIG_EM (CO_CONFIG_FLAG_CALLBACK_PRE | \
                      CO_CONFIG_FLAG_TIMERNEXT | \
                      CO_CONFIG_EM_CONSUMER)
#endif

#ifndef CO_CONFIG_HB_CONS
#define CO_CONFIG_HB_CONS (CO_CONFIG_FLAG_CALLBACK_PRE | \
                           CO_CONFIG_FLAG_TIMERNEXT | \
                           CO_CONFIG_HB_CONS_CALLBACK_CHANGE)
#endif

#ifndef CO_CONFIG_PDO
#define CO_CONFIG_PDO (CO_CONFIG_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_FLAG_TIMERNEXT | \
                       CO_CONFIG_PDO_SYNC_ENABLE | \
                       CO_CONFIG_RPDO_CALLS_EXTENSION | \
                       CO_CONFIG_TPDO_CALLS_EXTENSION)
#endif

#ifndef CO_CONFIG_SYNC
#define CO_CONFIG_SYNC (CO_CONFIG_FLAG_CALLBACK_PRE | \
                        CO_CONFIG_FLAG_TIMERNEXT)
#endif

#ifndef CO_CONFIG_SDO_CLI
#define CO_CONFIG_SDO_CLI (CO_CONFIG_FLAG_CALLBACK_PRE | \
                           CO_CONFIG_FLAG_TIMERNEXT | \
                           CO_CONFIG_SDO_CLI_SEGMENTED | \
                           CO_CONFIG_SDO_CLI_BLOCK | \
                           CO_CONFIG_SDO_CLI_LOCAL)
#endif

#ifndef CO_CONFIG_SDO_CLI_BUFFER_SIZE
#define CO_CONFIG_SDO_CLI_BUFFER_SIZE 1000
#endif

#ifndef CO_CONFIG_TIME
#define CO_CONFIG_TIME (CO_CONFIG_FLAG_CALLBACK_PRE)
#endif

#ifndef CO_CONFIG_LEDS
#define CO_CONFIG_LEDS (CO_CONFIG_FLAG_TIMERNEXT | \
                        CO_CONFIG_LEDS_ENABLE)
#endif

#ifndef CO_CONFIG_LSS
#define CO_CONFIG_LSS (CO_CONFIG_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_LSS_SLAVE | \
                       CO_CONFIG_LSS_SLAVE_FASTSCAN_DIRECT_RESPOND | \
                       CO_CONFIG_LSS_MASTER)
#endif

#ifndef CO_CONFIG_GTW
#define CO_CONFIG_GTW (CO_CONFIG_GTW_ASCII | \
                       CO_CONFIG_GTW_ASCII_SDO | \
                       CO_CONFIG_GTW_ASCII_NMT | \
                       CO_CONFIG_GTW_ASCII_LSS | \
                       CO_CONFIG_GTW_ASCII_LOG | \
                       CO_CONFIG_GTW_ASCII_ERROR_DESC | \
                       CO_CONFIG_GTW_ASCII_PRINT_HELP | \
                       CO_CONFIG_GTW_ASCII_PRINT_LEDS)
#define CO_CONFIG_GTW_BLOCK_DL_LOOP 3
#define CO_CONFIG_GTWA_COMM_BUF_SIZE 2000
#define CO_CONFIG_GTWA_LOG_BUF_SIZE 10000
#endif


/**
 * @defgroup CO_socketCAN_driver_target CO_driver_target.h
 * @ingroup CO_socketCAN
 * @{
 *
 * Linux socketCAN specific @ref CO_driver definitions for CANopenNode.
 */

/**
 * Multi interface support
 *
 * Enable this to use interface combining at driver level. This
 * adds functions to broadcast/selective transmit messages on the
 * given interfaces as well as combining all received message into
 * one queue.
 *
 * If CO_DRIVER_MULTI_INTERFACE is set to 0, then CO_CANmodule_init()
 * adds single socketCAN interface specified by CANptr argument. In case of
 * failure, CO_CANmodule_init() returns CO_ERROR_SYSCALL.
 *
 * If CO_DRIVER_MULTI_INTERFACE is set to 1, then CO_CANmodule_init()
 * ignores CANptr argument. Interfaces must be added by
 * CO_CANmodule_addInterface() function after CO_CANmodule_init().
 *
 * Macro is set to 0 (disabled) by default. It can be overridden.
 *
 * This is not intended to realize interface redundancy!!!
 */
#ifndef CO_DRIVER_MULTI_INTERFACE
#define CO_DRIVER_MULTI_INTERFACE 0
#endif

/**
 * CAN bus error reporting
 *
 * CO_DRIVER_ERROR_REPORTING enabled adds support for socketCAN error detection
 * and handling functions inside the driver. This is needed when you have
 * CANopen with "0" connected nodes as a use case, as this is normally
 * forbidden in CAN.
 *
 * Macro is set to 1 (enabled) by default. It can be overridden.
 *
 * you need to enable error reporting in your kernel driver using:
 * @code{.sh}
 * ip link set canX type can berr-reporting on
 * @endcode
 * Of course, the kernel driver for your hardware needs this functionality to be
 * implemented...
 */
#ifndef CO_DRIVER_ERROR_REPORTING
#define CO_DRIVER_ERROR_REPORTING 1
#endif

/* skip this section for Doxygen, because it is documented in CO_driver.h */
#ifndef CO_DOXYGEN

/* Basic definitions */
#ifdef __BYTE_ORDER
#if __BYTE_ORDER == __LITTLE_ENDIAN
    #define CO_LITTLE_ENDIAN
    #define CO_SWAP_16(x) x
    #define CO_SWAP_32(x) x
    #define CO_SWAP_64(x) x
#else
    #define CO_BIG_ENDIAN
    #include <byteswap.h>
    #define CO_SWAP_16(x) bswap_16(x)
    #define CO_SWAP_32(x) bswap_32(x)
    #define CO_SWAP_64(x) bswap_64(x)
#endif
#endif
/* #define CO_USE_LEDS */
/* NULL is defined in stddef.h */
/* true and false are defined in stdbool.h */
/* int8_t to uint64_t are defined in stdint.h */
typedef unsigned char           bool_t;
typedef float                   float32_t;
typedef double                  float64_t;
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
static inline uint16_t CO_CANrxMsg_readIdent(void *rxMsg) {
    CO_CANrxMsg_t *rxMsgCasted = (CO_CANrxMsg_t *)rxMsg;
    return (uint16_t) (rxMsgCasted->ident & CAN_SFF_MASK);
}
static inline uint8_t CO_CANrxMsg_readDLC(void *rxMsg) {
    CO_CANrxMsg_t *rxMsgCasted = (CO_CANrxMsg_t *)rxMsg;
    return (uint8_t) (rxMsgCasted->DLC);
}
static inline uint8_t *CO_CANrxMsg_readData(void *rxMsg) {
    CO_CANrxMsg_t *rxMsgCasted = (CO_CANrxMsg_t *)rxMsg;
    return (uint8_t *) (rxMsgCasted->data);
}


/* Received message object */
typedef struct {
    uint32_t ident;
    uint32_t mask;
    void *object;
    void (*CANrx_callback)(void *object, void *message);
    const void         *CANptr;         /* CAN Interface identifier from last
                                           message */
    struct timespec     timestamp;      /* time of reception of last message */
} CO_CANrx_t;

/* Transmit message object as aligned in socketCAN. */
typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t padding[3];     /* ensure alignment */
    uint8_t data[8];
    volatile bool_t bufferFull; /* not used */
    volatile bool_t syncFlag;   /* info about transmit message */
    const void *CANptr;         /* CAN Interface identifier to use */
} CO_CANtx_t;


/* Max COB ID for standard frame format */
#define CO_CAN_MSG_SFF_MAX_COB_ID (1 << CAN_SFF_ID_BITS)

/* socketCAN interface object */
typedef struct {
    const void *CANptr;         /* CAN Interface identifier */
    char ifName[IFNAMSIZ];      /* CAN Interface name */
    int fd;                     /* socketCAN file descriptor */
#if CO_DRIVER_ERROR_REPORTING > 0 || defined CO_DOXYGEN
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
    uint16_t CANerrorStatus;
    volatile bool_t CANnormal;
    int fdEvent;                /* notification event file descriptor */
    int fdEpoll;                /* epoll FD for event, CANrx sockets in all
                                   interfaces and fdTimerRead */
    int fdTimerRead;            /* timer handle from CANrxWait() */
#if CO_DRIVER_MULTI_INTERFACE > 0 || defined CO_DOXYGEN
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
static inline int CO_LOCK_EMCY() {
    return pthread_mutex_lock(&CO_EMCY_mutex);
}
static inline void CO_UNLOCK_EMCY() {
    (void)pthread_mutex_unlock(&CO_EMCY_mutex);
}

/* (un)lock critical section when accessing Object Dictionary */
extern pthread_mutex_t CO_OD_mutex;
static inline int CO_LOCK_OD() {
    return pthread_mutex_lock(&CO_OD_mutex);
}
static inline void CO_UNLOCK_OD() {
    (void)pthread_mutex_unlock(&CO_OD_mutex);
}

/* Synchronization between CAN receive and message processing threads. */
#define CO_MemoryBarrier() {__sync_synchronize();}
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew) {CO_MemoryBarrier(); rxNew = (void*)1L;}
#define CO_FLAG_CLEAR(rxNew) {CO_MemoryBarrier(); rxNew = NULL;}

#endif /* CO_DOXYGEN */


#if CO_DRIVER_MULTI_INTERFACE > 0 || defined CO_DOXYGEN
/**
 * Add socketCAN interface to can driver
 *
 * Function must be called after CO_CANmodule_init.
 *
 * @param CANmodule This object will be initialized.
 * @param CANptr CAN module interface index (return value if_nametoindex(), NO pointer!).
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT,
 * CO_ERROR_SYSCALL or CO_ERROR_INVALID_STATE.
 */
CO_ReturnError_t CO_CANmodule_addInterface(CO_CANmodule_t *CANmodule,
                                           const void *CANptr);

/**
 * Check on which interface the last message for one message buffer was received
 *
 * It is in the responsibility of the user to check that this information is
 * useful as some messages can be received at any time on any bus.
 *
 * @param CANmodule This object.
 * @param ident 11-bit standard CAN Identifier.
 * @param [out] CANptrRx message was received on this interface
 * @param [out] timestamp message was received at this time (system clock)
 *
 * @retval false message has never been received, therefore no base address
 * and timestamp are available
 * @retval true base address and timestamp are valid
 */
bool_t CO_CANrxBuffer_getInterface(CO_CANmodule_t *CANmodule,
                                   uint16_t ident,
                                   const void **const CANptrRx,
                                   struct timespec *timestamp);

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
 * @param CANptrTx use this interface. NULL = not specified
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_CANtxBuffer_setInterface(CO_CANmodule_t *CANmodule,
                                             uint16_t ident,
                                             const void *CANptrTx);
#endif /* CO_DRIVER_MULTI_INTERFACE */


/**
 * Functions receives CAN messages (blocking)
 *
 * This function waits for received CAN message, CAN error frame, notification
 * event or fdTimer expiration. In case of CAN message it searches _rxArray_
  from* CO_CANmodule_t and if matched it calls the corresponding CANrx_callback,
 * optionally copies received CAN message to _buffer_ and returns index of
 * matched _rxArray_.
 *
 * This function can be used in two ways, which can be combined:
 * - automatic mode: If CANrx_callback is specified for matched _rxArray_, then
 *   calls its callback.
 * - manual mode: evaluate message filters, return received message
 *
 * @param CANmodule This object.
 * @param fdTimer File descriptor with activated timeout. If set to -1, then
 *                timer will not be used. File descriptor must be read
 *                externally if retval == -1! Read must be nonblocking and
 *                provides number of timer expirations since last read.
 * @param [out] buffer Storage for received message or _NULL_ if not used.
 * @retval >= 0 index of received message in array from CO_CANmodule_t
 *              _rxArray_, copy of CAN message is available in _buffer_.
 * @retval -1 no message received (timer expired or notification event or error)
 */
int32_t CO_CANrxWait(CO_CANmodule_t* CANmodule, int fdTimer, CO_CANrxMsg_t* buffer);

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DRIVER_TARGET */
