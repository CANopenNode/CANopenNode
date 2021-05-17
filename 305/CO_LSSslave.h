/**
 * CANopen Layer Setting Service - slave protocol.
 *
 * @file        CO_LSSslave.h
 * @ingroup     CO_LSS
 * @author      Martin Wagner
 * @author      Janez Paternoster
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

#ifndef CO_LSSslave_H
#define CO_LSSslave_H

#include "305/CO_LSS.h"

#if ((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_LSSslave LSS Slave
 * CANopen Layer Setting Service - slave protocol.
 *
 * @ingroup CO_CANopen_305
 * @{
 * The slave provides the following services
 * - node selection via LSS address
 * - node selection via LSS fastscan
 * - Inquire LSS address of currently selected node
 * - Inquire node ID
 * - Configure bit timing
 * - Configure node ID
 * - Activate bit timing parameters
 * - Store configuration (bit rate and node ID)
 *
 * After CAN module start, the LSS slave and NMT slave are started and then
 * coexist alongside each other. To achieve this behaviour, the CANopen node
 * startup process has to be controlled more detailed. Therefore, CO_LSSinit()
 * must be invoked between CO_CANinit() and CO_CANopenInit() in the
 * communication reset section.
 *
 * Moreover, the LSS slave needs to pause the NMT slave initialization in case
 * no valid node ID is available at start up. In that case CO_CANopenInit()
 * skips initialization of other CANopen modules and CO_process() skips
 * processing of other modules than LSS slave automatically.
 *
 * Variables for CAN-bitrate and CANopen node-id must be initialized by
 * application from non-volatile memory or dip switches. Pointers to them are
 * passed to CO_LSSinit() function. Those variables represents pending values.
 * If node-id is valid in the moment it enters CO_LSSinit(), it also becomes
 * active node-id and the stack initialises normally. Otherwise, node-id must be
 * configured by lss and after successful configuration stack passes reset
 * communication autonomously.
 *
 * Device with all threads can be normally initialized and running despite that
 * node-id is not valid. Application must take care, because CANopen is not
 * initialized. In that case CO_CANopenInit() returns error condition
 * CO_ERROR_NODE_ID_UNCONFIGURED_LSS which must be handled properly. Status can
 * also be checked with CO->nodeIdUnconfigured variable.
 *
 * Some callback functions may be initialized by application with
 * CO_LSSslave_initCheckBitRateCallback(),
 * CO_LSSslave_initActivateBitRateCallback() and
 * CO_LSSslave_initCfgStoreCallback().
 */

/**
 * LSS slave object.
 */
typedef struct{
    CO_LSS_address_t        lssAddress;       /**< From #CO_LSSslave_init */
    CO_LSS_state_t          lssState;         /**< #CO_LSS_state_t */
    CO_LSS_address_t        lssSelect;        /**< Received LSS Address by select */

    CO_LSS_address_t        lssFastscan;      /**< Received LSS Address by fastscan */
    uint8_t                 fastscanPos;      /**< Current state of fastscan */

    uint16_t               *pendingBitRate;   /**< Bit rate value that is temporarily configured */
    uint8_t                *pendingNodeID;    /**< Node ID that is temporarily configured */
    uint8_t                 activeNodeID;     /**< Node ID used at the CAN interface */

    volatile void          *sendResponse;     /**< Variable indicates, if LSS response has to be sent by mainline processing function */
    CO_LSS_cs_t             service;          /**< Service, which will have to be processed by mainline processing function */
    uint8_t                 CANdata[8];       /**< Received CAN data, which will be processed by mainline processing function */

#if ((CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    void                  (*pFunctSignalPre)(void *object); /**< From CO_LSSslave_initCallbackPre() or NULL */
    void                   *functSignalObjectPre;/**< Pointer to object */
#endif
    bool_t                (*pFunctLSScheckBitRate)(void *object, uint16_t bitRate); /**< From CO_LSSslave_initCheckBitRateCallback() or NULL */
    void                   *functLSScheckBitRateObject; /** Pointer to object */
    void                  (*pFunctLSSactivateBitRate)(void *object, uint16_t delay); /**< From CO_LSSslave_initActivateBitRateCallback() or NULL. Delay is in ms */
    void                   *functLSSactivateBitRateObject; /** Pointer to object */
    bool_t                (*pFunctLSScfgStore)(void *object, uint8_t id, uint16_t bitRate); /**< From CO_LSSslave_initCfgStoreCallback() or NULL */
    void                   *functLSScfgStoreObject; /** Pointer to object */

    CO_CANmodule_t         *CANdevTx;         /**< From #CO_LSSslave_init() */
    CO_CANtx_t             *TXbuff;           /**< CAN transmit buffer */
}CO_LSSslave_t;

