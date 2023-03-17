/**
 * Interface between CAN hardware and CANopenNode.
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

#include <string.h>

#include "CO_config.h"
#include "CO_driver_target.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stack configuration default global values.
 * For more information see file CO_config.h. */
#ifndef CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE
 #define CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE (0)
#endif
#ifndef CO_CONFIG_GLOBAL_RT_FLAG_CALLBACK_PRE
 #define CO_CONFIG_GLOBAL_RT_FLAG_CALLBACK_PRE (0)
#endif
#ifndef CO_CONFIG_GLOBAL_FLAG_TIMERNEXT
 #define CO_CONFIG_GLOBAL_FLAG_TIMERNEXT (0)
#endif
#ifndef CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC
 #define CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC CO_CONFIG_FLAG_OD_DYNAMIC
#endif
#ifdef CO_DEBUG_COMMON
 #if (CO_CONFIG_DEBUG) & CO_CONFIG_DEBUG_SDO_CLIENT
  #define CO_DEBUG_SDO_CLIENT(msg) CO_DEBUG_COMMON(msg)
 #endif
 #if (CO_CONFIG_DEBUG) & CO_CONFIG_DEBUG_SDO_SERVER
  #define CO_DEBUG_SDO_SERVER(msg) CO_DEBUG_COMMON(msg)
 #endif
#endif

/**
 * @defgroup CO_driver Driver
 * Interface between CAN hardware and CANopenNode.
 *
 * @ingroup CO_CANopen_301
 * @{
 * CANopenNode is designed for speed and portability. It runs efficiently on
 * devices from simple 16-bit microcontrollers to PC computers. It can run in
 * multiple threads. Reception of CAN messages is pre-processed with very fast
 * functions. Time critical objects, such as PDO or SYNC are processed in
 * real-time thread and other objects are processed in normal thread. See
 * Flowchart in [README.md](index.html) for more information.
 *
 * @anchor CO_obj
 * #### CANopenNode Object
 * CANopenNode is implemented as a collection of different objects, for example
 * SDO, SYNC, Emergency, PDO, NMT, Heartbeat, etc. Code is written in C language
 * and tries to be object oriented. So each CANopenNode Object is implemented in
 * a pair of .h/.c files. It basically contains a structure with all necessary
 * variables and some functions which operates on it. CANopenNode Object is
 * usually connected with one or more CAN receive or transmit Message Objects.
 * (CAN message Object is a CAN message with specific 11-bit CAN identifier
 * (usually one fixed or a range).)
 *
 * #### Hardware interface of CANopenNode
 * It consists of minimum three files:
 * - **CO_driver.h** file declares common functions. This file is part of the
 * CANopenNode. It is included from each .c file from CANopenNode.
 * - **CO_driver_target.h** file declares microcontroller specific type
 * declarations and defines some macros, which are necessary for CANopenNode.
 * This file is included from CO_driver.h.
 * - **CO_driver.c** file defines functions declared in CO_driver.h.
 *
 * **CO_driver_target.h** and **CO_driver.c** files are specific for each
 * different microcontroller and are not part of CANopenNode. There are separate
 * projects for different microcontrollers, which usually include CANopenNode as
 * a git submodule. CANopenNode only includes those two files in the `example`
 * directory and they are basically empty. It should be possible to compile the
 * `CANopenNode/example` on any system, however compiled program is not usable.
 * CO_driver.h contains documentation for all necessary macros, types and
 * functions.
 *
 * See [CANopenNode/Wiki](https://github.com/CANopenNode/CANopenNode/wiki) for a
 * known list of available implementations of CANopenNode on different systems
 * and microcontrollers. Everybody is welcome to extend the list with a link to
 * his own implementation.
 *
 * Implementation of the hardware interface for specific microcontroller is not
 * always an easy task. For reliable and efficient operation it is necessary to
 * know some parts of the target microcontroller in detail (for example threads
 * (or interrupts), CAN module, etc.).
 */

