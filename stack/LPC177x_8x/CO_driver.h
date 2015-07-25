/**
 * CAN module object for NXP LPC177x (Cortex M3) and FreeRTOS.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.h
 * @ingroup     CO_driver
 * @version     SVN: \$Id: CO_driver.h 46 2013-08-24 09:18:16Z jani22 $
 * @author      Janez Paternoster
 * @author      Amit H
 * @copyright   2004 - 2014 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <http://canopennode.sourceforge.net>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef CO_DRIVER_H
#define CO_DRIVER_H
/*Kernel Includes. ----------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"



/* Include processor header file */
#include <stddef.h>         /* for 'NULL' */
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
 * @name CAN module base address
 * @{
 */
    #define ADDR_CAN1   0   /**< Starting address of CAN module 1 registers */
    #define ADDR_CAN2   1   /**< Starting address of CAN module 2 registers */
/** @} */

#define CAN_NODE_ID_0_PORT      1   
#define CAN_NODE_ID_0_PIN       23
#define CAN_NODE_ID_1_PORT      1   
#define CAN_NODE_ID_1_PIN       24
#define CAN_NODE_ID_2_PORT      1   
#define CAN_NODE_ID_2_PIN       25
#define CAN_NODE_ID_3_PORT      1   
#define CAN_NODE_ID_3_PIN       26
#define CAN_NODE_ID_4_PORT      1   
#define CAN_NODE_ID_4_PIN       28

#define CAN_RUN_LED_PORT			1
#define CAN_RUN_LED_PIN			    20

/**
 * @name Disabling interrupts
 * Interrupt masking is used to protect critical sections.
 * It is used in some places in library to protect short sections of code in
 * functions, which may be accessed from different tasks.
 * 
 * shared data is accessed only from thread level code (not from ISR or DSR)
 * so we simply do a scheduler lock here to prevent access from different
 * threads
 * @{
 */
    #define CO_DISABLE_INTERRUPTS()     taskENTER_CRITICAL() /**Preemptive context switches cannot occur when in a critical region */
    #define CO_ENABLE_INTERRUPTS()      taskEXIT_CRITICAL() /**< Reenable Preemptive context switches */
/** @} */


/**
 * @defgroup CO_dataTypes Data types
 * @{
 *
 * According to Misra C
 */
    typedef unsigned char CO_bool_t;
    typedef enum{
        CO_false = 0,
        CO_true = 1
    }CO_boolval_t;
    /* int8_t to uint64_t are defined in stdint.h */
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
    CO_ERROR_TX_PDO_WINDOW      = -10,  /**< Synchronous TPDO is outside window */
    CO_ERROR_TX_UNCONFIGURED    = -11,  /**< Transmit buffer was not confugured properly */
    CO_ERROR_PARAMETERS         = -12,  /**< Error in function function parameters */
    CO_ERROR_DATA_CORRUPT       = -13,  /**< Stored data are corrupt */
    CO_ERROR_CRC                = -14   /**< CRC does not match */
}CO_ReturnError_t;


/**
 * CAN receive message structure as aligned in CAN module. It is different in
 * different microcontrollers. It usually contains other variables.
 */
typedef struct{
    /** CAN identifier. It must be read through CO_CANrxMsg_readIdent() function. */
    uint32_t ident;					/*!< Message Identifier. If 30th-bit is set, this is 29-bit ID, othewise 11-bit ID */
	uint32_t Type;					/*!< Message Type. which can include: - CAN_REMOTE_MSG type*/
	uint32_t DLC;					/*!< Message Data Length: 0~8 */
	uint8_t  data[CAN_MSG_MAX_DATA_LEN];/*!< Message Data */
}CO_CANrxMsg_t;

//typedef struct{
//    /** CAN identifier. It must be read through CO_CANrxMsg_readIdent() function. */
//    uint32_t            ident;
//    uint8_t             DLC ;           /**< Length of CAN message */
//    uint8_t             data[8];        /**< 8 data bytes */
//}CO_CANrxMsg_t;


/**
 * Received message object
 */
typedef struct{
    uint16_t            ident;          /**< Standard CAN Identifier (bits 0..10) + RTR (bit 11) */
    uint16_t            mask;           /**< Standard Identifier mask with same alignment as ident */
    void               *object;         /**< From CO_CANrxBufferInit() */
    void              (*pFunct)(void *object, const CO_CANrxMsg_t *message);  /**< From CO_CANrxBufferInit() */
}CO_CANrx_t;