/**
 * Initialize LSS object.
 *
 * Function must be called in the communication reset section.
 *
 * pendingBitRate and pendingNodeID must be pointers to external variables. Both
 * variables must be initialized on program startup (after #CO_NMT_RESET_NODE)
 * from non-volatile memory, dip switches or similar. They must not change
 * during #CO_NMT_RESET_COMMUNICATION. Both variables can be changed by
 * CO_LSSslave_process(), depending on commands from the LSS master.
 *
 * If pendingNodeID is valid (1 <= pendingNodeID <= 0x7F), then this becomes
 * valid active nodeId just after exit of this function. In that case all other
 * CANopen objects may be initialized and processed in run time.
 *
 * If pendingNodeID is not valid (pendingNodeID == 0xFF), then only LSS slave is
 * initialized and processed in run time. In that state pendingNodeID can be
 * configured and after successful configuration reset communication with all
 * CANopen object is activated automatically.
 *
 * @remark The LSS address needs to be unique on the network. For this, the 128
 * bit wide identity object (1018h) is used. Therefore, this object has to be
 * fully initialized before passing it to this function (vendorID, product
 * code, revisionNo, serialNo are set to 0 by default). Otherwise, if
 * non-configured devices are present on CANopen network, LSS configuration may
 * behave unpredictable.
 *
 * @param LSSslave This object will be initialized.
 * @param lssAddress LSS address
 * @param [in,out] pendingBitRate Pending bit rate of the CAN interface
 * @param [in,out] pendingNodeID Pending node ID or 0xFF - invalid
 * @param CANdevRx CAN device for LSS slave reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANidLssMaster COB ID for reception.
 * @param CANdevTx CAN device for LSS slave transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 * @param CANidLssSlave COB ID for transmission.
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_LSSslave_init(
        CO_LSSslave_t          *LSSslave,
        CO_LSS_address_t       *lssAddress,
        uint16_t               *pendingBitRate,
        uint8_t                *pendingNodeID,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        uint16_t                CANidLssMaster,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx,
        uint16_t                CANidLssSlave);

/**
 * Process LSS communication
 *
 * Object is partially pre-processed after LSS message received. Further
 * processing is inside this function.
 *
 * In case that Node-Id is unconfigured, then this function may request CANopen
 * communication reset. This happens, when valid node-id is configured by LSS
 * master.
 *
 * @param LSSslave This object.
 * @return True, if #CO_NMT_RESET_COMMUNICATION is requested
 */
bool_t CO_LSSslave_process(CO_LSSslave_t *LSSslave);

/**
 * Get current LSS state
 *
 * @param LSSslave This object.
 * @return #CO_LSS_state_t
 */
static inline CO_LSS_state_t CO_LSSslave_getState(CO_LSSslave_t *LSSslave) {
    return (LSSslave == NULL) ? CO_LSS_STATE_WAITING : LSSslave->lssState;
}


#if ((CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize LSSslaveRx callback function.
 *
 * Function initializes optional callback function, which should immediately
 * start further LSS processing. Callback is called after LSS message is
 * received from the CAN bus. It should signal the RTOS to resume corresponding
 * task.
 *
 * @param LSSslave This object.
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can be NULL
 * @param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_LSSslave_initCallbackPre(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        void                  (*pFunctSignalPre)(void *object));
#endif


/**
 * Initialize verify bit rate callback
 *
 * Function initializes callback function, which is called when "config bit
 * timing parameters" is used. The callback function needs to check if the new bit
 * rate is supported by the CANopen device. Callback returns "true" if supported.
 * When no callback is set the LSS slave will no-ack the request, indicating to
 * the master that bit rate change is not supported.
 *
 * @param LSSslave This object.
 * @param object Pointer to object, which will be passed to pFunctLSScheckBitRate(). Can be NULL
 * @param pFunctLSScheckBitRate Pointer to the callback function. Not called if NULL.
 */
void CO_LSSslave_initCheckBitRateCallback(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        bool_t                (*pFunctLSScheckBitRate)(void *object, uint16_t bitRate));

/**
 * Initialize activate bit rate callback
 *
 * Function initializes callback function, which is called when "activate bit
 * timing parameters" is used. The callback function gives the user an event to
 * allow setting a timer or do calculations based on the exact time the request
 * arrived.
 * According to DSP 305 6.4.4, the delay has to be applied once before and once after
 * switching bit rates. During this time, a device mustn't send any messages.
 *
 * @param LSSslave This object.
 * @param object Pointer to object, which will be passed to pFunctLSSactivateBitRate(). Can be NULL
 * @param pFunctLSSactivateBitRate Pointer to the callback function. Not called if NULL.
 */
void CO_LSSslave_initActivateBitRateCallback(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        void                  (*pFunctLSSactivateBitRate)(void *object, uint16_t delay));

/**
 * Store configuration callback
 *
 * Function initializes callback function, which is called when "store configuration" is used.
 * The callback function gives the user an event to store the corresponding node id and bit rate
 * to NVM. Those values have to be supplied to the init function as "persistent values"
 * after reset. If callback returns "true", success is send to the LSS master. When no
 * callback is set the LSS slave will no-ack the request, indicating to the master
 * that storing is not supported.
 *
 * @param LSSslave This object.
 * @param object Pointer to object, which will be passed to pFunctLSScfgStore(). Can be NULL
 * @param pFunctLSScfgStore Pointer to the callback function. Not called if NULL.
 */
void CO_LSSslave_initCfgStoreCallback(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        bool_t                (*pFunctLSScfgStore)(void *object, uint8_t id, uint16_t bitRate));

/** @} */ /*@defgroup CO_LSSslave*/

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE */

#endif /*CO_LSSslave_H*/
