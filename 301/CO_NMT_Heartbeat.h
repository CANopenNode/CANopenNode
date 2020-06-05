/**
 * CANopen Network management and Heartbeat producer protocol.
 *
 * @file        CO_NMT_Heartbeat.h
 * @ingroup     CO_NMT_Heartbeat
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


#ifndef CO_NMT_HEARTBEAT_H
#define CO_NMT_HEARTBEAT_H

#include "301/CO_driver.h"
#include "301/CO_SDOserver.h"
#include "301/CO_Emergency.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_NMT_Heartbeat NMT and Heartbeat
 * @ingroup CO_CANopen_301
 * @{
 *
 * CANopen Network management and Heartbeat producer protocol.
 *
 * CANopen device can be in one of the #CO_NMT_internalState_t
 *  - Initializing. It is active before CANopen is initialized.
 *  - Pre-operational. All CANopen objects are active, except PDOs.
 *  - Operational. Process data objects (PDOs) are active too.
 *  - Stopped. Only Heartbeat producer and NMT consumer are active.
 *
 * NMT master can change the internal state of the devices by sending
 * #CO_NMT_command_t.
 *
 * ###NMT message contents:
 *
 *   Byte | Description
 *   -----|-----------------------------------------------------------
 *     0  | #CO_NMT_command_t
 *     1  | Node ID. If zero, command addresses all nodes.
 *
 * ###Heartbeat message contents:
 *
 *   Byte | Description
 *   -----|-----------------------------------------------------------
 *     0  | #CO_NMT_internalState_t
 *
 * @see #CO_Default_CAN_ID_t
 */


/**
 * Internal network state of the CANopen node
 */
typedef enum{
    CO_NMT_UNKNOWN                  = -1,   /**< Device state is unknown (for heartbeat consumer) */
    CO_NMT_INITIALIZING             = 0,    /**< Device is initializing */
    CO_NMT_PRE_OPERATIONAL          = 127,  /**< Device is in pre-operational state */
    CO_NMT_OPERATIONAL              = 5,    /**< Device is in operational state */
    CO_NMT_STOPPED                  = 4     /**< Device is stopped */
}CO_NMT_internalState_t;


/**
 * Commands from NMT master.
 */
typedef enum{
    CO_NMT_ENTER_OPERATIONAL        = 1,    /**< Start device */
    CO_NMT_ENTER_STOPPED            = 2,    /**< Stop device */
    CO_NMT_ENTER_PRE_OPERATIONAL    = 128,  /**< Put device into pre-operational */
    CO_NMT_RESET_NODE               = 129,  /**< Reset device */
    CO_NMT_RESET_COMMUNICATION      = 130   /**< Reset CANopen communication on device */
}CO_NMT_command_t;


/**
 * Return code for CO_NMT_process() that tells application code what to
 * reset.
 */
typedef enum{
    CO_RESET_NOT  = 0,/**< Normal return, no action */
    CO_RESET_COMM = 1,/**< Application must provide communication reset. */
    CO_RESET_APP  = 2,/**< Application must provide complete device reset */
    CO_RESET_QUIT = 3 /**< Application must quit, no reset of microcontroller (command is not requested by the stack.) */
}CO_NMT_reset_cmd_t;


/**
 * NMT consumer and Heartbeat producer object, initialized by CO_NMT_init()
 */
typedef struct{
    CO_NMT_internalState_t operatingState; /**< Current NMT operating state. */
    CO_NMT_internalState_t operatingStatePrev; /**< Previous NMT operating state. */
    uint8_t             resetCommand;   /**< If different than zero, device will reset */
    uint8_t             nodeId;         /**< CANopen Node ID of this device */
    uint32_t            HBproducerTimer;/**< Internal timer for HB producer */
    uint32_t            firstHBTime;    /**< From CO_NMT_init() */
    CO_EMpr_t          *emPr;           /**< From CO_NMT_init() */
#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER) || defined CO_DOXYGEN
    CO_CANmodule_t     *NMT_CANdevTx;   /**< From CO_NMT_init() */
    CO_CANtx_t         *NMT_TXbuff;     /**< CAN transmit buffer for NMT master message */
#endif
    CO_CANmodule_t     *HB_CANdevTx;    /**< From CO_NMT_init() */
    CO_CANtx_t         *HB_TXbuff;      /**< CAN transmit buffer for heartbeat message */
#if ((CO_CONFIG_NMT) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_NMT_initCallbackPre() or NULL */
    void              (*pFunctSignalPre)(void *object);
    /** From CO_NMT_initCallbackPre() or NULL */
    void               *functSignalObjectPre;
#endif
#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE) || defined CO_DOXYGEN
    void              (*pFunctNMT)(CO_NMT_internalState_t state); /**< From CO_NMT_initCallbackChanged() or NULL */
#endif
}CO_NMT_t;