/**
 * Transmit message object.
 */
//typedef struct{
//    uint32_t            ident;          /**< CAN identifier as aligned in CAN module */
//    uint8_t             data[8];        /**< 8 data bytes */
//    volatile CO_bool_t  bufferFull;     /**< True if previous message is still in buffer */
//    /** Synchronous PDO messages has this flag set. It prevents them to be sent outside the synchronous window */
//    volatile CO_bool_t  syncFlag;
//}CO_CANtx_t;


typedef struct						/*!< Message structure */
{
	uint32_t ident;					/*!< Message Identifier. If 30th-bit is set, this is 29-bit ID, othewise 11-bit ID */
	uint32_t Type;					/*!< Message Type. which can include: - CAN_REMOTE_MSG type*/
	uint32_t DLC;					/*!< Message Data Length: 0~8 */
	uint8_t  data[CAN_MSG_MAX_DATA_LEN];/*!< Message Data */
    volatile CO_bool_t  bufferFull;     /**< True if previous message is still in buffer */
    /** Synchronous PDO messages has this flag set. It prevents them to be sent outside the synchronous window */
    volatile CO_bool_t  syncFlag;
} CO_CANtx_t;

/**
 * CAN module object. It may be different in different microcontrollers.
 */
typedef struct{
    uint16_t            CANbaseAddress; /**< From CO_CANmodule_init() */
    CO_CANrx_t         *rxArray;        /**< From CO_CANmodule_init() */
    uint16_t            rxSize;         /**< From CO_CANmodule_init() */
    CO_CANtx_t         *txArray;        /**< From CO_CANmodule_init() */
    uint16_t            txSize;         /**< From CO_CANmodule_init() */
    /** Value different than zero indicates, that CAN module hardware filters
      * are used for CAN reception. If there is not enough hardware filters,
      * they won't be used. In this case will be *all* received CAN messages
      * processed by software. */
    volatile CO_bool_t  useCANrxFilters;
    /** If flag is true, then message in transmitt buffer is synchronous PDO
      * message, which will be aborted, if CO_clearPendingSyncPDOs() function
      * will be called by application. This may be necessary if Synchronous
      * window time was expired. */
    volatile CO_bool_t  bufferInhibitFlag;
    /** Equal to 1, when the first transmitted message (bootup message) is in CAN TX buffers */
    volatile CO_bool_t  firstCANtxMessage;
    /** Number of messages in transmit buffer, which are waiting to be copied to the CAN module */
    volatile uint16_t   CANtxCount;
    uint32_t            errOld;         /**< Previous state of CAN errors */
    void               *em;             /**< Emergency object */
}CO_CANmodule_t;


/**
 * Endianes.
 *
 * Depending on processor or compiler architecture, one of the two macros must
 * be defined: CO_LITTLE_ENDIAN or CO_BIG_ENDIAN. CANopen itself is little endian.
 */
#define CO_LITTLE_ENDIAN


/**
 * Request CAN configuration (stopped) mode and *wait* untill it is set.
 *
 * @param CANbaseAddress CAN module base address.
 */
void CO_CANsetConfigurationMode(uint16_t CANbaseAddress);


/**
 * Request CAN normal (opearational) mode and *wait* untill it is set.
 *
 * @param CANbaseAddress CAN module base address.
 */
void CO_CANsetNormalMode(uint16_t CANbaseAddress);


/**
 * Initialize CAN module object.
 *
 * Function must be called in the communication reset section. CAN module must
 * be in Configuration Mode before.
 *
 * @param CANmodule This object will be initialized.
 * @param CANbaseAddress CAN module base address.
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
        uint16_t                CANbaseAddress,
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
        CO_bool_t               rtr,
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
        CO_bool_t               rtr,
        uint8_t                 noOfBytes,
        CO_bool_t               syncFlag);


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


/**
 * Receives and transmits CAN messages.
 *
 * Function must be called directly from high priority CAN interrupt.
 *
 * @param CANmodule This object.
 */
void CO_CANinterrupt(CO_CANmodule_t *CANmodule);


/** @} */
#endif