/** Major version number of CANopenNode */
#define CO_VERSION_MAJOR 4
/** Minor version number of CANopenNode */
#define CO_VERSION_MINOR 0


/* Macros and declarations in following part are only used for documentation. */
#ifdef CO_DOXYGEN
/**
 * @defgroup CO_dataTypes Basic definitions
 * @{
 *
 * Target specific basic definitions and data types.
 *
 * Must be defined in the **CO_driver_target.h** file.
 *
 * Depending on processor or compiler architecture, one of the two macros must
 * be defined: CO_LITTLE_ENDIAN or CO_BIG_ENDIAN. CANopen itself is little
 * endian.
 *
 * Basic data types may be specified differently on different architectures.
 * Usually `true` and `false` are defined in `<stdbool.h>`, `NULL` is defined in
 * `<stddef.h>`, `int8_t` to `uint64_t` are defined in `<stdint.h>`.
 */
/** CO_LITTLE_ENDIAN or CO_BIG_ENDIAN must be defined */
#define CO_LITTLE_ENDIAN
/** Macro must swap bytes, if CO_BIG_ENDIAN is defined */
#define CO_SWAP_16(x) x
/** Macro must swap bytes, if CO_BIG_ENDIAN is defined */
#define CO_SWAP_32(x) x
/** Macro must swap bytes, if CO_BIG_ENDIAN is defined */
#define CO_SWAP_64(x) x
/** NULL, for general usage */
#define NULL (0)
/** Logical true, for general use */
#define true 1
/** Logical false, for general use */
#define false 0
/** Boolean data type for general use */
typedef uint_fast8_t bool_t;
/** INTEGER8 in CANopen (0002h), 8-bit signed integer */
typedef signed char int8_t;
/** INTEGER16 in CANopen (0003h), 16-bit signed integer */
typedef signed int int16_t;
/** INTEGER32 in CANopen (0004h), 32-bit signed integer */
typedef signed long int int32_t;
/** INTEGER64 in CANopen (0015h), 64-bit signed integer */
typedef signed long long int int64_t;
/** UNSIGNED8 in CANopen (0005h), 8-bit unsigned integer */
typedef unsigned char uint8_t;
/** UNSIGNED16 in CANopen (0006h), 16-bit unsigned integer */
typedef unsigned int uint16_t;
/** UNSIGNED32 in CANopen (0007h), 32-bit unsigned integer */
typedef unsigned long int uint32_t;
/** UNSIGNED64 in CANopen (001Bh), 64-bit unsigned integer */
typedef unsigned long long int uint64_t;
/** REAL32 in CANopen (0008h), single precision floating point value, 32-bit */
typedef float float32_t;
/** REAL64 in CANopen (0011h), double precision floating point value, 64-bit */
typedef double float64_t;
/** @} */


/**
 * @defgroup CO_CAN_Message_reception Reception of CAN messages
 * @{
 *
 * Target specific definitions and description of CAN message reception
 *
 * CAN messages in CANopenNode are usually received by its own thread or higher
 * priority interrupt. Received CAN messages are first filtered by hardware or
 * by software. Thread then examines its 11-bit CAN-id and mask and determines,
 * to which \ref CO_obj "CANopenNode Object" it belongs to. After that it calls
 * predefined CANrx_callback() function, which quickly pre-processes the message
 * and fetches the relevant data. CANrx_callback() function is defined by each
 * \ref CO_obj "CANopenNode Object" separately. Pre-processed fetched data are
 * later processed in another thread.
 *
 * If \ref CO_obj "CANopenNode Object" reception of specific CAN message, it
 * must first configure its own CO_CANrx_t object with the CO_CANrxBufferInit()
 * function.
 */

/**
 * CAN receive callback function which pre-processes received CAN message
 *
 * It is called by fast CAN receive thread. Each \ref CO_obj "CANopenNode
 * Object" defines its own and registers it with CO_CANrxBufferInit(), by
 * passing function pointer.
 *
 * @param object pointer to specific \ref CO_obj "CANopenNode Object",
 * registered with CO_CANrxBufferInit()
 * @param rxMsg pointer to received CAN message
 */
