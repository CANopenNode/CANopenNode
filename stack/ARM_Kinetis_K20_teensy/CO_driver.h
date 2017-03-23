/**
 * CAN module object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.h
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @copyright   2004 - 2015 Janez Paternoster
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


#ifndef CO_DRIVER_H
#define CO_DRIVER_H


/* Include processor header file */
#include "kinetis_flexcan.h"
#include "kinetis.h"
#include "wiring.h"


#define CO_LOCK_CAN_SEND()  cli() /**< Lock critical section in CO_CANsend() */
#define CO_UNLOCK_CAN_SEND() sei() /**< Unlock critical section in CO_CANsend() */

#define CO_LOCK_EMCY()      cli() /**< Lock critical section in CO_errorReport() or CO_errorReset() */
#define CO_UNLOCK_EMCY()    sei() /**< Unlock critical section in CO_errorReport() or CO_errorReset() */

#define CO_LOCK_OD()         /**< Lock critical section when accessing Object Dictionary */
#define CO_UNLOCK_OD()       /**< Unock critical section when accessing Object Dictionary */


/* Data types */

typedef unsigned char           bool_t;     /**< bool_t */
typedef float                   float32_t;  /**< float32_t */
typedef double                  float64_t;  /**< float64_t */
typedef char                    char_t;     /**< char_t */
typedef unsigned char           oChar_t;    /**< oChar_t */
typedef unsigned char           domain_t;   /**< domain_t */
typedef unsigned char           BOOLEAN;    /* For object dictionary */


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
 * CAN receive message structure (not aligned to the registers).  Since our DATA is stored as two DWORDS, reading it as a uint8_t[] doesn't work anyway (little endian kills us here). 
 */
typedef struct{
    uint8_t             DLC;            
    uint16_t            ident;         
    uint8_t             data[8];        /**< 8 data bytes */
}CO_CANrxMsg_t;

/**
 * Received message object
 */
typedef struct{
    uint16_t            ident;          /**< Standard CAN Identifier (bits 0..10) + RTR (bit 11)  (*not* as aligned in CAN module!) */
    uint16_t            mask;           /**< Standard Identifier mask with same alignment as ident */
    void               *object;         /**< From CO_CANrxBufferInit() */
    void              (*pFunct)(void *object, const CO_CANrxMsg_t *message);  /**< From CO_CANrxBufferInit() */
}CO_CANrx_t;


/**
 * Transmit message object.
 */
typedef struct{
    uint8_t             CODE;       /* Only the bottom nibble matters */
    uint8_t             DLC;        /* Data length */
    uint16_t            timestamp;  
    uint32_t            ident;          /**< CAN identifier (not as aligned in register) */
    uint8_t             data[8];        /**< 8 data bytes */
    uint8_t             DLC_flags;      /* K20 uses upper nibble of DLC to set (reserved), SRR, IDE, RTR flags */
    volatile bool_t     bufferFull;     /**< True if previous message is still in buffer */
    /** Synchronous PDO messages has this flag set. It prevents them to be sent outside the synchronous window */
    volatile bool_t     syncFlag;
}CO_CANtx_t;


/**
 * CAN module object. It may be different in different microcontrollers.
 */
typedef struct{
    int32_t             CANbaseAddress; /**< From CO_CANmodule_init() */
    CO_CANrx_t         *rxArray;        /**< From CO_CANmodule_init() */
    uint16_t            rxSize;         /**< From CO_CANmodule_init() */
    CO_CANtx_t         *txArray;        /**< From CO_CANmodule_init() */
    uint16_t            txSize;         /**< From CO_CANmodule_init() */
    volatile bool_t     CANnormal;      /**< CAN module is in normal mode */
    /** Value different than zero indicates, that CAN module hardware filters
      * are used for CAN reception. If there is not enough hardware filters,
      * they won't be used. In this case will be *all* received CAN messages
      * processed by software. */
    volatile bool_t     useCANrxFilters;
    /** If flag is true, then message in transmitt buffer is synchronous PDO
      * message, which will be aborted, if CO_clearPendingSyncPDOs() function
      * will be called by application. This may be necessary if Synchronous
      * window time was expired. */
    volatile bool_t     bufferInhibitFlag;
    /** Equal to 1, when the first transmitted message (bootup message) is in CAN TX buffers */
    volatile bool_t     firstCANtxMessage;
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
void CO_CANsetConfigurationMode(int32_t CANbaseAddress);


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
        int32_t                 CANbaseAddress,
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
//uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg);
//#define CO_CANrxMsg_readIdent(x)    ((uint32_t) (x->ident >> 18)& 0x07ff)
// uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg){
//     return (uint16_t) (rxMsg->ident >> 18)& 0x07ff;


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
