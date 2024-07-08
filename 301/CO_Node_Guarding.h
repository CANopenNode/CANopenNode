/**
 * CANopen Node Guarding slave and master objects.
 *
 * @file        CO_Node_Guarding.h
 * @ingroup     CO_Node_Guarding
 * @author      Janez Paternoster
 * @copyright   2023 Janez Paternoster
 *
 * This file is part of <https://github.com/CANopenNode/CANopenNode>, a CANopen Stack.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */

#ifndef CO_NODE_GUARDING_H
#define CO_NODE_GUARDING_H

#include "301/CO_driver.h"
#include "301/CO_ODinterface.h"
#include "301/CO_Emergency.h"
#include "301/CO_NMT_Heartbeat.h"

/* default configuration, see CO_config.h */
#ifndef CO_CONFIG_NODE_GUARDING
#define CO_CONFIG_NODE_GUARDING (0)
#endif
#ifndef CO_CONFIG_NODE_GUARDING_MASTER_COUNT
#define CO_CONFIG_NODE_GUARDING_MASTER_COUNT 0x7F
#endif

#if (((CO_CONFIG_NODE_GUARDING)&CO_CONFIG_NODE_GUARDING_SLAVE_ENABLE) != 0) || defined CO_DOXYGEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_Node_Guarding Node Guarding CANopen Node Guarding, an older alternative to the Heartbeat protocol.
 *
 * @ingroup CO_CANopen_301
 * @{
 * Node guarding master pools each node guarding slave at time intervals, called guard time. Master sends a CAN RTR
 * message, and slave responds. Slave also monitors presence of RTR message from master and indicates error, if it
 * wasn't received within life time. ('Life time' is 'Guard time' multiplied by 'Life time factor').
 *
 * Adding Node guarding to the project:
 *  - Make sure, driver supports it. RTR bit should be part of CAN identifier.
 *  - Enable it with 'CO_CONFIG_NODE_GUARDING', see CO_config.h
 *  - For slave add 0x100C and 0x100D objects to the Object dictionary.
 *  - For master use CO_nodeGuardingMaster_initNode() to add monitored nodes.
 *
 * @warning Usage of Node guarding is not recommended, as it is outdated and uses RTR CAN functionality, which is also
 * not recommended. Use Heartbeat and Heartbeat consumer, if possible.
 *
 * ### Node Guarding slave response message contents:
 *
 *   Byte, bits     | Description
 *   ---------------|-----------------------------------------------------------
 *     0, bits 0..6 | @ref CO_NMT_internalState_t
 *     0, bit 7     | toggle bit
 *
 * See @ref CO_Default_CAN_ID_t for CAN identifiers.
 */

/**
 * Node Guarding slave object
 */
typedef struct {
    CO_EM_t* em;                      /**< From CO_nodeGuardingSlave_init() */
    volatile void* CANrxNew;          /**< Indicates, if new rtr message received from CAN bus */
    uint32_t guardTime_us;            /**< Guard time in microseconds, calculated from OD_0x100C */
    uint32_t lifeTime_us;             /**< Life time in microseconds, calculated from guardTime_us * lifeTimeFactor */
    uint32_t lifeTimer;               /**< Timer for monitoring Life time, counting down from lifeTime_us. */
    uint8_t lifeTimeFactor;           /**< Life time factor, from OD_0x100D */
    bool_t toggle;                    /**< Toggle bit for response */
    bool_t lifeTimeTimeout;           /**< True if rtr from master is missing */
    OD_extension_t OD_100C_extension; /**< Extension for OD object */
    OD_extension_t OD_100D_extension; /**< Extension for OD object */
    CO_CANmodule_t* CANdevTx;         /**< From CO_nodeGuardingSlave_init() */
    CO_CANtx_t* CANtxBuff;            /**< CAN transmit buffer for the message */
} CO_nodeGuardingSlave_t;

/**
 * Initialize Node Guarding slave object.
 *
 * Function must be called in the communication reset section.
 *
 * @param ngs This object will be initialized.
 * @param OD_100C_GuardTime OD entry for 0x100C -"Guard time", entry is required.
 * @param OD_100D_LifeTimeFactor OD entry for 0x100D -"Life time factor", entry is required.
 * @param em Emergency object.
 * @param CANidNodeGuarding CAN identifier for Node Guarding rtr and response message (usually CO_CAN_ID_HEARTBEAT +
 * nodeId).
 * @param CANdevRx CAN device for Node Guarding rtr reception.
 * @param CANdevRxIdx Index of the receive buffer in the above CAN device.
 * @param CANdevTx CAN device for Node Guarding response transmission.
 * @param CANdevTxIdx Index of the transmit buffer in the above CAN device.
 * @param [out] errInfo Additional information in case of error, may be NULL.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_nodeGuardingSlave_init(CO_nodeGuardingSlave_t* ngs, OD_entry_t* OD_100C_GuardTime,
                                           OD_entry_t* OD_100D_LifeTimeFactor, CO_EM_t* em, uint16_t CANidNodeGuarding,
                                           CO_CANmodule_t* CANdevRx, uint16_t CANdevRxIdx, CO_CANmodule_t* CANdevTx,
                                           uint16_t CANdevTxIdx, uint32_t* errInfo);

/**
 * Process Node Guarding slave.
 *
 * Function must be called cyclically.
 *
 * @param ngs This object.
 * @param NMTstate NMT operating state.
 * @param slaveDisable If true, then Node guarding slave is disabled.
 * @param timeDifference_us Time difference from previous function call in microseconds.
 * @param [out] timerNext_us info to OS - see CO_process().
 */
void CO_nodeGuardingSlave_process(CO_nodeGuardingSlave_t* ngs, CO_NMT_internalState_t NMTstate, bool_t slaveDisable,
                                  uint32_t timeDifference_us, uint32_t* timerNext_us);

/**
 * Inquire, if Node guarding slave detected life time timeout
 *
 * Error is reset after pool request from master.
 *
 * @param ngs This object.
 *
 * @return true, if life time timeout was detected.
 */
static inline bool_t
CO_nodeGuardingSlave_isTimeout(CO_nodeGuardingSlave_t* ngs) {
    return (ngs == NULL) || ngs->lifeTimeTimeout;
}

/** @} */ /* CO_Node_Guarding */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_NODE_GUARDING) & CO_CONFIG_NODE_GUARDING_SLAVE_ENABLE */