void CANrx_callback(void *object, void *rxMsg);

/**
 * CANrx_callback() can read CAN identifier from received CAN message
 *
 * Must be defined in the **CO_driver_target.h** file.
 *
 * This is target specific function and is specific for specific
 * microcontroller. It is best to implement it by using inline function or
 * macro. `rxMsg` parameter should cast to a pointer to structure. For best
 * efficiency structure may have the same alignment as CAN registers inside CAN
 * module.
 *
 * @param rxMsg Pointer to received message
 * @return 11-bit CAN standard identifier.
 */
static inline uint16_t CO_CANrxMsg_readIdent(void *rxMsg) {
    return 0;
}

/**
 * CANrx_callback() can read Data Length Code from received CAN message
 *
 * See also CO_CANrxMsg_readIdent():
 *
 * @param rxMsg Pointer to received message
 * @return data length in bytes (0 to 8)
 */
static inline uint8_t CO_CANrxMsg_readDLC(void *rxMsg) {
    return 0;
}

/**
 * CANrx_callback() can read pointer to data from received CAN message
 *
 * See also CO_CANrxMsg_readIdent():
 *
 * @param rxMsg Pointer to received message
 * @return pointer to data buffer
 */
static inline uint8_t *CO_CANrxMsg_readData(void *rxMsg) {
    return NULL;
}

/**
 * Configuration object for CAN received message for specific \ref CO_obj
 * "CANopenNode Object".
 *
 * Must be defined in the **CO_driver_target.h** file.
 *
 * Data fields of this structure are used exclusively by the driver. Usually it
 * has the following data fields, but they may differ for different
 * microcontrollers. Array of multiple CO_CANrx_t objects is included inside
 * CO_CANmodule_t.
 */
typedef struct {
    uint16_t ident; /**< Standard CAN Identifier (bits 0..10) + RTR (bit 11) */
    uint16_t mask;  /**< Standard CAN Identifier mask with the same alignment as
                       ident */
    void *object;   /**< \ref CO_obj "CANopenNode Object" initialized in from
                       CO_CANrxBufferInit() */
    void (*pCANrx_callback)(
        void *object, void *message); /**< Pointer to CANrx_callback()
                                         initialized in CO_CANrxBufferInit() */
} CO_CANrx_t;
/** @} */


/**
 * @defgroup CO_CAN_Message_transmission Transmission of CAN messages
 * @{
 *
 * Target specific definitions and description of CAN message transmission
 *
 * If \ref CO_obj "CANopenNode Object" needs transmitting CAN message, it must
 * first configure its own CO_CANtx_t object with the CO_CANtxBufferInit()
 * function. CAN message can then be sent with CO_CANsend() function. If at that
 * moment CAN transmit buffer inside microcontroller's CAN module is free,
 * message is copied directly to the CAN module. Otherwise CO_CANsend() function
 * sets _bufferFull_ flag to true. Message will be then sent by CAN TX interrupt
 * as soon as CAN module is freed. Until message is not copied to CAN module,
 * its contents must not change. If there are multiple CO_CANtx_t objects with
 * _bufferFull_ flag set to true, then CO_CANtx_t with lower index will be sent
 * first.
 */

/**
 * Configuration object for CAN transmit message for specific \ref CO_obj
 * "CANopenNode Object".
 *
 * Must be defined in the **CO_driver_target.h** file.
 *
 * Data fields of this structure are used exclusively by the driver. Usually it
 * has the following data fields, but they may differ for different
 * microcontrollers. Array of multiple CO_CANtx_t objects is included inside
 * CO_CANmodule_t.
 */
typedef struct {
    uint32_t ident;             /**< CAN identifier as aligned in CAN module */
    uint8_t DLC;                /**< Length of CAN message */
    uint8_t data[8];            /**< 8 data bytes */
    volatile bool_t bufferFull; /**< True if previous message is still in the
                                     buffer */
    volatile bool_t syncFlag;   /**< Synchronous PDO messages has this flag set.
                  It prevents them to be sent outside the synchronous window */
} CO_CANtx_t;
/** @} */


/**
 * Complete CAN module object.
 *
 * Must be defined in the **CO_driver_target.h** file.
 *
 * Usually it has the following data fields, but they may differ for different
 * microcontrollers.
 */
typedef struct {
    void *CANptr;                      /**< From CO_CANmodule_init() */
    CO_CANrx_t *rxArray;               /**< From CO_CANmodule_init() */
    uint16_t rxSize;                   /**< From CO_CANmodule_init() */
    CO_CANtx_t *txArray;               /**< From CO_CANmodule_init() */
    uint16_t txSize;                   /**< From CO_CANmodule_init() */
    uint16_t CANerrorStatus;           /**< CAN error status bitfield,
                                            see @ref CO_CAN_ERR_status_t */
    volatile bool_t CANnormal;         /**< CAN module is in normal mode */
    volatile bool_t useCANrxFilters;   /**< Value different than zero indicates,
            that CAN module hardware filters are used for CAN reception. If
            there is not enough hardware filters, they won't be used. In this
            case will be *all* received CAN messages processed by software. */
    volatile bool_t bufferInhibitFlag; /**< If flag is true, then message in
            transmit buffer is synchronous PDO message, which will be aborted,
            if CO_clearPendingSyncPDOs() function will be called by application.
            This may be necessary if Synchronous window time was expired. */
    volatile bool_t firstCANtxMessage; /**< Equal to 1, when the first
            transmitted message (bootup message) is in CAN TX buffers */
    volatile uint16_t CANtxCount;      /**< Number of messages in transmit
            buffer, which are waiting to be copied to the CAN module */
    uint32_t errOld;                   /**< Previous state of CAN errors */
} CO_CANmodule_t;


/**
 * Data storage object for one entry.
 *
 * Must be defined in the **CO_driver_target.h** file.
 *
 * For more information on Data storage see @ref CO_storage or **CO_storage.h**
 * file. Structure members documented here are always required or required with
 * @ref CO_storage_eeprom. Target system may add own additional, hardware
 * specific variables.
 */
typedef struct {
    /** Address of data to store, always required. */
    void *addr;
    /** Length of data to store, always required. */
    size_t len;
    /** Sub index in OD objects 1010 and 1011, from 2 to 127. Writing
     * 0x65766173 to 1010,subIndexOD will store data to non-volatile memory.
     * Writing 0x64616F6C to 1011,subIndexOD will restore default data, always
     * required. */
    uint8_t subIndexOD;
    /** Attribute from @ref CO_storage_attributes_t, always required. */
    uint8_t attr;
    /** Pointer to storage module, target system specific usage, required with
     * @ref CO_storage_eeprom. */
    void *storageModule;
    /** CRC checksum of the data stored in eeprom, set on store, required with
     * @ref CO_storage_eeprom. */
    uint16_t crc;
    /** Address of entry signature inside eeprom, set by init, required with
     * @ref CO_storage_eeprom. */
    size_t eepromAddrSignature;
    /** Address of data inside eeprom, set by init, required with
     * @ref CO_storage_eeprom. */
    size_t eepromAddr;
    /** Offset of next byte being updated by automatic storage, required with
     * @ref CO_storage_eeprom. */
    size_t offset;
    /** Additional target specific parameters, optional. */
    void *additionalParameters;
} CO_storage_entry_t;