/**
 * Initialize NMT and Heartbeat producer object.
 *
 * Function must be called in the communication reset section.
 *
 * @param NMT This object will be initialized.
 * @param emPr Emergency main object.
 * @param nodeId CANopen Node ID of this device.
 * @param firstHBTime_ms Time between bootup and first heartbeat message in milliseconds.
 * If firstHBTime is greater than _Producer Heartbeat time_
 * (object dictionary, index 0x1017), latter is used instead.
 * @param NMT_CANdevRx CAN device for NMT reception.
 * @param NMT_rxIdx Index of receive buffer in above CAN device.
 * @param CANidRxNMT CAN identifier for NMT receive message.
 * @param NMT_CANdevTx CAN device for NMT master transmission.
 * @param NMT_txIdx Index of transmit buffer in above CAN device.
 * @param CANidTxNMT CAN identifier for NMT transmit message.
 * @param HB_CANdevTx CAN device for HB transmission.
 * @param HB_txIdx Index of transmit buffer in the above CAN device.
 * @param CANidTxHB CAN identifier for HB message.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_NMT_init(
        CO_NMT_t               *NMT,
        CO_EMpr_t              *emPr,
        uint8_t                 nodeId,
        uint16_t                firstHBTime_ms,
        CO_CANmodule_t         *NMT_CANdevRx,
        uint16_t                NMT_rxIdx,
        uint16_t                CANidRxNMT,
        CO_CANmodule_t         *NMT_CANdevTx,
        uint16_t                NMT_txIdx,
        uint16_t                CANidTxNMT,
        CO_CANmodule_t         *HB_CANdevTx,
        uint16_t                HB_txIdx,
        uint16_t                CANidTxHB);


#if ((CO_CONFIG_NMT) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize NMT callback function after message preprocessed.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_NMT_process() function.
 * Callback is called after NMT message is received from the CAN bus.
 *
 * @param NMT This object.
 * @param object Pointer to object, which will be passed to pFunctSignal(). Can be NULL
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_NMT_initCallbackPre(
        CO_NMT_t               *NMT,
        void                   *object,
        void                  (*pFunctSignal)(void *object));
#endif


#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE) || defined CO_DOXYGEN
/**
 * Initialize NMT callback function.
 *
 * Function initializes optional callback function, which is called after
 * NMT State change has occured. Function may wake up external task which
 * handles NMT events.
 * The first call is made immediately to give the consumer the current NMT state.
 *
 * @remark Be aware that the callback function is run inside the CAN receive
 * function context. Depending on the driver, this might be inside an interrupt!
 *
 * @param NMT This object.
 * @param pFunctNMT Pointer to the callback function. Not called if NULL.
 */
void CO_NMT_initCallbackChanged(
        CO_NMT_t               *NMT,
        void                  (*pFunctNMT)(CO_NMT_internalState_t state));
#endif


/**
 * Process received NMT and produce Heartbeat messages.
 *
 * Function must be called cyclically.
 *
 * @param NMT This object.
 * @param timeDifference_us Time difference from previous function call in [microseconds].
 * @param HBtime_ms _Producer Heartbeat time_ (object dictionary, index 0x1017).
 * @param NMTstartup _NMT startup behavior_ (object dictionary, index 0x1F80).
 * @param errorRegister _Error register_ (object dictionary, index 0x1001).
 * @param errorBehavior pointer to _Error behavior_ array (object dictionary, index 0x1029).
 *        Object controls, if device should leave NMT operational state.
 *        Length of array must be 6. If pointer is NULL, no calculation is made.
 * @param [out] timerNext_us info to OS - see CO_process().
 *
 * @return #CO_NMT_reset_cmd_t
 */
CO_NMT_reset_cmd_t CO_NMT_process(
        CO_NMT_t               *NMT,
        uint32_t                timeDifference_us,
        uint16_t                HBtime_ms,
        uint32_t                NMTstartup,
        uint8_t                 errorRegister,
        const uint8_t           errorBehavior[],
        uint32_t               *timerNext_us);


/**
 * Query current NMT state
 *
 * @param NMT This object.
 *
 * @return #CO_NMT_internalState_t
 */
static inline CO_NMT_internalState_t CO_NMT_getInternalState(CO_NMT_t *NMT) {
    return (NMT == NULL) ? CO_NMT_INITIALIZING : NMT->operatingState;
}


#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER) || defined CO_DOXYGEN
/**
 * Send NMT master command.
 *
 * This functionality can only be used from NMT master. There is one exception
 * where application from slave node may send NMT master command: If CANopen
 * object 0x1F80 has value of **0x2**, then NMT slave shall execute the NMT
 * service start remote node (CO_NMT_ENTER_OPERATIONAL) with nodeID set to 0.
 *
 * @param NMT This object.
 * @param command NMT command from CO_NMT_command_t.
 * @param nodeID Node ID of the remote node. 0 for all nodes including self.
 *
 * @return 0: Operation completed successfully.
 * @return other: same as CO_CANsend().
 */
CO_ReturnError_t CO_NMT_sendCommand(CO_NMT_t *NMT,
                                    CO_NMT_command_t command,
                                    uint8_t nodeID);
#endif

/** @} */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif
