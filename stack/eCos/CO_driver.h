/**
 * CAN module object for eCos RTOS CAN layer
 *
 * @file        CO_driver.h
 * @version     SVN: \$Id: CO_driver.h 32 2013-03-11 08:24:27Z jani22 $
 * @author      Uwe Kindler
 * @copyright   2013 Uwe Kindler
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


#ifndef _CO_DRIVER_H
#define _CO_DRIVER_H

//===========================================================================
//                                INCLUDES
//===========================================================================
#include <cyg/infra/cyg_type.h>
#include <cyg/io/canio.h>
#include <cyg/io/io.h>
#include <cyg/kernel/kapi.h>


//===========================================================================
//                              CONFIGURATION
//===========================================================================
#define CYGDBG_IO_CANOPEN_DEBUG 1 // define this if you want debug output via diagnostic channel


//===========================================================================
//                                DEFINES
//===========================================================================
#if (CYG_BYTEORDER == CYG_MSBFIRST)
#define CO_BIG_ENDIAN 1
#else
#define CO_LITTLE_ENDIAN 1
#endif

//
// Support debug output if this option is enabled in CDL file
//
#ifdef CYGDBG_IO_CANOPEN_DEBUG
#define CO_DBG_PRINT diag_printf
#else
#define CO_DBG_PRINT( fmt, ... )
#endif

// CAN module base address
// we don't really care about the addresses here because the eCos port
// uses I/O handles for accessing its CAN devices
#define ADDR_CAN1   0   /**< Starting address of CAN module 1 registers */
#define ADDR_CAN2   1   /**< Starting address of CAN module 2 registers */


// shared data is accessed only from thread level code (not from ISR or DSR)
// so we simply do a scheduler lock here to prevent access from different
// threads
#define CO_DISABLE_INTERRUPTS() cyg_scheduler_lock()
#define CO_ENABLE_INTERRUPTS() cyg_scheduler_unlock()



// data types
typedef unsigned char CO_bool_t;
typedef enum{
	CO_false = 0,
	CO_true = 1
}CO_boolval_t;
typedef cyg_uint8      uint8_t;    /**< uint8_t */
typedef cyg_uint16     uint16_t;   /**< uint16_t */
typedef cyg_uint32     uint32_t;   /**< uint32_t */
typedef cyg_uint64     uint64_t;   /**< uint64_t */
typedef cyg_int8       int8_t;     /**< int8_t */
typedef cyg_int16      int16_t;    /**< int16_t */
typedef cyg_int32      int32_t;    /**< int32_t */
typedef cyg_int64      int64_t;    /**< int64_t */
typedef float          float32_t;  /**< float32_t */
typedef long double    float64_t;  /**< float64_t */
typedef char           char_t;     /**< char_t */
typedef unsigned char  oChar_t;    /**< oChar_t */
typedef unsigned char  domain_t;   /**< domain_t */


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
    uint32_t            ID;
    uint8_t             DLC; /**< Length of CAN message */
    uint8_t             RTR;
    uint8_t             data[8]; /**< 8 data bytes */
}CO_CANrxMsg_t;


/**
 * Received message object
 */
typedef struct{
    uint16_t     ident;          /**< Standard CAN Identifier (bits 0..10) + RTR (bit 11) */
    void        *object;         /**< From CO_CANrxBufferInit() */
    void       (*pFunct)(void *object, const CO_CANrxMsg_t *message);  /**< From CO_CANrxBufferInit() */
}CO_CANrx_t;


/**
 * Transmit message object.
 */
typedef struct{
	uint16_t            ID;
	uint8_t             DLC;
	uint8_t             RTR;
    uint8_t             data[8];        /**< 8 data bytes */
    volatile uint8_t    bufferFull;     /**< True if previous message is still in buffer */
    /** Synchronous PDO messages has this flag set. It prevents them to be sent outside the synchronous window */
    volatile uint8_t    syncFlag;
}CO_CANtx_t;


/**
 * CAN module object. It may be different in different microcontrollers.
 */
typedef struct{
    CO_CANrx_t         *rxArray;        /**< From CO_CANmodule_init() */
    uint16_t            rxSize;         /**< From CO_CANmodule_init() */
    CO_CANtx_t         *txArray;        /**< From CO_CANmodule_init() */
    uint16_t            txSize;         /**< From CO_CANmodule_init() */
    /** Pointer to variable with same name inside CO_SYNC_t object. This pointer
      * is configured inside CO_SYNC_init() function. */
    volatile uint8_t   *curentSyncTimeIsInsideWindow;
    /** Value different than zero indicates, that CAN module hardware filters
      * are used for CAN reception. If there is not enough hardware filters,
      * they won't be used. In this case will be *all* received CAN messages
      * processed by software. */
    volatile uint8_t    useCANrxFilters;
    /** If flag is true, then message in transmitt buffer is synchronous PDO
      * message, which will be aborted, if CO_clearPendingSyncPDOs() function
      * will be called by application. This may be necessary if Synchronous
      * window time was expired. */
    volatile uint8_t    bufferInhibitFlag;
    /** Equal to 1, when the first transmitted message (bootup message) is in CAN TX buffers */
    volatile uint8_t    firstCANtxMessage;
    /** Number of messages in transmit buffer, which are waiting to be copied to the CAN module */
    volatile uint16_t   CANtxCount;
    uint32_t            errOld;         /**< Previous state of CAN errors */
    void               *em;             /**< Emergency object */
    void               *driverPrivate;
    uint16_t            rxBufferIndexArray[0x800]; ///< Array of pointers to rx buffers
    cyg_io_handle_t     ioHandle;
}CO_CANmodule_t;

#ifdef __cplusplus
extern "C"
{
#endif
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
int16_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        uint16_t                CANbaseAddress,
        CO_CANrx_t             *rxArray,
        uint16_t                rxSize,
        CO_CANtx_t             *txArray,
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
uint16_t CO_CANrxMsg_readIdent(CO_CANrxMsg_t *rxMsg);


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
int16_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        uint8_t                 rtr,
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
        uint8_t                 rtr,
        uint8_t                 noOfBytes,
        uint8_t                 syncFlag);


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
int16_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer);


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
} //extern "C"
#endif
/** @} */
#endif