/**
 * @defgroup CO_critical_sections Critical sections
 * @{
 *
 * Protection of critical sections in multi-threaded operation.
 *
 * CANopenNode is designed to run in different threads, as described in
 * [README.md](index.html). Threads are implemented differently in different
 * systems. In microcontrollers threads are interrupts with different
 * priorities, for example. It is necessary to protect sections, where different
 * threads access to the same resource. In simple systems interrupts or
 * scheduler may be temporary disabled between access to the shared resource.
 * Otherwise mutexes or semaphores can be used.
 *
 * #### Reentrant functions
 * Functions CO_CANsend() from C_driver.h, and CO_error() from CO_Emergency.h
 * may be called from different threads. Critical sections must be protected.
 * Either by disabling scheduler or interrupts or by mutexes or semaphores.
 * Lock/unlock macro is called with pointer to CAN module, which may be used
 * inside.
 *
 * #### Object Dictionary variables
 * In general, there are two threads, which accesses OD variables: mainline
 * (initialization, storage, SDO access) and timer (PDO access). CANopenNode
 * uses locking mechanism, where SDO server (or other mainline code) prevents
 * execution of the real-time thread at the moment it reads or writes OD
 * variable. CO_LOCK_OD(CAN_MODULE) and CO_UNLOCK_OD(CAN_MODULE) macros
 * are used to protect:
 * - Whole real-time thread,
 * - SDO server protects read/write access to OD variable.
 *   Locking of long OD variables, not accessible from real-time thread, may
 *   block RT thread.
 * - Any mainline code, which accesses PDO-mappable OD variable, must protect
 *   read/write with locking macros. Use @ref OD_mappable() for check.
 * - Other cases, where non-PDO-mappable OD variable is used inside real-time
 *   thread by some other part of the user application must be considered with
 *   special care. Also when there are multiple threads accessing the OD (e.g.
 *   when using a RTOS), you should always lock the OD.
 *
 * #### Synchronization functions for CAN receive
 * After CAN message is received, it is pre-processed in CANrx_callback(), which
 * copies some data into appropriate object and at the end sets **new_message**
 * flag. This flag is then pooled in another thread, which further processes the
 * message. The problem is, that compiler optimization may shuffle memory
 * operations, so it is necessary to ensure, that **new_message** flag is surely
 * set at the end. It is necessary to use [Memory
 * barrier](https://en.wikipedia.org/wiki/Memory_barrier).
 *
 * If receive function runs inside IRQ, no further synchronization is needed.
 * Otherwise, some kind of synchronization has to be included. The following
 * example uses GCC builtin memory barrier `__sync_synchronize()`. More
 * information can be found
 * [here](https://stackoverflow.com/questions/982129/what-does-sync-synchronize-do#982179).
 */

/** Lock critical section in CO_CANsend() */
#define CO_LOCK_CAN_SEND(CAN_MODULE)
/** Unlock critical section in CO_CANsend() */
#define CO_UNLOCK_CAN_SEND(CAN_MODULE)
/** Lock critical section in CO_errorReport() or CO_errorReset() */
#define CO_LOCK_EMCY(CAN_MODULE)
/** Unlock critical section in CO_errorReport() or CO_errorReset() */
#define CO_UNLOCK_EMCY(CAN_MODULE)
/** Lock critical section when accessing Object Dictionary */
#define CO_LOCK_OD(CAN_MODULE)
/** Unock critical section when accessing Object Dictionary */
#define CO_UNLOCK_OD(CAN_MODULE)

/** Check if new message has arrived */
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
/** Set new message flag */
#define CO_FLAG_SET(rxNew) { __sync_synchronize(); rxNew = (void *)1L; }
/** Clear new message flag */
#define CO_FLAG_CLEAR(rxNew) { __sync_synchronize(); rxNew = NULL; }

/** @} */
#endif /* CO_DOXYGEN */


/**
 * Default CANopen identifiers.
 *
 * Default CANopen identifiers for CANopen communication objects. Same as
 * 11-bit addresses of CAN messages. These are default identifiers and
 * can be changed in CANopen. Especially PDO identifiers are configured
 * in PDO linking phase of the CANopen network configuration.
 */
typedef enum {
    CO_CAN_ID_NMT_SERVICE = 0x000, /**< 0x000, Network management */
    CO_CAN_ID_GFC = 0x001,         /**< 0x001, Global fail-safe command */
    CO_CAN_ID_SYNC = 0x080,        /**< 0x080, Synchronous message */
    CO_CAN_ID_EMERGENCY = 0x080,   /**< 0x080, Emergency messages (+nodeID) */
    CO_CAN_ID_TIME = 0x100,        /**< 0x100, Time message */
    CO_CAN_ID_SRDO_1 = 0x0FF,      /**< 0x0FF, Default SRDO1 (+2*nodeID) */
    CO_CAN_ID_TPDO_1 = 0x180,      /**< 0x180, Default TPDO1 (+nodeID) */
    CO_CAN_ID_RPDO_1 = 0x200,      /**< 0x200, Default RPDO1 (+nodeID) */
    CO_CAN_ID_TPDO_2 = 0x280,      /**< 0x280, Default TPDO2 (+nodeID) */
    CO_CAN_ID_RPDO_2 = 0x300,      /**< 0x300, Default RPDO2 (+nodeID) */
    CO_CAN_ID_TPDO_3 = 0x380,      /**< 0x380, Default TPDO3 (+nodeID) */
    CO_CAN_ID_RPDO_3 = 0x400,      /**< 0x400, Default RPDO3 (+nodeID) */
    CO_CAN_ID_TPDO_4 = 0x480,      /**< 0x480, Default TPDO4 (+nodeID) */
    CO_CAN_ID_RPDO_4 = 0x500,      /**< 0x500, Default RPDO5 (+nodeID) */
    CO_CAN_ID_SDO_SRV = 0x580, /**< 0x580, SDO response from server (+nodeID) */
    CO_CAN_ID_SDO_CLI = 0x600, /**< 0x600, SDO request from client (+nodeID) */
    CO_CAN_ID_HEARTBEAT = 0x700,   /**< 0x700, Heartbeat message */
    CO_CAN_ID_LSS_SLV = 0x7E4,     /**< 0x7E4, LSS response from slave */
    CO_CAN_ID_LSS_MST = 0x7E5      /**< 0x7E5, LSS request from master */
} CO_Default_CAN_ID_t;


/**
 * Restricted CAN-IDs
 *
 * Macro for verifying 'Restricted CAN-IDs', as specified by standard CiA301.
 * They shall not be used for SYNC, TIME, EMCY, PDO and SDO.
 */
#ifndef CO_IS_RESTRICTED_CAN_ID
#define CO_IS_RESTRICTED_CAN_ID(CAN_ID) ((CAN_ID) <= 0x7F \
        || ((CAN_ID) >= 0x101 && (CAN_ID) <= 0x180) \
        || ((CAN_ID) >= 0x581 && (CAN_ID) <= 0x5FF) \
        || ((CAN_ID) >= 0x601 && (CAN_ID) <= 0x67F) \
        || ((CAN_ID) >= 0x6E0 && (CAN_ID) <= 0x6FF) \
        || (CAN_ID) >= 0x701)
#endif


/**
 * CAN error status bitmasks.
 *
 * CAN warning level is reached, if CAN transmit or receive error counter is
 * more or equal to 96. CAN passive level is reached, if counters are more or
 * equal to 128. Transmitter goes in error state 'bus off' if transmit error
 * counter is more or equal to 256.
 */
typedef enum {
    CO_CAN_ERRTX_WARNING = 0x0001,  /**< 0x0001, CAN transmitter warning */
    CO_CAN_ERRTX_PASSIVE = 0x0002,  /**< 0x0002, CAN transmitter passive */
    CO_CAN_ERRTX_BUS_OFF = 0x0004,  /**< 0x0004, CAN transmitter bus off */
    CO_CAN_ERRTX_OVERFLOW = 0x0008, /**< 0x0008, CAN transmitter overflow */

    CO_CAN_ERRTX_PDO_LATE = 0x0080, /**< 0x0080, TPDO is outside sync window */

    CO_CAN_ERRRX_WARNING = 0x0100,  /**< 0x0100, CAN receiver warning */
    CO_CAN_ERRRX_PASSIVE = 0x0200,  /**< 0x0200, CAN receiver passive */
    CO_CAN_ERRRX_OVERFLOW = 0x0800, /**< 0x0800, CAN receiver overflow */

    CO_CAN_ERR_WARN_PASSIVE = 0x0303/**< 0x0303, combination */
} CO_CAN_ERR_status_t;