#if (((CO_CONFIG_NODE_GUARDING)&CO_CONFIG_NODE_GUARDING_MASTER_ENABLE) != 0) || defined CO_DOXYGEN

#if CO_CONFIG_NODE_GUARDING_MASTER_COUNT < 1 || CO_CONFIG_NODE_GUARDING_MASTER_COUNT > 127
#error CO_CONFIG_NODE_GUARDING_MASTER_COUNT value is wrong!
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup CO_Node_Guarding
 * @{
 */

/**
 * Node Guarding master - monitored node
 */
typedef struct {
    uint32_t guardTime_us;           /**< Guard time in microseconds */
    uint32_t guardTimer;             /**< Guard timer in microseconds, counting down */
    uint16_t ident;                  /**< CAN identifier (CO_CAN_ID_HEARTBEAT + Node Id) */
    CO_NMT_internalState_t NMTstate; /**< NMT operating state */
    uint8_t toggle;                  /**< toggle bit7, expected from the next received message */
    bool_t responseRecived;          /**< True, if response was received since last rtr message */
    bool_t CANtxWasBusy;             /**< True, if CANtxBuff was busy since last processing */
    bool_t monitoringActive;         /**< True, if monitoring is active (response within time). */
} CO_nodeGuardingMasterNode_t;

/**
 * Node Guarding master object
 */
typedef struct {
    CO_EM_t* em;               /**< From CO_nodeGuardingMaster_init() */
    CO_CANmodule_t* CANdevTx;  /**< From CO_nodeGuardingMaster_init() */
    uint16_t CANdevTxIdx;      /**< From CO_nodeGuardingMaster_init() */
    CO_CANtx_t* CANtxBuff;     /**< CAN transmit buffer for the message */
    bool_t allMonitoredActive; /**< True, if all monitored nodes are active or no node is monitored. Can be read by the
                                  application */
    bool_t allMonitoredOperational; /**< True, if all monitored nodes are NMT operational or no node is monitored. Can
                                       be read by the application */
    CO_nodeGuardingMasterNode_t nodes[CO_CONFIG_NODE_GUARDING_MASTER_COUNT]; /**< Array of monitored nodes */
} CO_nodeGuardingMaster_t;

/**
 * Initialize Node Guarding master object.
 *
 * Function must be called in the communication reset section.
 *
 * @param ngm This object will be initialized.
 * @param em Emergency object.
 * @param CANdevRx CAN device for Node Guarding reception.
 * @param CANdevRxIdx Index of the receive buffer in the above CAN device.
 * @param CANdevTx CAN device for Node Guarding rtr transmission.
 * @param CANdevTxIdx Index of the transmit buffer in the above CAN device.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_nodeGuardingMaster_init(CO_nodeGuardingMaster_t* ngm, CO_EM_t* em, CO_CANmodule_t* CANdevRx,
                                            uint16_t CANdevRxIdx, CO_CANmodule_t* CANdevTx, uint16_t CANdevTxIdx);

/**
 * Initialize node inside Node Guarding master object.
 *
 * Function may be called any time after CO_nodeGuardingMaster_init(). It configures monitoring of the remote node.
 *
 * @param ngm Node Guarding master object.
 * @param index Index of the slot, which will be configured. 0 <= index < CO_CONFIG_NODE_GUARDING_MASTER_COUNT.
 * @param nodeId Node Id of the monitored node.
 * @param guardTime_ms Guard time of the monitored node.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO on success.
 */
CO_ReturnError_t CO_nodeGuardingMaster_initNode(CO_nodeGuardingMaster_t* ngm, uint8_t index, uint8_t nodeId,
                                                uint16_t guardTime_ms);

/**
 * Process Node Guarding master.
 *
 * Function must be called cyclically.
 *
 * @param ngm This object.
 * @param timeDifference_us Time difference from previous function call in microseconds.
 * @param [out] timerNext_us info to OS - see CO_process().
 */
void CO_nodeGuardingMaster_process(CO_nodeGuardingMaster_t* ngm, uint32_t timeDifference_us, uint32_t* timerNext_us);

/** @} */ /* @addtogroup CO_Node_Guarding */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* (CO_CONFIG_NODE_GUARDING) & CO_CONFIG_NODE_GUARDING_MASTER_ENABLE */

#endif /* CO_NODE_GUARDING_H */
