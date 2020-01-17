/**
 * CANopen LSS Master/Slave protocol.
 *
 * @file        CO_LSSmaster.h
 * @ingroup     CO_LSS
 * @author      Martin Wagner
 * @copyright   2017 - 2020 Neuberger Gebaeudeautomation GmbH
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


#ifndef CO_LSSmaster_H
#define CO_LSSmaster_H

#ifdef __cplusplus
extern "C" {
#endif

#if CO_NO_LSS_CLIENT == 1

#include "CO_LSS.h"

/**
 * @addtogroup CO_LSS
 * @defgroup CO_LSSmaster LSS Master
 * @ingroup CO_LSS
 * @{
 *
 * CANopen Layer Setting Service - client protocol
 *
 * The client/master can use the following services
 * - node selection via LSS address
 * - node selection via LSS fastscan
 * - Inquire LSS address of currently selected node
 * - Inquire node ID
 * - Configure bit timing
 * - Configure node ID
 * - Activate bit timing parameters
 * - Store configuration
 *
 * The LSS master is initalized during the CANopenNode initialization process.
 * Except for enabling the LSS master in the configurator, no further
 * run-time configuration is needed for basic operation.
 * The LSS master does basic checking of commands and command sequence.
 *
 * ###Usage
 *
 * Usage of the CANopen LSS master is demonstrated in CANopenSocket application,
 * see CO_LSS_master.c / CO_LSS_master.h files.
 *
 * It essentially is always as following:
 * - select node(s)
 * - call master command(s)
 * - evaluate return value
 * - deselect nodes
 *
 * All commands need to be run cyclically, e.g. like this
 * \code{.c}

    interval = 0;
    do {
        ret = CO_LSSmaster_InquireNodeId(LSSmaster, interval, &outval);

        interval = 1; ms
        sleep(interval);
    } while (ret == CO_LSSmaster_WAIT_SLAVE);

 * \endcode
 *
 * A more advanced implementation can make use of the callback function to
 * shorten waiting times.
 */

/**
 * Return values of LSS master functions.
 */
typedef enum {
    CO_LSSmaster_SCAN_FINISHED       = 2,    /**< Scanning finished successful */
    CO_LSSmaster_WAIT_SLAVE          = 1,    /**< No response arrived from server yet */
    CO_LSSmaster_OK                  = 0,    /**< Success, end of communication */
    CO_LSSmaster_TIMEOUT             = -1,   /**< No reply received */
    CO_LSSmaster_ILLEGAL_ARGUMENT    = -2,   /**< Invalid argument */
    CO_LSSmaster_INVALID_STATE       = -3,   /**< State machine not ready or already processing a request */
    CO_LSSmaster_SCAN_NOACK          = -4,   /**< No node found that matches scan request */
    CO_LSSmaster_SCAN_FAILED         = -5,   /**< An error occurred while scanning. Try again */
    CO_LSSmaster_OK_ILLEGAL_ARGUMENT = -101, /**< LSS success, node rejected argument because of non-supported value */
    CO_LSSmaster_OK_MANUFACTURER     = -102, /**< LSS success, node rejected argument with manufacturer error code */
} CO_LSSmaster_return_t;


/**
 * LSS master object.
 */
typedef struct{
    uint16_t         timeout;          /**< LSS response timeout in ms */

    uint8_t          state;            /**< Node is currently selected */
    uint8_t          command;          /**< Active command */
    uint16_t         timeoutTimer;     /**< Timeout timer for LSS communication */

    uint8_t          fsState;          /**< Current state of fastscan master state machine */
    uint8_t          fsLssSub;         /**< Current state of node state machine */
    uint8_t          fsBitChecked;     /**< Current scan bit position */
    uint32_t         fsIdNumber;       /**< Current scan result */

    volatile void   *CANrxNew;         /**< Indication if new LSS message is received from CAN bus. It needs to be cleared when received message is completely processed. */
    uint8_t          CANrxData[8];     /**< 8 data bytes of the received message */

    void           (*pFunctSignal)(void *object); /**< From CO_LSSmaster_initCallback() or NULL */
    void            *functSignalObject;/**< Pointer to object */

    CO_CANmodule_t  *CANdevTx;         /**< From #CO_LSSslave_init() */
    CO_CANtx_t      *TXbuff;           /**< CAN transmit buffer */
}CO_LSSmaster_t;


/**
 * Default timeout for LSS slave in ms. This is the same as for SDO. For more
 * info about LSS timeout see #CO_LSSmaster_changeTimeout()
 */