/**
 * Return values of some CANopen functions. If function was executed
 * successfully it returns 0 otherwise it returns <0.
 */
typedef enum {
    CO_ERROR_NO = 0,                /**< Operation completed successfully */
    CO_ERROR_ILLEGAL_ARGUMENT = -1, /**< Error in function arguments */
    CO_ERROR_OUT_OF_MEMORY = -2,    /**< Memory allocation failed */
    CO_ERROR_TIMEOUT = -3,          /**< Function timeout */
    CO_ERROR_ILLEGAL_BAUDRATE = -4, /**< Illegal baudrate passed to function
                                         CO_CANmodule_init() */
    CO_ERROR_RX_OVERFLOW = -5,      /**< Previous message was not processed
                                         yet */
    CO_ERROR_RX_PDO_OVERFLOW = -6,  /**< previous PDO was not processed yet */
    CO_ERROR_RX_MSG_LENGTH = -7,    /**< Wrong receive message length */
    CO_ERROR_RX_PDO_LENGTH = -8,    /**< Wrong receive PDO length */
    CO_ERROR_TX_OVERFLOW = -9,      /**< Previous message is still waiting,
                                         buffer full */
    CO_ERROR_TX_PDO_WINDOW = -10,   /**< Synchronous TPDO is outside window */
    CO_ERROR_TX_UNCONFIGURED = -11, /**< Transmit buffer was not configured
                                         properly */
    CO_ERROR_OD_PARAMETERS = -12,   /**< Error in Object Dictionary parameters*/
    CO_ERROR_DATA_CORRUPT = -13,    /**< Stored data are corrupt */
    CO_ERROR_CRC = -14,             /**< CRC does not match */
    CO_ERROR_TX_BUSY = -15,         /**< Sending rejected because driver is
                                         busy. Try again */
    CO_ERROR_WRONG_NMT_STATE = -16, /**< Command can't be processed in current
                                         state */
    CO_ERROR_SYSCALL = -17,         /**< Syscall failed */
    CO_ERROR_INVALID_STATE = -18,   /**< Driver not ready */
    CO_ERROR_NODE_ID_UNCONFIGURED_LSS = -19 /**< Node-id is in LSS unconfigured
                                         state. If objects are handled properly,
                                         this may not be an error. */
} CO_ReturnError_t;


/**
 * Request CAN configuration (stopped) mode and *wait* until it is set.
 *
 * @param CANptr Pointer to CAN device
 */
void CO_CANsetConfigurationMode(void *CANptr);


/**
 * Request CAN normal (operational) mode and *wait* until it is set.
 *
 * @param CANmodule CO_CANmodule_t object.
 */
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule);


/**
 * Initialize CAN module object.
 *
 * Function must be called in the communication reset section. CAN module must
 * be in Configuration Mode before.
 *
 * @param CANmodule This object will be initialized.
 * @param CANptr Pointer to CAN device.
 * @param rxArray Array for handling received CAN messages
 * @param rxSize Size of the above array. Must be equal to number of receiving
 * CAN objects.
 * @param txArray Array for handling transmitting CAN messages
 * @param txSize Size of the above array. Must be equal to number of
 * transmitting CAN objects.
 * @param CANbitRate Valid values are (in kbps): 10, 20, 50, 125, 250, 500, 800,
 * 1000. If value is illegal, bitrate defaults to 125.
 *
 * Return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t *CANmodule,
                                   void *CANptr,
                                   CO_CANrx_t rxArray[],
                                   uint16_t rxSize,
                                   CO_CANtx_t txArray[],
                                   uint16_t txSize,
                                   uint16_t CANbitRate);


/**
 * Switch off CANmodule. Call at program exit.
 *
 * @param CANmodule CAN module object.
 */
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule);


