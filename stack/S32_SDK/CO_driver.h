/**
 * CAN module object for generic microcontroller.
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>         /* for 'NULL' */
#include <stdint.h>         /* for 'int8_t' to 'uint64_t' */
#include <stdbool.h>        /* for 'true', 'false' */
/* Include processor header file */
#include "flexcan_driver.h"
#include "interrupt_manager.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/**
 * @name Critical sections
 * @{
 */
    #define CO_LOCK_CAN_SEND()      INT_SYS_DisableIRQGlobal()  /**< Lock critical section in CO_CANsend() */
    #define CO_UNLOCK_CAN_SEND()    INT_SYS_EnableIRQGlobal()   /**< Unlock critical section in CO_CANsend() */

    #define CO_LOCK_EMCY()          INT_SYS_DisableIRQGlobal()  /**< Lock critical section in CO_errorReport() or CO_errorReset() */
    #define CO_UNLOCK_EMCY()        INT_SYS_EnableIRQGlobal()   /**< Unlock critical section in CO_errorReport() or CO_errorReset() */

    #define CO_LOCK_OD()            INT_SYS_DisableIRQGlobal()  /**< Lock critical section when accessing Object Dictionary */
    #define CO_UNLOCK_OD()          INT_SYS_EnableIRQGlobal()   /**< Unock critical section when accessing Object Dictionary */
/** @} */

/**
 * @defgroup Malibox assignments
 * @{
 *
 * Each CANopen module is assigned a mailbox for receiving messages and one for transmitting messages
 */

    #define RX_MESSAGEID          0UL
    #define RX_MAILBOXID          0UL
    #define TX_MAILBOXID          1UL
    #define MAILBOX_NR            2UL

/*@}*/

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
    CO_ERROR_TX_PDO_WINDOW      = -10,  /**< Synchronous TPDO is outside window */
    CO_ERROR_TX_UNCONFIGURED    = -11,  /**< Transmit buffer was not configured properly */
    CO_ERROR_PARAMETERS         = -12,  /**< Error in function parameters */
    CO_ERROR_DATA_CORRUPT       = -13,  /**< Stored data are corrupt */
    CO_ERROR_CRC                = -14   /**< CRC does not match */
}CO_ReturnError_t;

/**
 * FlexCAN configuration structure
 */
typedef struct{
    uint8_t                        CAN_instance;    /**< FlexCAN instance number */
    flexcan_state_t               *CAN_state;       /**< FlexCAN state structure */
    const flexcan_user_config_t   *CAN_user_config; /**< FlexCAN configuration structure */
    uint16_t                       nodeID;          /**< Node ID of CAN network device */
}CO_FlexCAN_config_t;

/**
 * CAN receive message structure as aligned in CAN module. It is different in
 * different microcontrollers. It usually contains other variables.
 */
typedef struct{
    /** CAN identifier. It must be read through CO_CANrxMsg_readIdent() function. */
    uint32_t            ident;
    uint8_t             DLC;           /**< Length of CAN message */
    uint8_t            *data;          /**< 8 data bytes */
    /* FlexCAN specific members */
    uint16_t            messageId;     /**< ID of the message received */
}CO_CANrxMsg_t;


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
typedef struct{
    uint32_t            ident;          /**< CAN identifier as aligned in CAN module */
    uint8_t             DLC ;           /**< Length of CAN message. (DLC may also be part of ident) */
    uint8_t             data[8];        /**< 8 data bytes */
    volatile bool_t     bufferFull;     /**< True if previous message is still in buffer */
    /** Synchronous PDO messages has this flag set. It prevents them to be sent outside the synchronous window */
    volatile bool_t     syncFlag;
    /* FlexCAN specific members */
    flexcan_data_info_t dataInfo;      /**< FlexCAN structure with mailbox TX configuration for this message */
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
    /** If flag is true, then message in transmit buffer is synchronous PDO
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
    /* FlexCAN specific members */
    uint8_t                        INST_CANCOM;         /**< FlexCAN instance number */
    flexcan_state_t               *canCom_State;        /**< FlexCAN state structure */
    const flexcan_user_config_t   *canCom_InitConfig;   /**< FlexCAN configuration structure */
    uint16_t                       nodeID;              /**< Node ID of CAN network device */
    flexcan_msgbuff_t              rxBuffer;            /**< buffer for data received over FlexCAN */
}CO_CANmodule_t;


/**
 * Endianness.
 *
 * Depending on processor or compiler architecture, one of the two macros must
 * be defined: CO_LITTLE_ENDIAN or CO_BIG_ENDIAN. CANopen itself is little endian.
 */
#if defined(CORE_LITTLE_ENDIAN)
    #define CO_LITTLE_ENDIAN
#elif defined(CORE_BIG_ENDIAN)
    #define CO_BIG_ENDIAN
#endif


/************************************************************************************************
 * Functions
 ***********************************************************************************************/


/**
 * Initialize CANopen stack and FlexCAN module.
 *
 * This function must be called before any other CAN related functions in the communication reset section.
 *
 * @param   instance           A FlexCAN instance number
 * @param   state              Pointer to the FlexCAN driver state structure.
 * @param   data               The FlexCAN user configuration data
 */
CO_ReturnError_t CO_FLEXCAN_Init(
        uint8_t                            instance,
        flexcan_state_t                    *state,
        const flexcan_user_config_t        *data,
        uint16_t                        nodeID);


/**
 * Request CAN configuration (stopped) mode and *wait* until it is set.
 *
 * @param CANbaseAddress CAN module base address.
 */
void CO_CANsetConfigurationMode(int32_t CANbaseAddress);


/**
 * Request CAN normal (operational) mode and *wait* until it is set.
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
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg);


/**
 * Configure CAN message receive buffer.
 *
 * Function configures specific CAN receive buffer. It sets CAN identifier
 * and connects buffer with specific object. Function must be called for each
 * member in _rxArray_ from CO_CANmodule_t.
 *
 * @param CANmodule        This object.
 * @param index            Index of the specific buffer in _rxArray_.
 * @param ident            11-bit standard CAN Identifier.
 * @param mask             11-bit mask for identifier. Most usually set to 0x7FF.
 *                         Received message (rcvMsg) will be accepted if the following
 *                         condition is true: (((rcvMsgId ^ ident) & mask) == 0).
 * @param rtr              If true, 'Remote Transmit Request' messages will be accepted.
 * @param object           CANopen object, to which buffer is connected. It will be used as
 *                         an argument to pFunct. Its type is (void), pFunct will change its
 *                         type back to the correct object type.
 * @param pFunct           Pointer to function, which will be called, if received CAN
 *                         message matches the identifier. It must be fast function.
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
 * message will not be sent, if current time is outside synchronous window.
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
void CO_CANinterrupt(/*CO_CANmodule_t *CANmodule*/
        uint8_t                     instance,
        flexcan_event_type_t        eventType,
        flexcan_state_t            *flexcanState);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
/*******************************************************************************
 * EOF
 ******************************************************************************/