#define CO_LSSmaster_DEFAULT_TIMEOUT 1000U /* ms */


/**
 * Initialize LSS object.
 *
 * Function must be called in the communication reset section.
 *
 * @param LSSmaster This object will be initialized.
 * @param timeout_ms slave response timeout in ms, for more detail see
 * #CO_LSSmaster_changeTimeout()
 * @param CANdevRx CAN device for LSS master reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANidLssSlave COB ID for reception.
 * @param CANdevTx CAN device for LSS master transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 * @param CANidLssMaster COB ID for transmission.
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_LSSmaster_init(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeout_ms,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        uint32_t                CANidLssSlave,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx,
        uint32_t                CANidLssMaster);

/**
 * Change LSS master timeout
 *
 * On LSS, a "negative ack" is signaled by the slave not answering. Because of
 * that, a low timeout value can significantly increase protocol speed in some
 * cases (e.g. fastscan). However, as soon as there is activity on the bus,
 * LSS messages can be delayed because of their low CAN network priority (see
 * #CO_Default_CAN_ID_t).
 *
 * @remark Be aware that a "late response" will seriously mess up LSS, so this
 * value must be selected "as high as necessary and as low as possible". CiA does
 * neither specify nor recommend a value.
 *
 * @remark This timeout is per-transfer. If a command internally needs multiple
 * transfers to complete, this timeout is applied on each transfer.
 *
 * @param LSSmaster This object.
 * @param timeout_ms timeout value in ms
 */
void CO_LSSmaster_changeTimeout(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeout_ms);


/**
 * Initialize LSSserverRx callback function.
 *
 * Function initializes optional callback function, which is called after new
 * message is received from the CAN bus. Function may wake up external task,
 * which processes mainline CANopen functions.
 *
 * @param LSSmaster This object.
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can be NULL
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_LSSmaster_initCallback(
        CO_LSSmaster_t         *LSSmaster,
        void                   *object,
        void                  (*pFunctSignal)(void *object));


/**
 * Request LSS switch state select
 *
 * This function can select one specific or all nodes.
 *
 * Function must be called cyclically until it returns != #CO_LSSmaster_WAIT_SLAVE
 * Function is non-blocking.
 *
 * @remark Only one selection can be active at any time.
 *
 * @param LSSmaster This object.
 * @param timeDifference_ms Time difference from previous function call in
 * [milliseconds]. Zero when request is started.
 * @param lssAddress LSS target address. If NULL, all nodes are selected
 * @return #CO_LSSmaster_ILLEGAL_ARGUMENT,  #CO_LSSmaster_INVALID_STATE,
 * #CO_LSSmaster_WAIT_SLAVE, #CO_LSSmaster_OK, #CO_LSSmaster_TIMEOUT
 */
CO_LSSmaster_return_t CO_LSSmaster_switchStateSelect(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        CO_LSS_address_t       *lssAddress);


/**
 * Request LSS switch state deselect
 *
 * This function deselects all nodes, so it doesn't matter if a specific
 * node is selected.
 *
 * This function also resets the LSS master state machine to a clean state
 *
 * @param LSSmaster This object.
 * @return #CO_LSSmaster_ILLEGAL_ARGUMENT,  #CO_LSSmaster_INVALID_STATE,
 * #CO_LSSmaster_OK
 */
CO_LSSmaster_return_t CO_LSSmaster_switchStateDeselect(
        CO_LSSmaster_t         *LSSmaster);


/**
 * Request LSS configure Bit Timing
 *
 * The new bit rate is set as new pending value.
 *
 * This function needs one specific node to be selected.
 *
 * Function must be called cyclically until it returns != #CO_LSSmaster_WAIT_SLAVE.
 * Function is non-blocking.
 *
 * @param LSSmaster This object.
 * @param timeDifference_ms Time difference from previous function call in
 * [milliseconds]. Zero when request is started.
 * @param bit new bit rate
 * @return #CO_LSSmaster_ILLEGAL_ARGUMENT,  #CO_LSSmaster_INVALID_STATE,
 * #CO_LSSmaster_WAIT_SLAVE, #CO_LSSmaster_OK, #CO_LSSmaster_TIMEOUT,
 * #CO_LSSmaster_OK_MANUFACTURER, #CO_LSSmaster_OK_ILLEGAL_ARGUMENT
 */
CO_LSSmaster_return_t CO_LSSmaster_configureBitTiming(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        uint16_t                bit);


/**
 * Request LSS configure node ID
 *
 * The new node id is set as new pending node ID.
 *
 * This function needs one specific node to be selected.
 *
 * Function must be called cyclically until it returns != #CO_LSSmaster_WAIT_SLAVE.
 * Function is non-blocking.
 *
 * @param LSSmaster This object.
 * @param timeDifference_ms Time difference from previous function call in
 * [milliseconds]. Zero when request is started.
 * @param nodeId new node ID. Special value #CO_LSS_NODE_ID_ASSIGNMENT can be
 * used to invalidate node ID.
 * @return #CO_LSSmaster_ILLEGAL_ARGUMENT,  #CO_LSSmaster_INVALID_STATE,
 * #CO_LSSmaster_WAIT_SLAVE, #CO_LSSmaster_OK, #CO_LSSmaster_TIMEOUT,
 * #CO_LSSmaster_OK_MANUFACTURER, #CO_LSSmaster_OK_ILLEGAL_ARGUMENT
 */
CO_LSSmaster_return_t CO_LSSmaster_configureNodeId(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        uint8_t                 nodeId);


/**
 * Request LSS store configuration
 *
 * The current "pending" values for bit rate and node ID in LSS slave are
 * stored as "permanent" values.
 *
 * This function needs one specific node to be selected.
 *
 * Function must be called cyclically until it returns != #CO_LSSmaster_WAIT_SLAVE.
 * Function is non-blocking.
 *
 * @param LSSmaster This object.
 * @param timeDifference_ms Time difference from previous function call in
 * [milliseconds]. Zero when request is started.
 * @return #CO_LSSmaster_ILLEGAL_ARGUMENT,  #CO_LSSmaster_INVALID_STATE,
 * #CO_LSSmaster_WAIT_SLAVE, #CO_LSSmaster_OK, #CO_LSSmaster_TIMEOUT,
 * #CO_LSSmaster_OK_MANUFACTURER, #CO_LSSmaster_OK_ILLEGAL_ARGUMENT
 */
CO_LSSmaster_return_t CO_LSSmaster_configureStore(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms);


/**
 * Request LSS activate bit timing
 *
 * The current "pending" bit rate in LSS slave is applied.
 *
 * Be aware that changing the bit rate is a critical step for the network. A
 * failure will render the network unusable! Therefore, this function only
 * should be called if the following conditions are met:
 * - all nodes support changing bit timing
 * - new bit timing is successfully set as "pending" in all nodes
 * - all nodes have to activate the new bit timing roughly at the same time.
 *   Therefore this function needs all nodes to be selected.
 *
 * @param LSSmaster This object.
 * @param switchDelay_ms delay that is applied by the slave once before and
 * once after switching in ms.
 * @return #CO_LSSmaster_ILLEGAL_ARGUMENT,  #CO_LSSmaster_INVALID_STATE,
 * #CO_LSSmaster_OK
 */
CO_LSSmaster_return_t CO_LSSmaster_ActivateBit(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                switchDelay_ms);


/**
 * Request LSS inquire LSS address
 *
 * The LSS address value is read from the node. This is useful when the node
 * was selected by fastscan.
 *
 * This function needs one specific node to be selected.
 *
 * Function must be called cyclically until it returns != #CO_LSSmaster_WAIT_SLAVE.
 * Function is non-blocking.
 *
 * @param LSSmaster This object.
 * @param timeDifference_ms Time difference from previous function call in
 * [milliseconds]. Zero when request is started.
 * @param lssAddress [out] read result when function returns successfully
 * @return #CO_LSSmaster_ILLEGAL_ARGUMENT,  #CO_LSSmaster_INVALID_STATE,
 * #CO_LSSmaster_WAIT_SLAVE, #CO_LSSmaster_OK, #CO_LSSmaster_TIMEOUT
 */
CO_LSSmaster_return_t CO_LSSmaster_InquireLssAddress(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        CO_LSS_address_t       *lssAddress);


/**
 * Request LSS inquire node ID
 *
 * The node ID value is read from the node.
 *
 * This function needs one specific node to be selected.
 *
 * Function must be called cyclically until it returns != #CO_LSSmaster_WAIT_SLAVE.
 * Function is non-blocking.
 *
 * @param LSSmaster This object.
 * @param timeDifference_ms Time difference from previous function call in
 * [milliseconds]. Zero when request is started.
 * @param nodeId [out] read result when function returns successfully
 * @return #CO_LSSmaster_ILLEGAL_ARGUMENT,  #CO_LSSmaster_INVALID_STATE,
 * #CO_LSSmaster_WAIT_SLAVE, #CO_LSSmaster_OK, #CO_LSSmaster_TIMEOUT
 */
CO_LSSmaster_return_t CO_LSSmaster_InquireNodeId(
        CO_LSSmaster_t         *LSSmaster,
        uint16_t                timeDifference_ms,
        uint8_t                *nodeId);


/**
 * Scan type for #CO_LSSmaster_fastscan_t scan
 */
typedef enum {
    CO_LSSmaster_FS_SCAN  = 0,    /**< Do full 32 bit scan */
    CO_LSSmaster_FS_SKIP  = 1,    /**< Skip this value */
    CO_LSSmaster_FS_MATCH = 2,    /**< Full 32 bit value is given as argument, just verify */
} CO_LSSmaster_scantype_t;

/**
 * Parameters for LSS fastscan #CO_LSSmaster_IdentifyFastscan
 */
typedef struct{
    CO_LSSmaster_scantype_t scan[4];  /**< Scan type for each part of the LSS address */
    CO_LSS_address_t        match;    /**< Value to match in case of #CO_LSSmaster_FS_MATCH */
    CO_LSS_address_t        found;    /**< Scan result */
} CO_LSSmaster_fastscan_t;

/**
 * Select a node by LSS identify fastscan
 *
 * This initiates searching for a unconfigured node by the means of LSS fastscan
 * mechanism. When this function is finished
 * - a (more or less) arbitrary node is selected and ready for node ID assingment
 * - no node is selected because the given criteria do not match a node
 * - no node is selected because all nodes are already configured
 *
 * There are multiple ways to scan for a node. Depending on those, the scan
 * will take different amounts of time:
 * - full scan
 * - partial scan
 * - verification
 *
 * Most of the time, those are used in combination. Consider the following example:
 * - Vendor ID and product code are known
 * - Software version doesn't matter
 * - Serial number is unknown
 *
 * In this case, the fastscan structure should be set up as following:
 * \code{.c}
CO_LSSmaster_fastscan_t fastscan;
fastscan.scan[CO_LSS_FASTSCAN_VENDOR_ID] = CO_LSSmaster_FS_MATCH;
fastscan.match.vendorID = YOUR_VENDOR_ID;
fastscan.scan[CO_LSS_FASTSCAN_PRODUCT] = CO_LSSmaster_FS_MATCH;
fastscan.match.productCode = YOUR_PRODUCT_CODE;
fastscan.scan[CO_LSS_FASTSCAN_REV] = CO_LSSmaster_FS_SKIP;
fastscan.scan[CO_LSS_FASTSCAN_SERIAL] = CO_LSSmaster_FS_SCAN;
 * \endcode
 *
 * This example will take 2 scan cyles for verifying vendor ID and product code
 * and 33 scan cycles to find the serial number.
 *
 * For scanning, the following limitations apply:
 * - No more than two values can be skipped
 * - Vendor ID cannot be skipped
 *
 * @remark When doing partial scans, it is in the responsibility of the user
 * that the LSS address is unique.
 *
 * This function needs that no node is selected when starting the scan process.
 *
 * Function must be called cyclically until it returns != #CO_LSSmaster_WAIT_SLAVE.
 * Function is non-blocking.
 *
 * @param LSSmaster This object.
 * @param timeDifference_ms Time difference from previous function call in
 * [milliseconds]. Zero when request is started.
 * @param fastscan struct according to #CO_LSSmaster_fastscan_t.
 * @return #CO_LSSmaster_ILLEGAL_ARGUMENT,  #CO_LSSmaster_INVALID_STATE,
 * #CO_LSSmaster_WAIT_SLAVE, #CO_LSSmaster_SCAN_FINISHED, #CO_LSSmaster_SCAN_NOACK,
 * #CO_LSSmaster_SCAN_FAILED
 */
CO_LSSmaster_return_t CO_LSSmaster_IdentifyFastscan(
        CO_LSSmaster_t                  *LSSmaster,
        uint16_t                         timeDifference_ms,
        CO_LSSmaster_fastscan_t         *fastscan);


#else /* CO_NO_LSS_CLIENT == 1 */

/**
 * @addtogroup CO_LSS
 * @{
 * If you need documetation for LSS master usage, add "CO_NO_LSS_CLIENT=1" to doxygen
 * "PREDEFINED" variable.
 *
 */

#endif /* CO_NO_LSS_CLIENT == 1 */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