/**
 * Configure CAN message receive buffer.
 *
 * Function configures specific CAN receive buffer. It sets CAN identifier
 * and connects buffer with specific object. Function must be called for each
 * member in _rxArray_ from CO_CANmodule_t.
 *
 * @param CANmodule This object.
 * @param index Index of the specific buffer in _rxArray_.
 * @param ident 11-bit standard CAN Identifier. If two or more CANrx buffers
 * have the same _ident_, then buffer with lowest _index_ has precedence and
 * other CANrx buffers will be ignored.
 * @param mask 11-bit mask for identifier. Most usually set to 0x7FF.
 * Received message (rcvMsg) will be accepted if the following
 * condition is true: (((rcvMsgId ^ ident) & mask) == 0).
 * @param rtr If true, 'Remote Transmit Request' messages will be accepted.
 * @param object CANopen object, to which buffer is connected. It will be used
 * as an argument to CANrx_callback. Its type is (void), CANrx_callback will
 * change its type back to the correct object type.
 * @param CANrx_callback Pointer to function, which will be called, if received
 * CAN message matches the identifier. It must be fast function.
 *
 * Return #CO_ReturnError_t: CO_ERROR_NO CO_ERROR_ILLEGAL_ARGUMENT or
 * CO_ERROR_OUT_OF_MEMORY (not enough masks for configuration).
 */
CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule,
                                    uint16_t index,
                                    uint16_t ident,
                                    uint16_t mask,
                                    bool_t rtr,
                                    void *object,
                                    void (*CANrx_callback)(void *object,
                                                           void *message));


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
 * @param syncFlag This flag bit is used for synchronous TPDO messages. If it is
 * set, message will not be sent, if current time is outside synchronous window.
 *
 * @return Pointer to CAN transmit message buffer. 8 bytes data array inside
 * buffer should be written, before CO_CANsend() function is called.
 * Zero is returned in case of wrong arguments.
 */
CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule,
                               uint16_t index,
                               uint16_t ident,
                               bool_t rtr,
                               uint8_t noOfBytes,
                               bool_t syncFlag);


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
 * may optionally be cleared.
 *
 * This function checks (and aborts transmission if necessary) CAN TX buffers
 * when it is called. Function should be called by the stack in the moment,
 * when SYNC time was just passed out of synchronous window.
 *
 * @param CANmodule This object.
 */
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule);


/**
 * Process can module - verify CAN errors
 *
 * Function must be called cyclically. It should calculate CANerrorStatus
 * bitfield for CAN errors defined in @ref CO_CAN_ERR_status_t.
 *
 * @param CANmodule This object.
 */
void CO_CANmodule_process(CO_CANmodule_t *CANmodule);


/**
 * Get uint8_t value from memory buffer
 *
 * @param buf Memory buffer to get value from.
 *
 * @return Value
 */
static inline uint8_t CO_getUint8(const void *buf) {
    uint8_t value; memmove(&value, buf, sizeof(value)); return value;
}
/** Get uint16_t value from memory buffer, see @ref CO_getUint8 */
static inline uint16_t CO_getUint16(const void *buf) {
    uint16_t value; memmove(&value, buf, sizeof(value)); return value;
}
/** Get uint32_t value from memory buffer, see @ref CO_getUint8 */
static inline uint32_t CO_getUint32(const void *buf) {
    uint32_t value; memmove(&value, buf, sizeof(value)); return value;
}

/**
 * Write uint8_t value into memory buffer
 *
 * @param buf Memory buffer.
 * @param value Value to be written into buf.
 *
 * @return number of bytes written.
 */
static inline uint8_t CO_setUint8(void *buf, uint8_t value) {
    memmove(buf, &value, sizeof(value)); return sizeof(value);
}
/** Write uint16_t value into memory buffer, see @ref CO_setUint8 */
static inline uint8_t CO_setUint16(void *buf, uint16_t value) {
    memmove(buf, &value, sizeof(value)); return sizeof(value);
}
/** Write uint32_t value into memory buffer, see @ref CO_setUint8 */
static inline uint8_t CO_setUint32(void *buf, uint32_t value) {
    memmove(buf, &value, sizeof(value)); return sizeof(value);
}

/** @} */ /* CO_driver */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DRIVER_H */
