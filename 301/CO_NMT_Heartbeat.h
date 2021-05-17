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
#include "301/CO_ODinterface.h"
#include "301/CO_Emergency.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_NMT
#define CO_CONFIG_NMT (CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_GLOBAL_FLAG_TIMERNEXT)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_NMT_Heartbeat NMT and Heartbeat
 * CANopen Network management and Heartbeat producer protocol.
 *
 * @ingroup CO_CANopen_301
 * @{
 * CANopen device can be in one of the @ref CO_NMT_internalState_t
 *  - Initializing. It is active before CANopen is initialized.
 *  - Pre-operational. All CANopen objects are active, except PDOs.
 *  - Operational. Process data objects (PDOs) are active too.
 *  - Stopped. Only Heartbeat producer and NMT consumer are active.
 *
 * NMT master can change the internal state of the devices by sending
 * @ref CO_NMT_command_t.
 *
 * ### NMT message contents:
 *
 *   Byte | Description
 *   -----|-----------------------------------------------------------
 *     0  | @ref CO_NMT_command_t
 *     1  | Node ID. If zero, command addresses all nodes.
 *
 * ### Heartbeat message contents:
 *
 *   Byte | Description
 *   -----|-----------------------------------------------------------
 *     0  | @ref CO_NMT_internalState_t
 *
 * See @ref CO_Default_CAN_ID_t for CAN identifiers.
 */

/**
 * Internal network state of the CANopen node
 */
typedef enum {
    /** -1, Device state is unknown (for heartbeat consumer) */
    CO_NMT_UNKNOWN = -1,
    /** 0, Device is initializing */
    CO_NMT_INITIALIZING = 0,
    /** 127, Device is in pre-operational state */
    CO_NMT_PRE_OPERATIONAL = 127,
    /** 5, Device is in operational state */
    CO_NMT_OPERATIONAL = 5,
    /** 4, Device is stopped */
    CO_NMT_STOPPED = 4
} CO_NMT_internalState_t;


/**
 * Commands from NMT master.
 */
typedef enum {
    /** 1, Start device */
    CO_NMT_ENTER_OPERATIONAL = 1,
    /** 2, Stop device */
    CO_NMT_ENTER_STOPPED = 2,
    /** 128, Put device into pre-operational */
    CO_NMT_ENTER_PRE_OPERATIONAL = 128,
    /** 129, Reset device */
    CO_NMT_RESET_NODE = 129,
    /** 130, Reset CANopen communication on device */
    CO_NMT_RESET_COMMUNICATION = 130
} CO_NMT_command_t;


/**
 * Return code from CO_NMT_process() that tells application code what to
 * reset.
 */
typedef enum {
    /** 0, Normal return, no action */
    CO_RESET_NOT = 0,
    /** 1, Application must provide communication reset. */
    CO_RESET_COMM = 1,
    /** 2, Application must provide complete device reset */
    CO_RESET_APP = 2,
    /** 3, Application must quit, no reset of microcontroller (command is not
     * requested by the stack.) */
    CO_RESET_QUIT = 3
} CO_NMT_reset_cmd_t;


/**
 * NMT control bitfield for NMT internal state.
 *
 * Variable of this type is passed to @ref CO_NMT_init() function. It
 * controls behavior of the @ref CO_NMT_internalState_t of the device according
 * to CANopen error register.
 *
 * Internal NMT state is controlled also with external NMT command,
 * @ref CO_NMT_sendInternalCommand() or @ref CO_NMT_sendCommand() functions.
 */
typedef enum {
    /** First 8 bits can be used to specify bitmask for the
     * @ref CO_errorRegister_t, to get relevant bits for the calculation. */
    CO_NMT_ERR_REG_MASK = 0x00FFU,
    /** If bit is set, then device enters NMT operational state after the
     * initialization phase, otherwise it enters NMT pre-operational state. */
    CO_NMT_STARTUP_TO_OPERATIONAL = 0x0100U,
    /** If bit is set and device is operational, it enters NMT pre-operational
     * or stopped state, if CAN bus is off or heartbeat consumer timeout is
     * detected. */
    CO_NMT_ERR_ON_BUSOFF_HB = 0x1000U,
    /** If bit is set and device is operational, it enters NMT pre-operational
     * or stopped state, if masked CANopen error register is different than
     * zero. */
    CO_NMT_ERR_ON_ERR_REG = 0x2000U,
    /** If bit is set and CO_NMT_ERR_ON_xx condition is met, then device will
     * enter NMT stopped state, otherwise it will enter NMT pre-op state. */
    CO_NMT_ERR_TO_STOPPED = 0x4000U,
    /** If bit is set and device is pre-operational, it enters NMT operational
     * state automatically, if conditions from CO_NMT_ERR_ON_xx are all false.*/
    CO_NMT_ERR_FREE_TO_OPERATIONAL = 0x8000U
} CO_NMT_control_t;


/**
 * NMT consumer and Heartbeat producer object
 */
typedef struct {
    /** Current NMT operating state. */
    uint8_t operatingState;
    /** Previous NMT operating state. */
    uint8_t operatingStatePrev;
    /** NMT internal command from CO_NMT_receive() or CO_NMT_sendCommand(),
     * processed in CO_NMT_process(). 0 if no command or CO_NMT_command_t */
    uint8_t internalCommand;
    /** From CO_NMT_init() */
    uint8_t nodeId;
    /** From CO_NMT_init() */
    CO_NMT_control_t NMTcontrol;
    /** Producer heartbeat time, calculated from OD 0x1017 */
    uint32_t HBproducerTime_us;
    /** Internal timer for HB producer */
    uint32_t HBproducerTimer;
    /** Extension for OD object */
    OD_extension_t OD_1017_extension;
    /** From CO_NMT_init() */
    CO_EM_t *em;
#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER) || defined CO_DOXYGEN
    /** From CO_NMT_init() */
    CO_CANmodule_t *NMT_CANdevTx;
    /** CAN transmit buffer for NMT master message */
    CO_CANtx_t *NMT_TXbuff;
#endif
    /** From CO_NMT_init() */
    CO_CANmodule_t *HB_CANdevTx;
    /** CAN transmit buffer for heartbeat message */
    CO_CANtx_t *HB_TXbuff;
#if ((CO_CONFIG_NMT) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
    /** From CO_NMT_initCallbackPre() or NULL */
    void (*pFunctSignalPre)(void *object);
    /** From CO_NMT_initCallbackPre() or NULL */
    void *functSignalObjectPre;
#endif
#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE) || defined CO_DOXYGEN
    /** From CO_NMT_initCallbackChanged() or NULL */
    void (*pFunctNMT)(CO_NMT_internalState_t state);
#endif
} CO_NMT_t;


/**
 * Initialize NMT and Heartbeat producer object.
 *
 * Function must be called in the communication reset section.
 *
 * @param NMT This object will be initialized.
 * @param OD_1017_ProducerHbTime OD entry for 0x1017 -"Producer heartbeat time",
 * entry is required, IO extension is optional for runtime configuration.
 * @param em Emergency object.
 * @param nodeId CANopen Node ID of this device.
 * @param NMTcontrol Control variable for calculation of NMT internal state,
 * based on error register, startup and runtime behavior.
 * @param firstHBTime_ms Time between bootup and first heartbeat message in
 * milliseconds. If firstHBTime_ms is greater than "Producer Heartbeat time"
 * (OD object 0x1017), latter is used instead. Entry is required, IO extension
 * is optional.
 * @param NMT_CANdevRx CAN device for NMT reception.
 * @param NMT_rxIdx Index of receive buffer in above CAN device.
 * @param CANidRxNMT CAN identifier for NMT receive message.
 * @param NMT_CANdevTx CAN device for NMT master transmission.
 * @param NMT_txIdx Index of transmit buffer in above CAN device.
 * @param CANidTxNMT CAN identifier for NMT transmit message.
 * @param HB_CANdevTx CAN device for HB transmission.
 * @param HB_txIdx Index of transmit buffer in the above CAN device.
 * @param CANidTxHB CAN identifier for HB message.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_NMT_init(CO_NMT_t *NMT,
                             OD_entry_t *OD_1017_ProducerHbTime,
                             CO_EM_t *em,
                             uint8_t nodeId,
                             CO_NMT_control_t NMTcontrol,
                             uint16_t firstHBTime_ms,
                             CO_CANmodule_t *NMT_CANdevRx,
                             uint16_t NMT_rxIdx,
                             uint16_t CANidRxNMT,
#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER) || defined CO_DOXYGEN
                             CO_CANmodule_t *NMT_CANdevTx,
                             uint16_t NMT_txIdx,
                             uint16_t CANidTxNMT,
#endif
                             CO_CANmodule_t *HB_CANdevTx,
                             uint16_t HB_txIdx,
                             uint16_t CANidTxHB,
                             uint32_t *errInfo);


#if ((CO_CONFIG_NMT) & CO_CONFIG_FLAG_CALLBACK_PRE) || defined CO_DOXYGEN
/**
 * Initialize NMT callback function after message preprocessed.
 *
 * Function initializes optional callback function, which should immediately
 * start processing of CO_NMT_process() function.
 * Callback is called after NMT message is received from the CAN bus.
 *
 * @param NMT This object.
 * @param object Pointer to object, which will be passed to pFunctSignal().
 * Can be NULL
 * @param pFunctSignal Pointer to the callback function. Not called if NULL.
 */
void CO_NMT_initCallbackPre(CO_NMT_t *NMT,
                            void *object,
                            void (*pFunctSignal)(void *object));
#endif


#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE) || defined CO_DOXYGEN
/**
 * Initialize NMT callback function.
 *
 * Function initializes optional callback function, which is called after
 * NMT State change has occurred. Function may wake up external task which
 * handles NMT events. The first call is made immediately to give the consumer
 * the current NMT state.
 *
 * @param NMT This object.
 * @param pFunctNMT Pointer to the callback function. Not called if NULL.
 */
void CO_NMT_initCallbackChanged(CO_NMT_t *NMT,
                                void (*pFunctNMT)(CO_NMT_internalState_t state));
#endif


/**
 * Process received NMT and produce Heartbeat messages.
 *
 * Function must be called cyclically.
 *
 * @param NMT This object.
 * @param [out] NMTstate If not NULL, CANopen NMT internal state is returned.
 * @param timeDifference_us Time difference from previous function call in
 * microseconds.
 * @param [out] timerNext_us info to OS - see CO_process().
 *
 * @return #CO_NMT_reset_cmd_t
 */
CO_NMT_reset_cmd_t CO_NMT_process(CO_NMT_t *NMT,
                                  CO_NMT_internalState_t *NMTstate,
                                  uint32_t timeDifference_us,
                                  uint32_t *timerNext_us);


/**
 * Query current NMT state
 *
 * @param NMT This object.
 *
 * @return @ref CO_NMT_internalState_t
 */
static inline CO_NMT_internalState_t CO_NMT_getInternalState(CO_NMT_t *NMT) {
    return (NMT == NULL) ? CO_NMT_INITIALIZING : (CO_NMT_internalState_t)NMT->operatingState;
}


/**
 * Send NMT command to self, without sending NMT message
 *
 * Internal NMT state will be verified and switched inside @ref CO_NMT_process()
 *
 * @param NMT This object.
 * @param command NMT command
 */
static inline void CO_NMT_sendInternalCommand(CO_NMT_t *NMT,
                                              CO_NMT_command_t command)
{
    if (NMT != NULL) NMT->internalCommand = (uint8_t)command;
}


#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER) || defined CO_DOXYGEN
/**
 * Send NMT master command.
 *
 * This functionality may only be used from NMT master, as specified by
 * standard CiA302-2. Standard provides one exception, where application from
 * slave node may send NMT master command: "If CANopen object 0x1F80 has value
 * of **0x2**, then NMT slave shall execute the NMT service start remote node
 * (CO_NMT_ENTER_OPERATIONAL) with nodeID set to 0."
 *
 * @param NMT This object.
 * @param command NMT command from CO_NMT_command_t.
 * @param nodeID Node ID of the remote node. 0 for all nodes including self.
 *
 * @return CO_ERROR_NO on success or CO_ReturnError_t from CO_CANsend().
 */
CO_ReturnError_t CO_NMT_sendCommand(CO_NMT_t *NMT,
                                    CO_NMT_command_t command,
                                    uint8_t nodeID);

#endif /* (CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER */

/** @} */ /* CO_NMT_Heartbeat */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_NMT_HEARTBEAT_H */
